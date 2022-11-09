#include <catch.hpp>
#include <cstring>
#include <iostream>
#include <lz4.h>
#include <lz4frame.h>
#include <msgnet/client.hpp>
#include <msgnet/server.hpp>

using namespace MsgNet;

TEST_CASE("Start and close the server") {
    Pkey pkey{};
    Cert cert{pkey};
    Dh ec{};
    Server server{8009, pkey, ec, cert};
    server.start();
}

TEST_CASE("Connect to non existing server and expect connection refused") {
    Client client{};
    client.start();
    REQUIRE_THROWS_WITH(client.connect("localhost", 8009), "Unable to connect to the address");
}

TEST_CASE("Connect to stalled server and expect timeout") {
    Pkey pkey{};
    Cert cert{pkey};
    Dh ec{};
    Server server{8009, pkey, ec, cert};

    Client client{};
    client.start();
    REQUIRE_THROWS_WITH(client.connect("localhost", 8009), "Timeout TLS handshake");
}

TEST_CASE("Start and connect to the server") {
    Pkey pkey{};
    Cert cert{pkey};
    Dh ec{};

    for (auto i = 0; i < 100; i++) {
        Server server{8009, pkey, ec, cert};
        server.start();
        Client client{};

        REQUIRE(client.isConnected() == false);

        client.start();

        REQUIRE(client.isConnected() == false);

        client.connect("localhost", 8009);

        REQUIRE(client.isConnected() == true);

        client.stop();

        REQUIRE(client.isConnected() == false);
    }
}

TEST_CASE("Send dummy packet to the server") {
    Pkey pkey{};
    Cert cert{pkey};
    Dh ec{};

    Server server{8009, pkey, ec, cert};
    server.start();
    Client client{};
    client.start();
    client.connect("localhost", 8009);

    std::string msg = "Hello world!";

    auto packet = std::make_shared<msgpack::sbuffer>();
    msgpack::pack(*packet, msg);
    client.sendPacket(packet);

    client.stop();
    server.stop();
}

struct MessageFoo {
    std::string msg;

    MESSAGE_DEFINE(MessageFoo, msg);
};

struct MessageBar {
    size_t count;

    MESSAGE_DEFINE(MessageBar, count);
};

struct MessageBaz {
    size_t count;
    bool value;

    MESSAGE_DEFINE(MessageBaz, count, value);
};

TEST_CASE("Message hash sanity check") {
    // This must be platform independent!
    REQUIRE(MessageBar::hash == 0xcb4327037bc501e8);
}

class SimpleServer : public Server {
public:
    SimpleServer(unsigned int port, const Pkey& pkey, const Dh& ec, const Cert& cert) : Server{port, pkey, ec, cert} {
        addHandler(this, &SimpleServer::handleFoo);
        addHandler([this](const std::shared_ptr<Peer>& peer, MessageBar req) {
            return this->handleBar(peer, std::move(req));
        });
        start();
    }

    virtual ~SimpleServer() {
        stop();
    }

    void handleFoo(const std::shared_ptr<Peer>& peer, MessageFoo req) {
        std::lock_guard<std::mutex> lock{mutex};
        foos.emplace_back(peer, std::move(req));
    }

    MessageBaz handleBar(const std::shared_ptr<Peer>& peer, MessageBar req) {
        std::lock_guard<std::mutex> lock{mutex};
        bars.emplace_back(peer, std::move(req));

        MessageBaz baz{};
        baz.value = true;
        baz.count = req.count * req.count;
        return baz;
    }

    void onAcceptSuccess(std::shared_ptr<Peer> peer) override {
        std::lock_guard<std::mutex> lock{mutex};
        peers.push_back(peer);
    }

    std::vector<std::shared_ptr<Peer>> getPeers() {
        std::lock_guard<std::mutex> lock{mutex};
        return peers;
    }

    std::vector<std::tuple<std::shared_ptr<Peer>, MessageFoo>> getFoos() {
        std::lock_guard<std::mutex> lock{mutex};
        return foos;
    }

    std::vector<std::tuple<std::shared_ptr<Peer>, MessageBar>> getBars() {
        std::lock_guard<std::mutex> lock{mutex};
        return bars;
    }

private:
    std::mutex mutex;
    std::vector<std::shared_ptr<Peer>> peers;
    std::vector<std::tuple<std::shared_ptr<Peer>, MessageFoo>> foos;
    std::vector<std::tuple<std::shared_ptr<Peer>, MessageBar>> bars;
};

class SimpleClient : public Client {
public:
    SimpleClient(const std::string& address, unsigned int port) {
        start();
        connect(address, port);
    }

