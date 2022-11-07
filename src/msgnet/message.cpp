#include "message.hpp"
#include <openssl/sha.h>

using namespace MsgNet;

uint64_t Detail::getMessageHash(const std::string& name) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, name.data(), name.size());
    SHA256_Final(hash, &sha256);

    // Only convert the first part
    return *reinterpret_cast<const uint64_t*>(hash);
}
