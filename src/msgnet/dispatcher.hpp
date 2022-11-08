#pragma once

#include "message.hpp"
#include "peer.hpp"
#include <functional>

namespace MsgNet {
class MSGNET_API Peer;

class MSGNET_API Dispatcher {
public:
    using PeerPtr = std::shared_ptr<Peer>;

    template <typename F> struct Traits;

    template <typename C, typename R, typename T> struct Traits<R (C::*)(const PeerPtr&, T) const> {
        using Ret = R;
        using Arg = T;
    };

    using Handler = std::function<void(const PeerPtr&, uint64_t, const msgpack::object&)>;
    using HandlerMap = std::unordered_map<uint64_t, Handler>;

    explicit Dispatcher(ErrorHandler& errorHandler);
    virtual ~Dispatcher() = default;

    template <typename Fn> void addHandler(Fn fn) {
        using Req = typename Traits<decltype(&Fn::operator())>::Arg;
        using Res = typename Traits<decltype(&Fn::operator())>::Ret;
        HandlerFactory<Res, Req>::create(handlers, std::forward<Fn>(fn));
    }

    template <typename C, typename R, typename T> void addHandler(C* instance, R (C::*fn)(const PeerPtr&, T)) {
        HandlerFactory<R, T>::create(handlers,
                                     [instance, fn](const PeerPtr& peer, T m) { (instance->*fn)(peer, std::move(m)); });
    }

    virtual void dispatch(const PeerPtr& peer, uint64_t id, uint64_t reqId, const msgpack::object& object);
    virtual void postDispatch(std::function<void()> fn) = 0;

private:
    template <typename Res, typename Req> struct HandlerFactory {
        static void create(HandlerMap& handlers, std::function<Res(const PeerPtr&, Req)> fn) {
            // Sanity check
            const auto check = handlers.find(Req::hash);
            if (check != handlers.end()) {
                throw std::runtime_error("The type of this message has already been registered");
            }

            handlers[Req::hash] = [fn = std::move(fn)](const PeerPtr& peer, const uint64_t reqId,
                                                       const msgpack::object& object) {
                Req req{};
                object.convert(req);

                Res res = fn(peer, std::move(req));
                peer->send<Res>(res, reqId, true);
            };
        }
    };

    template <typename Req> struct HandlerFactory<void, Req> {
        static void create(HandlerMap& handlers, std::function<void(const PeerPtr&, Req)> fn) {
            // Sanity check
            const auto check = handlers.find(Req::hash);
            if (check != handlers.end()) {
                throw std::runtime_error("The type of this message has already been registered");
            }

            handlers[Req::hash] = [fn = std::move(fn)](const PeerPtr& peer, const uint64_t reqId,
                                                       const msgpack::object& object) {
                (void)reqId;

                Req req{};
                object.convert(req);

                fn(peer, std::move(req));
            };
        }
    };

    ErrorHandler& errorHandler;
    HandlerMap handlers;
};
} // namespace MsgNet
