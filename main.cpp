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
{
    int failures = 0;
    int testsRun = 0;

    void check(bool condition, const char* description)
    {
        ++testsRun;
        if (condition)
            std::cout << "[PASS] " << description << '\n';
        else
        {
            ++failures;
            std::cout << "[FAIL] " << description << '\n';
        }
    }

    void writeTestFile(const char* path, const std::vector<Byte>& bytes)
    {
        std::ofstream file(path, std::ios::binary);
        if (!bytes.empty())
            file.write(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size()));
    }

    void testFileSize()
    {
        std::cout << "\nTesting file size...\n";
        writeTestFile("test.bin", { 'H', 'e', 'l', 'l', 'o' });

        size_t size = 0;
        check(getFileSize("test.bin", size) == noError,
              "Get file size succeeds");
        check(size == 5, "File size is five bytes");

        size = 123;
        check(getFileSize("file-that-does-not-exist.bin", size)
                  == couldNotOpenFile,
              "Missing file returns couldNotOpenFile");
        check(size == 0, "Size resets to zero on error");

        std::remove("test.bin");
    }

    void testVectorReadWrite()
    {
        std::cout << "\nTesting vector read/write...\n";
        const std::vector<Byte> original = { 'H', 'i', 0, 42 };

        check(writeEntireFile(original, "test.bin") == noError,
              "Write vector succeeds");

        std::vector<Byte> loaded;
        check(readEntireFile(loaded, "test.bin") == noError,
              "Read vector succeeds");
        check(loaded == original, "Loaded bytes match original bytes");

        std::remove("test.bin");
    }

    void testFixedBufferRead()
    {
        std::cout << "\nTesting fixed-buffer read...\n";
        const std::vector<Byte> original = { 10, 20, 30, 40, 50 };
        writeTestFile("test.bin", original);

        Byte buffer[5] = {};
        check(readEntireFile(buffer, sizeof(buffer), "test.bin") == noError,
              "Read exact-size fixed buffer succeeds");

        bool matches = true;
        for (size_t i = 0; i < original.size(); ++i)
            matches = matches && buffer[i] == original[i];

        check(matches, "Fixed buffer contains expected bytes");

        std::remove("test.bin");
    }

    void testEmptyFile()
    {
        std::cout << "\nTesting empty file...\n";
        writeTestFile("test.bin", {});

        std::vector<Byte> data = { 42 };
        check(readEntireFile(data, "test.bin") == noError,
              "Empty-file read succeeds");
        check(data.empty(), "Empty file produces empty vector");

        size_t size = 100;
        check(getFileSize("test.bin", size) == noError,
              "Get empty-file size succeeds");
        check(size == 0, "Empty file size is zero");

        std::remove("test.bin");
    }

    void testMissingFileReads()
    {
        std::cout << "\nTesting reads from a missing file...\n";
        std::remove("missing.bin");

        std::vector<Byte> data;
        check(readEntireFile(data, "missing.bin") == couldNotOpenFile,
              "Vector read of missing file returns couldNotOpenFile");

        Byte buffer[4] = {};
        check(readEntireFile(buffer, sizeof(buffer), "missing.bin")
                  == couldNotOpenFile,
              "Fixed-buffer read of missing file returns couldNotOpenFile");
    }

    void testErrorStrings()
    {
        std::cout << "\nTesting error strings...\n";
        check(std::string(getErrorString(noError)) == "noError",
              "noError string");
        check(std::string(getErrorString(couldNotOpenFile))
                  == "couldNotOpenFile",
              "couldNotOpenFile string");
    }
    void testShortFileRead()
{
    std::cout << "\nTesting short-file read...\n";

    const std::vector<Byte> fileBytes = { 11, 22, 33 };
    writeTestFile("test.bin", fileBytes);

    Byte buffer[5] = { 0, 0, 0, 0, 0 };

    check(readEntireFile(buffer, sizeof(buffer), "test.bin") == noError,
          "Short file read returns noError");

    check(buffer[0] == 11 && buffer[1] == 22 && buffer[2] == 33,
          "Available bytes are read");

    check(buffer[3] == 0 && buffer[4] == 0,
          "Bytes beyond end of file remain unchanged");

    std::remove("test.bin");
}

void testFailedWrite()
{
    std::cout << "\nTesting failed write...\n";

    const std::vector<Byte> bytes = { 1, 2, 3 };

    check(writeEntireFile(bytes, "directory-that-does-not-exist/test.bin")
              == couldNotOpenFile,
          "Write to unavailable path returns couldNotOpenFile");
}
}

int main()
{
    std::cout << "=== UllrIOcpp Tests ===\n";

    testFileSize();
    testVectorReadWrite();
    testFixedBufferRead();
    testEmptyFile();
    testMissingFileReads();
    testErrorStrings();
    testShortFileRead();
testFailedWrite();


    std::remove("test.bin");
    std::remove("missing.bin");

    std::cout << "\n=== Results ===\n"
              << "Tests run: " << testsRun << '\n'
              << "Failures: " << failures << '\n';

    return failures == 0 ? 0 : 1;
}
