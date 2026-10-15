#include "UllrIOcpp.h"
#include <cstdint>
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
        "dataFormatValueAlreadyExists",
        "dataFormatValueNotFound",
        "dataFormatValueDifferentDataType",
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

    //=====================================================
    //SERIALIZATION AND DESERIALIZATION BINARY MAP FORMAT
    //=====================================================

    bool BinaryDataFormat::MapValueFormat::operator== (const MapValueFormat &other) const
    {
        if (type != other.type)
            return false;

        switch (type)
        {
            case Types::noType: { break; }
            case Types::rawBytesType: { if (data != other.data) { return false; } break; }
            case Types::boolType: { if (primitives.boolData != other.primitives.boolData) { return false; } break; }
            case Types::intType: { if (primitives.int32Data != other.primitives.int32Data) { return false; } break; }
            case Types::uintType: { if (primitives.uint32Data != other.primitives.uint32Data) { return false; } break; }
            case Types::int64Type: { if (primitives.int64Data != other.primitives.int64Data) { return false; } break; }
            case Types::uint64Type: { if (primitives.uint64Data != other.primitives.uint64Data) { return false; } break; }
            case Types::floatType: { if (primitives.floatData != other.primitives.floatData) { return false; } break; }
            case Types::doubleType: { if (primitives.doubleData != other.primitives.doubleData) { return false; } break; }
            case Types::charType: { if (primitives.charData != other.primitives.charData) { return false; } break; }
            case Types::ucharType: { if (primitives.uCharData != other.primitives.uCharData) { return false; } break; }
            case Types::stringType: { if (data != other.data) { return false; } break; }

            case Types::vec2Type: { if (primitives.vec.x != other.primitives.vec.x ||
                                        primitives.vec.y != other.primitives.vec.y)
                                      { return false; } break; }
            case Types::vec3Type: { if (primitives.vec.x != other.primitives.vec.x ||
                                        primitives.vec.y != other.primitives.vec.y ||
                                        primitives.vec.z != other.primitives.vec.z)
                                      { return false; } break; }
            case Types::vec4Type: { if (primitives.vec.x != other.primitives.vec.x ||
                                        primitives.vec.y != other.primitives.vec.y ||
                                        primitives.vec.z != other.primitives.vec.z ||
                                        primitives.vec.w != other.primitives.vec.w)
                                      { return false; } break; }
            case Types::ivec2Type: { if (primitives.ivec.x != other.primitives.ivec.x ||
                                         primitives.ivec.y != other.primitives.ivec.y)
                                       { return false; } break; }
            case Types::ivec3Type: { if (primitives.ivec.x != other.primitives.ivec.x ||
                                         primitives.ivec.y != other.primitives.ivec.y ||
                                         primitives.ivec.z != other.primitives.ivec.z)
                                       { return false; } break; }
            case Types::ivec4Type: { if (primitives.ivec.x != other.primitives.ivec.x ||
                                         primitives.ivec.y != other.primitives.ivec.y ||
                                         primitives.ivec.z != other.primitives.ivec.z ||
                                         primitives.ivec.w != other.primitives.ivec.w)
                                       { return false; } break; }
            case Types::keyValueType: { if (data != other.data) { return false; } break; }

            default: { return false; }
        }

        return true;

    }


    bool BinaryDataFormat::operator== (const BinaryDataFormat& other) const
    {
        if (umData.size() != other.umData.size())
            return false;

        for (const auto& a : umData)
        {
            const auto found = other.umData.find(a.first);
            if (found == other.umData.end())
                return false;

            if (a.second != found->second)
                return false;
        }

        return true;
    }

    bool BinaryDataFormat::valueExists(std::string at)
    {
        return umData.find(at) != umData.end();
    }

    //===========Getters and Setters===============

    Errors BinaryDataFormat::getMapValueFormatType(std::string at, MapValueFormat::Types& type)
    {
        type = MapValueFormat::Types::noType;

        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        type = it->second.type;
        return Errors::noError;
    }


    //==================================================
    // Raw Bytes
    //==================================================

    Errors BinaryDataFormat::setRawBytes(std::string at, const std::vector<Byte>& bytes)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::rawBytesType;
            it->second.data = bytes;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::rawBytesType;
        mvf.data = bytes;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getRawBytes(std::string at, std::vector<Byte>& bytes)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::rawBytesType)
            return Errors::dataFormatValueDifferentDataType;

        bytes = it->second.data;
        return Errors::noError;
    }


    //==================================================
    // Boolean
    //==================================================

    Errors BinaryDataFormat::setBool(std::string at, bool b)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::boolType;
            it->second.data.clear();
            it->second.primitives.boolData = b;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::boolType;
        mvf.primitives.boolData = b;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getBool(std::string at, bool& b)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::boolType)
            return Errors::dataFormatValueDifferentDataType;

        b = it->second.primitives.boolData;
        return Errors::noError;
    }


    //==================================================
    // Int32
    //==================================================

    Errors BinaryDataFormat::setInt32(std::string at, std::int32_t i)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::intType;
            it->second.data.clear();
            it->second.primitives.int32Data = i;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::intType;
        mvf.primitives.int32Data = i;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getInt32(std::string at, std::int32_t& i)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::intType)
            return Errors::dataFormatValueDifferentDataType;

        i = it->second.primitives.int32Data;
        return Errors::noError;
    }


    //==================================================
    // UInt32
    //==================================================

    Errors BinaryDataFormat::setUInt32(std::string at, std::uint32_t i)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::uintType;
            it->second.data.clear();
            it->second.primitives.uint32Data = i;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::uintType;
        mvf.primitives.uint32Data = i;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getUInt32(std::string at, std::uint32_t& i)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::uintType)
            return Errors::dataFormatValueDifferentDataType;

        i = it->second.primitives.uint32Data;
        return Errors::noError;
    }


    //==================================================
    // Int64
    //==================================================

    Errors BinaryDataFormat::setInt64(std::string at, std::int64_t i)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::int64Type;
            it->second.data.clear();
            it->second.primitives.int64Data = i;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::int64Type;
        mvf.primitives.int64Data = i;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getInt64(std::string at, std::int64_t& i)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::int64Type)
            return Errors::dataFormatValueDifferentDataType;

        i = it->second.primitives.int64Data;
        return Errors::noError;
    }


    //==================================================
    // UInt64
    //==================================================

    Errors BinaryDataFormat::setUInt64(std::string at, std::uint64_t i)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::uint64Type;
            it->second.data.clear();
            it->second.primitives.uint64Data = i;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::uint64Type;
        mvf.primitives.uint64Data = i;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getUInt64(std::string at, std::uint64_t& i)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::uint64Type)
            return Errors::dataFormatValueDifferentDataType;

        i = it->second.primitives.uint64Data;
        return Errors::noError;
    }


    //==================================================
    // Float
    //==================================================

    Errors BinaryDataFormat::setFloat(std::string at, float f)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::floatType;
            it->second.data.clear();
            it->second.primitives.floatData = f;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::floatType;
        mvf.primitives.floatData = f;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getFloat(std::string at, float& f)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::floatType)
            return Errors::dataFormatValueDifferentDataType;

        f = it->second.primitives.floatData;
        return Errors::noError;
    }


    //==================================================
    // Double
    //==================================================

    Errors BinaryDataFormat::setDouble(std::string at, double d)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::doubleType;
            it->second.data.clear();
            it->second.primitives.doubleData = d;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::doubleType;
        mvf.primitives.doubleData = d;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getDouble(std::string at, double& d)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::doubleType)
            return Errors::dataFormatValueDifferentDataType;

        d = it->second.primitives.doubleData;
        return Errors::noError;
    }


    //==================================================
    // Char
    //==================================================

    Errors BinaryDataFormat::setChar(std::string at, char c)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::charType;
            it->second.data.clear();
            it->second.primitives.charData = c;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::charType;
        mvf.primitives.charData = c;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getChar(std::string at, char& c)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::charType)
            return Errors::dataFormatValueDifferentDataType;

        c = it->second.primitives.charData;
        return Errors::noError;
    }


    //==================================================
    // Unsigned Char
    //==================================================

    Errors BinaryDataFormat::setUChar(std::string at, unsigned char c)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::ucharType;
            it->second.data.clear();
            it->second.primitives.uCharData = c;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::ucharType;
        mvf.primitives.uCharData = c;
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getUChar(std::string at, unsigned char& c)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::ucharType)
            return Errors::dataFormatValueDifferentDataType;

        c = it->second.primitives.uCharData;
        return Errors::noError;
    }


    //==================================================
    // String
    //==================================================

    Errors BinaryDataFormat::setString(std::string at, const std::string& s)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::stringType;
            it->second.data.assign(s.begin(), s.end());
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::stringType;
        mvf.data.assign(s.begin(), s.end());
        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getString(std::string at, std::string& s)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::stringType)
            return Errors::dataFormatValueDifferentDataType;

        s.assign(it->second.data.begin(), it->second.data.end());
        return Errors::noError;
    }


    //==================================================
    // Vec2
    //==================================================

    Errors BinaryDataFormat::setVec2(std::string at, float x, float y)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::vec2Type;
            it->second.data.clear();

            it->second.primitives.vec.x = x;
            it->second.primitives.vec.y = y;

            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::vec2Type;

        mvf.primitives.vec.x = x;
        mvf.primitives.vec.y = y;

        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getVec2(std::string at, float& x, float& y)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::vec2Type)
            return Errors::dataFormatValueDifferentDataType;

        x = it->second.primitives.vec.x;
        y = it->second.primitives.vec.y;

        return Errors::noError;
    }


    //==================================================
    // Vec3
    //==================================================

    Errors BinaryDataFormat::setVec3(std::string at, float x, float y, float z)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::vec3Type;
            it->second.data.clear();

            it->second.primitives.vec.x = x;
            it->second.primitives.vec.y = y;
            it->second.primitives.vec.z = z;

            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::vec3Type;

        mvf.primitives.vec.x = x;
        mvf.primitives.vec.y = y;
        mvf.primitives.vec.z = z;

        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getVec3(std::string at, float& x, float& y, float& z)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::vec3Type)
            return Errors::dataFormatValueDifferentDataType;

        x = it->second.primitives.vec.x;
        y = it->second.primitives.vec.y;
        z = it->second.primitives.vec.z;

        return Errors::noError;
    }


    //==================================================
    // Vec4
    //==================================================

    Errors BinaryDataFormat::setVec4(std::string at, float x, float y, float z, float w)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::vec4Type;
            it->second.data.clear();

            it->second.primitives.vec.x = x;
            it->second.primitives.vec.y = y;
            it->second.primitives.vec.z = z;
            it->second.primitives.vec.w = w;

            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::vec4Type;

        mvf.primitives.vec.x = x;
        mvf.primitives.vec.y = y;
        mvf.primitives.vec.z = z;
        mvf.primitives.vec.w = w;

        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getVec4(std::string at, float& x, float& y, float& z, float& w)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::vec4Type)
            return Errors::dataFormatValueDifferentDataType;

        x = it->second.primitives.vec.x;
        y = it->second.primitives.vec.y;
        z = it->second.primitives.vec.z;
        w = it->second.primitives.vec.w;

        return Errors::noError;
    }


    //==================================================
    // IVec2
    //==================================================

    Errors BinaryDataFormat::setIVec2(std::string at, int x, int y)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::ivec2Type;
            it->second.data.clear();

            it->second.primitives.ivec.x = x;
            it->second.primitives.ivec.y = y;

            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::ivec2Type;

        mvf.primitives.ivec.x = x;
        mvf.primitives.ivec.y = y;

        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getIVec2(std::string at, int& x, int& y)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::ivec2Type)
            return Errors::dataFormatValueDifferentDataType;

        x = it->second.primitives.ivec.x;
        y = it->second.primitives.ivec.y;

        return Errors::noError;
    }


    //==================================================
    // IVec3
    //==================================================

    Errors BinaryDataFormat::setIVec3(std::string at, int x, int y, int z)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::ivec3Type;
            it->second.data.clear();

            it->second.primitives.ivec.x = x;
            it->second.primitives.ivec.y = y;
            it->second.primitives.ivec.z = z;

            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::ivec3Type;

        mvf.primitives.ivec.x = x;
        mvf.primitives.ivec.y = y;
        mvf.primitives.ivec.z = z;

        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getIVec3(std::string at, int& x, int& y, int& z)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::ivec3Type)
            return Errors::dataFormatValueDifferentDataType;

        x = it->second.primitives.ivec.x;
        y = it->second.primitives.ivec.y;
        z = it->second.primitives.ivec.z;

        return Errors::noError;
    }


    //==================================================
    // IVec4
    //==================================================

    Errors BinaryDataFormat::setIVec4(std::string at, int x, int y, int z, int w)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::ivec4Type;
            it->second.data.clear();

            it->second.primitives.ivec.x = x;
            it->second.primitives.ivec.y = y;
            it->second.primitives.ivec.z = z;
            it->second.primitives.ivec.w = w;

            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::ivec4Type;

        mvf.primitives.ivec.x = x;
        mvf.primitives.ivec.y = y;
        mvf.primitives.ivec.z = z;
        mvf.primitives.ivec.w = w;

        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getIVec4(std::string at, int& x, int& y, int& z, int& w)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::ivec4Type)
            return Errors::dataFormatValueDifferentDataType;

        x = it->second.primitives.ivec.x;
        y = it->second.primitives.ivec.y;
        z = it->second.primitives.ivec.z;
        w = it->second.primitives.ivec.w;

        return Errors::noError;
    }


    //==================================================
    // Key-Value Payload
    //==================================================

    Errors BinaryDataFormat::setKeyValue(std::string at, const std::vector<Byte>& bytes)
    {
        auto it = umData.find(at);

        if (it != umData.end())
        {
            it->second.type = MapValueFormat::Types::keyValueType;
            it->second.data = bytes;
            return Errors::dataFormatValueAlreadyExists;
        }

        MapValueFormat mvf = {};
        mvf.type = MapValueFormat::Types::keyValueType;
        mvf.data = bytes;

        umData.insert({at, std::move(mvf)});

        return Errors::noError;
    }

    Errors BinaryDataFormat::getKeyValue(std::string at, std::vector<Byte>& bytes)
    {
        auto it = umData.find(at);

        if (it == umData.end())
            return Errors::dataFormatValueNotFound;

        if (it->second.type != MapValueFormat::Types::keyValueType)
            return Errors::dataFormatValueDifferentDataType;

        bytes = it->second.data;
        return Errors::noError;
    }

    //==================================
    //SERIALIZER AND DESERIALIZER
    //==================================

    //PRIMITIVE READERS
    bool canRead(const ReadCursor& cursor, size_t amount)
    {
        size_t remaining = cursor.end - cursor.current;
        return remaining >= amount;
    }

    bool readByte(ReadCursor& cursor, Byte& value)
    {
        if (!canRead(cursor, sizeof(value)))
            return false;

        value = *cursor.current;
        cursor.current += sizeof(value);
        return true;
    }
    bool readU32LE(ReadCursor& cursor, std::uint32_t& value)
    {

        if (!canRead(cursor, sizeof(value)))
            return false;

        //0 So the bitwise OR write the bits correctly
        std::uint32_t result = 0;

        for (size_t i = 0; i < sizeof(result); ++i)
        {
            Byte byte = cursor.current[i]; //because current is a pointer we can use [] operator like *(cursor.current + i)
            //The sizeof is getting the bytes but we must multiply by the bits inside a byte
            result |= static_cast<std::uint32_t>(byte) << (i * 8);
        }

        value = result;

        cursor.current += sizeof(result);

        return true;
    }

    bool readI32LE( ReadCursor& cursor, std::int32_t& value)
    {
        std::uint32_t raw;

        if (!readU32LE(cursor, raw))
            return false;

        value = static_cast<std::int32_t>(raw);
        return true;
    }

    bool readU64LE( ReadCursor& cursor, std::uint64_t& value)
    {
        if (!canRead(cursor, sizeof(std::uint64_t)))
            return false;

        std::uint64_t result = 0;

        for (size_t i = 0; i < sizeof(result); ++i)
        {
            result |=
                static_cast<std::uint64_t>(cursor.current[i])
                << (i * 8);
        }

        value = result;
        cursor.current += sizeof(result);

        return true;
    }

    bool readI64LE( ReadCursor& cursor, std::int64_t& value)
    {
        std::uint64_t raw;

        if (!readU64LE(cursor, raw))
            return false;

        value = static_cast<std::int64_t>(raw);
        return true;
    }

    bool readFloatLE( ReadCursor& cursor, float& value)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));

        std::uint32_t raw;

        if (!readU32LE(cursor, raw))
            return false;

        value = std::bit_cast<float>(raw);
        return true;
    }

    bool readDoubleLE( ReadCursor& cursor, double& value)
    {
        static_assert(sizeof(double) == sizeof(std::uint64_t));

        std::uint64_t raw;

        if (!readU64LE(cursor, raw))
            return false;

        value = std::bit_cast<double>(raw);
        return true;
    }

    //PRIMITIVE WRITERS
    void writeByte( std::vector<Byte>& output, Byte value)
    {
        output.push_back(value);
    }

    void writeU32LE( std::vector<Byte>& output, std::uint32_t value)
    {
        for (size_t i = 0; i < sizeof(value); ++i)
        {
            output.push_back( static_cast<Byte>( (value >> (i * 8)) & 0xFF));
        }
    }

    void writeI32LE( std::vector<Byte>& output, std::int32_t value)
    {
        writeU32LE( output, static_cast<std::uint32_t>(value));
    }

    void writeU64LE( std::vector<Byte>& output, std::uint64_t value)
    {
        for (size_t i = 0; i < sizeof(value); ++i)
        {
            output.push_back( static_cast<Byte>( (value >> (i * 8)) & 0xFF));
        }
    }

    void writeI64LE( std::vector<Byte>& output, std::int64_t value)
    {
        writeU64LE( output, static_cast<std::uint64_t>(value));
    }

    void writeFloatLE( std::vector<Byte>& output, float value)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));

        std::uint32_t raw = std::bit_cast<std::uint32_t>(value);

        writeU32LE(output, raw);
    }

    void writeDoubleLE( std::vector<Byte>& output, double value)
    {
        static_assert(sizeof(double) == sizeof(std::uint64_t));

        std::uint64_t raw = std::bit_cast<std::uint64_t>(value);

        writeU64LE(output, raw);
    }

    bool BinaryDataFormat::serializeMapValue(const MapValueFormat& value, std::vector<Byte>& output)
    {
        using Types = MapValueFormat::Types;

        switch (value.type)
        {
            case Types::noType:
                return true;

            case Types::rawBytesType:
            case Types::stringType:
            case Types::keyValueType:
            {
                output.insert(output.end(), value.data.begin(), value.data.end());
                return true;
            }
            case Types::boolType:
            {
                writeByte(output, value.primitives.boolData ? static_cast<Byte>(1) : static_cast<Byte>(0));
                return true;
            }
            case Types::intType:
                writeI32LE(output, value.primitives.int32Data);
                return true;

             case Types::uintType:
                writeU32LE(output, value.primitives.uint32Data);
                return true;

            case Types::int64Type:
                writeI64LE(output, value.primitives.int64Data);
                return true;

            case Types::uint64Type:
                writeU64LE(output, value.primitives.uint64Data);
                return true;

            case Types::floatType:
                writeFloatLE(output, value.primitives.floatData);
                return true;

            case Types::doubleType:
                writeDoubleLE(output, value.primitives.doubleData);
                return true;

            case Types::charType:
                writeByte( output, static_cast<Byte>( value.primitives.charData));
                return true;

            case Types::ucharType:
                writeByte( output, static_cast<Byte>( value.primitives.uCharData));
                return true;

            case Types::vec2Type:
                writeFloatLE(output, value.primitives.vec.x);
                writeFloatLE(output, value.primitives.vec.y);
                return true;

            case Types::vec3Type:
                writeFloatLE(output, value.primitives.vec.x);
                writeFloatLE(output, value.primitives.vec.y);
                writeFloatLE(output, value.primitives.vec.z);
                return true;

            case Types::vec4Type:
                writeFloatLE(output, value.primitives.vec.x);
                writeFloatLE(output, value.primitives.vec.y);
                writeFloatLE(output, value.primitives.vec.z);
                writeFloatLE(output, value.primitives.vec.w);
                return true;

            case Types::ivec2Type:
                writeI32LE(output, value.primitives.ivec.x);
                writeI32LE(output, value.primitives.ivec.y);
                return true;

            case Types::ivec3Type:
                writeI32LE(output, value.primitives.ivec.x);
                writeI32LE(output, value.primitives.ivec.y);
                writeI32LE(output, value.primitives.ivec.z);
                return true;

            case Types::ivec4Type:
                writeI32LE(output, value.primitives.ivec.x);
                writeI32LE(output, value.primitives.ivec.y);
                writeI32LE(output, value.primitives.ivec.z);
                writeI32LE(output, value.primitives.ivec.w);
                return true;
        }
    }

    bool BinaryDataFormat::deserializeMapValue( MapValueFormat::Types type, ReadCursor& cursor, MapValueFormat& value)
    {
        using Types = MapValueFormat::Types;

        ReadCursor tempCursor = cursor;

        MapValueFormat result;
        result.type = type;

        switch (type)
        {
            case Types::noType:
                break;

            case Types::rawBytesType:
            case Types::stringType:
            case Types::keyValueType:
            {
                result.data.assign( tempCursor.current, tempCursor.end);
                tempCursor.current = tempCursor.end;
                break;
            }

            case Types::boolType:
            {
                Byte raw;

                if (!readByte(tempCursor, raw))
                    return false;

                if (raw > 1)
                    return false;

                result.primitives.boolData = raw != 0;
                break;
            }

            case Types::intType:
            {
                if (!readI32LE( tempCursor, result.primitives.int32Data))
                    return false;

                break;
            }

            case Types::uintType:
            {
                if (!readU32LE( tempCursor, result.primitives.uint32Data))
                    return false;

                break;
            }

            case Types::int64Type:
            {
                if (!readI64LE(
                        tempCursor,
                        result.primitives.int64Data))
                    return false;

                break;
            }

            case Types::uint64Type:
            {
                if (!readU64LE( tempCursor, result.primitives.uint64Data))
                    return false;

                break;
            }

            case Types::floatType:
            {
                if (!readFloatLE( tempCursor, result.primitives.floatData))
                    return false;

                break;
            }

            case Types::doubleType:
            {
                if (!readDoubleLE( tempCursor, result.primitives.doubleData))
                    return false;

                break;
            }

            case Types::charType:
            {
                Byte raw;

                if (!readByte(tempCursor, raw))
                    return false;

                result.primitives.charData = static_cast<char>(raw);

                break;
            }

            case Types::ucharType:
            {
                Byte raw;

                if (!readByte(tempCursor, raw))
                    return false;

                result.primitives.uCharData = static_cast<unsigned char>(raw);

                break;
            }

            case Types::vec2Type:
            {
                if (!readFloatLE(tempCursor, result.primitives.vec.x))
                    return false;

                if (!readFloatLE(tempCursor, result.primitives.vec.y))
                    return false;

                break;
            }

            case Types::vec3Type:
            {
                if (!readFloatLE(tempCursor, result.primitives.vec.x))
                    return false;

                if (!readFloatLE(tempCursor, result.primitives.vec.y))
                    return false;

                if (!readFloatLE(tempCursor, result.primitives.vec.z))
                    return false;

                break;
            }

            case Types::vec4Type:
            {
                if (!readFloatLE(tempCursor, result.primitives.vec.x))
                    return false;

                if (!readFloatLE(tempCursor, result.primitives.vec.y))
                    return false;

                if (!readFloatLE(tempCursor, result.primitives.vec.z))
                    return false;

                if (!readFloatLE(tempCursor, result.primitives.vec.w))
                    return false;

                break;
            }

            case Types::ivec2Type:
            {
                if (!readI32LE(tempCursor, result.primitives.ivec.x))
                    return false;

                if (!readI32LE(tempCursor, result.primitives.ivec.y))
                    return false;

                break;
            }

            case Types::ivec3Type:
            {
                if (!readI32LE(tempCursor, result.primitives.ivec.x))
                    return false;

                if (!readI32LE(tempCursor, result.primitives.ivec.y))
                    return false;

                if (!readI32LE(tempCursor, result.primitives.ivec.z))
                    return false;

                break;
            }

            case Types::ivec4Type:
            {
                if (!readI32LE(tempCursor, result.primitives.ivec.x))
                    return false;

                if (!readI32LE(tempCursor, result.primitives.ivec.y))
                    return false;

                if (!readI32LE(tempCursor, result.primitives.ivec.z))
                    return false;

                if (!readI32LE(tempCursor, result.primitives.ivec.w))
                    return false;

                break;
            }

            default:
                return false;
        }

        // The payload must be consumed exactly.
        if (tempCursor.current != tempCursor.end)
            return false;

        value = std::move(result);
        cursor = tempCursor;

        return true;
    }

    //using u32 bits for the header (entreCount, keySize, type, payloadSize)
    Errors BinaryDataFormat::serializeToFileData(std::vector<Byte>& output)
    {
        if (umData.size() > UINT32_MAX)
            return fileSizeNotMatch;

        std::vector<Byte> result;

        writeU32LE(result, static_cast<uint32_t>(umData.size()));

        for (const auto& [key, value] : umData)
        {
            if (key.size() > UINT32_MAX)
                return fileSizeNotMatch;

            writeU32LE(result, static_cast<uint32_t>(key.size()));

            result.insert(result.end(), key.begin(), key.end());

            writeU32LE(result, static_cast<uint32_t>(value.type));

            std::vector<Byte> payload;

            if (!serializeMapValue(value, payload))
                return couldNotParseData;

            if (payload.size() > UINT32_MAX)
                return fileSizeNotMatch;

            writeU32LE(result, static_cast<uint32_t>(payload.size()));

            result.insert(result.end(), payload.begin(), payload.end());
        }

        output = std::move(result);
        return noError;
    }

    Errors BinaryDataFormat::deserializeFromFileData(const Byte* data, size_t size)
    {
        if (data == nullptr && size != 0)
            return couldNotParseData;

        if (size < sizeof(std::uint32_t))
            return fileSizeNotBigEnough;

        ReadCursor cursor {data, data + size};

        std::uint32_t entryCount;

        if (!readU32LE(cursor, entryCount))
            return fileSizeNotBigEnough;

        std::unordered_map<std::string, BinaryDataFormat::MapValueFormat> result;

        for (std::uint32_t i = 0; i < entryCount; ++i)
        {
            std::uint32_t keySize;

            if (!readU32LE(cursor, keySize))
                return fileSizeNotBigEnough;

            if (!canRead(cursor, keySize))
                return fileSizeNotBigEnough;

            std::string key(reinterpret_cast<const char*>(cursor.current), keySize);

            cursor.current += keySize;

            std::uint32_t rawType;

            if (!readU32LE(cursor, rawType))
                return fileSizeNotBigEnough;

            if (rawType > static_cast<std::uint32_t>(BinaryDataFormat::MapValueFormat::Types::keyValueType))
                return couldNotParseData;

            BinaryDataFormat::MapValueFormat::Types type = static_cast<BinaryDataFormat::MapValueFormat::Types>(rawType);

            std::uint32_t payloadSize;

            if (!readU32LE(cursor, payloadSize))
                return fileSizeNotBigEnough;

            if (!canRead(cursor, payloadSize))
                return fileSizeNotBigEnough;

            ReadCursor payloadCursor { cursor.current, cursor.current + payloadSize };

            BinaryDataFormat::MapValueFormat value;

            if (!BinaryDataFormat::deserializeMapValue(type, payloadCursor, value))
                return couldNotParseData;

            cursor.current += payloadSize;

            auto [iterator, inserted] = result.emplace(std::move(key), std::move(value));

            if (!inserted)
                return dataFormatValueAlreadyExists;
        }

        if (cursor.current != cursor.end)
            return fileSizeNotMatch;

        umData = std::move(result);

        return noError;
    }
}

