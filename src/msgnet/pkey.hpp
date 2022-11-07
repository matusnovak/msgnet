#pragma once

#include "library.hpp"
#include <memory>
#include <string>

// Forward definition
struct evp_pkey_st;
typedef struct evp_pkey_st EVP_PKEY;

namespace MsgNet {
class MSGNET_API Pkey {
public:
    explicit Pkey(std::string raw);
    Pkey();
    ~Pkey();

    [[nodiscard]] const std::string& pem() const {
        return raw;
    }

    [[nodiscard]] EVP_PKEY* get() const {
        return pkey.get();
    }

private:
    std::shared_ptr<EVP_PKEY> pkey;
    std::string raw{};
};
} // namespace MsgNet
