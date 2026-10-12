#include "UllrIOcpp.h"
#include <filesystem>
#include <ios>
#include <iostream>
#include <limits>
#include <system_error>
#include <vector>
#include <algorithm>
#include <fstream>
#define NOMINMAX
#include <Windows.h>

namespace UllrIOcpp
{
    constexpr const char* errorNames[] =
    {
        "noError",
        "couldNotOpenFile",
        "fileSizeNotMatch",
        "checkSumFail",
        "couldNotMakeBackup",
        "readBackup",
        "entryAlreadyExists",
        "entryNotFound",
        "entryDifferentDataType",
        "couldNotParseData",
        "fileSizeNotBigEnough"
    };

    const char* getErrorString(Errors error)
    {
        if (error >= 0 && error < sizeof(errorNames)/sizeof(errorNames[0]))
        {
            return errorNames[error];
        }

        return "unknown error";
    }

    //Getting file size for outside the library means
    Errors getFileSize(const char* filePath, size_t& size)
    {
        size = 0;

        std::ifstream f(filePath, std::ios::binary);

        if (f.is_open())
        {
            f.seekg(0, std::ios_base::end);
            size_t readSize = f.tellg();
            f.close();
            size = readSize;

            return noError;
        }
        else
        {
            return couldNotOpenFile;
        }
    }

    //Implementation where we know the size beforehand
    Errors readEntireFile(void* data, size_t size, const char* filePath)
    {
        std::ifstream f(filePath, std::ios::binary);

        if (f.is_open())
        {
            //setting the position to the end and then the start for the size and then read
            f.seekg(0, std::ios_base::end);
            size_t readSize = f.tellg();
            f.seekg(0, std::ios_base::beg);

            f.read((char*)data, std::min(size, readSize));
            return noError;
        }

        return couldNotOpenFile;
    }

    //Implementation where we don't know the size beforehand
    Errors readEntireFile(std::vector<Byte>& data, const char* filePath)
    {
        data.clear();
        std::ifstream f(filePath, std::ios::binary);

        if(f.is_open())
        {
            f.seekg(0, std::ios_base::end);
            size_t size = f.tellg();
            f.seekg(0, std::ios_base::beg);

            if((std::streampos)size < 0)
                return fileSizeNotBigEnough;

            data.resize(size);

            if (size == 0)
                return noError;

            if (!f)
            {
                data.clear();
                return couldNotParseData;
            }

            f.read((char*)&data[0], data.size());
            return noError;
        }

        return couldNotOpenFile;
    }

    //Implementation where we know the size beforehand
    Errors writeEntireFile(const void* data, size_t size, const char* filePath)
    {
        std::ofstream f(filePath, std::ios::binary);

        if(f.is_open())
        {
            f.write((char*)data, size);
            return noError;
        }
        else
        {
            return couldNotOpenFile;
        }
    }

    //Implementation where we don't know the size beforehand
    Errors writeEntireFile(const std::vector<Byte>& data, const char* filePath)
    {
        return writeEntireFile(data.data(), data.size(), filePath);
    }

    //NOTE(ullr): Independent readers, this time with checksum ---
    //=================================================================

    //8 bytes | 64 bits
    using HashTyped = std::uint64_t;

    //algorithm for hashing the checksum
    unsigned long long fnv_hash_1a_64(const void* key, int len)
	{
		const unsigned char* p = (const unsigned char*)key;
		unsigned long long h = 0xcbf29ce484222325ULL;

        size_t i = 0;

        for (; len - i >= 4; i += 4)
        {
            h = (h ^ p[i + 0]) * 0x100000001b3ULL;
            h = (h ^ p[i + 1]) * 0x100000001b3ULL;
            h = (h ^ p[i + 2]) * 0x100000001b3ULL;
            h = (h ^ p[i + 3]) * 0x100000001b3ULL;
        }

        for (; i < len; ++i)
        {
            h = (h ^ p[i]) * 0x100000001b3ULL;
        }

		return h;
	}


    Errors readEntireFileCheckSum(void* data, size_t size, const char* filePath)
    {
        std::ifstream f(filePath, std::ios::binary);
        if (f.is_open())
        {
            f.seekg(0, std::ios_base::end);
            size_t readSize = f.tellg();
            f.seekg(0, std::ios_base::beg);

            if (size > std::numeric_limits<size_t>::max() - sizeof(HashTyped))
                return fileSizeNotBigEnough;

            size_t sizeCheckSum = size + sizeof(HashTyped);

            if (readSize != sizeCheckSum)
                return fileSizeNotMatch;
            f.read((char*)data, size);
            HashTyped checkSum = 0;
            f.read((char*)&checkSum, sizeof(HashTyped));

            auto testCheck = fnv_hash_1a_64(data, size);
            if (testCheck != checkSum)
                return checkSumFail;

            return noError;
        }

        return couldNotOpenFile;
    }

    Errors readEntireFileCheckSum(std::vector<Byte>& data, const char* filePath)
    {
        data.clear();

        std::ifstream f(filePath, std::ios::binary);

        if (f.is_open())
        {
            f.seekg(0, std::ios_base::end);
            size_t readSize = f.tellg();
            f.seekg(0, std::ios_base::beg);

            if (readSize < sizeof(HashTyped))
                return fileSizeNotBigEnough;

            size_t size = readSize - sizeof(HashTyped);

            data.resize(size);

            if (size > 0)
                f.read((char*)data.data(), size);

            HashTyped checkSum = 0;
            f.read((char*)&checkSum, sizeof(HashTyped));

            auto testCheck = fnv_hash_1a_64(data.data(), data.size());

            if (testCheck != checkSum)
                return checkSumFail;

            return noError;
        }
        return couldNotOpenFile;
    }

