#pragma once

#include "pkey.hpp"

// Forward declaration
struct x509_st;
typedef struct x509_st X509;

namespace MsgNet {
class MSGNET_API Cert {
public:
    explicit Cert(std::string raw);
    explicit Cert(const Pkey& pkey);
    ~Cert();

    [[nodiscard]] const std::string& pem() const {
        return raw;
    }

    [[nodiscard]] X509* get() const {
        return x509.get();
    }

private:
    std::shared_ptr<X509> x509;
    std::string raw{};
};
} // namespace MsgNet
