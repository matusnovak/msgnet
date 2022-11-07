#pragma once

#include "library.hpp"
#include <iostream>
#include <msgpack.hpp>

/*namespace MsgNet {
class MSGNET_API PacketBuffer : public std::vector<unsigned char> {
public:
    void write(const char* src, size_t len) {
        const auto prev = size();
        resize(size() + len);
        std::memcpy(data() + prev, src, len);
    }

    void swap(PacketBuffer& other) noexcept {
        std::vector<unsigned char>::swap(other);
    }
};

inline void swap(PacketBuffer& a, PacketBuffer& b) {
    a.swap(b);
}
} // namespace MsgNet

namespace msgpack {
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
    namespace adaptor {
    template <> struct convert<MsgNet::PacketBuffer> {
        msgpack::object const& operator()(msgpack::object const& o, MsgNet::PacketBuffer& v) const {
            v.resize(o.via.bin.size);
            std::memcpy(v.data(), o.via.bin.ptr, v.size());
            return o;
        }
    };

    template <> struct pack<MsgNet::PacketBuffer> {
        template <typename Stream>
        packer<Stream>& operator()(msgpack::packer<Stream>& o, MsgNet::PacketBuffer const& v) const {
            o.pack_bin(static_cast<uint32_t>(v.size()));
            o.pack_bin_body(reinterpret_cast<const char*>(v.data()), v.size());
            return o;
        }
    };
    } // namespace adaptor
}
} // namespace msgpack*/

namespace MsgNet {
/*struct MSGNET_API Packet {
    Packet() = default;
    Packet(const Packet& other) = delete;
    Packet(Packet&& other) = default;
    Packet& operator=(const Packet& other) = delete;
    Packet& operator=(Packet&& other) = default;

    uint64_t id{0};
    uint64_t reqId{0};
    bool isResponse{false};
    PacketBuffer data;

    MSGPACK_DEFINE_ARRAY(id, reqId, isResponse, data);
};*/

struct MSGNET_API PacketInfo {
    uint64_t id{0};
    uint64_t reqId{0};
    bool isResponse{false};

    MSGPACK_DEFINE_ARRAY(id, reqId, isResponse);
};
} // namespace MsgNet
