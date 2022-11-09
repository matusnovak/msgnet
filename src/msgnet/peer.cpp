#include "peer.hpp"
#include "server.hpp"
#include <iostream>
#include <lz4.h>

using namespace MsgNet;

static const size_t blockBytes = 1024 * 8;

struct Peer::Lz4 {
    Lz4() : encodeBody{}, encode{&encodeBody}, decodeBody{}, decode{&decodeBody} {
        LZ4_initStream(encode, sizeof(*encode));
        LZ4_setStreamDecode(decode, nullptr, 0);
    }

    LZ4_stream_t encodeBody{};
    LZ4_stream_t* encode;
    LZ4_streamDecode_t decodeBody{};
    LZ4_streamDecode_t* decode;
    char readBuffer[blockBytes];
};

Peer::Peer(ErrorHandler& errorHandler, Dispatcher& dispatcher, asio::io_service& service,
           std::shared_ptr<Socket> socket) :
    errorHandler{errorHandler},
    dispatcher{dispatcher},
    lz4{std::make_unique<Lz4>()},
    strand{service},
    socket{std::move(socket)} {

    address = toString(this->socket->lowest_layer().remote_endpoint());
}

Peer::~Peer() {
    close();
}

void Peer::start() {
    receive();
}

void Peer::close() {
    socket.reset();
}

void Peer::receive() {
    const auto b = asio::buffer(lz4->readBuffer, sizeof(Lz4::readBuffer));
    auto self = this->shared_from_this();

    socket->async_read_some(b, [self](const asio::error_code ec, const size_t length) {
        if (ec) {
            self->errorHandler.onError(self, ec);
        } else {
            self->unp.reserve_buffer(sizeof(Lz4::readBuffer));
            const auto decBytes =
                LZ4_decompress_safe_continue(self->lz4->decode, self->lz4->readBuffer, self->unp.buffer(),
                                             static_cast<int>(length), sizeof(Lz4::readBuffer));

            if (decBytes <= 0) {
                self->errorHandler.onError(self, ::make_error_code(Error::DecompressError));
            } else {
                self->unp.buffer_consumed(decBytes);

                auto oh = std::make_shared<msgpack::object_handle>();

                while (self->unp.next(*oh)) {
                    self->receiveObject(std::move(oh));
                    oh = std::make_shared<msgpack::object_handle>();
                }

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
    std::lock_guard<std::mutex> lock{mutex};
    auto it = requests.find(reqId);
    if (it != requests.end()) {
        try {
            it->second.callback(object);
            requests.erase(it);
        } catch (std::exception_ptr& e) {
            requests.erase(it);
            errorHandler.onUnhandledException(shared_from_this(), e);
        }
    } else {
        errorHandler.onError(shared_from_this(), ::make_error_code(Error::UnexpectedResponse));
    }
}

void Peer::sendPacket(std::shared_ptr<msgpack::sbuffer> packet) {
    auto self = shared_from_this();

    strand.post([self, packet]() {
        auto compressed = std::make_shared<std::vector<char>>();
        compressed->resize(LZ4_COMPRESSBOUND(packet->size()));

        const auto cmpBytes =
            LZ4_compress_fast_continue(self->lz4->encode, packet->data(), compressed->data(),
                                       static_cast<int>(packet->size()), static_cast<int>(compressed->size()), 1);
        if (cmpBytes <= 0) {
            throw std::runtime_error("Unable to compress packet");
        }

        compressed->resize(cmpBytes);

        if (self->socket && self->socket->lowest_layer().is_open()) {
            self->sendBuffer(std::move(compressed));
        } else {
            self->errorHandler.onError(self, asio::error::connection_aborted);
        }
    });
}

void MsgNet::Peer::sendBuffer(std::shared_ptr<std::vector<char>> buffer) {
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
    return socket && socket->lowest_layer().is_open();
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
