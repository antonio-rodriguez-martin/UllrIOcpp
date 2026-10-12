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

    std::vector<char> payload = {'h','e','l','l','o'};
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

    std::vector<char> payload = {'o','r','i','g','i','n','a','l'};
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

    std::vector<char> payload = {'o','r','i','g','i','n','a','l'};
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

    std::vector<char> payload = {'x','y','z'};
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

    std::vector<char> payload = {1,2,3,4};
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

    std::vector<char> payload = {'a','b','c'};
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

    std::vector<char> payload = {'a','b','c'};
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

    std::vector<char> good = {'g','o','o','d','\0','d','a','t','a'};
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

    std::vector<char> payload = {'b','a','c','k','u','p'};
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

    std::vector<char> empty;
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
        std::vector<char> p = {1};
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
        std::vector<char> p = {1};
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
        std::vector<char> p = {1};
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
        std::vector<char> p = {1};
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
        std::vector<char> p(10, (char)i);
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


    if (g_failures == 0) std::printf("\nALL PASS\n");
    else                 std::printf("\n%d FAILURES\n", g_failures);
    return g_failures ? 1 : 0;
}
