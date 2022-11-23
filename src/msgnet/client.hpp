#pragma once

#include "cert.hpp"
#include "dispatcher.hpp"
#include "peer.hpp"
#include <thread>

namespace MsgNet {
class MSGNET_API Client : public ErrorHandler, public Dispatcher {
public:
    using Socket = asio::ssl::stream<asio::ip::tcp::socket>;

    /**
     * Constructs a client.
     * To start the client you must call start() method.
     */
    Client();
    ~Client();

    /**
     * Connects to the remote server.
     * @warnin You must call the start() method before trying to connect to the server.
     * @param address The address (an IP address or hostname) of the remote server.
     * @param port Port of the remote server.
     * @param timeout Connection timeout, this includes timeout for the TLS handshake.
     */
    void connect(const std::string& address, unsigned int port, int timeout = 5000);

    /**
     * Starts the client either in async or sync mode. When set to true (default) the client will start
     * in its own thread and all handlers and request callbacks will be handled by this one thread.
     * If you wish to run the client in a thread pool, pass false into this function, and use getIoService()
     * to call asio::io_service::run method on it from each thread.
     *
     * @note This function is non blocking! This has the exact same behavior as the Server::start() method.
     *
     * @param async Should start within own thread?
     */
    void start(bool async = true);

    /**
     * Returns a asio io service that is used to run all reads and writes on the sockets.
     *
     * @return A reference to the underlying asio::io_service context.
     */
    asio::io_service& getIoService() {
        return service;
    }

    /**
     * Stops the client. This is also called automatically from the destructor.
     * Calling this multiple times is allowed.
     */
    void stop();

    /**
     * Returns the address of the remote server.
     *
     * @return Address with port in string format. The IPv6 will be formatted as "[address]:port"
     */
    const std::string& getAddress() const;

    /**
     * Set the TLS verify callback. The callback must return true or false to accept or deny
     * the server certificate, respectively.
     *
     * @param callback The function that will handle the server TLS handshake certificate.
     */
    void setVerifyCallback(std::function<bool(Cert)> callback);

    /**
     * Returns true if the client is connected to the server.
     *
     * @return True if connected.
     */
    bool isConnected();

    /**
     * Send some message to the server.
     *
     * @tparam Req The type of the message to send. This is auto deduced from the parameter.
     * @param message The message to send to the server.
     */
    template <typename Req> void send(const Req& message) {
        if (peer) {
            peer->send<Req>(message);
        }
    }

    /**
     * Send some message to the server, with a callback method. This send the message as a request.
     * The server responds back a response message to the request. Once the response is received the
     * callback is executed with the response message.
     * The server handler must produce a response message (by return value of the handler).
     * If the server handler does not produce the message the callback of this request will not be executed.
     *
     * @note The callback is executed on the client's I/O thread if start() is called with true. If the start()
     * is called with false, then the callback is executed by the thread that
     * calls getIoService().run() from this client.
     * Alternatively, you can capture the work in the "postDispatch" and move the work to some thread.
     * Executing that work (the fn) will call the callback in this request.
     *
     * @tparam Req The type of the message to send. This is auto deduced from the parameter.
     * @param message The message to send to the server.
     */
    template <typename Req, typename Fn> void send(const Req& message, Fn fn) {
        if (peer) {
            peer->send<Req, Fn>(message, std::forward<Fn>(fn));
        }
    }

protected:
    /**
     * This function is executed every time there is some work to be done.
     * Such work can be a request message that needs to execute some handler function,
     * or some callback function from a request.
     * If you wish to use multiple threads, one for network I/O and one (or more) for handling the messages,
     * you can override this function and forward the function fn to any thread you wish to use.
     * This could also be used to synchronize with the main thread (rendering thread in a game, for example).
     * @param fn The function that wraps the work needed to execute some handler or some callback request function.
     */
    void postDispatch(std::function<void()> fn) override;

private:
    asio::io_service service;
    std::unique_ptr<asio::io_service::work> work;
    asio::ssl::context ssl;
    std::shared_ptr<Socket> socket;
    std::shared_ptr<Peer> peer;
    std::thread thread;
};
} // namespace MsgNet
