#pragma once

#define ASIO_STANDALONE

#include "error.hpp"
#include "message.hpp"
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace MsgNet {
class MSGNET_API Dispatcher;

class MSGNET_API Peer : public std::enable_shared_from_this<Peer> {
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

    void start();

    void close();

    void sendPacket(std::shared_ptr<msgpack::sbuffer> packet);
    void sendBuffer(std::shared_ptr<std::vector<char>> buffer);

    bool isConnected();

    const std::string& getAddress() const {
        return address;
    }

    template <typename Req> void send(const Req& message, uint64_t reqId, bool isResponse) {
        PacketInfo info;

        info.id = Req::hash;
        info.reqId = reqId;
        info.isResponse = isResponse;

        const auto packet = std::make_shared<msgpack::sbuffer>();

        msgpack::packer<msgpack::sbuffer> packer{*packet};
        packer.pack_array(2);
        packer.pack(info);
        packer.pack(message);

        sendPacket(packet);
    }

    template <typename Req> void send(const Req& message) {
        send<Req>(message, 0, false);
    }

    template <typename Req, typename Fn> void send(const Req& message, Fn fn) {
        using Res = typename Traits<decltype(&Fn::operator())>::Arg;
        sendInternal<Req, Res, Fn>(message, std::forward<Fn>(fn));
    }

    void handle(uint64_t reqId, const msgpack::object& object);

private:
    struct Lz4;

    struct Handler {
        Callback callback;
    };

    void receive();
    void receiveObject(std::shared_ptr<msgpack::object_handle> oh);

    template <typename Req, typename Res, typename Fn> void sendInternal(const Req& message, Fn fn) {
        const auto reqId = nextRequestId.fetch_add(1ULL);

        Handler handler{};
        handler.callback = [fn](const msgpack::object& object) {
            Res res{};
            object.convert(res);

            fn(std::move(res));
        };

        {
            std::lock_guard<std::mutex> lock{mutex};
            requests[reqId] = std::move(handler);
        }

        send(message, reqId, false);
    }

    ErrorHandler& errorHandler;
    Dispatcher& dispatcher;
    std::unique_ptr<Lz4> lz4;
    asio::io_context::strand strand;
    msgpack::unpacker unp;
    std::shared_ptr<Socket> socket;
    std::string address;

    std::atomic_uint64_t nextRequestId{0};
    std::mutex mutex;
    std::unordered_map<uint64_t, Handler> requests;
};

MSGNET_API std::string toString(const asio::ip::tcp::endpoint& endpoint);
} // namespace MsgNet
