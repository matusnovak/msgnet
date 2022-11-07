#pragma once

#include "packet.hpp"
#include <typeindex>
#include <unordered_map>

namespace MsgNet {
/*struct MSGNET_API Message {
    uint64_t id{0};

    MSGPACK_DEFINE_ARRAY(id);
};*/

// using TypeIndexIdMap = std::unordered_map<std::type_index, uint64_t>;

namespace Detail {
MSGNET_API uint64_t getMessageHash(const std::string& name);

/*template<typename Type>
inline void getTypeIndexIdMap(TypeIndexIdMap &map, const size_t idx = 0) {
    map[typeid(Type)] = idx + 1;
}

template<typename Type, typename... Types>
inline void getTypeIndexIdMap(TypeIndexIdMap &map, const size_t idx = 0) {
    map[typeid(Type)] = idx + 1;
    getTypeIndexIdMap<Types...>(map, idx + 1);
}*/
} // namespace Detail

/*template<typename... Types>
struct Messages {
    static TypeIndexIdMap getTypeIndexIdMap() {
        TypeIndexIdMap map;
        Detail::getTypeIndexIdMap<Types...>(map);
    }
};*/
} // namespace MsgNet

#define MESSAGE_DEFINE(Type, ...)                                                                                      \
    static inline const uint64_t hash = MsgNet::Detail::getMessageHash(#Type);                                         \
    MSGPACK_DEFINE_ARRAY(__VA_ARGS__);
