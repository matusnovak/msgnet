#pragma once

#include "packet.hpp"
#include <typeindex>
#include <unordered_map>

namespace MsgNet::Detail {
MSGNET_API uint64_t getMessageHash(const std::string& name);
} // namespace MsgNet::Detail

#define MESSAGE_DEFINE(Type, ...)                                                                                      \
    static inline const uint64_t hash = MsgNet::Detail::getMessageHash(#Type);                                         \
    MSGPACK_DEFINE_ARRAY(__VA_ARGS__);
