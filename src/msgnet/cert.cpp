#include "cert.hpp"
#include <memory>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <stdexcept>

using namespace MsgNet;

Cert::Cert(std::string raw) : raw{std::move(raw)} {
    auto bio = std::shared_ptr<BIO>(BIO_new(BIO_s_mem()), [](BIO* b) { BIO_free(b); });

    if (!BIO_write(bio.get(), this->raw.data(), this->raw.size())) {
        throw std::runtime_error("Failed to read certificate from string");
    }

    auto ptr = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
    if (!ptr) {
        throw std::runtime_error("Failed to read X509 certificate");
    }

    x509 = std::shared_ptr<X509>(ptr, [](X509* p) { X509_free(p); });
}

Cert::Cert(const Pkey& pkey) {
    static const std::string subC = "EU";
    static const std::string subO = "msgnet";
    static const std::string subCN = "msgnet";
    static const std::string issuer = "msgnet";

    x509 = std::shared_ptr<X509>(X509_new(), [](X509* p) { X509_free(p); });

    if (!X509_set_version(x509.get(), 2)) {
        throw std::runtime_error("Failed to set X509 version");
    }

    auto serial = std::shared_ptr<ASN1_INTEGER>(ASN1_INTEGER_new(), [](ASN1_INTEGER* p) { ASN1_INTEGER_free(p); });
    if (!X509_set_serialNumber(x509.get(), serial.get())) {
        throw std::runtime_error("Failed to set X509 serial number");
    }

    if (!X509_gmtime_adj(X509_get_notBefore(x509.get()), 0)) {
        throw std::runtime_error("Failed to adjust X509 gmtime");
    }

    if (!X509_gmtime_adj(X509_get_notAfter(x509.get()), (long)60 * 60 * 24 * 365 * 10)) {
        throw std::runtime_error("Failed to adjust X509 gmtime");
    }

    if (!X509_set_pubkey(x509.get(), pkey.get())) {
        throw std::runtime_error("Failed to set X509 public key");
    }

    auto name = X509_get_subject_name(x509.get());
    if (!name) {
        throw std::runtime_error("Failed to get X509 subject name");
    }

    const auto c = reinterpret_cast<const unsigned char*>(subC.c_str());
    if (!X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, c, -1, -1, 0)) {
        throw std::runtime_error("Failed to set X509 subject country");
    }

    const auto o = reinterpret_cast<const unsigned char*>(subO.c_str());
    if (!X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, o, -1, -1, 0)) {
        throw std::runtime_error("Failed to set X509 subject organization");
    }

    const auto cn = reinterpret_cast<const unsigned char*>(subCN.c_str());
    if (!X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, cn, -1, -1, 0)) {
        throw std::runtime_error("Failed to set X509 subject common name");
    }

    if (!X509_set_issuer_name(x509.get(), name)) {
        throw std::runtime_error("Failed to set X509 issuer");
    }

    if (!X509_sign(x509.get(), pkey.get(), EVP_sha512())) {
        throw std::runtime_error("Failed to sign X509");
    }

    auto bio = std::shared_ptr<BIO>(BIO_new(BIO_s_mem()), [](BIO* b) { BIO_free(b); });

    if (!PEM_write_bio_X509(bio.get(), x509.get())) {
        throw std::runtime_error("Failed to write BIO for the X509");
    }

    const auto len = BIO_pending(bio.get());
    raw.resize(len);
    if (!BIO_read(bio.get(), raw.data(), len)) {
        throw std::runtime_error("Failed to read X509");
    }
}

Cert::~Cert() = default;
