#include <iostream>
#include "UllrIOcpp.cpp"

#include "UllrIOcpp.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>


#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace UllrIOcpp;

static int g_failures = 0;

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

#include <filesystem>
namespace fs = std::filesystem;

// Compute the three paths the same way safeSave/safeLoad do, so tests can
// check them without duplicating the stem/extension logic.
struct Trio {
    fs::path file1;
    fs::path file2;
    fs::path tmp;
};

static Trio pathsFor(const char* name) {
    fs::path p(name);
    std::string stem = p.stem().string();
    std::string ext = p.extension().string();
    if (ext.empty()) ext = ".bin";
    return {
        p.parent_path() / (stem + "1" + ext),
        p.parent_path() / (stem + "2" + ext),
        p.parent_path() / (stem + "1.tmp"),
    };
}

static void removeIfExists(const fs::path& p) {
    std::error_code ec;
    fs::remove(p, ec);
}

static void cleanAll(const char* name) {
    auto t = pathsFor(name);
    removeIfExists(t.file1);
    removeIfExists(t.file2);
    removeIfExists(t.tmp);
}

static bool myExists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

static size_t fileSizeOf(const fs::path& p) {
    std::error_code ec;
    auto s = fs::file_size(p, ec);
    return ec ? 0 : (size_t)s;
}

// Overwrite the byte at `offset` with `value`, regardless of file size.
static void pokeByte(const fs::path& p, size_t offset, char value) {
    std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
    f.seekp(offset);
    f.write(&value, 1);
}

static void flipByte(const fs::path& p, size_t offset) {
    std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
    f.seekg(offset);
    char c = 0;
    f.read(&c, 1);
    c ^= 0xFF;
    f.seekp(offset);
    f.write(&c, 1);
}

static void writeRaw(const fs::path& p, const void* data, size_t size) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write((const char*)data, size);
}

static std::vector<char> readRaw(const fs::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    size_t s = f.tellg();
    f.seekg(0);
    std::vector<char> v(s);
    f.read(v.data(), s);
    return v;
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

static void test_safe_roundtrip() {
    std::printf("test_safe_roundtrip\n");
    const char* name = "t_rt";
    cleanAll(name);

    std::vector<Byte> payload = {'h','e','l','l','o'};
    CHECK_EQ(safeSave(payload, name, false), noError);

    auto t = pathsFor(name);
    CHECK(exists(t.file1));
    CHECK(exists(t.file2));
    CHECK(!exists(t.tmp));                 // tmp must be gone after rename

    // Both files have the exact same size (payload + 8-byte hash)
    CHECK_EQ(fileSizeOf(t.file1), payload.size() + sizeof(HashTyped));
    CHECK_EQ(fileSizeOf(t.file2), payload.size() + sizeof(HashTyped));

    // Byte-identical (this is the property the copy-based backup guarantees)
    CHECK(readRaw(t.file1) == readRaw(t.file2));

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, false), noError);
    CHECK(loaded == std::vector<Byte>(payload.begin(), payload.end()));

    cleanAll(name);
}

static void test_backup_recovery_silent() {
    std::printf("test_backup_recovery_silent\n");
    const char* name = "t_brs";
    cleanAll(name);

    std::vector<Byte> payload = {'o','r','i','g','i','n','a','l'};
    CHECK_EQ(safeSave(payload, name, false), noError);

    auto t = pathsFor(name);
    flipByte(t.file1, 0);                  // corrupt primary

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, /*reportBackup=*/false), noError);
    CHECK(loaded == std::vector<Byte>(payload.begin(), payload.end()));

    cleanAll(name);
}

static void test_backup_recovery_reported() {
    std::printf("test_backup_recovery_reported\n");
    const char* name = "t_brr";
    cleanAll(name);

    std::vector<Byte> payload = {'o','r','i','g','i','n','a','l'};
    CHECK_EQ(safeSave(payload, name, false), noError);

    auto t = pathsFor(name);
    flipByte(t.file1, 0);

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, /*reportBackup=*/true), readBackup);
    // Data must still be correct even though an error is reported.
    CHECK(loaded == std::vector<Byte>(payload.begin(), payload.end()));

    cleanAll(name);
}

static void test_both_corrupt() {
    std::printf("test_both_corrupt\n");
    const char* name = "t_bc";
    cleanAll(name);

    std::vector<Byte> payload = {'x','y','z'};
    CHECK_EQ(safeSave(payload, name, false), noError);

    auto t = pathsFor(name);
    flipByte(t.file1, 0);
    flipByte(t.file2, 0);

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, false), checkSumFail);

    // Verify data is empty on failure (see note at the end of my previous msg)
    // — you added a `data.clear()` at the end of safeLoad? If not, this may
    // be garbage. Test what you actually implemented.
    // CHECK(loaded.empty());

    cleanAll(name);
}

