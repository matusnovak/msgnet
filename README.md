# MsgNet

[![build](https://github.com/matusnovak/msgnet/actions/workflows/build.yml/badge.svg?branch=master&event=push)](https://github.com/matusnovak/msgnet/actions/workflows/build.yml)

A C++ networking library that supports RPC-like bidirectional messaging style using
[Msgpack C++](https://github.com/msgpack/msgpack-c/tree/cpp_master) on top of TLS v1.3 via
[Asio](https://think-async.com/Asio/) and [OpenSSL](https://github.com/openssl/openssl)
with [LZ4](https://github.com/lz4/lz4) compression.

## Features

* User friendly interface
* Bidirectional communication between the client and the server via [Asio](https://think-async.com/Asio/).
* RPC-like messaging.
* Serialization and deserialization via [Msgpack C++](https://github.com/msgpack/msgpack-c/tree/cpp_master).
* TLS v1.3 via [OpenSSL](https://github.com/openssl/openssl).
* Fast compression via [LZ4](https://github.com/lz4/lz4).
* Multithreaded client and server.
* Helper functions for RSA private key, x509 certificate, and Diffie-Hellman parameters.
* Optional client x509 certificate validation.

## Example

A very simple hello world:

```cpp
#include <msgnet.hpp>

struct MessageHelloRequest {
    std::string msg;

    MESSAGE_DEFINE(MessageHelloRequest, msg); // Calls MSGPACK_DEFINE(msg);
};

struct MessageHelloResponse {
    std::string msg;

    MESSAGE_DEFINE(MessageHelloResponse, msg); // Calls MSGPACK_DEFINE(msg);
};

int main() {
    using PeerPtr = std::shared_ptr<MsgNet::Peer>; // Alias

    MsgNet::Pkey pkey{};     // Create random private key
    MsgNet::Cert cert{pkey}; // Create self-signed certificate
    MsgNet::Dh ec{};         // Diffie-Hellman parameters

    // Create the server
    MsgNet::Server server{8009, pkey, ec, cert};

    // Add handler to the server for type MessageHelloRequest
    server.addHandler([&](const PeerPtr& peer, MessageHelloRequest req) {
        // This will be handled asynchronously, or by the thread that calls server.getIoService().run();

        // Construct response
        MessageHelloResponse res;
        res.msg = "Received on server side: " + req.msg;

        // Send an optional response to the client.
        // You can choose "void" -> no response.
        return res;
    });

    // Start the server in async mode
    // true => async mode, returns immediately, can be used now
    // false => sync mode, returns immediately, must call server.getIoService().run() somewhere!
    server.start(true);

    // Create the client
    MsgNet::Client client{};

    // Start the server in async mode
    // true => async mode, returns immediately, can be used now
    // false => sync mode, returns immediately, must call client.getIoService().run() somewhere!
    client.start(true);

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
        // This will be handled asynchronously, or by the thread that calls client.getIoService().run()
        std::cout << "Received response from the server: " << res.msg << std::endl;
    });

    // Optional, server and client call this in the destructor
    client.stop();
    server.stop();

    return EXIT_SUCCESS;
}
```

### More examples

File: [examples/hello_world.cpp](examples/hello_world.cpp)

## Documentation

### Server

To create an async server you can do the following below. The server will run on a single thread.

```cpp
MsgNet::Pkey pkey{};     // Create random private key
MsgNet::Cert cert{pkey}; // Create self-signed certificate
MsgNet::Dh ec{};         // Diffie-Hellman parameters

// Start on port 8009
MsgNet::Server server{8009, pkey, ec, cert};
server.start(true); // Async, starts right away, non-blocking
```

If you want to control this server with your own thread,
or you wish to use a pool of threads, you can do the following:

```cpp
MsgNet::Pkey pkey{};     // Create random private key
MsgNet::Cert cert{pkey}; // Create self-signed certificate
MsgNet::Dh ec{};         // Diffie-Hellman parameters

// Start on port 8009
MsgNet::Server server{8009, pkey, ec, cert};
server.start(false); // Sync, non-blocking

// Run on custom thread
auto thread = std::thread([&]() {
    asio::io_service& service = server.getIoService();
    service.run();
});
```

### Client

Client works in a similar way to the server. You do not need to pass in any certificates. Simply connect
to the remote server.
For certificate validation see the section "Private keys, x509 certificates, and DH params" below.

```cpp
MsgNet::Client client{};
client.start(true); // Async, non-blocking

// Connect to the server, blocking call
client.connect("localhost", 8009, /* timeout in ms = */ 5000);

// Send something
client.send(...);
```

Similarly to the server, the client supports running the connection in some custom thread.

```cpp
MsgNet::Client client{};
client.start(false); // Sync, non-blocking

// Run on custom thread
auto thread = std::thread([&]() {
    asio::io_service& service = server.getIoService();
    service.run();
});

// Connect to the server, blocking call
client.connect("localhost", 8009, /* timeout in ms = */ 5000);

// Send something
client.send(...);
```

### Messages

Before you can receive any message you must define at least one message type.
For example, in here we register a request and a response message. You can define any
fields within the message, as long as they are accepted by the Msgpack. Simple types and
some STL containers are supported out of the box. Some custom complex types will
need to have `MSGPACK_DEFINE` inside of them,
or some [Msgpack adaptor](https://github.com/msgpack/msgpack-c/wiki/v2_0_cpp_adaptor) defined.

```cpp
struct MessageFooRequest {
    std::string a;
    bool b;
    float c;
    SomeOtherComplexStructure d;
    
    // First argument is always the struct name!
    // The rest of the arguments go to MSGPACK_DEFINE(...);
    MESSAGE_DEFINE(MessageFooRequest, a, b, c, d);
};

struct MessageFooResponse {
    std::vector<char> data;
    
    // First argument is always the struct name!
    // The rest of the arguments go to MSGPACK_DEFINE(...);
    MESSAGE_DEFINE(MessageFooResponse, data);
};
```

There are no custom code generators involved. Everything is also multithread supported.

### Handlers

To handle any message in your server you must register such message via `addHandler`.
**The addHandler function figures out the required types on its own, you do not need to specify
template arguments for this function call.** If the request type or the response type
can not be serialized or deserialized via Msgpack then it will cause a compilation error.

```cpp
using PeerPtr = std::shared_ptr<MsgNet::Peer>; // Alias

server.addHandler([](const PeerPtr& peer, MessageFooRequest req) {
    // Do something with the request

    // Construct a response
    MessageFooResponse res{};
    
    // Send the response back to the client
    return res;
});

// Always start the server AFTER you have added your handlers!
server.start(true);
```

You can create handlers that simply do not return any message. Such request could be
considered as events, a message/request that is only received.

```cpp
using PeerPtr = std::shared_ptr<MsgNet::Peer>; // Alias

server.addHandler([](const PeerPtr& peer, MessageFooRequest req) -> void {
    // Do something with the request
    
    // Do not send anything back
    return;
});

// Always start the server AFTER you have added your handlers!
server.start(true);
```

The `addHandler` is not limited to the `Server` only. You can also do the exact same thing
for the `Client`. **You can have a bidirectional way of communication between the server and the client.**

```cpp
using PeerPtr = std::shared_ptr<MsgNet::Peer>; // Alias

client.addHandler([](const PeerPtr& peer, SomeClientRequest req) {
    // Do something with the request
    
    // In the client case of addHandler, the peer is an underlying type
    // that is managed by the Client class. The client always has
    // exactly one peer.
    // Any calls made to client.send(...) are in reality client.peer.send(...);

    // Construct a response
    SomeClientResponse res{};
    
    // Send the response back to the client
    return res;
});

// Always start the client AFTER you have added your handlers!
client.start(true);
```

An alternative way of adding a handler is providing the handler function directly.

```cpp
class SomeHandlerClass {
public:
    [...]
    
    MessageFooResponse handleFoo(const PeerPtr& peer, MessageFooRequest req) {
        // Do something with the request
        
        MessageFooResponse res{};
        return res;
    }
};

SomeHandlerClass handler;

// Register it
client.addHandler(&handler, &SomeHandlerClass::handleFoo);

// Always start the server AFTER you have added your handlers!
client.start(true);
```

### Send a request

To send a request, you must have a defined handler for such request, and you must
send the request via the `client.send();` or `peer.send();` functions. The `send()` function is
non blocking. The send will only schedule the message to be sent out. The actual sending of the
request will be done by the thread that the Client runs on (see the Client section above).

```cpp
MessageFooRequest req{};

// Fill in some request data
req.a = ...;
req.b = ...;

// Sends the message, non blocking
client.send(req, [](MessageFooResponse res) -> void {
    // Do something with the response!
    // This will be called asynchronously
});
```

If the server has no response for such request message type,
then you must not define a callback for the response. This also could be useful
for sending a one off message back to the server. This also applies the other way around.
The server is able to send a message via `peer.send();` that accepts an optional callback.

```cpp
MessageFooWithNoResponse req{};

// Fill in some request data
req.a = ...;
req.b = ...;

// Sends the message, non blocking, no response expected!
client.send(req);
```

The following example below is an alternative way of handling communication between the server and the client.
In here the server handler sends back the response for the request via `peer.send();`. This will cause
the client to treat the response as a regular request. There is no callback involved.

This will cause a problem when you want to have a specific functionality logic associated with
a specific response on the client side. You must implement such logic yourself, if you need such thing.
Otherwise the recommended approach is to use `client.send(req, [](Response res){});` callback style.

```cpp
using PeerPtr = std::shared_ptr<MsgNet::Peer>; // Alias

client.addHandler([](const PeerPtr& peer, MessageFooResponse res) -> void {
    // Do something with the response
    
    // Do not send anything back
    return;
});

server.addHandler([](const PeerPtr& peer, MessageFooRequest req) -> void {
    // Do something with the request
    
    MessageFooResponse res{};
    
    // Prepare the response data
    res.a = ...;
    
    // Send the response back to the client
    peer.send(res);
});

// Always start the client and the server AFTER you have added your handlers!
client.start(true);
server.start(true);

MessageFooRequest req{};

// Fill in some request data
req.a = ...;

client.send(req);
```

### Private keys, x509 certificates, and DH params

The most easiest way on how to start a server is with a self signed certificate. The following code below
creates a random RSA private key, followed by a server certificate using the RSA key, and a Diffie-Hellman
parameters.

```cpp
MsgNet::Pkey pkey{};     // Create random private key
MsgNet::Cert cert{pkey}; // Create self-signed certificate
MsgNet::Dh ec{};         // Diffie-Hellman parameters

// Start on port 8009
MsgNet::Server server{8009, pkey, ec, cert};
server.start(true);
```

You can provide your own key, certificate, and DH parameters, by passing in a `std::string` into
the constructors. This library does not provide a functionality to create those things with some
specific parameters. You must create such private key, certificate, and DH params, yourself
(or use the auto generated ones, as explained above).

```cpp
std::string pkeyRaw = "-----BEGIN RSA PRIVATE KEY-----....";
std::string certRaw = "-----BEGIN CERTIFICATE-----....";
std::string dhRaw = "-----BEGIN DH PARAMETERS-----....";

MsgNet::Pkey pkey{pkeyRaw};
MsgNet::Cert cert{certRaw};
MsgNet::Dh ec{dhRaw}; 

// Start on port 8009
MsgNet::Server server{8009, pkey, ec, cert};
server.start(true);
```

**By default the client will accept any certificate.** If you want to verify the server certificate,
you must specify a verify callback as shown below. The callback will provide you with `MsgNet::Cert`
that provides the raw OpenSSL `X509` pointer via `.get()` function and the raw string via `.pem()` function.
You can also use `.getSubjectName()` to get the subject string.

The `MsgNet::Cert` is passed to the callback by a move by the design. The class is not copyable.

It is up to you to verify the certificate against a root certificate or some certificate chain. This
library does not provide such functionality to you.

```cpp
client.setVerifyCallback([promise](MsgNet::Cert cert) {
    // Prints out: "-----BEGIN CERTIFICATE-----...."
    std::cout << "received cert: " << cert.pem() << std::endl;
    
    // Prints out: "/C=.../O=.../CN=..."
    std::cout << "cert subject: " << cert.getSubjectName() << std::endl;

    return true; // Accept (true) or decline (false)
});
```

### Error handling

The error handling is done by overriding virtual functions from the `MsgNet::Client` and the
`MsgNet::Server`. Both inherit from `MsgNet::ErrorHandler` class which defines the following error handlers:

```cpp
// Called by the Server only when:
// * There is a accept new peer error (end of stream, disconnected, generic network errors)
// * There is a compression or decompression error from LZ4
void ErrorHandler::onError(std::error_code ec) {
    std::cout << "Error: " << ec.message() << std::endl;
}

// Called by the Client or by the Server's Peer when:
// * There is a socket error (end of stream, disconnected, generic network errors)
// * There is a SSL error (any SSL error, for example handshake error)
// * There is a compression or decompression error from LZ4
void ErrorHandler::onError(const std::shared_ptr<Peer>& peer, std::error_code ec) {
    // Handle the error
    std::cout << "Error: " << ec.message() << " from peer: " << peer->getAddress() << std::endl;
    
    // On error close the peer
    peer->close();
}

// Called by the Client or by the Server's Peer when an unexpected exception happens when
// handling a callback from .send() or when handling a request registered via .addHandler()
void ErrorHandler::onUnhandledException(const std::shared_ptr<Peer>& peer, std::exception_ptr& eptr) {
    // Handle the exception
    try {
        std::rethrow_exception(eptr);
    } catch (std::exception& e) {
        // Do something with the exception
        std::cout << "Error: " << ec.message() << " from peer: " << e.what() << std::endl;
    }
    // On error close the peer
    peer->close();
}
```

It is recommended that you override such functions in your custom class. Example below.

```cpp
class MyServer : public MsgNet::Server {
public:
    MyServer(unsigned int port, const Pkey& pkey, const Dh& ec, const Cert& cert) :
        MsgNet::Server(port, pkey, ec, cert) {
    }
    
    void onError(std::error_code ec) override {
        ...
    }
    
    void onError(const std::shared_ptr<Peer>& peer, std::error_code ec) override {
        ...
    }
    
    void onUnhandledException(const std::shared_ptr<Peer>& peer, std::exception_ptr& eptr) override {
        ...
    }
};

class MyClient : public MsgNet::Client {
public:
    MyClient() {
    }
    
    void onError(std::error_code ec) override {
        ...
    }
    
    void onError(const std::shared_ptr<Peer>& peer, std::error_code ec) override {
        ...
    }
    
    void onUnhandledException(const std::shared_ptr<Peer>& peer, std::exception_ptr& eptr) override {
        ...
    }
};
```

### Threading

When you start the server or the client in async mode (specified by the boolean flag), the server and the client
will run in their own threads.

```cpp
server.start(true);
client.strart(true);
```

You can run in some custom thread. For that you need to call `getIoService()` which returns `asio::io_service`.
You must call the `run()` method on it, or any other method that the `asio::io_service` provides. It is up to you.

```cpp
server.start(false); // Sync mode, do not spawn own thread
client.strart(false); // Sync mode, do not spawn own thread

thread1 = std::thread([&]() { 
    server.getIoService().run(); 
});

thread2 = std::thread([&]() { 
    client.getIoService().run(); 
});

server.close(); // Stops the server, the .run() returns, the thread will stop
client.close(); // Stops the server, the .run() returns, the thread will stop
```

#### What about the handlers and send callbacks?

All handlers and send callbacks are handled by the same thread that runs `getIoService().run()`.

If you wish to handle the callbacks in a polling mode, or on a different thread, you must override the
following functions below (similarly as for error handling).

You could use it in a situation where you need two threads, one that handles the socket connection,
and one that handles the message delivery. Additionally, this could be useful for a game where you
would want to receive message from the game server and update the client graphical state on the same
thread that runs the graphics.

```cpp
class PollingServer : public MsgNet::Server {
public:
    PollingServer(unsigned int port, const Pkey& pkey, const Dh& ec, const Cert& cert) :
        MsgNet::Server(port, pkey, ec, cert) {
        flag.store(true);
    }
    
    void close() {
        MsgNet::Server::close();
        flag.reset();
    }

    void poll() {
        // Restart the work queue
        // Does not erase the scheduled work!
        worker.reset();
        
        // Run all scheduled work
        worker.run();
    }
    
    void pollUntilStopped() {
        flag = std::make_unique<asio::io_service::work>(worker);
        worker.reset();
        worker.run();
    }

private:
    void postDispatch(std::function<void()> fn) override {
        // Schedule the handle or send callback work
        worker.post(std::forward<decltype(fn)>(fn));
    }
    
    // Where all of the work for .addHandler or .send(..., callback) will be handled!
    asio::io_service worker;
    std::unique_ptr<asio::io_service::work> flag;
};
```

#### Multiple threads for a single server

You can also create multiple threads and run the same `getIoService().run()` on all of them. This will also
work for the client, but the client only has one socket, therefore it does not make sense to have more than
one thread for the client. For the server, the multiple threads will handle multiple server peers at the same time.
The limitation is that only one thread can handle one peer/socket.

```cpp
server.start(false); // Sync mode / Do not spawn own thread

for (auto i = 0; i < 16; i++) {
    threads[i] = std::thread([&]() { 
        server.getIoService().run(); 
    });
}

server.close(); // Stops the server, the .run() returns, all of the threads will stop
```

## License

[Boost Software License 1.0](https://choosealicense.com/licenses/bsl-1.0/)
