#include <msgnet.hpp>

struct MessageHelloRequest {
    std::string msg;

    MESSAGE_DEFINE(MessageHelloRequest, msg);
};

struct MessageHelloResponse {
    std::string msg;
    bool result{false};

    MESSAGE_DEFINE(MessageHelloResponse, msg, result);
};

struct MessageFoo {
    int data;

    MESSAGE_DEFINE(MessageFoo, data);
};

int main() {
    // Alias
    using PeerPtr = std::shared_ptr<MsgNet::Peer>;

    // We will store peers in here
    std::unordered_set<PeerPtr> peers;

    MsgNet::Pkey pkey{};     // Create random private key
    MsgNet::Cert cert{pkey}; // Create self-signed certificate
    MsgNet::Dh ec{};         // Diffie-Hellman parameters

    // Create the server
    MsgNet::Server server{8009, pkey, ec, cert};

    // Add handler to the server for type MessageHelloRequest
    server.addHandler([&](const PeerPtr& peer, MessageHelloRequest req) {
        // This will be handled asynchronously, or by the thread that calls client.run();

        // Remember this peer, only for example purposes only.
        peers.insert(peer);

        // Construct response
        MessageHelloResponse res;
        res.result = true;
        res.msg = "Received on server side: " + req.msg;

        // Send a response to the client.
        // It is optional!
        // You can choose "void" -> no response.
        return res;
    });

    // Start the server in async mode
    // true => async mode, returns immediately, can be used now
    // false => sync mode, returns immediately, must call server.run() somewhere!
    server.start(true);
    // server.run(); // Blocking mode, must call start(false) first!

    // Create the client
    MsgNet::Client client{};

    // Add handler to the client for type MessageHelloRequest.
    // The "peer" in this case (client) is an underlying type within the client.
    // The client always has exactly one peer.
    client.addHandler([](const PeerPtr& peer, MessageFoo req) -> void {
        // This will be handled asynchronously, or by the thread that calls client.run();
        std::cout << "Received foo data: " << req.data << std::endl;

        // No response back to the server.
        // However, you can respond back with any message.
        // It is optional!
        /* return;*/
    });

    // Start the server in async mode
    // true => async mode, returns immediately, can be used now
    // false => sync mode, returns immediately, must call client.run() somewhere!
    client.start(true);
    // client.run(); // Blocking mode, must call start(false) first!

    // Connect to the server, always blocking
    client.connect("localhost", 8009, /* timeout (ms) = */ 5000);

    // We will store the server async response in here.
    auto promise = std::make_shared<std::promise<MessageHelloResponse>>();
    auto future = promise->get_future();

    MessageHelloRequest req{};
    req.msg = "Hello World!";

    // Send a request to the server and use the callback to handle the response.
    // The callback is optional.
    std::cout << "Sending request to the server" << std::endl;
    client.send(req, [promise](MessageHelloResponse res) {
        // This will be handled asynchronously, or by the thread that calls client.run();
        promise->set_value(res);
    });

    // Wait for the message received by the client
    assert(future.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready);
    MessageHelloResponse res = future.get();
    std::cout << "Received response from the server: " << res.msg << std::endl;

    // Sanity check that the server has received some peer
    assert(peers.size() == 1);

    // Send message from the server back to the client.
    // The client did not specify response in the handler, therefore
    // we should not add a callback in this send() function.
    MessageFoo foo{};
    foo.data = 42;
    peers.begin()->get()->send(foo);

    // Optional, server and client call this in the destructor
    client.stop();
    server.stop();

    return EXIT_SUCCESS;
}