static void test_primary_missing() {
    std::printf("test_primary_missing\n");
    const char* name = "t_pm";
    cleanAll(name);

    std::vector<Byte> payload = {1,2,3,4};
    CHECK_EQ(safeSave(payload, name, false), noError);

    auto t = pathsFor(name);
    removeIfExists(t.file1);

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, true), readBackup);
    CHECK(loaded == std::vector<Byte>(payload.begin(), payload.end()));

    cleanAll(name);
}

static void test_backup_fail_silent() {
    std::printf("test_backup_fail_silent\n");
    const char* name = "t_bfs";
    cleanAll(name);

    auto t = pathsFor(name);
    // Make file2 a directory — copy_file cannot overwrite a directory with a file.
    fs::create_directory(t.file2);

    std::vector<Byte> payload = {'a','b','c'};
    CHECK_EQ(safeSave(payload, name, /*reportBackupError=*/false), noError);

    // Primary must still be valid and complete.
    CHECK(exists(t.file1));
    CHECK_EQ(fileSizeOf(t.file1), payload.size() + sizeof(HashTyped));

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, false), noError);
    CHECK(loaded == std::vector<Byte>(payload.begin(), payload.end()));

    fs::remove_all(t.file2);
    cleanAll(name);
}

static void test_backup_fail_reported() {
    std::printf("test_backup_fail_reported\n");
    const char* name = "t_bfr";
    cleanAll(name);

    auto t = pathsFor(name);
    fs::create_directory(t.file2);

    std::vector<Byte> payload = {'a','b','c'};
    CHECK_EQ(safeSave(payload, name, /*reportBackupError=*/true), couldNotMakeBackup);

    // Even though the return code is an error, the primary is valid.
    // That's what makes the flag useful: the caller can decide to ignore
    // the error and continue.
    CHECK(exists(t.file1));
    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, false), noError);
    CHECK(loaded == std::vector<Byte>(payload.begin(), payload.end()));

    fs::remove_all(t.file2);
    cleanAll(name);
}

static void test_crash_tmp_ignored() {
    std::printf("test_crash_tmp_ignored\n");
    const char* name = "t_crash";
    cleanAll(name);

    std::vector<Byte> good = {'g','o','o','d','\0','d','a','t','a'};
    CHECK_EQ(safeSave(good, name, false), noError);

    // Simulate a crash: leave garbage in the tmp file after a valid save.
    auto t = pathsFor(name);
    const char* garbage = "THIS IS NOT VALID CHECKSUMMED DATA AT ALL";
    writeRaw(t.tmp, garbage, std::strlen(garbage));

    // Loading must succeed from file1, ignoring the tmp completely.
    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, false), noError);
    CHECK(loaded == std::vector<Byte>(good.begin(), good.end()));

    cleanAll(name);
}


static void test_load_backup_directly() {
    std::printf("test_load_backup_directly\n");
    const char* name = "t_lbd";
    cleanAll(name);

    std::vector<Byte> payload = {'b','a','c','k','u','p'};
    CHECK_EQ(safeSave(payload, name, false), noError);

    auto t = pathsFor(name);
    flipByte(t.file1, 0);                  // primary is bad, backup is good

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoadBackup(loaded, name), noError);
    CHECK(loaded == std::vector<Byte>(payload.begin(), payload.end()));

    // Corrupt backup too — now nothing works.
    flipByte(t.file2, 0);
    loaded.clear();
    CHECK_EQ(safeLoadBackup(loaded, name), checkSumFail);

    // And standard safeLoad also fails (both are corrupt).
    loaded.clear();
    CHECK_EQ(safeLoad(loaded, name, false), checkSumFail);

    cleanAll(name);
}

static void test_safe_empty_payload() {
    std::printf("test_safe_empty_payload\n");
    const char* name = "t_empty";
    cleanAll(name);

    std::vector<Byte> empty;
    CHECK_EQ(safeSave(empty, name, false), noError);

    auto t = pathsFor(name);
    CHECK_EQ(fileSizeOf(t.file1), sizeof(HashTyped));   // just the hash
    CHECK_EQ(fileSizeOf(t.file2), sizeof(HashTyped));

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, false), noError);
    CHECK(loaded.empty());

    cleanAll(name);
}