    virtual ~SimpleClient() {
        stop();
    }
};

TEST_CASE("Connect and disconnect from the server") {
    Pkey pkey{};
    Cert cert{pkey};
    Dh ec{};

    SimpleServer server{8009, pkey, ec, cert};
    SimpleClient client{"localhost", 8009};

    // Wait for server to accept the peer
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto peers = server.getPeers();
    REQUIRE(peers.empty() == false);
    REQUIRE(peers.front()->isConnected() == true);
    REQUIRE(client.isConnected() == true);

    client.stop();

    // Wait for server to handle the disconnect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(client.isConnected() == false);
}

TEST_CASE("Send message to the server and get response") {
    Pkey pkey{};
    Cert cert{pkey};
    Dh ec{};

    SimpleServer server{8009, pkey, ec, cert};
    SimpleClient client{"localhost", 8009};

    // Wait for server to accept the peer
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto peers = server.getPeers();
    REQUIRE(peers.empty() == false);

    MessageFoo foo{};
    foo.msg = "Message from Foo!";
    client.send(foo);

    // Wait for server to receive the message
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto foos = server.getFoos();
    REQUIRE(foos.empty() == false);
    REQUIRE(std::get<1>(foos.front()).msg == foo.msg);

    MessageBar bar{};
    bar.count = 42;

    std::promise<MessageBaz> promise;
    auto future = promise.get_future();

    client.send(bar, [&](MessageBaz res) { promise.set_value(res); });

    REQUIRE(future.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready);
    MessageBaz baz = future.get();

    REQUIRE(baz.value == true);
    REQUIRE(baz.count == 42 * 42);
}

TEST_CASE("Dispatch with override func") {
    class SimplePollingServer : public SimpleServer {
    public:
        SimplePollingServer(unsigned int port, const Pkey& pkey, const Dh& ec, const Cert& cert) :
            SimpleServer(port, pkey, ec, cert) {
        }

        void poll() {
            worker.reset();
            worker.run();
        }

    private:
        void postDispatch(std::function<void()> fn) override {
            worker.post(std::forward<decltype(fn)>(fn));
        }

        asio::io_service worker;
    };

    class SimplePollingClient : public SimpleClient {
    public:
        SimplePollingClient(const std::string& address, unsigned int port) : SimpleClient(address, port) {
        }

        void poll() {
            worker.reset();
            worker.run();
        }

    private:
        void postDispatch(std::function<void()> fn) override {
            worker.post(std::forward<decltype(fn)>(fn));
        }

        asio::io_service worker;
    };

    Pkey pkey{};
    Cert cert{pkey};
    Dh ec{};

    SimplePollingServer server{8009, pkey, ec, cert};
    SimplePollingClient client{"localhost", 8009};

    MessageBar bar{};
    bar.count = 42;

    std::promise<MessageBaz> promise;
    auto future = promise.get_future();

    client.send(bar, [&](MessageBaz res) {
        std::cout << "Received MessageBaz!" << std::endl;
        promise.set_value(res);
    });

    // Wait for server to receive the message
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Server should not handle the message at this time
    auto bars = server.getBars();
    REQUIRE(bars.empty() == true);

    // Poll for messages
    server.poll();

    // Server should now handle the message
    bars = server.getBars();
    REQUIRE(bars.empty() == false);
    REQUIRE(bars.size() == 1);

    // Client should not receive the response message at this time
    REQUIRE(future.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready);

    // Poll for messages
    client.poll();

    // Client should now receive the response message
    REQUIRE(future.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready);

    // Get the response
    MessageBaz baz = future.get();
    REQUIRE(baz.value == true);
    REQUIRE(baz.count == 42 * 42);
}

TEST_CASE("Custom certificate validation function") {
    Pkey pkey{};
    Cert cert{pkey};
    Dh ec{};

    Server server{8009, pkey, ec, cert};
    server.start();

    Client client{};

    auto promise = std::make_shared<std::promise<Cert>>();
    auto future = promise->get_future();

    client.setVerifyCallback([promise](Cert cert) {
        std::cout << "received cert!" << std::endl;
        promise->set_value(std::move(cert));
        return true;
    });

    client.start();
    REQUIRE_NOTHROW(client.connect("localhost", 8009));

    // Wait for server to accept the peer
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    REQUIRE(future.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready);
    auto received = future.get();

    REQUIRE(received.pem() == cert.pem());

    REQUIRE(received.getSubjectName() == "/C=EU/O=msgnet/CN=msgnet");
}
