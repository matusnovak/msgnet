#pragma once

#include "cert.hpp"
#include "dispatcher.hpp"
#include "peer.hpp"
#include <thread>

namespace MsgNet {
class MSGNET_API Client : public ErrorHandler, public Dispatcher {
public:
    using Socket = asio::ssl::stream<asio::ip::tcp::socket>;

    Client();

    ~Client();

    void connect(const std::string& address, unsigned int port, int timeout = 5000);

    void start(bool async = true);

    asio::io_service& getIoService() {
        return service;
    }

    void stop();

    const std::string& getAddress() const;

    void setVerifyCallback(std::function<bool(Cert)> callback);

    bool isConnected();

    void sendPacket(std::shared_ptr<msgpack::sbuffer> packet) {
        if (peer) {
            peer->sendPacket(std::move(packet));
        }
    }

    template <typename Req> void send(const Req& message) {
        if (peer) {
            peer->send<Req>(message);
        }
    }

    template <typename Req, typename Fn> void send(const Req& message, Fn fn) {
        if (peer) {
            peer->send<Req, Fn>(message, std::forward<Fn>(fn));
        }
    }

protected:
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