static void test_path_variants() {
    std::printf("test_path_variants\n");

    // Case A: plain name, no extension
    {
        const char* name = "t_path_a";
        cleanAll(name);
        std::vector<Byte> p = {1};
        CHECK_EQ(safeSave(p, name, false), noError);
        CHECK(myExists("t_path_a1.bin"));
        CHECK(myExists("t_path_a2.bin"));
        CHECK(!myExists("t_path_a1.tmp"));
        cleanAll(name);
    }

    // Case B: name with .bin extension
    {
        const char* name = "t_path_b.bin";
        cleanAll(name);
        std::vector<Byte> p = {1};
        CHECK_EQ(safeSave(p, name, false), noError);
        CHECK(myExists("t_path_b1.bin"));
        CHECK(myExists("t_path_b2.bin"));
        cleanAll(name);
    }

    // Case C: name inside a subdirectory
    {
        fs::create_directory("t_path_dir");
        const char* name = "t_path_dir/save.bin";
        cleanAll(name);
        std::vector<Byte> p = {1};
        CHECK_EQ(safeSave(p, name, false), noError);
        CHECK(myExists("t_path_dir/save1.bin"));
        CHECK(myExists("t_path_dir/save2.bin"));
        cleanAll(name);
        fs::remove("t_path_dir");
    }

    // Case D: multiple dots. This is the interesting one.
    // stem = "save.data", extension = ".bin", so file1 = "save.data1.bin"
    {
        const char* name = "t_multi.data.bin";
        cleanAll(name);
        std::vector<Byte> p = {1};
        CHECK_EQ(safeSave(p, name, false), noError);
        CHECK(myExists("t_multi.data1.bin"));   // <-- number goes before last dot
        CHECK(myExists("t_multi.data2.bin"));
        cleanAll(name);
    }
}

static void test_no_backup_save() {
    std::printf("test_no_backup_save\n");
    const char* name = "t_nb";
    cleanAll(name);

    std::vector<Byte> payload = {9,9,9};
    CHECK_EQ(safeSaveNoBackup(payload, name), noError);

    auto t = pathsFor(name);
    CHECK(exists(t.file1));
    CHECK(!exists(t.tmp));
    CHECK(!exists(t.file2));               // no backup was written

    std::vector<Byte> loaded;
    CHECK_EQ(safeLoad(loaded, name, false), noError);
    CHECK(loaded == std::vector<Byte>(payload.begin(), payload.end()));

    // Now corrupt file1 — there's no backup to fall back to.
    flipByte(t.file1, 0);
    loaded.clear();
    CHECK_EQ(safeLoad(loaded, name, false), checkSumFail);

    cleanAll(name);
}

static void test_load_nonexistent() {
    std::printf("test_load_nonexistent\n");
    const char* name = "t_never_written";
    cleanAll(name);

    std::vector<Byte> loaded;
    // Neither file1 nor file2 exists → couldNotOpenFile from the last attempt
    CHECK_EQ(safeLoad(loaded, name, false), couldNotOpenFile);

    // safeLoadBackup also fails, same reason
    CHECK_EQ(safeLoadBackup(loaded, name), couldNotOpenFile);
}

static void test_repeated_saves() {
    std::printf("test_repeated_saves\n");
    const char* name = "t_repeat";
    cleanAll(name);

    for (int i = 0; i < 5; i++) {
        std::vector<Byte> p(10, (char)i);
        CHECK_EQ(safeSave(p, name, false), noError);

        std::vector<Byte> loaded;
        CHECK_EQ(safeLoad(loaded, name, false), noError);
        CHECK_EQ(loaded.size(), (size_t)10);
        CHECK(loaded[0] == (Byte)i);
    }

    auto t = pathsFor(name);
    CHECK(!exists(t.tmp));                 // no tmp left behind

    cleanAll(name);
}

// ============================================================
// MAP LAYER TESTS
// ============================================================

static bool doRoundtrip(BinaryDataFormat& src, BinaryDataFormat& dst) {
    std::vector<Byte> bytes;
    if (src.serializeToFileData(bytes) != noError) return false;
    return dst.deserializeFromFileData(bytes.data(), bytes.size()) == noError;
}

static void test_map_empty() {
    std::printf("test_map_empty\n");
    BinaryDataFormat m;
    std::vector<Byte> bytes;
    CHECK_EQ(m.serializeToFileData(bytes), noError);

    // Just the entry count: 4 bytes of zero.
    CHECK_EQ(bytes.size(), (size_t)4);
    CHECK_EQ(bytes[0], 0); CHECK_EQ(bytes[1], 0);
    CHECK_EQ(bytes[2], 0); CHECK_EQ(bytes[3], 0);

    BinaryDataFormat m2;
    CHECK_EQ(m2.deserializeFromFileData(bytes.data(), bytes.size()), noError);
    CHECK(m2.umData.empty());
}

