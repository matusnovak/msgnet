#include "pkey.hpp"
#include <memory>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <stdexcept>

using namespace MsgNet;

Pkey::Pkey(std::string raw) : raw{std::move(raw)} {
    auto bio = std::shared_ptr<BIO>(BIO_new(BIO_s_mem()), [](BIO* b) { BIO_free(b); });

    if (!BIO_write(bio.get(), this->raw.data(), this->raw.size())) {
        throw std::runtime_error("Failed to read private key from string");
    }

    auto rsa = PEM_read_bio_RSAPrivateKey(bio.get(), nullptr, nullptr, nullptr);
    if (!rsa) {
        throw std::runtime_error("Failed to read RSA key");
    }

    pkey = std::shared_ptr<EVP_PKEY>(EVP_PKEY_new(), [](EVP_PKEY* p) { EVP_PKEY_free(p); });

    if (!EVP_PKEY_assign_RSA(pkey.get(), rsa)) {
        RSA_free(rsa);
        throw std::runtime_error("Failed to assign RSA to EVP_PKEY");
    }
}

Pkey::Pkey() {
    auto bne = std::shared_ptr<BIGNUM>(BN_new(), [](BIGNUM* p) { BN_free(p); });

    if (!BN_set_word(bne.get(), RSA_F4)) {
        throw std::runtime_error("Failed to set RSA_F4");
    }

    auto rsa = RSA_new();

    if (!RSA_generate_key_ex(rsa, 2048, bne.get(), nullptr)) {
        RSA_free(rsa);
        throw std::runtime_error("Failed to generate RSA key");
    }

    pkey = std::shared_ptr<EVP_PKEY>(EVP_PKEY_new(), [](EVP_PKEY* p) { EVP_PKEY_free(p); });

    if (!EVP_PKEY_assign_RSA(pkey.get(), rsa)) {
        throw std::runtime_error("Failed to assign RSA to EVP_PKEY");
    }

    auto bio = std::shared_ptr<BIO>(BIO_new(BIO_s_mem()), [](BIO* b) { BIO_free(b); });

    if (!PEM_write_bio_RSAPrivateKey(bio.get(), rsa, nullptr, nullptr, 0, nullptr, nullptr)) {
        throw std::runtime_error("Failed to write BIO for the RSA key");
    }

    const auto len = BIO_pending(bio.get());
    raw.resize(len);
    if (!BIO_read(bio.get(), raw.data(), len)) {
        throw std::runtime_error("Failed to read RSA private key");
    }
}

Pkey::~Pkey() = default;
