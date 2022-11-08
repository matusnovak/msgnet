#include "client.hpp"
#include <iostream>

using namespace MsgNet;

Client::Client() :
    Dispatcher{static_cast<ErrorHandler&>(*this)},
    work{std::make_unique<asio::io_service::work>(service)},
    ssl{asio::ssl::context::tlsv13} {

    ssl.set_verify_mode(asio::ssl::verify_none);
}

Client::~Client() {
    stop();
}

void Client::start(bool async) {
    socket = std::make_shared<Socket>(service, ssl);

    if (async) {
        thread = std::thread([this]() { getIoService().run(); });
    }
}

void Client::setVerifyCallback(std::function<bool(Cert)> callback) {
    ssl.set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
    ssl.set_verify_callback([callback = std::move(callback)](bool preverified, asio::ssl::verify_context& ctx) {
        return callback(Cert{ctx});
    });
}

void Client::connect(const std::string& address, unsigned int port, int timeout) {
    const auto tp = std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);

    const asio::ip::tcp::resolver::query query(address, std::to_string(port));
    asio::ip::tcp::resolver resolver(service);

    auto endpointsFuture = resolver.async_resolve(query, asio::use_future);
    if (endpointsFuture.wait_until(tp) != std::future_status::ready) {
        throw std::runtime_error("Failed to resolve address");
    }

    const auto endpoints = endpointsFuture.get();
    auto endpoint = endpoints.end();

    for (auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        auto connect = socket->next_layer().async_connect(*it, asio::use_future);
        if (connect.wait_until(tp) != std::future_status::ready) {
            throw std::runtime_error("Timeout connecting to the address");
        }

        try {
            connect.get();
            endpoint = it;
        } catch (std::exception& e) {
            continue;
        }
    }

    if (endpoint == endpoints.end()) {
        throw std::runtime_error("Unable to connect to the address");
    }

    auto handshake = socket->async_handshake(asio::ssl::stream_base::client, asio::use_future);
    if (handshake.wait_until(tp) != std::future_status::ready) {
        throw std::runtime_error("Timeout TLS handshake");
    }

    peer = std::make_shared<Peer>(*this, *this, service, socket);
    peer->start();
}

void Client::stop() {
    if (peer) {
        peer->close();
    }

    if (!service.stopped()) {
        service.post([this]() { work.reset(); });
        service.stop();
    }

    if (thread.joinable()) {
        thread.join();
    }
}

bool Client::isConnected() {
    return peer && peer->isConnected();
}

const std::string& Client::getAddress() const {
    static const std::string empty{};
    if (!peer) {
        return empty;
    }
    return peer->getAddress();
}

void Client::postDispatch(std::function<void()> fn) {
    service.post(std::forward<decltype(fn)>(fn));
}