static void test_map_single_int() {
    std::printf("test_map_single_int\n");
    BinaryDataFormat m;
    CHECK_EQ(m.setInt32("x", 42), noError);

    std::vector<Byte> bytes;
    CHECK_EQ(m.serializeToFileData(bytes), noError);
    // 4 count + 4 keySize + 1 key + 4 type + 4 payloadSize + 4 payload = 21
    CHECK_EQ(bytes.size(), (size_t)21);

    BinaryDataFormat m2;
    CHECK_EQ(m2.deserializeFromFileData(bytes.data(), bytes.size()), noError);

    std::int32_t v = -1;
    CHECK_EQ(m2.getInt32("x", v), noError);
    CHECK_EQ(v, 42);
}

static void test_map_many_ints() {
    std::printf("test_map_many_ints\n");
    BinaryDataFormat m;
    for (int i = 0; i < 20; i++) {
        std::string k = "key" + std::to_string(i);
        CHECK_EQ(m.setInt32(k, i * 100), noError);
    }

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));
    CHECK_EQ(m2.umData.size(), (size_t)20);
    for (int i = 0; i < 20; i++) {
        std::string k = "key" + std::to_string(i);
        std::int32_t v = -1;
        CHECK_EQ(m2.getInt32(k, v), noError);
        CHECK_EQ(v, i * 100);
    }
}

static void test_map_overwrite() {
    std::printf("test_map_overwrite\n");
    BinaryDataFormat m;
    CHECK_EQ(m.setInt32("x", 1), noError);
    CHECK_EQ(m.setInt32("x", 2), dataFormatValueAlreadyExists);
    CHECK_EQ(m.umData.size(), (size_t)1);

    std::int32_t v = 0;
    CHECK_EQ(m.getInt32("x", v), noError);
    CHECK_EQ(v, 2);
}

static void test_type_bool() {
    std::printf("test_type_bool\n");
    BinaryDataFormat m;
    m.setBool("t", true);
    m.setBool("f", false);

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    bool b = false;
    CHECK_EQ(m2.getBool("t", b), noError); CHECK_EQ(b, true);
    CHECK_EQ(m2.getBool("f", b), noError); CHECK_EQ(b, false);
}

static void test_type_int32() {
    std::printf("test_type_int32\n");
    BinaryDataFormat m;
    m.setInt32("min", std::numeric_limits<std::int32_t>::min());
    m.setInt32("max", std::numeric_limits<std::int32_t>::max());
    m.setInt32("neg", -12345);

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    std::int32_t v = 0;
    CHECK_EQ(m2.getInt32("min", v), noError);
    CHECK_EQ(v, std::numeric_limits<std::int32_t>::min());
    CHECK_EQ(m2.getInt32("max", v), noError);
    CHECK_EQ(v, std::numeric_limits<std::int32_t>::max());
    CHECK_EQ(m2.getInt32("neg", v), noError);
    CHECK_EQ(v, -12345);
}

static void test_type_uint32() {
    std::printf("test_type_uint32\n");
    BinaryDataFormat m;
    m.setUInt32("zero", 0u);
    m.setUInt32("max", std::numeric_limits<std::uint32_t>::max());

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    std::uint32_t v = 0;
    CHECK_EQ(m2.getUInt32("zero", v), noError); CHECK_EQ(v, 0u);
    CHECK_EQ(m2.getUInt32("max", v), noError);
    CHECK_EQ(v, std::numeric_limits<std::uint32_t>::max());
}

static void test_type_int64() {
    std::printf("test_type_int64\n");
    BinaryDataFormat m;
    m.setInt64("neg", INT64_C(-987654321012345678));
    m.setInt64("max", std::numeric_limits<std::int64_t>::max());

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    std::int64_t v = 0;
    CHECK_EQ(m2.getInt64("neg", v), noError);
    CHECK_EQ(v, INT64_C(-987654321012345678));
    CHECK_EQ(m2.getInt64("max", v), noError);
    CHECK_EQ(v, std::numeric_limits<std::int64_t>::max());
}

static void test_type_uint64() {
    std::printf("test_type_uint64\n");
    BinaryDataFormat m;
    m.setUInt64("max", std::numeric_limits<std::uint64_t>::max());
    m.setUInt64("mid", UINT64_C(0xDEADBEEFCAFEBABE));

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    std::uint64_t v = 0;
    CHECK_EQ(m2.getUInt64("max", v), noError);
    CHECK_EQ(v, std::numeric_limits<std::uint64_t>::max());
    CHECK_EQ(m2.getUInt64("mid", v), noError);
    CHECK_EQ(v, UINT64_C(0xDEADBEEFCAFEBABE));
}

