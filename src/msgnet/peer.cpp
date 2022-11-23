#include "peer.hpp"
#include "server.hpp"

using namespace MsgNet;

static const size_t blockBytes = 1024 * 8;

Peer::Peer(ErrorHandler& errorHandler, Dispatcher& dispatcher, asio::io_service& service,
           std::shared_ptr<Socket> socket) :
    CompressionStream{blockBytes},
    DecompressionStream{blockBytes},
    errorHandler{errorHandler},
    dispatcher{dispatcher},
    runFlag{true},
    strand{service},
    socket{std::move(socket)} {

    this->socket->lowest_layer().set_option(asio::ip::tcp::no_delay{true});

    address = toString(this->socket->lowest_layer().remote_endpoint());
    receiveBuffer.resize(1024);
}

Peer::~Peer() {
    close();
}

void Peer::start() {
    receive();
}

void Peer::close() {
    runFlag.store(false);
    socket.reset();
}

void Peer::receive() {
    if (!runFlag.load()) {
        return;
    }

    const auto b = asio::buffer(receiveBuffer.data(), receiveBuffer.size());
    auto self = this->shared_from_this();

    socket->async_read_some(b, [self](const asio::error_code ec, const size_t length) {
        if (ec) {
            self->errorHandler.onError(self, ec);
        } else {
            try {
                self->accept(self->receiveBuffer.data(), length);
            } catch (std::exception_ptr& e) {
                self->errorHandler.onUnhandledException(self, e);
            }

            if (self->runFlag.load()) {
                self->receive();
            }
        }
    });
}

void Peer::receiveObject(std::shared_ptr<msgpack::object_handle> oh) {
    auto self = this->shared_from_this();

    dispatcher.postDispatch([self, oh = std::move(oh)]() {
        try {
            const auto& o = oh->get();
            if (o.type != msgpack::type::ARRAY || o.via.array.size != 2) {
                self->errorHandler.onError(self, ::make_error_code(Error::BadMessageFormat));
                return;
            }

            PacketInfo info;
            o.via.array.ptr[0].convert(info);

            const auto& object = o.via.array.ptr[1];

            if (info.isResponse) {
                self->handle(info.reqId, object);
            } else {
                self->dispatcher.dispatch(self, info.id, info.reqId, object);
            };
        } catch (msgpack::unpack_error& e) {
            self->errorHandler.onError(self, ::make_error_code(Error::UnpackError));
        } catch (std::exception_ptr& e) {
            self->errorHandler.onUnhandledException(self, e);
        }
    });
}

void MsgNet::Peer::handle(const uint64_t reqId, const msgpack::object& object) {
    Callback callback;

    {
        std::lock_guard<std::mutex> lock{requests.mutex};
        auto it = requests.map.find(reqId);
        if (it != requests.map.end()) {
            std::swap(it->second.callback, callback);
            requests.map.erase(it);
        } else {
            errorHandler.onError(shared_from_this(), ::make_error_code(Error::UnexpectedResponse));
        }
    }

    if (callback) {
        try {
            callback(object);
        } catch (std::exception_ptr& e) {
            errorHandler.onUnhandledException(shared_from_this(), e);
        }
    }
}

void MsgNet::Peer::sendBuffer(std::shared_ptr<std::vector<char>> buffer) {
    if (!runFlag.load()) {
        return;
    }

    auto self = shared_from_this();
    const auto b = asio::buffer(buffer->data(), buffer->size());

    self->socket->async_write_some(b, [self, buffer](const asio::error_code ec, const size_t length) {
        (void)self;
        (void)buffer;

        if (ec) {
            self->errorHandler.onError(self, ec);
        }
    });
}

bool Peer::isConnected() {
    return runFlag.load() && socket && socket->lowest_layer().is_open();
}

std::string MsgNet::toString(const asio::ip::tcp::endpoint& endpoint) {
    std::stringstream ss;

    if (endpoint.address().is_v6()) {
        ss << "[" << endpoint.address().to_string() << "]:" << endpoint.port();
    } else {
        ss << "" << endpoint.address().to_string() << ":" << endpoint.port();
    }

    return ss.str();
}
