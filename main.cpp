#include <iostream>
#include "UllrIOcpp.cpp"

#include "UllrIOcpp.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>

using namespace UllrIOcpp;

namespace
{static int g_failures = 0;

#define CHECK(cond) do {                                                    \
    if (!(cond)) {                                                          \
        std::printf("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        g_failures++;                                                       \
    }                                                                       \
} while (0)

#define CHECK_EQ(a, b) do {                                                 \
    auto _a = (a); auto _b = (b);                                           \
    if (!(_a == _b)) {                                                      \
        std::printf("  FAIL  %s:%d  %s == %s  (%lld vs %lld)\n",            \
            __FILE__, __LINE__, #a, #b,                                     \
            (long long)_a, (long long)_b);                                  \
        g_failures++;                                                       \
    }                                                                       \
} while (0)

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static size_t fileSizeOf(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    return f.is_open() ? (size_t)f.tellg() : 0;
}

static void flipByte(const char* path, size_t offset) {
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    f.seekg(offset);
    char c;
    f.read(&c, 1);
    c ^= 0xFF;
    f.seekp(offset);
    f.write(&c, 1);
}

static void truncateTo(const char* path, size_t newSize) {
    std::ifstream in(path, std::ios::binary);
    std::vector<char> buf(newSize);
    in.read(buf.data(), newSize);
    in.close();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(buf.data(), newSize);
}

// ---------------------------------------------------------------------------
// 1. Round trip, fixed-size payload
// ---------------------------------------------------------------------------

static void test_roundtrip_fixed() {
    std::printf("test_roundtrip_fixed\n");
    const char* path = "test_rt_fixed.bin";

    char payload[5] = {1, 2, 3, 4, 5};
    CHECK_EQ(writeEntireFileCheckSum(payload, 5, path), noError);

    // File must be payload + 8-byte hash
    CHECK_EQ(fileSizeOf(path), 5 + sizeof(HashTyped));

    char readback[5] = {};
    CHECK_EQ(readEntireFileCheckSum(readback, 5, path), noError);
    CHECK(std::memcmp(payload, readback, 5) == 0);
}

// ---------------------------------------------------------------------------
// 2. Round trip, vector payload
// ---------------------------------------------------------------------------

static void test_roundtrip_vector() {
    std::printf("test_roundtrip_vector\n");
    const char* path = "test_rt_vec.bin";

    std::vector<Byte> payload = {'h', 'e', 'l', 'l', 'o'};
    CHECK_EQ(writeEntireFileCheckSum(payload, path), noError);

    std::vector<Byte> readback;
    CHECK_EQ(readEntireFileCheckSum(readback, path), noError);
    CHECK_EQ(readback.size(), payload.size());
    CHECK(readback == payload);
}

// ---------------------------------------------------------------------------
// 3. Empty payload (both overloads)
// ---------------------------------------------------------------------------

static void test_empty_payload() {
    std::printf("test_empty_payload\n");
    const char* path = "test_empty.bin";

    // Empty vector
    std::vector<Byte> empty;
    CHECK_EQ(writeEntireFileCheckSum(empty, path), noError);

    // File must still be 8 bytes (just the hash of nothing)
    CHECK_EQ(fileSizeOf(path), sizeof(HashTyped));

    std::vector<Byte> readback;
    CHECK_EQ(readEntireFileCheckSum(readback, path), noError);
    CHECK_EQ(readback.size(), (size_t)0);

    // Fixed-size, zero bytes
    CHECK_EQ(writeEntireFileCheckSum((const void*)"", (size_t)0, path), noError);
    CHECK_EQ(readEntireFileCheckSum(nullptr, (size_t)0, path), noError);
}

// ---------------------------------------------------------------------------
// 4. Corrupt one payload byte
// ---------------------------------------------------------------------------

static void test_corrupt_payload() {
    std::printf("test_corrupt_payload\n");
    const char* path = "test_corrupt_payload.bin";

    char payload[16] = {};
    std::memset(payload, 0xAA, sizeof(payload));
    CHECK_EQ(writeEntireFileCheckSum(payload, 16, path), noError);

    // Flip a byte in the middle of the payload
    flipByte(path, 8);

    // Size is still 24, so the length gate passes; the hash gate must fail
    char readback[16];
    CHECK_EQ(readEntireFileCheckSum(readback, 16, path), checkSumFail);

    // Same via the vector overload
    std::vector<Byte> v;
    CHECK_EQ(readEntireFileCheckSum(v, path), checkSumFail);
}

// ---------------------------------------------------------------------------
// 5. Corrupt one hash byte (payload intact)
// ---------------------------------------------------------------------------

static void test_corrupt_hash() {
    std::printf("test_corrupt_hash\n");
    const char* path = "test_corrupt_hash.bin";

    char payload[16] = {};
    std::memset(payload, 0xAA, sizeof(payload));
    CHECK_EQ(writeEntireFileCheckSum(payload, 16, path), noError);

    // First byte of the hash is at offset 16
    flipByte(path, 16);

    char readback[16];
    CHECK_EQ(readEntireFileCheckSum(readback, 16, path), checkSumFail);

    std::vector<Byte> v;
    CHECK_EQ(readEntireFileCheckSum(v, path), checkSumFail);
}

// ---------------------------------------------------------------------------
// 6. Wrong expected size (fixed-size overload)
// ---------------------------------------------------------------------------

static void test_wrong_expected_size() {
    std::printf("test_wrong_expected_size\n");
    const char* path = "test_wrong_size.bin";

    char payload[10] = {};
    CHECK_EQ(writeEntireFileCheckSum(payload, 10, path), noError);

    char readback[10];
    // File is 18 bytes; saying we expect 11 bytes means we expect 19 total
    CHECK_EQ(readEntireFileCheckSum(readback, 11, path), fileSizeNotMatch);
    // And saying we expect 9 bytes means we expect 17 total
    CHECK_EQ(readEntireFileCheckSum(readback, 9, path), fileSizeNotMatch);
}

// ---------------------------------------------------------------------------
// 7. Truncated file
// ---------------------------------------------------------------------------

static void test_truncated_file() {
    std::printf("test_truncated_file\n");
    const char* path = "test_trunc.bin";

    char payload[32] = {};
    CHECK_EQ(writeEntireFileCheckSum(payload, 32, path), noError);   // 40 bytes

    truncateTo(path, 20);   // chop off the hash and some payload

    std::vector<Byte> v;
    // readSize = 20, so size = 12.  fnv(12 bytes) != stored hash → checkSumFail.
    // (If the truncation had landed such that size == 0, we'd hit fileSizeNotBigEnough.)
    CHECK_EQ(readEntireFileCheckSum(v, path), checkSumFail);
}

// ---------------------------------------------------------------------------
// 8. File smaller than the hash itself
// ---------------------------------------------------------------------------

static void test_too_small() {
    std::printf("test_too_small\n");
    const char* path = "test_too_small.bin";

    // 4-byte file: smaller than sizeof(HashTyped) == 8
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write("abcd", 4);
    f.close();

    std::vector<Byte> v;
    CHECK_EQ(readEntireFileCheckSum(v, path), fileSizeNotBigEnough);

    // Exactly 8 bytes: contains only a hash, zero payload — but it must have
    // been WRITTEN by writeEntireFileCheckSum to have a valid hash.
    CHECK_EQ(writeEntireFileCheckSum((const void*)"", (size_t)0, path), noError);
    CHECK_EQ(fileSizeOf(path), sizeof(HashTyped));
    CHECK_EQ(readEntireFileCheckSum(v, path), noError);
    CHECK_EQ(v.size(), (size_t)0);

    // Now manually write 8 garbage bytes — should fail the hash gate, not the
    // size gate.
    {
        std::ofstream g(path, std::ios::binary | std::ios::trunc);
        g.write("12345678", 8);
    }
    CHECK_EQ(readEntireFileCheckSum(v, path), checkSumFail);
}

// ---------------------------------------------------------------------------
// 9. Nonexistent file
// ---------------------------------------------------------------------------

static void test_nonexistent() {
    std::printf("test_nonexistent\n");

    char buf[4];
    CHECK_EQ(readEntireFileCheckSum(buf, 4, "this_file_does_not_exist_xyz.bin"),
             couldNotOpenFile);

    std::vector<Byte> v;
    CHECK_EQ(readEntireFileCheckSum(v, "this_file_does_not_exist_xyz.bin"),
             couldNotOpenFile);
}

// ---------------------------------------------------------------------------
// 10. Hash is order-sensitive and byte-sensitive
// ---------------------------------------------------------------------------

static void test_hash_sensitivity() {
    std::printf("test_hash_sensitivity\n");

    const char* a = "abc";
    const char* b = "abd";
    const char* c = "acb";

    auto ha = fnv_hash_1a_64(a, 3);
    auto hb = fnv_hash_1a_64(b, 3);
    auto hc = fnv_hash_1a_64(c, 3);

    CHECK(ha != hb);   // last byte differs
    CHECK(ha != hc);   // two bytes swapped

    // Empty input: must be a fixed, nonzero value (the offset basis)
    auto h0 = fnv_hash_1a_64("", 0);
    CHECK_EQ((unsigned long long)h0, 0xcbf29ce484222325ULL);

    // Tail handling: lengths 0..7 must all be distinct from each other when
    // the extra byte differs
    const char* p0 = "";
    const char* p1 = "a";
    const char* p2 = "ab";
    const char* p3 = "abc";
    const char* p4 = "abcd";
    const char* p5 = "abcde";
    const char* p6 = "abcdef";
    const char* p7 = "abcdefg";
    CHECK(fnv_hash_1a_64(p0,0) != fnv_hash_1a_64(p1,1));
    CHECK(fnv_hash_1a_64(p1,1) != fnv_hash_1a_64(p2,2));
    CHECK(fnv_hash_1a_64(p2,2) != fnv_hash_1a_64(p3,3));
    CHECK(fnv_hash_1a_64(p3,3) != fnv_hash_1a_64(p4,4));
    CHECK(fnv_hash_1a_64(p4,4) != fnv_hash_1a_64(p5,5));
    CHECK(fnv_hash_1a_64(p5,5) != fnv_hash_1a_64(p6,6));
    CHECK(fnv_hash_1a_64(p6,6) != fnv_hash_1a_64(p7,7));
}

// ---------------------------------------------------------------------------
// 11. Round trip via the fixed-size overload with a non-byte-aligned size
// ---------------------------------------------------------------------------

static void test_odd_sizes() {
    std::printf("test_odd_sizes\n");
    const char* path = "test_odd.bin";

    for (size_t n : {1u, 3u, 4u, 5u, 7u, 8u, 9u, 63u, 64u, 65u}) {
        std::vector<char> payload(n);
        for (size_t i = 0; i < n; i++) payload[i] = (char)(i * 31 + 7);

        CHECK_EQ(writeEntireFileCheckSum(payload.data(), n, path), noError);

        std::vector<char> rb(n);
        CHECK_EQ(readEntireFileCheckSum(rb.data(), n, path), noError);
        CHECK(std::memcmp(payload.data(), rb.data(), n) == 0);

        std::vector<Byte> vrb;
        CHECK_EQ(readEntireFileCheckSum(vrb, path), noError);
        CHECK_EQ(vrb.size(), n);
        CHECK(std::memcmp(payload.data(), vrb.data(), n) == 0);
    }
}
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    test_roundtrip_fixed();
    test_roundtrip_vector();
    test_empty_payload();
    test_corrupt_payload();
    test_corrupt_hash();
    test_wrong_expected_size();
    test_truncated_file();
    test_too_small();
    test_nonexistent();
    test_hash_sensitivity();
    test_odd_sizes();

    if (g_failures == 0) std::printf("\nALL PASS\n");
    else                 std::printf("\n%d FAILURES\n", g_failures);
    return g_failures ? 1 : 0;
}