static void test_type_float() {
    std::printf("test_type_float\n");
    BinaryDataFormat m;
    m.setFloat("pi", 3.14159f);
    m.setFloat("neg", -0.5f);

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    float v = 0;
    CHECK_EQ(m2.getFloat("pi", v), noError);
    CHECK(v == 3.14159f);   // bit-exact for round-tripped float
    CHECK_EQ(m2.getFloat("neg", v), noError);
    CHECK(v == -0.5f);
}

static void test_type_double() {
    std::printf("test_type_double\n");
    BinaryDataFormat m;
    m.setDouble("e", 2.718281828459045);
    m.setDouble("tiny", 1e-300);

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    double v = 0;
    CHECK_EQ(m2.getDouble("e", v), noError);
    CHECK(v == 2.718281828459045);
    CHECK_EQ(m2.getDouble("tiny", v), noError);
    CHECK(v == 1e-300);
}

static void test_type_char_uchar() {
    std::printf("test_type_char_uchar\n");
    BinaryDataFormat m;
    m.setChar("a", 'A');
    m.setUChar("b", 200);

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    char c = 0;
    CHECK_EQ(m2.getChar("a", c), noError); CHECK_EQ(c, 'A');
    unsigned char u = 0;
    CHECK_EQ(m2.getUChar("b", u), noError); CHECK_EQ(u, (unsigned char)200);
}

static void test_type_string() {
    std::printf("test_type_string\n");
    BinaryDataFormat m;
    m.setString("empty", "");
    m.setString("ascii", "hello world");
    m.setString("with_null", std::string("a\0b", 3));  // embedded NUL

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    std::string s;
    CHECK_EQ(m2.getString("empty", s), noError);
    CHECK(s.empty());
    CHECK_EQ(m2.getString("ascii", s), noError);
    CHECK(s == "hello world");
    CHECK_EQ(m2.getString("with_null", s), noError);
    CHECK_EQ(s.size(), (size_t)3);
    CHECK(s[0] == 'a'); CHECK(s[1] == '\0'); CHECK(s[2] == 'b');
}

static void test_type_raw_bytes() {
    std::printf("test_type_raw_bytes\n");
    std::vector<Byte> payload = {0x00, 0xFF, 0x42, 0x13, 0x37};
    BinaryDataFormat m;
    m.setRawBytes("blob", payload);

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    std::vector<Byte> out;
    CHECK_EQ(m2.getRawBytes("blob", out), noError);
    CHECK(out == payload);
}

static void test_type_vec234() {
    std::printf("test_type_vec234\n");
    BinaryDataFormat m;
    m.setVec2("v2", 1.0f, 2.0f);
    m.setVec3("v3", 1.0f, 2.0f, 3.0f);
    m.setVec4("v4", 1.0f, 2.0f, 3.0f, 4.0f);

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    float x, y, z, w;
    CHECK_EQ(m2.getVec2("v2", x, y), noError);
    CHECK(x == 1.0f && y == 2.0f);
    CHECK_EQ(m2.getVec3("v3", x, y, z), noError);
    CHECK(x == 1.0f && y == 2.0f && z == 3.0f);
    CHECK_EQ(m2.getVec4("v4", x, y, z, w), noError);
    CHECK(x == 1.0f && y == 2.0f && z == 3.0f && w == 4.0f);
}

static void test_type_ivec234() {
    std::printf("test_type_ivec234\n");
    BinaryDataFormat m;
    m.setIVec2("v2", -1, 2);
    m.setIVec3("v3", -1, 2, -3);
    m.setIVec4("v4", -1, 2, -3, 4);

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));

    int x, y, z, w;
    CHECK_EQ(m2.getIVec2("v2", x, y), noError);
    CHECK(x == -1 && y == 2);
    CHECK_EQ(m2.getIVec3("v3", x, y, z), noError);
    CHECK(x == -1 && y == 2 && z == -3);
    CHECK_EQ(m2.getIVec4("v4", x, y, z, w), noError);
    CHECK(x == -1 && y == 2 && z == -3 && w == 4);
}

