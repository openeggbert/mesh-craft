# MCB Format Specification

MCB ("MeshCraft Binary") is a compact tagged binary encoding of the same
data model as [MC3 XML](MC3_FORMAT.md) (`.mc3.xml`). It is produced/consumed
by `mcb/src/McbWriter.cpp` / `mcb/src/McbReader.cpp` and the `mc3tomcb` CLI.

**MCB is a faster-load cache format, not an authoring format.** There is no
MCB editor, no hand-written `.mcb` files, and no plan to add either — always
author in `.mc3.xml` and convert to `.mcb` as a build/export step.

---

## File header

| Offset | Size | Field | Value |
|--------|------|-------|-------|
| 0 | 4 bytes | Magic | `'M','C','B','\0'` (`MCB_MAGIC` in `McbFormat.hpp`) |
| 4 | 1 byte | Version | `MCB_VERSION` (currently `1`) |
| 5 | 1 byte | Flags | bit 0 = `MCB_FLAG_COMPRESSED` (zlib payload — **defined, not yet implemented**; a file with this bit set is rejected with a clear error, not silently misread) |
| 6 | 2 bytes | Reserved | always `0x00 0x00`; not currently validated on read (skipped) |
| 8 | 1 byte | Root tag | always `TAG_OBJ` — the document root is always a keyed object |
| 9+ | — | Document body | a `TAG_OBJ` value (see below) |

`McbReader::loadFromBinary()` validates the magic, version, flags, and root
tag before parsing the body; each check throws a `std::runtime_error` with a
specific message on failure (`"MCB: invalid magic"`, `"MCB: unsupported
version N"`, `"MCB: compressed format not yet supported"`, `"MCB: root is
not an object"`). A truncated or all-zero file also fails cleanly — every
low-level reader (`rU8`/`rU32`/`rRawStr`) throws on a short read rather than
reading uninitialized/out-of-bounds memory (empirically verified in
`mcb/test/mcb_roundtrip_test.cpp`'s `testTruncatedFile()` /
`testAllZerosInput()` / `testSingleByteInput()`, STAB-0133/0134/0135).

---

## Type tags (`TAG_*`, `McbFormat.hpp`)

| Constant | Value | Wire encoding |
|----------|-------|----------------|
| `TAG_NULL` | `0x00` | no data bytes — an absent optional value |
| `TAG_BOOL` | `0x01` | 1 byte: `0` or `1` |
| `TAG_I32`  | `0x02` | 4 bytes, signed 32-bit integer |
| `TAG_F32`  | `0x03` | 4 bytes, IEEE-754 single-precision float |
| `TAG_STR`  | `0x04` | `uint32` length (bytes, not codepoints) + that many raw bytes, no NUL terminator |
| `TAG_VEC3` | `0x05` | 3 × `TAG_F32` (x, y, z) |
| `TAG_VEC4` | `0x06` | 4 × `TAG_F32` (x, y, z, w) |
| `TAG_OBJ`  | `0x07` | a sequence of `key + tag + value` triples, terminated by a zero-length key (see "Object encoding" below) |
| `TAG_ARR`  | `0x08` | `uint32` count, then that many `tag + value` pairs (elements may have mixed tags) |
| `TAG_MAP`  | `0x09` | `uint32` count, then that many `TAG_STR key + tag + value` triples (like an object, but with full-length string keys and an explicit count instead of a zero-length sentinel) |

All multi-byte integers (`uint32`, the 4 bytes of a `TAG_I32`/`TAG_F32`) are
written **little-endian**, and this is a *fixed wire encoding*, not a
reflection of the host's native byte order — see "Endianness" below.

### Object encoding (`TAG_OBJ`)

An object is a flat sequence of fields, each `key(1-byte length + bytes) +
tag(1 byte) + value(tag-dependent)`, terminated by a key with length `0`
(no tag/value follows the terminator). Key length is a single `uint8_t`, so
keys are capped at 255 bytes — every real key name in the schema is a short
ASCII identifier (`"position"`, `"csgType"`, etc.), well under that limit.

```
field := key_len(u8) key_bytes(key_len) tag(u8) value(tag-dependent)
object := field* 0x00
```

### Map encoding (`TAG_MAP`)

Used for `Mc3Document`/`Mc3Object` maps whose keys are arbitrary
user-supplied strings (asset ids, material names, meta keys) rather than a
fixed schema field name — e.g. `doc.materials`, `doc.textures`, `doc.meta`,
`obj.states`. Uses a `TAG_STR` key (length-prefixed `uint32`, so not capped
at 255 bytes like an object field key) and an explicit `uint32` count
instead of a zero-length sentinel:

```
entry := key(TAG_STR) tag(u8) value(tag-dependent)
map := count(u32) entry{count}
```

### Array encoding (`TAG_ARR`)

Used for ordered lists (`doc.objects`, `obj.tags`, `obj.children`,
`ch.keyframes`, etc.):

```
element := tag(u8) value(tag-dependent)
array := count(u32) element{count}
```

---

## Field key names and document layout

`McbWriter.cpp`'s `writeDocument()` writes the document's top-level fields
in this order (mirrored exactly by `McbReader.cpp`'s `readDocument()`, which
does not require this order — see "Forward compatibility" below):

```
version, model, unit, coordinateSystem, rotationUnits, eulerOrder, defaultCamera,
library, imports,
meta, metadata, includes, includedDefs, includedMaterials, includedTextures, includedEmbeds,
environment, lights, cameras,
textures, svgTextures, embeds, scripts, sounds, musicTracks, triggers,
sceneStates, materials, definitions, objects, actions
```

