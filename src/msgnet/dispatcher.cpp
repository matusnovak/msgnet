#include "dispatcher.hpp"
#include "peer.hpp"
#include <iostream>

using namespace MsgNet;

Dispatcher::Dispatcher(ErrorHandler& errorHandler) : errorHandler{errorHandler} {
}

void Dispatcher::dispatch(const PeerPtr& peer, const uint64_t id, const uint64_t reqId, const msgpack::object& object) {
    const auto it = handlers.find(id);
    if (it != handlers.end()) {
        it->second(peer, reqId, object);
    } else {
        errorHandler.onError(peer, ::make_error_code(Error::UnexpectedRequest));
    }
}