static void test_type_keyvalue() {
    std::printf("test_type_keyvalue\n");
    // Serialize an inner map into bytes, store it as a keyValue value
    BinaryDataFormat inner;
    inner.setInt32("hp", 100);
    inner.setString("name", "bob");

    std::vector<Byte> innerBytes;
    CHECK_EQ(inner.serializeToFileData(innerBytes), noError);

    BinaryDataFormat outer;
    outer.setKeyValue("player", innerBytes);

    BinaryDataFormat outer2;
    CHECK(doRoundtrip(outer, outer2));

    std::vector<Byte> retrieved;
    CHECK_EQ(outer2.getKeyValue("player", retrieved), noError);
    CHECK(retrieved == innerBytes);

    // Decode the inner bytes and verify
    BinaryDataFormat inner2;
    CHECK_EQ(inner2.deserializeFromFileData(retrieved.data(), retrieved.size()),
             noError);
    std::int32_t hp = 0;
    CHECK_EQ(inner2.getInt32("hp", hp), noError);
    CHECK_EQ(hp, 100);
}

static void test_map_mixed() {
    std::printf("test_map_mixed\n");
    BinaryDataFormat m;
    m.setInt32("health", 100);
    m.setFloat("gravity", 9.8f);
    m.setBool("alive", true);
    m.setString("name", "player one");
    m.setInt64("score", INT64_C(1234567890123));
    m.setVec3("position", 1.5f, 2.5f, -3.5f);
    m.setIVec2("tile", 12, 34);
    m.setRawBytes("blob", {0x01, 0x02, 0x03});

    BinaryDataFormat m2;
    CHECK(doRoundtrip(m, m2));
    CHECK(m == m2);

    // Spot-check a few values
    std::int32_t i = 0;
    CHECK_EQ(m2.getInt32("health", i), noError); CHECK_EQ(i, 100);
    std::string s;
    CHECK_EQ(m2.getString("name", s), noError); CHECK(s == "player one");
}

static void test_equality() {
    std::printf("test_equality\n");
    BinaryDataFormat a, b;
    CHECK(a == b);

    a.setInt32("x", 1);
    CHECK(a != b);

    b.setInt32("x", 1);
    CHECK(a == b);

    b.setInt32("x", 2);
    CHECK(a != b);

    // Different key, same value
    BinaryDataFormat c, d;
    c.setInt32("x", 5);
    d.setInt32("y", 5);
    CHECK(c != d);

    // Different types, same logical value
    BinaryDataFormat e, f;
    e.setInt32("x", 0);
    f.setBool("x", false);
    CHECK(e != f);
}

static void test_missing_key() {
    std::printf("test_missing_key\n");
    BinaryDataFormat m;
    m.setInt32("present", 5);

    std::int32_t v = -999;
    CHECK_EQ(m.getInt32("absent", v), dataFormatValueNotFound);
    CHECK_EQ(v, -999);      // untouched

    CHECK(m.valueExists("present"));
    CHECK(!m.valueExists("absent"));
}

static void test_wrong_type() {
    std::printf("test_wrong_type\n");
    BinaryDataFormat m;
    m.setInt32("x", 42);

    // Getting a different type than what was set
    float f = -999.0f;
    CHECK_EQ(m.getFloat("x", f), dataFormatValueDifferentDataType);
    CHECK(f == -999.0f);

    std::string s = "untouched";
    CHECK_EQ(m.getString("x", s), dataFormatValueDifferentDataType);
    CHECK(s == "untouched");

    std::vector<Byte> b = {0xFF};
    CHECK_EQ(m.getRawBytes("x", b), dataFormatValueDifferentDataType);
    CHECK(b == std::vector<Byte>{0xFF});
}

static void test_get_type() {
    std::printf("test_get_type\n");
    BinaryDataFormat m;
    m.setInt32("i", 0);
    m.setFloat("f", 0);
    m.setString("s", "");

    BinaryDataFormat::MapValueFormat::Types t;

    CHECK_EQ(m.getMapValueFormatType("i", t), noError);
    CHECK(t == BinaryDataFormat::MapValueFormat::Types::intType);

    CHECK_EQ(m.getMapValueFormatType("f", t), noError);
    CHECK(t == BinaryDataFormat::MapValueFormat::Types::floatType);

    CHECK_EQ(m.getMapValueFormatType("s", t), noError);
    CHECK(t == BinaryDataFormat::MapValueFormat::Types::stringType);

    t = BinaryDataFormat::MapValueFormat::Types::intType;
    CHECK_EQ(m.getMapValueFormatType("nope", t), dataFormatValueNotFound);
    CHECK(t == BinaryDataFormat::MapValueFormat::Types::noType);  // reset
}

