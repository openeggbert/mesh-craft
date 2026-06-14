#pragma once
#include <cstdint>

namespace MeshCraft::Mcb {

inline constexpr char     MCB_MAGIC[4] = {'M','C','B','\0'};
inline constexpr uint8_t  MCB_VERSION  = 1;

// Flags byte (offset 5 in header)
inline constexpr uint8_t  MCB_FLAG_COMPRESSED = 0x01; // zlib payload (not yet implemented)

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
