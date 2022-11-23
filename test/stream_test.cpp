#include <catch.hpp>
#include <cstring>
#include <iostream>
#include <lz4.h>
#include <lz4frame.h>
#include <msgnet/stream.hpp>
#include <msgpack.hpp>

using namespace MsgNet;

static const size_t maxMessageSize = 1024 * 16;

class TestCompressionStream : public CompressionStream {
public:
    explicit TestCompressionStream() : CompressionStream{maxMessageSize} {
    }

    void sendBuffer(std::shared_ptr<std::vector<char>> buffer) override {
        REQUIRE(!!buffer);
        REQUIRE(buffer->size() <= LZ4_COMPRESSBOUND(maxMessageSize));
        buffers.push_back(buffer);
    }

    std::vector<std::shared_ptr<std::vector<char>>> buffers;
};

class TestDecompressionStream : public DecompressionStream {
public:
    TestDecompressionStream() : DecompressionStream{maxMessageSize} {
    }

    void receiveObject(std::shared_ptr<msgpack::object_handle> oh) override {
        objects.push_back(std::move(oh));
    }

    std::vector<std::shared_ptr<msgpack::object_handle>> objects;
};

TEST_CASE("Compress and decompress stream of bytes") {
    TestCompressionStream compress{};
    TestDecompressionStream decompress{};

    std::string msg = "Hello World!";
    int foo = 42;
    bool bar = true;
    std::vector<uint64_t> baz;

    std::mt19937_64 rng{9725674ULL};
    std::uniform_int_distribution<uint64_t> dist{};
    baz.resize(1024 * 1024 * 8);
    for (size_t i = 0; i < baz.size(); i++) {
        baz[i] = dist(rng);
    }

    msgpack::pack(compress, msg);
    msgpack::pack(compress, foo);
    msgpack::pack(compress, bar);
    msgpack::pack(compress, baz);
    compress.flush();

    REQUIRE(compress.buffers.empty() == false);
    // REQUIRE(compress.cmpBuffers.size() > 1);

    const auto& buff = *compress.buffers.front();
    REQUIRE(buff.size() > 15);

    decompress.accept(buff.data(), 1);
    decompress.accept(buff.data() + 1, 2);
    decompress.accept(buff.data() + 3, 10);
    decompress.accept(buff.data() + 13, 1);
    decompress.accept(buff.data() + 14, buff.size() - 14);

    for (const auto& b : compress.buffers) {
        if (&b == &compress.buffers.front()) {
            continue;
        }

        decompress.accept(b->data(), b->size());
    }

    REQUIRE(decompress.objects.empty() == false);
    REQUIRE(decompress.objects.size() == 4);

    std::string msgDec;
    int fooDec;
    bool barDec;
    std::vector<uint64_t> bazDec;

    decompress.objects[0]->get().convert(msgDec);
    decompress.objects[1]->get().convert(fooDec);
    decompress.objects[2]->get().convert(barDec);
    decompress.objects[3]->get().convert(bazDec);

    REQUIRE(msgDec == msg);
    REQUIRE(fooDec == foo);
    REQUIRE(barDec == bar);
    REQUIRE(bazDec == baz);
}

TEST_CASE("Compress and decompress random chunks") {
    TestCompressionStream compress{};
    TestDecompressionStream decompress{};

    std::mt19937_64 rng{9725674ULL};
    std::uniform_int_distribution<uint64_t> dist{};
    std::uniform_int_distribution<uint64_t> distLength{128, 1024 * 16};

    const size_t maxTotal = 1024 * 1024 * 64;
    size_t total = 0;

    std::vector<std::vector<char>> originals;

    // Compress and send all buffers
    while (total < maxTotal) {
        originals.emplace_back();
        auto& buffer = originals.back();

        auto length = distLength(rng);
        if (total + length < maxTotal) {
            length = maxTotal - total;
        }

        buffer.resize(length);
        for (size_t i = 0; i < buffer.size(); i++) {
            buffer[i] = dist(rng);
        }

        total += buffer.size();

        msgpack::pack(compress, buffer);
        compress.flush();
    }

    REQUIRE(compress.buffers.empty() == false);

    // Decompress
    size_t totalCompressed = 0;
    for (const auto& b : compress.buffers) {
        totalCompressed += b->size();
        decompress.accept(b->data(), b->size());
    }

    REQUIRE(decompress.objects.empty() == false);
    REQUIRE(decompress.objects.size() == originals.size());

    for (size_t i = 0; i < originals.size(); i++) {
        std::vector<char> data;
        decompress.objects[i]->get().convert(data);

        REQUIRE(data == originals[i]);
    }

    std::cout << "Original: " << maxTotal << " bytes, compressed: " << totalCompressed << " bytes" << std::endl;
}
