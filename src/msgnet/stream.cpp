#include "stream.hpp"
#include <cstring>
#include <lz4.h>

using namespace MsgNet;

struct CompressionStream::LZ4 {
    LZ4() {
        LZ4_initStream(lz4Stream, sizeof(*lz4Stream));
    }

    LZ4_stream_t lz4StreamBody{};
    LZ4_stream_t* lz4Stream = &lz4StreamBody;
};

CompressionStream::CompressionStream(const size_t blockBytes) :
    lz4{std::make_unique<LZ4>()}, idx{0}, offset{0}, buffers{nullptr, nullptr} {
    raw.resize(blockBytes * 2);
    buffers[0] = raw.data();
    buffers[1] = raw.data() + blockBytes;
}

CompressionStream::~CompressionStream() = default;

void MsgNet::CompressionStream::write(const char* src, size_t length) {
    while (length > 0) {
        if (offset + length > raw.size() / 2) {
            flush();
        }

        const auto toWrite = std::min(raw.size() / 2 - offset, length);
        if (toWrite == 1) {
            buffers[idx][offset] = *src;
        } else {
            std::memcpy(buffers[idx] + offset, src, toWrite);
        }

        offset += toWrite;
        length -= toWrite;
        src += toWrite;
    }
}

void MsgNet::CompressionStream::flush() {
    auto compressed = std::make_shared<std::vector<char>>();
    const auto compressBound = LZ4_COMPRESSBOUND(raw.size() / 2);

    compressed->resize(sizeof(uint32_t) + compressBound);
    const auto cmpBuf = compressed->data() + sizeof(uint32_t);

    const uint32_t cmpBytes = LZ4_compress_fast_continue(lz4->lz4Stream,                  // Stream
                                                         buffers[idx],                    // Source uncompressed data
                                                         cmpBuf,                          // Destination compressed data
                                                         static_cast<int>(offset),        // Source data length
                                                         static_cast<int>(compressBound), // Destination buffer length
                                                         1);

    // Reset for the next iteration
    offset = 0;
    idx = (idx + 1) % 2;

    if (cmpBytes == 0) {
        return;
    }

    // Resize the target buffer
    compressed->resize(cmpBytes + sizeof(uint32_t));

    // Write the data length (needed for decompression)
    std::memcpy(compressed->data(), &cmpBytes, sizeof(cmpBytes));

    // Send out the buffer
    sendBuffer(std::move(compressed));
}

struct DecompressionStream::LZ4 {
    LZ4() {
        LZ4_setStreamDecode(lz4StreamDecode, nullptr, 0);
    }

    LZ4_streamDecode_t lz4StreamDecodeBody{};
    LZ4_streamDecode_t* lz4StreamDecode = &lz4StreamDecodeBody;
};

DecompressionStream::DecompressionStream(const size_t blockBytes) :
    lz4{std::make_unique<LZ4>()}, idx{0}, offset{0}, readCount{0}, decBuf{nullptr, nullptr} {
    raw.resize(blockBytes * 2);
    decBuf[0] = raw.data();
    decBuf[1] = raw.data() + blockBytes;
    cmpBuf.resize(LZ4_COMPRESSBOUND(blockBytes));
}

DecompressionStream::~DecompressionStream() = default;

void DecompressionStream::accept(const char* src, size_t length) {
    while (length > 0) {
        if (offset < sizeof(uint32_t)) {
            auto readToCopy = std::min(length, sizeof(uint32_t));
            if (offset + readToCopy > sizeof(uint32_t)) {
                readToCopy = sizeof(uint32_t) - offset;
            }

            auto* dst = reinterpret_cast<char*>(&readCount) + offset;
            std::memcpy(dst, src, readToCopy);

            length -= readToCopy;
            offset += readToCopy;
            src += readToCopy;
        }

        if (length == 0) {
            break;
        }

        const auto dst = cmpBuf.data() + offset - sizeof(uint32_t);
        const auto toRead = std::min(static_cast<size_t>(readCount), length);

        if (toRead > LZ4_COMPRESSBOUND(raw.size() / 2)) {
            throw std::runtime_error("Decompress block is too large");
        }

        std::memcpy(dst, src, toRead);

        length -= toRead;
        offset += toRead;
        src += toRead;

        if (offset == readCount + sizeof(uint32_t)) {
            decompress();
            offset = 0;
        }
    }
}

void MsgNet::DecompressionStream::decompress() {
    const int decBytes = LZ4_decompress_safe_continue(lz4->lz4StreamDecode,            // Stream
                                                      cmpBuf.data(),                   // Source compressed data
                                                      decBuf[idx],                     // Destination decompressed data
                                                      static_cast<int>(readCount),     // Number of compressed bytes
                                                      static_cast<int>(raw.size() / 2) // Max size of the destination
    );

    if (decBytes > 0) {
        unp.reserve_buffer(decBytes);
        std::memcpy(unp.buffer(), decBuf[idx], decBytes);
        unp.buffer_consumed(decBytes);

        auto oh = std::make_shared<msgpack::object_handle>();
        while (unp.next(*oh)) {
            receiveObject(std::move(oh));
            oh = std::make_shared<msgpack::object_handle>();
        }
    }

    idx = (idx + 1) % 2;
}
