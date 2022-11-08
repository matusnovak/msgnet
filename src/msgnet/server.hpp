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

    Server(unsigned int port, const Pkey& pkey, const Dh& ec, const Cert& cert);

    ~Server();

    void start(bool async = true);

    asio::io_service& getIoService() {
        return service;
    }

    void stop();

    void postDispatch(std::function<void()> fn) override;

    virtual void onAcceptSuccess(const std::shared_ptr<Peer>& peer);

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
