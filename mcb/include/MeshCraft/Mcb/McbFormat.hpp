#pragma once
#include <cstdint>

namespace MeshCraft::Mcb {

inline constexpr char     MCB_MAGIC[4] = {'M','C','B','\0'};
inline constexpr uint8_t  MCB_VERSION  = 1;

// AUDIT-0038: oldest MCB version loadFromBinary() will still accept, via
// the chained-upgrade mechanism in McbReader.cpp. Equal to MCB_VERSION
// today -- no version older than 1 has ever existed, so there is nothing
// to upgrade from yet. A future version bump that changes wire-level
// *meaning* (not just adds new optional keys, which the key-tagged format
// already tolerates via skipValue()) should register an upgrade function
// in McbReader.cpp and can leave this constant alone, unless a version is
// deliberately dropped from support.
inline constexpr uint8_t  MCB_MIN_SUPPORTED_VERSION = 1;

// Flags byte (offset 5 in header)
//
// SYS-W14-25 (2026-07-20): when set, the header's 2 reserved bytes are
// followed by two little-endian uint32 fields (uncompressed payload size,
// compressed payload size) and then the zlib (deflate)-compressed payload
// itself, instead of the plain TAG_OBJ + document bytes an unset flag
// means. See McbWriter::saveToBinary's `compress` parameter / McbReader's
// loadFromBinaryImpl. Requires this build to have been compiled with zlib
// available (mcb/CMakeLists.txt's `find_package(ZLIB)`) -- writing throws
// if compression is requested without it; reading a compressed file
// throws a distinct "requires zlib" error rather than misparsing it.
inline constexpr uint8_t  MCB_FLAG_COMPRESSED = 0x01;

// Type tags
inline constexpr uint8_t TAG_NULL    = 0x00; // absent optional — no data bytes
inline constexpr uint8_t TAG_BOOL    = 0x01; // 1 byte: 0 or 1
inline constexpr uint8_t TAG_I32     = 0x02; // 4 bytes little-endian signed
inline constexpr uint8_t TAG_F32     = 0x03; // 4 bytes IEEE 754
inline constexpr uint8_t TAG_STR     = 0x04; // uint32 len + bytes (UTF-8, no null)
inline constexpr uint8_t TAG_VEC3    = 0x05; // 3 × float32
inline constexpr uint8_t TAG_VEC4    = 0x06; // 4 × float32
inline constexpr uint8_t TAG_OBJ     = 0x07; // key-value pairs, terminated by key_len=0
inline constexpr uint8_t TAG_ARR     = 0x08; // uint32 count, then count × (tag + data)
inline constexpr uint8_t TAG_MAP     = 0x09; // uint32 count, then count × (STR key + tag + data)

} // namespace MeshCraft::Mcb