static void test_deser_truncated() {
    std::printf("test_deser_truncated\n");

    // Empty buffer
    {
        BinaryDataFormat m;
        CHECK_EQ(m.deserializeFromFileData(nullptr, 0), fileSizeNotBigEnough);
    }

    // Fewer than 4 bytes (no entry count)
    {
        Byte buf[3] = {0, 0, 0};
        BinaryDataFormat m;
        CHECK_EQ(m.deserializeFromFileData(buf, 3), fileSizeNotBigEnough);
    }

    // Says 1 entry but nothing follows
    {
        std::vector<Byte> buf;
        writeU32LE(buf, 1);
        BinaryDataFormat m;
        CHECK_EQ(m.deserializeFromFileData(buf.data(), buf.size()),
                 fileSizeNotBigEnough);
    }

    // Valid up to keySize, but key bytes missing
    {
        std::vector<Byte> buf;
        writeU32LE(buf, 1);         // count
        writeU32LE(buf, 5);         // keySize = 5 but no key follows
        BinaryDataFormat m;
        CHECK_EQ(m.deserializeFromFileData(buf.data(), buf.size()),
                 fileSizeNotBigEnough);
    }

    // Valid header, but payload truncated
    {
        std::vector<Byte> buf;
        writeU32LE(buf, 1);
        writeU32LE(buf, 1); buf.push_back('x');
        writeU32LE(buf, 3);         // intType
        writeU32LE(buf, 4);         // payloadSize = 4
        buf.push_back(0);           // only 1 byte of payload
        BinaryDataFormat m;
        CHECK_EQ(m.deserializeFromFileData(buf.data(), buf.size()),
                 fileSizeNotBigEnough);
    }
}

static void test_deser_bad_type() {
    std::printf("test_deser_bad_type\n");
    std::vector<Byte> buf;
    writeU32LE(buf, 1);             // count
    writeU32LE(buf, 1); buf.push_back('x');
    writeU32LE(buf, 99);            // invalid type tag
    writeU32LE(buf, 0);             // payloadSize = 0

    BinaryDataFormat m;
    CHECK_EQ(m.deserializeFromFileData(buf.data(), buf.size()),
             couldNotParseData);
}

static void test_deser_wrong_payload_size() {
    std::printf("test_deser_wrong_payload_size\n");

    // intType with payloadSize = 3 (should be 4)
    {
        std::vector<Byte> buf;
        writeU32LE(buf, 1);
        writeU32LE(buf, 1); buf.push_back('x');
        writeU32LE(buf, 3);         // intType
        writeU32LE(buf, 3);         // wrong payloadSize
        buf.push_back(0); buf.push_back(0); buf.push_back(0);
        BinaryDataFormat m;
        CHECK_EQ(m.deserializeFromFileData(buf.data(), buf.size()),
                 couldNotParseData);
    }

    // intType with payloadSize = 5 (should be 4) — extra bytes must fail
    {
        std::vector<Byte> buf;
        writeU32LE(buf, 1);
        writeU32LE(buf, 1); buf.push_back('x');
        writeU32LE(buf, 3);         // intType
        writeU32LE(buf, 5);         // too many
        for (int i = 0; i < 5; i++) buf.push_back(0);
        BinaryDataFormat m;
        CHECK_EQ(m.deserializeFromFileData(buf.data(), buf.size()),
                 couldNotParseData);
    }
}

static void test_deser_duplicate_key() {
    std::printf("test_deser_duplicate_key\n");
    std::vector<Byte> buf;
    writeU32LE(buf, 2);             // count = 2

    // Entry 1: "x" = int 1
    writeU32LE(buf, 1); buf.push_back('x');
    writeU32LE(buf, 3);             // intType
    writeU32LE(buf, 4);
    writeI32LE(buf, 1);

    // Entry 2: "x" = int 2 (same key!)
    writeU32LE(buf, 1); buf.push_back('x');
    writeU32LE(buf, 3);
    writeU32LE(buf, 4);
    writeI32LE(buf, 2);

    BinaryDataFormat m;
    CHECK_EQ(m.deserializeFromFileData(buf.data(), buf.size()),
             dataFormatValueAlreadyExists);
}

static void test_deser_trailing_bytes() {
    std::printf("test_deser_trailing_bytes\n");

    // Build a valid serialization, then append garbage
    BinaryDataFormat src;
    src.setInt32("x", 42);
    std::vector<Byte> buf;
    src.serializeToFileData(buf);
    buf.push_back(0xFF);

    BinaryDataFormat dst;
    CHECK_EQ(dst.deserializeFromFileData(buf.data(), buf.size()),
             fileSizeNotMatch);
}

