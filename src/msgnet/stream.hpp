#pragma once

#include "library.hpp"
#include <memory>
#include <msgpack.hpp>
#include <vector>

namespace MsgNet {

/**
 * Compression stream that can be used with Msgpack.
 * It produces compressed buffers via sendBuffer().
 * You must call flush() after each message end.
 */
class MSGNET_API CompressionStream {
public:
    /**
     * @param blockBytes Maximum number of bytes per each compressed block.
     */
    explicit CompressionStream(size_t blockBytes = 1024 * 8);
    ~CompressionStream();

    /**
     * Write arbitrary number of bytes into the stream.
     *
     * @param src The source data.
     * @param length Length of the source data.
     */
    void write(const char* src, size_t length);

    /**
     * Flush the compressed buffer out. Must be called
     * after each message end.
     */
    void flush();

protected:
    /**
     * The method that gets called every time some buffer needs to be sent out.
     * This will be triggered if the current buffer size reaches "blockBytes"
     * or the flush has been called.
     *
     * @param buffer
     */
    virtual void sendBuffer(std::shared_ptr<std::vector<char>> buffer) = 0;

private:
    struct LZ4;
    std::unique_ptr<LZ4> lz4;
    std::vector<char> raw;
    char* buffers[2];
    size_t idx;
    size_t offset;
};

/**
 * Decompression stream that produces Msgpack object handles
 */
class MSGNET_API DecompressionStream {
public:
    /**
     * @param blockBytes Maximum number of bytes per each compressed block.
     * @warning The blockBytes must match the compression stream's blockBytes!
     */
    explicit DecompressionStream(size_t blockBytes = 1024 * 8);
    ~DecompressionStream();

    /**
     * Accept arbitrary number of bytes.
     * @param src The compressed source data.
     * @param length The length of the source data.
     */
    void accept(const char* src, size_t length);

protected:
    /**
     * Called each time there is an object in the decompressed stream.
     * @param oh The Msgpack object handle.
     */
    virtual void receiveObject(std::shared_ptr<msgpack::object_handle> oh) = 0;

private:
    void decompress();

    struct LZ4;
    std::unique_ptr<LZ4> lz4;
    std::vector<char> raw;
    char* decBuf[2];
    std::vector<char> cmpBuf;
    msgpack::unpacker unp;
    size_t idx;
    size_t offset;
    uint32_t readCount;
};
} // namespace MsgNet
