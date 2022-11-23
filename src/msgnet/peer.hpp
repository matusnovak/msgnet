#pragma once

#define ASIO_STANDALONE

#include "error.hpp"
#include "message.hpp"
#include "stream.hpp"
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace MsgNet {
class MSGNET_API Dispatcher;

class MSGNET_API Peer : public CompressionStream,
                        public DecompressionStream,
                        public std::enable_shared_from_this<Peer> {
public:
    using Socket = asio::ssl::stream<asio::ip::tcp::socket>;

    template <typename F> struct Traits;

    template <typename C, typename T> struct Traits<void (C::*)(T) const> {
        using Arg = T;
    };

    using Callback = std::function<void(const msgpack::object& object)>;

    explicit Peer(ErrorHandler& errorHandler, Dispatcher& dispatcher, asio::io_service& service,
                  std::shared_ptr<Socket> socket);

    ~Peer();

    /**
     * Starts the peer receive I/O loop. Internal use only.
     *
     * @warning Do not call this method! This method is called by the server or the client!
     */
    void start();

    /**
     * Closes the peer. This will shutdown the TLS and the socket. This shutdown will be executed
     * asynchronously. The peer may stay connected to the remote client/server until the async
     * shutdown is completed by the I/O thread.
     */
    void close();

    /**
     * Returns true if the peer is connected to the server.
     *
     * @return True if connected.
     */
    bool isConnected();

    /**
     * Returns the address of the remote server or client.
     *
     * @return Address with port in string format. The IPv6 will be formatted as "[address]:port"
     */
    const std::string& getAddress() const {
        return address;
    }

    /**
     * Internal use only, do not call.
     */
    template <typename Req> void send(const Req& message, uint64_t reqId, bool isResponse) {
        // Only one thread can write to the compression stream at the time.
        std::lock_guard<std::mutex> lock{mutex};

        PacketInfo info;

        info.id = Req::hash;
        info.reqId = reqId;
        info.isResponse = isResponse;

        msgpack::packer<CompressionStream> packer{*this};
        packer.pack_array(2);
        packer.pack(info);
        packer.pack(message);
        flush();
    }

    /**
     * Send some message to the server/client.
     *
     * @tparam Req The type of the message to send. This is auto deduced from the parameter.
     * @param message The message to send to the server.
     */
    template <typename Req> void send(const Req& message) {
        send<Req>(message, 0, false);
    }

    /**
     * Send some message to the server/client, with a callback method. This send the message as a request.
     * The server/client responds back a response message to the request. Once the response is received the
     * callback is executed with the response message.
     * The server/client handler must produce a response message (by return value of the handler).
     * If the server handler does not produce the message the callback of this request will not be executed.
     *
     * @note The callback is executed on the client's I/O thread if start() is called with true. If the start()
     * is called with false, then the callback is executed by the thread that
     * calls getIoService().run() from this client.
     * Alternatively, you can capture the work in the "postDispatch" and move the work to some thread.
     * Executing that work (the fn) will call the callback in this request.
     *
     * @tparam Req The type of the message to send. This is auto deduced from the parameter.
     * @param message The message to send to the server/client.
     */
    template <typename Req, typename Fn> void send(const Req& message, Fn fn) {
        using Res = typename Traits<decltype(&Fn::operator())>::Arg;
        sendInternal<Req, Res, Fn>(message, std::forward<Fn>(fn));
    }

private:
    struct Handler {
        Callback callback;
    };

    void sendBuffer(std::shared_ptr<std::vector<char>> buffer) override;
    void handle(uint64_t reqId, const msgpack::object& object);
    void receive();
    void receiveObject(std::shared_ptr<msgpack::object_handle> oh) override;

    template <typename Req, typename Res, typename Fn> void sendInternal(const Req& message, Fn fn) {
        const auto reqId = requests.nextId.fetch_add(1ULL);

        Handler handler{};
        handler.callback = [fn](const msgpack::object& object) {
            Res res{};
            object.convert(res);

            fn(std::move(res));
        };

        {
            std::lock_guard<std::mutex> lock{requests.mutex};
            requests.map[reqId] = std::move(handler);
        }

        send(message, reqId, false);
    }

    ErrorHandler& errorHandler;
    Dispatcher& dispatcher;
    asio::io_context::strand strand;
    std::shared_ptr<Socket> socket;
    std::string address;
    std::vector<char> receiveBuffer;
    std::mutex mutex;

    struct {
        std::atomic_uint64_t nextId{0};
        std::mutex mutex;
        std::unordered_map<uint64_t, Handler> map;
    } requests;
};

MSGNET_API std::string toString(const asio::ip::tcp::endpoint& endpoint);
} // namespace MsgNet
