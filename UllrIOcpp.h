#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
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
        dataFormatValueAlreadyExists,
        dataFormatValueNotFound,
        dataFormatValueDifferentDataType,
        couldNotParseData,
        fileSizeNotBigEnough
    };


    const char* getErrorString(Errors error);
    Errors getFileSize(const char* filePath, size_t& size);
    Errors readEntireFile(std::vector<Byte>& data, const char* filePath);
    Errors writeEntireFile(const void* data, size_t size, const char* filePath);

    unsigned long long fnv_hash_1a_64(const void* key, size_t len);
    Errors writeEntireFile(const void* data, size_t size, const char* filePath);
    Errors writeEntireFile(const std::vector<Byte>& data, const char* filePath);

    Errors readEntireFileCheckSum(void* data, size_t size, const char* filePath);
    Errors readEntireFileCheckSum(std::vector<Byte>& data, const char* filePath);
    Errors writeEntireFileCheckSum(const void* data, size_t size, const char* filePath);
    Errors writeEntireFileCheckSum(const std::vector<Byte>& data, const char* filePath);

    Errors safeLoadBackup(std::vector<Byte>& data, const char* filePath);
    Errors safeLoadBackup(void* data, size_t size, const char* filePathWithExtension);
    Errors safeSave(const void* data, size_t size, const char* filePathWithExtension, bool reportNotMakingBackupAsError);
    Errors safeSave(const std::vector<Byte>& data, const char* filePathWithExtension, bool reportNotMakingBackupAsError);
    Errors safeLoad(void* data, size_t size, const char* filePathWithExtension, bool reportBackupAsError);
    Errors safeLoad(std::vector<Byte>& data, const char* filePathWithExtension, bool reportBackupError);

    Errors safeSaveNoBackup(const void* data, size_t size, const char* filePathWithExtension);
    Errors safeSaveNoBackup(const std::vector<Byte>& data, const char* filePathWithExtension);

    //helper functions and Cursor struct
    struct ReadCursor
    {
        const Byte* current;
        const Byte* end;
    };
    bool canRead(const ReadCursor& cursor, size_t amount);
    bool readByte(ReadCursor& cursor, Byte& value);
    bool readU32LE(ReadCursor& cursor, std::uint32_t& value);
    bool readI32LE(ReadCursor& cursor, std::int32_t& value);
    bool readU64LE(ReadCursor& cursor, std::uint64_t& value);
    bool readI64LE(ReadCursor& cursor, std::int64_t& value);
    bool readFloatLE(ReadCursor& cursor, float& value);
    bool readDoubleLE(ReadCursor& cursor, double& value);

    void writeByte(std::vector<Byte>& output, Byte value);
    void writeU32LE(std::vector<Byte>& output, std::uint32_t value);
    void writeI32LE(std::vector<Byte>& output, std::int32_t value);
    void writeU64LE(std::vector<Byte>& output, std::uint64_t value);
    void writeI64LE(std::vector<Byte>& output, std::int64_t value);
    void writeFloatLE(std::vector<Byte>& output, float value);
    void writeDoubleLE(std::vector<Byte>& output, double value);

    struct BinaryDataFormat
    {
        struct MapValueFormat
        {
            enum class Types
            {
                noType = 0,
                rawBytesType,

                //bool
                boolType,

                //integers
                intType,
                uintType,
                int64Type,
                uint64Type,

                //Floating point
                floatType,
                doubleType,

                //characters and strings
                charType,
                ucharType,
                stringType,

                //floating-point vectors
                vec2Type,
                vec3Type,
                vec4Type,

                //Integer vectors
                ivec2Type,
                ivec3Type,
                ivec4Type,

                //Key-value pair
                keyValueType,
            };

            //Buffer
            std::vector<Byte> data;
            Types type = Types::noType;
            union Primitives
            {
                std::int32_t int32Data = 0;
                std::uint32_t uint32Data;
                std::int64_t int64Data;
                std::uint64_t uint64Data;

                bool boolData;

                float floatData;
                double doubleData;

                char charData;
                unsigned char uCharData;

                struct Vec
                {
                    float x;
                    float y;
                    float z;
                    float w;
                } vec;

                struct IVec
                {
                    std::int32_t x;
                    std::int32_t y;
                    std::int32_t z;
                    std::int32_t w;
                } ivec;

            } primitives;

            bool operator== (const MapValueFormat &other) const;
            bool operator!= (const MapValueFormat &other) const { return !(*this == other); }

            MapValueFormat() = default;
            MapValueFormat(std::int32_t i) { type = Types::intType; primitives.int32Data = i; }
            MapValueFormat(const std::vector<Byte>& bytes) { type = Types::rawBytesType; data = bytes; }
            MapValueFormat(bool b) { type = Types::boolType; primitives.boolData = b; }
            MapValueFormat(std::uint32_t i) { type = Types::uintType; primitives.uint32Data = i; }
            MapValueFormat(std::int64_t i) { type = Types::int64Type; primitives.int64Data = i; }
            MapValueFormat(std::uint64_t i) { type = Types::uint64Type; primitives.uint64Data = i; }
            MapValueFormat(float f) { type = Types::floatType; primitives.floatData = f; }
            MapValueFormat(double d) { type = Types::doubleType; primitives.doubleData = d; }
            MapValueFormat(char c) { type = Types::charType; primitives.charData = c; }
            MapValueFormat(unsigned char c) { type = Types::ucharType; primitives.uCharData = c; }
            MapValueFormat(const std::string& s) { type = Types::stringType; data.assign(s.begin(), s.end()); }

            //Vector constructor
            MapValueFormat(float x, float y)
            { type = Types::vec2Type; primitives.vec.x = x; primitives.vec.y = y; }

            MapValueFormat(float x, float y, float z)
            { type = Types::vec3Type; primitives.vec.x = x; primitives.vec.y = y; primitives.vec.z = z; }

            MapValueFormat(float x, float y, float z, float w)
            { type = Types::vec4Type; primitives.vec.x = x; primitives.vec.y = y; primitives.vec.z = z; primitives.vec.w = w; }

            MapValueFormat(int x, int y)
            { type = Types::ivec2Type; primitives.ivec.x = x; primitives.ivec.y = y; }

            MapValueFormat(int x, int y, int z)
            { type = Types::ivec3Type; primitives.ivec.x = x; primitives.ivec.y = y; primitives.ivec.z = z; }

            MapValueFormat(int x, int y, int z, int w)
            { type = Types::ivec4Type; primitives.ivec.x = x; primitives.ivec.y = y; primitives.ivec.z = z; primitives.ivec.w = w; }

        };

        BinaryDataFormat() = default;
        BinaryDataFormat(std::initializer_list<std::pair<std::string, MapValueFormat>> init)
        {
            for (const auto &pair : init)
                umData[pair.first] = pair.second;
        }

        std::unordered_map<std::string, MapValueFormat> umData;

        bool operator== (const BinaryDataFormat& other) const;

        bool operator!= (const BinaryDataFormat& other) const
        {
            return !(*this == other);
        }

        bool valueExists(std::string at);
        //Getter and Setters
        // Type
        Errors getMapValueFormatType(std::string at, MapValueFormat::Types& type);

        // Raw bytes
        Errors setRawBytes(std::string at, const std::vector<Byte>& bytes);
        Errors getRawBytes(std::string at, std::vector<Byte>& bytes);

        // Boolean
        Errors setBool(std::string at, bool b);
        Errors getBool(std::string at, bool& b);

        // Integers
        Errors setInt32(std::string at, std::int32_t i);
        Errors getInt32(std::string at, std::int32_t& i);

        Errors setUInt32(std::string at, std::uint32_t i);
        Errors getUInt32(std::string at, std::uint32_t& i);

        Errors setInt64(std::string at, std::int64_t i);
        Errors getInt64(std::string at, std::int64_t& i);

        Errors setUInt64(std::string at, std::uint64_t i);
        Errors getUInt64(std::string at, std::uint64_t& i);

        // Floating point
        Errors setFloat(std::string at, float f);
        Errors getFloat(std::string at, float& f);

        Errors setDouble(std::string at, double d);
        Errors getDouble(std::string at, double& d);

        // Characters
        Errors setChar(std::string at, char c);
        Errors getChar(std::string at, char& c);

        Errors setUChar(std::string at, unsigned char c);
        Errors getUChar(std::string at, unsigned char& c);

        // Strings
        Errors setString(std::string at, const std::string& s);
        Errors getString(std::string at, std::string& s);

        // Floating-point vectors
        Errors setVec2(std::string at, float x, float y);
        Errors getVec2(std::string at, float& x, float& y);

        Errors setVec3(std::string at, float x, float y, float z);
        Errors getVec3(std::string at, float& x, float& y, float& z);

        Errors setVec4(std::string at, float x, float y, float z, float w);
        Errors getVec4(std::string at, float& x, float& y, float& z, float& w);

        // Integer vectors
        Errors setIVec2(std::string at, int x, int y);
        Errors getIVec2(std::string at, int& x, int& y);

        Errors setIVec3(std::string at, int x, int y, int z);
        Errors getIVec3(std::string at, int& x, int& y, int& z);

        Errors setIVec4(std::string at, int x, int y, int z, int w);
        Errors getIVec4(std::string at, int& x, int& y, int& z, int& w);

        // Key-value payload
        Errors setKeyValue(std::string at, const std::vector<Byte>& bytes);
        Errors getKeyValue(std::string at, std::vector<Byte>& bytes);

        static bool serializeMapValue(const MapValueFormat& value, std::vector<Byte>& output);
        static bool deserializeMapValue(MapValueFormat::Types type, ReadCursor& cursor, MapValueFormat& value);

        Errors serializeToFileData(std::vector<Byte>& output);
        Errors deserializeFromFileData(const Byte* data, size_t size);
    };

    Errors safeLoad(BinaryDataFormat& data, const char* filePath, bool reportNotMakingBackupAsError);
    Errors safeSave(BinaryDataFormat& data, const char* filePath, bool reportNotMakingBackupAsError);
}
