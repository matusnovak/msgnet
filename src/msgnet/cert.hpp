#pragma once

#define ASIO_STANDALONE

#include "pkey.hpp"
#include <asio.hpp>
#include <asio/ssl.hpp>

// Forward declaration
struct x509_st;
typedef struct x509_st X509;

namespace MsgNet {
class MSGNET_API Cert {
public:
    static std::string certFromAsioContext(asio::ssl::verify_context& ctx);

    explicit Cert(std::string raw);
    explicit Cert(const Pkey& pkey);
    explicit Cert(asio::ssl::verify_context& ctx);
    ~Cert();

    [[nodiscard]] const std::string& pem() const {
        return raw;
    }

    [[nodiscard]] X509* get() const {
        return x509.get();
    }

    std::string getSubjectName() const;

private:
    std::shared_ptr<X509> x509;
    std::string raw{};
};
} // namespace MsgNet
