#include "server.hpp"
#include <iostream>

using namespace MsgNet;

Server::Server(unsigned int port, const Pkey& pkey, const Dh& ec, const Cert& cert) :
    Dispatcher{static_cast<ErrorHandler&>(*this)},
    ssl{asio::ssl::context::tlsv13},
    acceptor{service, getEndpoint(port)} {

    ssl.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
                    asio::ssl::context::no_sslv3 | asio::ssl::context::no_tlsv1_1 | asio::ssl::context::no_tlsv1_2 |
                    asio::ssl::context::single_dh_use);
    ssl.use_certificate_chain(asio::buffer(cert.pem()));
    ssl.use_private_key(asio::buffer(pkey.pem()), asio::ssl::context::pem);
    ssl.use_tmp_dh(asio::buffer(ec.pem()));
}

Server::~Server() {
    stop();
}

void Server::start(bool async) {
    accept();

    if (async) {
        thread = std::thread([this]() { run(); });
    }
}

void Server::run() {
    service.run();
}

void Server::stop() {
    acceptor.close();
    service.stop();
    if (thread.joinable()) {
        thread.join();
    }
}

void Server::accept() {
    auto socket = std::make_shared<Socket>(service, ssl);

    acceptor.async_accept(socket->lowest_layer(), [this, socket](const std::error_code ec) {
        if (ec) {
            onError(ec);
        } else if (acceptor.is_open()) {
            handshake(socket, std::make_shared<Peer>(*this, *this, service, socket));
        }
    });
}

void Server::handshake(const std::shared_ptr<Socket>& socket, const std::shared_ptr<Peer>& peer) {
    socket->async_handshake(asio::ssl::stream_base::server, [this, socket, peer](const std::error_code ec) {
        if (ec) {
            onError(peer, ec);
            socket->lowest_layer().close();
        } else {
            peer->start();
            onAcceptSuccess(peer);
        }
    });
}

asio::ip::tcp::endpoint Server::getEndpoint(const unsigned int port) {
    return {asio::ip::tcp::v6(), static_cast<asio::ip::port_type>(port)};
}

void Server::postDispatch(std::function<void()> fn) {
    service.post(std::forward<decltype(fn)>(fn));
}

void Server::onAcceptSuccess(const std::shared_ptr<Peer>& peer) {
    std::cout << "New peer: " << peer->getAddress() << std::endl;
}
