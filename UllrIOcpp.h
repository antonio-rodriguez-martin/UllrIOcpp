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

    Errors safeSaveNoBackup(const void* data, size_t size, const char* filePathWithExtension);
    Errors safeSaveNoBackup(std::vector<Byte>& data, const char* filePathWithExtension);
    Errors safeLoadBackup(void* data, size_t size, const char* filePathWithExtension);
    Errors safeSave(const void* data, size_t size, const char* filePathWithExtension, bool reportNotMakingBackupAsError);
    Errors safeSave(const std::vector<char>& data, const char* filePathWithExtension, bool reportNotMakingBackupAsError);
    Errors safeLoad(void* data, size_t size, const char* filePathWithExtension);
    Errors safeLoad(std::vector<Byte>& data, const char* filePathWithExtension);

Errors safeSaveNoBackup(const std::vector<Byte>& data, const char* name);
Errors safeLoadBackup(std::vector<Byte>& data, const char* name);
}
