#pragma once

#include "library.hpp"
#include <memory>
#include <system_error>

namespace MsgNet {
class MSGNET_API Peer;

enum Error {
    HandshakeError,
    UnexpectedResponse,
    BadMessageFormat,
    UnexpectedRequest,
    UnpackError,
    DecompressError,
};

class MSGNET_API ErrorCategory : public std::error_category {
public:
    virtual const char* name() const noexcept;
    virtual std::string message(int ev) const;
    virtual std::error_condition default_error_condition(int ev) const noexcept;
};

static inline const ErrorCategory errorCategory{};

class MSGNET_API ErrorHandler {
public:
    virtual ~ErrorHandler() = default;

    virtual void onError(std::error_code ec);
    virtual void onError(const std::shared_ptr<Peer>& peer, std::error_code ec);
    virtual void onUnhandledException(const std::shared_ptr<Peer>& peer, std::exception_ptr& eptr);
};

inline std::error_code make_error_code(MsgNet::Error e) {
    return {static_cast<int>(e), MsgNet::errorCategory};
}
} // namespace MsgNet

namespace std {
template <> struct is_error_code_enum<MsgNet::Error> : true_type {};
} // namespace std