    Errors writeEntireFileCheckSum(const void* data, size_t size, const char* filePath)
    {
        std::ofstream f(filePath, std::ios::binary);
        if (f.is_open())
        {
            f.write((char*)data, size);
            auto testCheck = fnv_hash_1a_64(data, size);

            f.write((char*)&testCheck, sizeof(testCheck));
            return noError;
        }
        return couldNotOpenFile;
    }

    Errors writeEntireFileCheckSum(const std::vector<Byte>& data, const char* filePath)
    {
        return writeEntireFileCheckSum(data.data(), data.size(), filePath);
    }

    // ===================================================
    // Safe Loading using the previous checkSum functions
    // ===================================================


    Errors safeSaveNoBackup(const void* data, size_t size, const char* filePathWithExtension)
    {
        namespace fs = std::filesystem;

        fs::path path(filePathWithExtension);

        std::string ext = path.extension().string();
        if (ext.empty()) ext = ".bin";

        auto file1 = path.parent_path() / (path.stem().string() + "1" + ext);
        auto tmpFile = path.parent_path() / (path.stem().string() + "1.tmp");

        auto err = writeEntireFileCheckSum(data, size, tmpFile.string().c_str());

        if (err != noError)
            return err;

        std::error_code renameError;
        fs::rename(tmpFile, file1, renameError);

        if (renameError)
            return couldNotOpenFile;

        return noError;
    }

    Errors safeSaveNoBackup(std::vector<Byte>& data, const char* filePathWithExtension)
    {
        return safeSaveNoBackup(data.data(), data.size(), filePathWithExtension);
    }

    Errors safeSave(const void* data, size_t size, const char* filePathWithExtension, bool reportNotMakingBackupAsError)
    {
        namespace fs = std::filesystem;

        fs::path path(filePathWithExtension);

        std::string ext = path.extension().string();
        if (ext.empty()) ext = ".bin";

        auto file1 = path.parent_path() / (path.stem().string() + "1" + ext);
        auto file2 = path.parent_path() / (path.stem().string() + "2" + ext);
        auto tmpFile = path.parent_path() / (path.stem().string() + "1.tmp");

        auto err = writeEntireFileCheckSum(data, size, tmpFile.string().c_str());

        if (err != noError)
            return err;

        std::error_code renameError;
        fs::rename(tmpFile, file1, renameError);

        if (renameError)
            return couldNotOpenFile;

        std::error_code errorCode;
        fs::copy_file(file1, file2, fs::copy_options::overwrite_existing, errorCode);

        if(errorCode)
            return reportNotMakingBackupAsError ? couldNotMakeBackup : noError;

        return noError;
    }

    Errors safeSave(const std::vector<char>& data, const char* filePathWithExtension, bool reportNotMakingBackupAsError)
    {
        return safeSave(data.data(), data.size(), filePathWithExtension, reportNotMakingBackupAsError);
    }

    Errors safeLoadBackup(void* data, size_t size, const char* filePathWithExtension)
    {
        namespace fs = std::filesystem;

        fs::path path(filePathWithExtension);

        std::string ext = path.extension().string();
        if (ext.empty()) ext = ".bin";

        auto file2 = path.parent_path() / (path.stem().string() + "2" + ext);

        auto err2 = readEntireFileCheckSum(data,  size, file2.string().c_str());

        return err2;
    }


    Errors safeLoad(void* data, size_t size, const char* filePathWithExtension, bool reportBackupAsError)
    {
        namespace fs = std::filesystem;

        fs::path path(filePathWithExtension);

        std::string ext = path.extension().string();
        if (ext.empty()) ext = ".bin";

        auto file1 = path.parent_path() / (path.stem().string() + "1" + ext);
        auto file2 = path.parent_path() / (path.stem().string() + "2" + ext);

        auto err = readEntireFileCheckSum(data, size, file1.string().c_str());

        if (err == noError)
            return noError;

        auto err2 = readEntireFileCheckSum(data,  size, file2.string().c_str());

        if (err2 == noError)
        {
            if (reportBackupAsError)
                return readBackup;
            else
                return noError;
        }

        if (err2 == couldNotOpenFile)
            return err;

        return err2;
    }

    Errors safeLoad(std::vector<Byte>& data, const char* filePathWithExtension, bool reportBackupError)
    {
        namespace fs = std::filesystem;

        data.clear();

        fs::path path(filePathWithExtension);

        std::string ext = path.extension().string();
        if (ext.empty()) ext = ".bin";

        auto file1 = path.parent_path() / (path.stem().string() + "1" + ext);
        auto file2 = path.parent_path() / (path.stem().string() + "2" + ext);

        auto err = readEntireFileCheckSum(data, file1.string().c_str());

        if (err == noError)
            return noError;

        auto err2 = readEntireFileCheckSum(data, file2.string().c_str());

        if (err2 == noError)
        {
            if (reportBackupError)
                return readBackup;
            else
                return noError;
        }

        if (err2 == couldNotOpenFile)
            return err;

        return err2;
    }
    // --- Forwarders
Errors safeSaveNoBackup(const std::vector<Byte>& data, const char* name) {
    return safeSaveNoBackup(data.data(), data.size(), name);
}
Errors safeLoadBackup(std::vector<Byte>& data, const char* name) {
    namespace fs = std::filesystem;
    data.clear();

    fs::path path(name);

    std::string ext = path.extension().string();
    if (ext.empty()) ext = ".bin";

    auto file2 = path.parent_path() / (path.stem().string() + "2" + ext);
    return readEntireFileCheckSum(data, file2.string().c_str());
}
}