Each nested type (`Mc3Object`, `Mc3Material`, `Mc3Light`, `Mc3Camera`,
`Mc3Transform`, `Mc3Primitive`, `Mc3Extrude`, `Mc3CrossSection`,
`Mc3ExtrudePath`, `Mc3Deform`, `Mc3CsgOperation`, `Mc3Keyframe`,
`Mc3Channel`, `Mc3Action`, `Mc3SceneState`/`Mc3ObjectOverride`,
`Mc3Trigger`/`Mc3TriggerStep`, `Mc3Script`, `Mc3Sound`, `Mc3Music`,
`Mc3EmbedGltf`, `Mc3SvgTexture`, `Mc3Texture`, `Mc3Environment`/`Mc3Fog`,
`Mc3AssetMetadata` (R111, `Mc3Object::assetMetadata`), `Mc3LibraryInfo`
(R110, root `library`), `Mc3Import` (R101, root `imports` array)) is
its own `TAG_OBJ` with its own field key names — see the paired
`write*()`/`read*()` function for each type in `McbWriter.cpp`/
`McbReader.cpp` for the authoritative field list; they are kept in lockstep
by construction (same file, adjacent functions, same key-name string
literals on both sides).

**Only non-default fields are written** (`wIfStr`/`wIfF32`/`wIfI32`/
`wIfBool`/`wIfVec3`/`wIfVec4`, vs. the unconditional `wField*`) — this is
what makes MCB smaller than the equivalent XML for a typical scene where
most fields sit at their default value. A field entirely absent from the
byte stream is indistinguishable on read from "reader doesn't recognize
this key"; both cases fall through to the object's default-constructed
value.

---

## Forward compatibility (unknown-key skipping)

`McbReader.cpp`'s `skipValue()`/`skipObject()` recursively skip any key a
given reader function doesn't recognize, by tag — including a whole nested
`TAG_OBJ` "section" or a `TAG_ARR` of elements the reader has never heard
of. This is what lets an older `McbReader` open a file written by a newer
`McbWriter` that has added new fields or even a new top-level section,
without crashing or misparsing the rest of the document: it just silently
drops what it doesn't understand and keeps reading everything after it
correctly.

Empirically verified in `mcb_roundtrip_test.cpp`'s
`testUnknownKeySkipping()` (STAB-0132/0145): a hand-constructed byte stream
with an unrecognized scalar key, an unrecognized nested-object "future
section", and an unrecognized array in between two real fields round-trips
those two real fields correctly.

Adding a new *optional* key to an existing object is therefore a
**non-breaking** change and does not require bumping `MCB_VERSION`. Only a
change to how *existing* keys are interpreted, or a change to the header
layout itself, needs a version bump.

---

## Endianness

The wire format is little-endian by construction, not by assumption about
the host: `wU32()`/`rU32()` build/read the 32-bit value via explicit bit
shifts (`v & 0xFF`, `(v >> 8) & 0xFF`, …), never a raw multi-byte `memcpy`
of an integer. `wF32()`/`rF32()` do use `memcpy` — but only to reinterpret
a `float`'s bit pattern as a `uint32_t` (or back) *within the same host*,
which is safe on every real platform because a given machine's `float` and
`int` types always share the same byte order. The result: MCB files are
byte-for-byte identical regardless of which machine wrote them, and would
round-trip correctly even on a hypothetical big-endian host with the
*existing* code — no `static_assert` restricting the build to little-endian
hosts was added, since that would incorrectly reject a case the code
already handles correctly.

Empirically verified in `testEndiannessLittleEndian()` (STAB-0150): writes
`3.5f` (well-known IEEE-754 bit pattern `0x40600000`) and confirms its 4
wire bytes are `00 00 60 40`, independent of the host running the test.

---

## Determinism

Writing the same `Mc3Document` twice produces byte-identical output. Every
container in `Mc3Document`/`Mc3Object` is `std::map`/`std::set` (iteration
order = key-sorted) or `std::vector` (iteration order = insertion order) —
never `unordered_map`/`unordered_set` or anything keyed by pointer/address,
so there is no non-deterministic iteration order anywhere in the write
path. Verified in `testWriterDeterminism()` (STAB-0140).

---

## Format version history

- **Version 1** (current, `MCB_VERSION = 1`): initial format. All fields
  documented above were present from the start except `includes`/
  `includedDefs`/`includedMaterials`/`includedTextures`/`metadata`, which
  were added additively mid-effort (STAB-0131/0144) without a version bump,
  per the forward-compatibility rule above.

**Version migration policy (AUDIT-0038, decided 2026-07-11).** `McbReader::
loadFromBinary()` accepts any version in the range `MCB_MIN_SUPPORTED_
VERSION..MCB_VERSION` (`"MCB: unsupported version N"` outside that range).
Both constants are currently `1` — no version older than 1 has ever
existed, so there is nothing to migrate from yet. A version bump that adds
new *optional* keys needs no bump at all (the key-tagged format already
tolerates those, both directions, via `skipValue()`/missing-key defaults —
see `includes`/`includedDefs`/etc. above). A version bump that changes
wire-level *meaning* must register a chained-upgrade function in
`McbReader.cpp` (`mcbUpgrades()`), keyed by the version it upgrades FROM;
`loadFromBinary()` applies every registered upgrade whose source version
is `>=` the file's own version, in ascending order, so a `v1` file read by
a hypothetical `v3` reader runs the `v1→v2` upgrade then the `v2→v3`
upgrade in sequence. `MCB_MIN_SUPPORTED_VERSION` only needs to move if a
version is deliberately dropped from support (its upgrade function
removed). This mechanism is real but currently exercises no live upgrade
path — it exists so the next version bump has a place to plug into rather
than needing this decision re-litigated.
