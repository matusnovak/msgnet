#include "error.hpp"
#include "peer.hpp"
#include <iostream>

using namespace MsgNet;

const char* ErrorCategory::name() const noexcept {
    return "msgnet";
}

std::string ErrorCategory::message(int ev) const {
    switch (static_cast<Error>(ev)) {
    case Error::HandshakeError: {
        return "TLS handshake error";
    }
    case Error::UnexpectedResponse: {
        return "Receive an unexpected response for a request";
    }
    case Error::BadMessageFormat: {
        return "Receive an incorrectly formatted packet";
    }
    case Error::UnexpectedRequest: {
        return "Unable to dispatch unknown packet, no handler found";
    }
    case Error::UnpackError: {
        return "Msgpack unpack error";
    }
    case Error::DecompressError: {
        return "Unable to decompress data";
    }
    }
}

std::error_condition ErrorCategory::default_error_condition(int ev) const noexcept {
    return std::error_category::default_error_condition(ev);
}

void ErrorHandler::onError(std::error_code ec) {
    std::cout << "Error: " << ec.message() << std::endl;
}

void ErrorHandler::onError(const std::shared_ptr<Peer>& peer, std::error_code ec) {
    std::cout << "Error: " << ec.message() << " from: " << peer->getAddress() << std::endl;
    peer->close();
}

void ErrorHandler::onUnhandledException(const std::shared_ptr<Peer>& peer, std::exception_ptr& eptr) {
    try {
        std::rethrow_exception(eptr);
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what() << " from: " << peer->getAddress() << std::endl;
    }
}