static void test_deser_preserves_on_failure() {
    std::printf("test_deser_preserves_on_failure\n");
    BinaryDataFormat m;
    m.setInt32("keep", 111);

    // Feed it garbage
    Byte bad[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    CHECK(m.deserializeFromFileData(bad, 4) != noError);

    // Original data must survive a failed deserialize
    std::int32_t v = 0;
    CHECK_EQ(m.getInt32("keep", v), noError);
    CHECK_EQ(v, 111);
}

static void test_bridge_roundtrip() {
    std::printf("test_bridge_roundtrip\n");
    const char* name = "t_map_rt";
    cleanAll(name);

    BinaryDataFormat m;
    m.setInt32("hp", 100);
    m.setString("name", "player");
    m.setBool("alive", true);

    CHECK_EQ(safeSave(m, name, false), noError);

    BinaryDataFormat m2;
    CHECK_EQ(safeLoad(m2, name, false), noError);
    CHECK(m == m2);

    cleanAll(name);
}

static void test_bridge_backup_recovery() {
    std::printf("test_bridge_backup_recovery\n");
    const char* name = "t_map_br";
    cleanAll(name);

    BinaryDataFormat m;
    m.setInt32("hp", 100);
    m.setString("name", "player");

    CHECK_EQ(safeSave(m, name, false), noError);

    // Corrupt file1
    auto t = pathsFor(name);
    flipByte(t.file1, 0);

    // Load — should recover from backup and not return an error
    BinaryDataFormat m2;
    CHECK_EQ(safeLoad(m2, name, false), noError);
    CHECK(m == m2);

    // Same but with reportBackupAsError = true
    BinaryDataFormat m3;
    CHECK_EQ(safeLoad(m3, name, true), readBackup);
    CHECK(m == m3);

    cleanAll(name);
}

static void test_bridge_both_corrupt() {
    std::printf("test_bridge_both_corrupt\n");
    const char* name = "t_map_bc";
    cleanAll(name);

    BinaryDataFormat m;
    m.setInt32("hp", 100);
    CHECK_EQ(safeSave(m, name, false), noError);

    auto t = pathsFor(name);
    flipByte(t.file1, 0);
    flipByte(t.file2, 0);

    BinaryDataFormat m2;
    CHECK_EQ(safeLoad(m2, name, false), checkSumFail);

    cleanAll(name);
}

static void test_bridge_overwrite() {
    std::printf("test_bridge_overwrite\n");
    const char* name = "t_map_ov";
    cleanAll(name);

    BinaryDataFormat first;
    first.setInt32("a", 1);
    first.setInt32("b", 2);
    CHECK_EQ(safeSave(first, name, false), noError);

    BinaryDataFormat second;
    second.setInt32("c", 3);
    CHECK_EQ(safeSave(second, name, false), noError);

    BinaryDataFormat loaded;
    CHECK_EQ(safeLoad(loaded, name, false), noError);

    // The second save completely replaced the first
    CHECK(loaded == second);
    CHECK(!loaded.valueExists("a"));
    CHECK(!loaded.valueExists("b"));
    CHECK(loaded.valueExists("c"));

    cleanAll(name);
}


// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    // --- checksum layer (from before) ---
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

    std::printf("\n=== safe layer ===\n");
    test_safe_roundtrip();
    test_backup_recovery_silent();
    test_backup_recovery_reported();
    test_both_corrupt();
    test_primary_missing();
    test_backup_fail_silent();
    test_backup_fail_reported();
    test_crash_tmp_ignored();
    test_no_backup_save();
    test_load_backup_directly();
    test_safe_empty_payload();
    test_path_variants();
    test_load_nonexistent();
    test_repeated_saves();

    std::printf("\n=== map layer ===\n");
    test_map_empty();
    test_map_single_int();
    test_map_many_ints();
    test_map_overwrite();

    test_type_bool();
    test_type_int32();
    test_type_uint32();
    test_type_int64();
    test_type_uint64();
    test_type_float();
    test_type_double();
    test_type_char_uchar();
    test_type_string();
    test_type_raw_bytes();
    test_type_vec234();
    test_type_ivec234();
    test_type_keyvalue();
    test_map_mixed();

    test_equality();

    test_missing_key();
    test_wrong_type();
    test_get_type();

    test_deser_truncated();
    test_deser_bad_type();
    test_deser_wrong_payload_size();
    test_deser_duplicate_key();
    test_deser_trailing_bytes();
    test_deser_preserves_on_failure();

    std::printf("\n=== map <-> safe bridge ===\n");
    test_bridge_roundtrip();
    test_bridge_backup_recovery();
    test_bridge_both_corrupt();
    test_bridge_overwrite();


    if (g_failures == 0) std::printf("\nALL PASS\n");
    else                 std::printf("\n%d FAILURES\n", g_failures);
    return g_failures ? 1 : 0;
}
