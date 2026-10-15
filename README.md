# UllrIOcpp

> A small, dependency-free C++20 library for crash-safe, corruption-resistant file storage.

**UllrIOcpp** is a lightweight C++ library for saving and loading structured data with **checksum verification**, **automatic backup recovery**, and **atomic file replacement**.

Built as a from-scratch reimplementation and redesign of the ideas behind `meemknight/safeSave`, UllrIOcpp uses an explicitly framed binary format and a strictly layered architecture.

**[Overview](#overview) · [Features](#features) · [Quick Start](#quick-start) · [File Format](#file-format) · [API](#api-at-a-glance) · [Safety](#safety-guarantees) · [Building](#building) · [Testing](#testing)**

---

## Overview

Save files can become corrupted. Disks fail, machines lose power during writes, and filesystems do not guarantee that ordinary file operations are crash-safe.

Writing a structure directly to a file offers little protection against these failures. UllrIOcpp adds three important properties on top of ordinary file I/O:

| Guarantee | Description |
| :--- | :--- |
| **Integrity** | Every file carries a checksum. Corrupted data is rejected instead of silently returned. |
| **Redundancy** | Every save maintains a primary file and a backup. If the primary is invalid, loading automatically attempts the backup. |
| **Atomicity** | Data is written to a temporary file and then renamed into place, preventing a partially written file from replacing the existing primary. |

> **Storage overhead**
>
> UllrIOcpp adds only **8 bytes per file** for the checksum, plus one additional file copy per save.

---

## Features

- **Heterogeneous key-value storage** — Store multiple value types in a single `BinaryDataFormat` map.
- **18 supported value types** — Booleans, integers, floating-point values, characters, strings, raw bytes, vectors, integer vectors, and nested key-value payloads.
- **Explicit binary framing** — Every field is length-prefixed, eliminating ambiguous parsing and unbounded scanning.
- **Little-endian serialization** — Consistent byte order across platforms, subject to compatible fundamental type representations.
- **FNV-1a 64-bit checksums** — Fast, dependency-free detection of accidental data corruption.
- **Crash-conscious save pipeline** — Temporary-file writes, atomic rename, and backup copying.
- **Automatic backup recovery** — Loading falls back to the backup when the primary file fails validation or cannot be read.
- **Zero external dependencies** — Uses only the C++ standard library.
- **Cross-platform design** — Intended for Windows, Linux, and macOS. Windows is currently tested.

---

## Quick Start

### Basic Usage

```cpp
#include "UllrIOcpp.h"

#include <cstdint>
#include <cstdio>

using namespace UllrIOcpp;

int main() {
    BinaryDataFormat save;

    save.setInt32("health", 100);
    save.setString("name", "player one");
    save.setBool("alive", true);
    save.setVec3("position", 1.5f, 2.5f, -3.5f);

    // Creates save1.bin and save2.bin.
    Errors e = safeSave(save, "save", false);

    if (e != noError) {
        std::printf("save failed: %s\n", getErrorString(e));
        return 1;
    }

    BinaryDataFormat loaded;

    e = safeLoad(loaded, "save", false);

    if (e != noError) {
        std::printf("load failed: %s\n", getErrorString(e));
        return 1;
    }

    std::int32_t hp;
    loaded.getInt32("health", hp); // hp == 100

    return 0;
}
```

### Generated Files

The filename passed to `safeSave` or `safeLoad` determines the primary and backup paths.

| Input | Primary | Backup |
| :--- | :--- | :--- |
| `save` | `save1.bin` | `save2.bin` |
| `data/game.bin` | `data/game1.bin` | `data/game2.bin` |

During a save, UllrIOcpp first writes to a temporary `.tmp` file. The temporary file is then renamed into place before the primary write is considered complete.

> **Note**
>
> `reportBackupAsError` and `reportNotMakingBackupAsError` control whether certain recoverable conditions are surfaced through the `Errors` enum. See [Error Model](#error-model).

---

## File Format

Each saved file consists of a serialized payload followed by an 8-byte checksum.

```text
+---------------------------+--------------------+
|          Payload          |  8-byte checksum   |
+---------------------------+--------------------+
```

The checksum always occupies the final 8 bytes of the file.

### Payload Structure

```text
[u32 entryCount]

Repeat entryCount times:

    [u32 keySize]        Key length in bytes; no terminator
    [bytes key]          Exactly keySize bytes
    [u32 type]           Type enum: 0 = noType ... 18 = keyValueType
    [u32 payloadSize]    Value length in bytes
    [bytes payload]      Exactly payloadSize bytes
```

All integer fields use **little-endian byte order**.

Every length field is a `uint32_t`, limiting individual keys and payloads to just under 4 GiB.

The parser validates field boundaries before accessing their contents, allowing malformed or truncated files to be rejected cleanly.

### Value Payload Encoding

| Type | Size | Encoding |
| :--- | ---: | :--- |
| `noType` | 0 bytes | No payload |
| `rawBytes` | N bytes | N raw bytes |
| `bool` | 1 byte | `0x00` or `0x01` |
| `int32` | 4 bytes | Two's-complement, little-endian |
| `uint32` | 4 bytes | Unsigned, little-endian |
| `int64` | 8 bytes | Two's-complement, little-endian |
| `uint64` | 8 bytes | Unsigned, little-endian |
| `float` | 4 bytes | IEEE-754 binary32, little-endian |
| `double` | 8 bytes | IEEE-754 binary64, little-endian |
| `char` | 1 byte | One byte |
| `uchar` | 1 byte | One byte |
| `string` | N bytes | N raw bytes; may contain `\0` |
| `vec2` | 8 bytes | Two little-endian floats |
| `vec3` | 12 bytes | Three little-endian floats |
| `vec4` | 16 bytes | Four little-endian floats |
| `ivec2` | 8 bytes | Two little-endian `int32` values |
| `ivec3` | 12 bytes | Three little-endian `int32` values |
| `ivec4` | 16 bytes | Four little-endian `int32` values |
| `keyValue` | N bytes | Serialized nested key-value map |

Strings can contain null bytes because their lengths are explicitly stored.

Keys are also length-prefixed, although embedded null bytes are not currently validated.

### Checksum

UllrIOcpp computes an **FNV-1a 64-bit checksum** over the payload and appends the resulting 8-byte value as a trailer.

During loading:

```text
Read file
    |
    v
Separate payload and checksum
    |
    v
Recompute FNV-1a checksum
    |
    v
Compare with stored checksum
    |
    +---- Match ----> Continue loading
    |
    +---- Mismatch -> checkSumFail
```

A checksum mismatch causes the load to fail with `checkSumFail`.

> **Security**
>
> FNV-1a is used for efficient detection of accidental corruption. It is **not** a cryptographic hash and does not protect against deliberate modification.

---

## Error Model

Every I/O function returns an `Errors` enum value. Data is delivered through output parameters rather than return values.

### Error Codes

```cpp
enum Errors : int {
    noError = 0,
    couldNotOpenFile,                 // I/O failure
    fileSizeNotMatch,                 // Size validation failed
    checkSumFail,                     // Checksum validation failed
    couldNotMakeBackup,               // Backup copy failed, if requested
    readBackup,                       // Backup loaded successfully, if requested
    dataFormatValueAlreadyExists,     // Setter overwrote an existing key
    dataFormatValueNotFound,          // Getter requested a missing key
    dataFormatValueDifferentDataType, // Getter type mismatch
    couldNotParseData,                // Malformed binary data
    fileSizeNotBigEnough,             // File too small for checksum trailer
};
```

### Recoverable Signals

Two error codes describe conditions that do not necessarily indicate failure.

| Code | Meaning |
| :--- | :--- |
| `readBackup` | Loading succeeded using the backup because the primary could not be loaded successfully. Returned only when `reportBackupAsError = true`. |
| `dataFormatValueAlreadyExists` | A setter replaced an existing key. The map remains valid. |

All other nonzero codes indicate an actual failure.

> **Important**
>
> On failure, an output parameter may contain invalid or incomplete data. Do not use it unless the API explicitly documents otherwise.
>
> The byte-level `safeLoad` has one documented exception: on `checkSumFail`, its buffer contains the bytes read before checksum validation failed. Treat this data as **untrusted**.

---

## API at a Glance

UllrIOcpp is divided into four layers:

```text
+--------------------------------------+
|  4. Structured Key-Value Storage     |
+--------------------------------------+
|  3. Safe Save / Load                 |
+--------------------------------------+
|  2. Checksum Handling                |
+--------------------------------------+
|  1. Raw File I/O                     |
+--------------------------------------+
```

Each layer builds on the functionality of the layer below it.

### 1. Raw File I/O

```cpp
Errors getFileSize(const char* path, size_t& size);

Errors readEntireFile(
    std::vector<Byte>& data,
    const char* path
);

Errors readEntireFile(
    void* data,
    size_t size,
    const char* path
);

Errors writeEntireFile(
    const void* data,
    size_t size,
    const char* path
);

Errors writeEntireFile(
    const std::vector<Byte>& data,
    const char* path
);
```

Provides basic file reading, writing, and size inspection without checksum processing.

### 2. Checksum Layer

```cpp
Errors readEntireFileCheckSum(
    void* data,
    size_t size,
    const char* path
);

Errors readEntireFileCheckSum(
    std::vector<Byte>& data,
    const char* path
);

Errors writeEntireFileCheckSum(
    const void* data,
    size_t size,
    const char* path
);

Errors writeEntireFileCheckSum(
    const std::vector<Byte>& data,
    const char* path
);
```

Adds checksum verification to file reads and checksum generation to file writes.

### 3. Safe Save / Load Layer

```cpp
Errors safeSave(
    const void* data,
    size_t size,
    const char* name,
    bool reportNotMakingBackupAsError
);

Errors safeSave(
    const std::vector<Byte>& data,
    const char* name,
    bool reportNotMakingBackupAsError
);

Errors safeSaveNoBackup(
    const void* data,
    size_t size,
    const char* name
);

Errors safeSaveNoBackup(
    const std::vector<Byte>& data,
    const char* name
);

Errors safeLoad(
    void* data,
    size_t size,
    const char* name,
    bool reportBackupAsError
);

Errors safeLoad(
    std::vector<Byte>& data,
    const char* name,
    bool reportBackupAsError
);

Errors safeLoadBackup(
    void* data,
    size_t size,
    const char* name
);

Errors safeLoadBackup(
    std::vector<Byte>& data,
    const char* name
);
```

Provides checksum-protected persistence, temporary-file replacement, backup creation, and backup fallback.

### 4. Structured Map Layer

`BinaryDataFormat` stores heterogeneous values in a single key-value map.

```cpp
struct BinaryDataFormat {
    std::unordered_map<std::string, MapValueFormat> umData;

    bool valueExists(std::string key);

    Errors getMapValueFormatType(
        std::string key,
        MapValueFormat::Types& out
    );

    // Setter and getter for every supported type.
    Errors setInt32(std::string key, std::int32_t i);
    Errors getInt32(std::string key, std::int32_t& out);

    // Also available:
    // uint32, int64, uint64, float, double, bool,
    // char, uchar, string, rawBytes,
    // vec2/3/4, ivec2/3/4, keyValue

    Errors serializeToFileData(std::vector<Byte>& out);

    Errors deserializeFromFileData(
        const Byte* data,
        size_t size
    );
};

Errors safeSave(
    BinaryDataFormat& data,
    const char* name,
    bool reportNotMakingBackupAsError
);

Errors safeLoad(
    BinaryDataFormat& data,
    const char* name,
    bool reportBackupAsError
);
```

Every setter returns `dataFormatValueAlreadyExists` when it overwrites an existing key.

Every getter returns either `dataFormatValueNotFound` or `dataFormatValueDifferentDataType` when the requested value is unavailable or has the wrong type. On these failures, the output parameter is left untouched.

---

## Safety Guarantees

### Save Pipeline

A `safeSave` operation proceeds in four stages:

```text
BinaryDataFormat
       |
       v
   Serialize
       |
       v
<name>1.tmp
       |
       | checksum-protected write
       v
Atomic rename
       |
       v
<name>1.bin
       |
       | backup copy
       v
<name>2.bin
```

More precisely:

1. Serialize the map into a `std::vector<Byte>`.
2. Write the payload and checksum to `<name>1.tmp`.
3. Rename `<name>1.tmp` to `<name>1.bin`.
4. Copy `<name>1.bin` to `<name>2.bin`, replacing the previous backup.

The rename provides atomic replacement on supported filesystems when the operation succeeds. The destination is not exposed as a partially written file.

### Failure Behavior

| Failure Point | Expected Behavior |
| :--- | :--- |
| Serialization or temporary-file write fails | The existing primary is not replaced. |
| Rename fails | The primary remains unchanged; the temporary file may remain. |
| Backup copy fails | The primary is already valid, but the backup may be stale or absent. Reporting depends on `reportNotMakingBackupAsError`. |

A backup-copy failure does **not** invalidate a successfully written primary.

### Load Pipeline

A `safeLoad` operation follows this sequence:

```text
              safeLoad
                  |
                  v
          Load primary file
            <name>1.bin
                  |
          +-------+-------+
          |               |
        Valid           Invalid
          |               |
          v               v
       Return       Load backup file
                       <name>2.bin
                           |
                   +-------+-------+
                   |               |
                 Valid           Invalid
                   |               |
                   v               v
                Return          Error
```

In detail:

1. Attempt to load `<name>1.bin`.
2. Validate the file size and checksum.
3. If the primary fails, attempt `<name>2.bin`.
4. If the backup succeeds, return its data and optionally report `readBackup`.
5. If both attempts fail, return the more informative error.

The temporary `.tmp` file is never used as a load source.

If a crash leaves a temporary file behind, the next save overwrites it. Normal loading continues to use only the primary and backup files.

---

## Design Decisions

### Explicit Framing

UllrIOcpp uses 32-bit length prefixes for keys, payloads, and the entry count rather than relying on null-terminated values or implicit boundaries.

This allows the parser to validate boundaries and reject truncated files without reading past the available data.

### Filesystem-Aware Path Handling

`std::filesystem::stem()` and `std::filesystem::extension()` are used instead of naive string concatenation.

For example:

```text
Input:   save.bin
Primary: save1.bin
Backup:  save2.bin
```

This avoids paths such as `save.bin1` and `save.bin2`.

If no extension is supplied, `.bin` is assumed.

### Standard-Compliant Floating-Point Serialization

`std::bit_cast` converts floating-point values to their bit representations without type-punning through unions or relying on undefined behavior.

### Explicit Little-Endian Conversion

The `readXLE` and `writeXLE` helpers perform byte-order conversion explicitly.

Serialization therefore does not depend on the host machine's endianness.

Cross-platform compatibility still depends on compatible representations for the underlying types, particularly IEEE-754 floating-point values.

### Transactional Deserialization

`deserializeFromFileData` constructs a temporary map and commits it to `umData` only after the entire input has been validated.

```text
Input bytes
    |
    v
Temporary map
    |
    +---- Parse failure ----> Discard
    |
    +---- Valid input ------> Commit to umData
```

If parsing fails, the existing map remains unchanged.

### Complete Buffer Consumption

After parsing all declared entries, the parser verifies that no unexpected bytes remain.

Trailing data is rejected with `fileSizeNotMatch` rather than silently ignored.

---

## Limitations

UllrIOcpp deliberately remains small and focused. The following constraints are important when deciding whether it fits a project.

| Limitation | Details |
| :--- | :--- |
| **No cryptographic protection** | FNV-1a detects accidental corruption but does not prevent deliberate tampering. |
| **No format versioning** | There is no version field. Incompatible format changes can invalidate existing save files. |
| **No streaming** | Files are read entirely into memory. This is appropriate for ordinary save files, not massive datasets. |
| **No thread safety** | Concurrent operations using the same filename may race on the temporary file. |
| **Unspecified entry order** | `std::unordered_map` iteration order is not stable. Byte-for-byte reproducibility is not guaranteed. |
| **No explicit string-size policy** | Keys and payloads use 32-bit lengths, limiting their sizes to just under 4 GiB. |
| **Embedded nulls in keys** | Keys can contain null bytes, but these are not currently validated and may surprise external tools. |
| **No memory-mapped I/O** | File operations use `std::ifstream` and `std::ofstream`. |

### Reproducible Serialization

If deterministic, byte-for-byte identical output is required, sort keys before serialization instead of relying on `std::unordered_map` iteration order.

### Format Evolution

The binary format currently has no version prefix.

If compatibility across future releases is required, introduce an explicit version field before shipping a format that must remain stable.

### Filesystem Durability

Atomic rename and checksum verification address important failure modes, but they are not a universal guarantee against every form of power-loss data loss.

In particular, the implementation does not establish a portable guarantee that file contents and directory metadata have been flushed to durable storage before returning.

---

## Building

UllrIOcpp consists of a header and a single implementation file. No external dependencies or build system are required.

### Source Files

```text
UllrIOcpp.h
UllrIOcpp.cpp
```

Add both files to your project.

### Requirements

| Requirement | Details |
| :--- | :--- |
| **Language** | C++20 or later |
| **Dependencies** | C++ standard library only |
| **Filesystem** | Standard file I/O and `std::filesystem` support |
| **Platforms** | Windows, Linux, macOS |
| **Currently tested** | Windows |

The implementation uses `std::bit_cast` and `std::filesystem`.

### CMake

```cmake
add_library(UllrIOcpp UllrIOcpp.cpp)

target_compile_features(
    UllrIOcpp PUBLIC cxx_std_20
)
```

Link the `UllrIOcpp` target to your application as needed.

---

## Testing

A test suite is provided in `main.cpp`. It tests each library layer independently before verifying their interaction.

### Checksum Layer

- Successful read/write round trips
- Corruption at every byte offset
- Truncated files
- Empty payloads
- Hash sensitivity
- Odd-sized payloads

### Safe Layer

- Backup fallback
- Silent versus reported backup recovery
- Failure when both files are corrupted
- Missing primary files
- Backup-copy failure simulated by using a directory in place of the destination file
- Crash simulation using a stale `.tmp` file alongside a valid primary
- Filename and extension handling

### Map Layer

- Round trips for every supported type
- Mixed-type maps
- Missing keys
- Incorrect value types
- Malformed input
  - Truncation
  - Invalid types
  - Incorrect payload sizes
  - Duplicate keys
  - Trailing bytes
- Preservation of the original map when deserialization fails

### Integration

- Interaction between the map and safe-storage layers
- Propagation of the `readBackup` signal

### Running the Tests

Build and run `main.cpp` as a normal executable.

Each test prints one line. Failures report the source file, line number, and failed expression.

A successful run exits with:

```text
0
```

---

## Project Principles

UllrIOcpp follows five practical engineering principles:

| Principle | Meaning |
| :--- | :--- |
| **Explicit over implicit** | Every field has a defined boundary and representation. |
| **Validate before consuming** | Malformed data is rejected before unsafe access. |
| **Preserve valid state** | Failed deserialization does not partially mutate the existing map. |
| **Recover when possible** | A valid backup can recover data when the primary is unavailable or corrupted. |
| **Keep dependencies minimal** | The library relies exclusively on the C++ standard library. |

The goal is straightforward:

> **Make structured file persistence safer without introducing a large framework or unnecessary complexity.**

---

## License

Add your project's license information here.

If you redistribute UllrIOcpp, include the applicable license and attribution details.

---

<p align="center">
  <strong>UllrIOcpp</strong><br>
  Structured persistence with explicit framing, integrity checks, and automatic recovery.
</p>
