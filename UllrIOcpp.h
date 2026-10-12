#pragma once
#include <cstdint>
#include <vector>

namespace UllrIOcpp
{
    using Byte = uint8_t;

    enum Errors : int
    {
        noError = 0,
        couldNotOpenFile,
        fileSizeNotMatch,
        checkSumFail,
        couldNotMakeBackup,
        readBackup,
        entryAlreadyExists,
        entryNotFound,
        entryDifferentDataType,
        couldNotParseData,
        fileSizeNotBigEnough
    };


    const char* getErrorString(Errors error);
    Errors getFileSize(const char* filePath, size_t& size);
    Errors readEntireFile(std::vector<Byte>& data, const char* filePath);
    Errors writeEntireFile(const void* data, size_t size, const char* filePath);
    unsigned long long fnv_hash_1a_64(const void* key, int len);
    Errors writeEntireFile(const void* data, size_t size, const char* filePath);
    Errors writeEntireFile(const std::vector<Byte>& data, const char* filePath);
    Errors readEntireFileCheckSum(void* data, size_t size, const char* filePath);
    Errors readEntireFileCheckSum(std::vector<Byte>& data, const char* filePath);
    Errors writeEntireFileCheckSum(const void* data, size_t size, const char* filePath);
    Errors writeEntireFileCheckSum(const std::vector<Byte>& data, const char* filePath);
}
