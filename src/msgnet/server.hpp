#pragma once

#include "cert.hpp"
#include "dh.hpp"
#include "dispatcher.hpp"
#include "peer.hpp"
#include "pkey.hpp"
#include <thread>

namespace MsgNet {
class MSGNET_API Server : public ErrorHandler, public Dispatcher {
public:
    using Socket = asio::ssl::stream<asio::ip::tcp::socket>;

    /**
     * Construct a TCP server. The server won't start on its own. You must call start() method.
     *
     * @param port The port to start the server at.
     * @param pkey Private key;
     * @param ec Diffie-Hellman parameters.
     * @param cert Certificate for the private key.
     */
    Server(unsigned int port, const Pkey& pkey, const Dh& ec, const Cert& cert);
    ~Server();

    /**
     * Starts the server either in async or sync mode. When set to true (default) the server will start
     * in its own thread and all handlers and request callbacks will be handled by this one thread.
     * If you wish to run the server in a thread pool, pass false into this function, and use getIoService()
     * to call asio::io_service::run method on it from each thread.
     *
     * @note This function is non blocking! This has the exact same behavior as the Client::start() method.
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
     * Stops the server. This is also called automatically from the destructor.
     * Calling this multiple times is allowed.
     */
    void stop();

protected:
    /**
     * This function is executed every time there is some work to be done.
     * Such work can be a request message that needs to execute some handler function,
     * or some callback function from a request.
     * If you wish to use multiple threads, one for network I/O and one (or more) for handling the messages,
     * you can override this function and forward the function fn to any thread you wish to use.
     * This could also be used to synchronize with the main thread (rendering thread in a game, for example).
     *
     * @param fn The function that wraps the work needed to execute some handler or some callback request function.
     */
    void postDispatch(std::function<void()> fn) override;

    /**
     * Called every time a new peer is connected to the server.
     * This is called after the TLS handshake has been completed.
     *
     * @param peer Shared pointer to the peer.
     */
    virtual void onAcceptSuccess(std::shared_ptr<Peer> peer);

private:
    static asio::ip::tcp::endpoint getEndpoint(unsigned int port);

    void accept();
    void handshake(const std::shared_ptr<Socket>& socket, const std::shared_ptr<Peer>& peer);

    asio::io_service service;
    asio::ssl::context ssl;
    asio::ip::tcp::acceptor acceptor;
    std::thread thread;
};
} // namespace MsgNet
