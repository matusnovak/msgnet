#include <catch.hpp>
#include <msgnet/dh.hpp>
#include <msgnet/pkey.hpp>
#include <msgnet/cert.hpp>
#include <cstring>

using namespace MsgNet;

TEST_CASE("Generate random DH params and then load it back") {
    Dh ec{};
    const auto& pem = ec.pem();
    REQUIRE(pem.empty() == false);
    REQUIRE(pem.find("BEGIN DH PARAMETERS") != std::string::npos);
    REQUIRE(pem.front() == '-');
    REQUIRE(pem.at(pem.size() - 2) == '-');
    REQUIRE(pem.back() == '\n');

    REQUIRE_NOTHROW(Dh{pem});

    REQUIRE(Dh{pem}.pem() == pem);
}

TEST_CASE("Generate random private key and then load it back") {
    Pkey key{};
    const auto& pem = key.pem();
    REQUIRE(pem.empty() == false);
    REQUIRE(pem.find("BEGIN RSA PRIVATE KEY") != std::string::npos);
    REQUIRE(pem.front() == '-');
    REQUIRE(pem.at(pem.size() - 2) == '-');
    REQUIRE(pem.back() == '\n');

    REQUIRE_NOTHROW(Pkey{pem});

    REQUIRE(Pkey{pem}.pem() == pem);
}

TEST_CASE("Generate random certificate and then load it back") {
    Pkey key{};
    Cert cert{key};

    const auto& pem = cert.pem();
    REQUIRE(pem.empty() == false);
    REQUIRE(pem.find("BEGIN CERTIFICATE") != std::string::npos);
    REQUIRE(pem.front() == '-');
    REQUIRE(pem.at(pem.size() - 2) == '-');
    REQUIRE(pem.back() == '\n');

    REQUIRE_NOTHROW(Cert{pem});

    REQUIRE(Cert{pem}.pem() == pem);
}
