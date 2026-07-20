cmake_minimum_required(VERSION 3.21)
project(Mcb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------------
# Mc3 library (from parent repo)
# When included from the root CMakeLists the target already exists; this
# guard adds it as a subdirectory only for standalone `mcb` builds so the
# Mc3 headers/include dirs are available.
# --------------------------------------------------------------------------
if(NOT TARGET Mc3)
    get_filename_component(_mc3_dir "${CMAKE_CURRENT_SOURCE_DIR}/../mc3" ABSOLUTE)
    add_subdirectory("${_mc3_dir}" mc3_dep)
endif()

# -----------------------------------------------------------------------------
# SYS-W11-05: opt-in genuine libFuzzer corpus-driven harness (mcb_libfuzzer)
# over McbReader, mirroring mc3/CMakeLists.txt's MESHCRAFT_FUZZ block (see
# there for full rationale -- same option, same reasoning, applied here to
# the Mcb static library instead of Mc3). This option() call is a no-op when
# mcb is pulled in via the root project or when mc3's own CMakeLists.txt
# already registered it above (option() never overrides an existing cache
# value); it exists here too purely so a from-scratch standalone `mcb`
# configure is self-contained and doesn't depend on reading mc3's file to
# know this flag exists. add_link_options() here is directory-scoped to
# this file (Mcb + mcb's own test executables below) -- it does NOT
# reach back into mc3's targets, which mc3/CMakeLists.txt already handled
# for itself above.
# -----------------------------------------------------------------------------
option(MESHCRAFT_FUZZ
       "Build the opt-in libFuzzer corpus-driven harness (mcb_libfuzzer) over McbReader. Requires Clang."
       OFF)
if(MESHCRAFT_FUZZ AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "MESHCRAFT_FUZZ requires Clang (libFuzzer); "
                    "CMAKE_CXX_COMPILER_ID is '${CMAKE_CXX_COMPILER_ID}' -- "
                    "skipping mcb_libfuzzer. Reconfigure with "
                    "-DCMAKE_CXX_COMPILER=clang++ to build it.")
    set(MESHCRAFT_FUZZ OFF)
endif()
if(MESHCRAFT_FUZZ)
    set(MESHCRAFT_FUZZ_COMPILE_OPTIONS -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(Mcb STATIC
    src/McbWriter.cpp
    src/McbReader.cpp
)

target_include_directories(Mcb
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(Mcb PUBLIC Mc3)

# SYS-W14-25 (2026-07-20): zlib (deflate) support for MCB's reserved
# compression flag (McbFormat.hpp's MCB_FLAG_COMPRESSED) -- optional, not
# REQUIRED, matching the same "guarded, graceful absence" pattern the root
# CMakeLists.txt already uses for SQLite3/OpenSSL/LibXml2: an environment or
# cross-compile toolchain without a zlib dev package still builds Mcb fine,
# it just can't write with compress=true, and correctly refuses (rather than
# misparses) a compressed file it's asked to read. MESHCRAFT_HAS_ZLIB is
# PUBLIC so any translation unit that sees McbWriter.hpp/McbReader.cpp's
# #ifdef-guarded code paths agrees on whether zlib support is compiled in.
find_package(ZLIB)
if(ZLIB_FOUND)
    target_link_libraries(Mcb PUBLIC ZLIB::ZLIB)
    target_compile_definitions(Mcb PUBLIC MESHCRAFT_HAS_ZLIB=1)
    message(STATUS "zlib found: MCB compression (SYS-W14-25) enabled")
else()
    message(STATUS "zlib not found -- MCB compression (SYS-W14-25) unavailable at runtime")
endif()

# AUD-054/AUD-055: Mcb (the binary reader/writer -- untrusted-input parsing
# surface) previously had no warning flags at all.
target_compile_options(Mcb PRIVATE -Wall -Wextra)
if(COMMAND meshcraft_apply_sanitize)
    meshcraft_apply_sanitize(Mcb)
endif()

# SYS-W11-05: see the MESHCRAFT_FUZZ block above -- instruments Mcb's own
# object code for libFuzzer coverage-guided fuzzing when opted in.
if(MESHCRAFT_FUZZ)
    target_compile_options(Mcb PRIVATE ${MESHCRAFT_FUZZ_COMPILE_OPTIONS})
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(mcb_roundtrip_test
        test/mcb_roundtrip_test.cpp
    )
    target_link_libraries(mcb_roundtrip_test PRIVATE Mcb)
    add_test(NAME mcb_roundtrip COMMAND mcb_roundtrip_test)
    set_tests_properties(mcb_roundtrip PROPERTIES TIMEOUT 30 LABELS "format")

    # SYS-W1-04 / SYS-W6-02: MCB corruption coverage beyond mcb_roundtrip_test
    # -- a byte-offset truncation sweep over a rich multi-section document,
    # and genuinely random (not just structured all-zeros) byte-noise fuzzing.
    add_executable(mcb_corruption_test
        test/mcb_corruption_test.cpp
    )
    target_link_libraries(mcb_corruption_test PRIVATE Mcb)
    add_test(NAME mcb_corruption COMMAND mcb_corruption_test)
    set_tests_properties(mcb_corruption PROPERTIES TIMEOUT 30 LABELS "format")

    # AUD-066: a claimed collection count under kMcbMaxCollectionCount (10M)
    # used to be .reserve()'d in full up front regardless of whether the
    # stream actually contained that many elements -- a tiny corrupted/
    # malicious file claiming e.g. 9,000,000 lights could force a near-
    # gigabyte transient allocation. Proves the fix (reserveHint()) via real
    # peak-RSS measurement, not just code inspection.
    add_executable(mcb_reserve_bomb_test
        test/reserve_bomb_test.cpp
    )
    target_link_libraries(mcb_reserve_bomb_test PRIVATE Mcb)
    add_test(NAME mcb_reserve_bomb COMMAND mcb_reserve_bomb_test)
    set_tests_properties(mcb_reserve_bomb PROPERTIES TIMEOUT 30 LABELS "format")

    # 2026-07-20 audit F1: segments/sides/subdivisionsX/subdivisionsZ used to
    # be read via a bare rI32() with no clamp at all, unlike the XML/JSON
    # parsers' kMaxTessellation=4096 -- a hostile/corrupted .mcb file could
    # claim an arbitrarily large tessellation field, driving a downstream
    # geometry-building allocation many orders of magnitude beyond any
    # legitimate scene's needs.
    add_executable(mcb_tessellation_clamp_test
        test/tessellation_clamp_test.cpp
    )
    target_link_libraries(mcb_tessellation_clamp_test PRIVATE Mcb)
    add_test(NAME mcb_tessellation_clamp COMMAND mcb_tessellation_clamp_test)
    set_tests_properties(mcb_tessellation_clamp PROPERTIES TIMEOUT 15 LABELS "format")

    # 2026-07-20 audit F17: Mc3XmlParser.cpp sorts a channel's keyframes by
    # time after parsing; McbReader.cpp never did, and Mc3Animation.cpp's
    # evaluateChannel() assumes sorted input (std::upper_bound).
    add_executable(mcb_keyframe_sort_test test/keyframe_sort_test.cpp)
    target_link_libraries(mcb_keyframe_sort_test PRIVATE Mcb)
    add_test(NAME mcb_keyframe_sort COMMAND mcb_keyframe_sort_test)
    set_tests_properties(mcb_keyframe_sort PROPERTIES TIMEOUT 15 LABELS "format")

    # 2026-07-20 audit F3: McbReader/McbWriter had no Mc3LoadPolicy
    # integration at all -- resource-path confinement (meshSource/texture
    # uri/SVG/embed/sound/music src) was structurally impossible for MCB,
    # unlike the XML/JSON parsers. Mirrors mc3/test/load_policy_test.cpp's
    # own coverage, adapted for MCB.
    add_executable(mcb_load_policy_test
        test/load_policy_test.cpp
    )
    target_link_libraries(mcb_load_policy_test PRIVATE Mcb)
    add_test(NAME mcb_load_policy COMMAND mcb_load_policy_test)
    set_tests_properties(mcb_load_policy PROPERTIES TIMEOUT 15 LABELS "format")

    # AUD-071: Mc3SceneState's `overrides` and Mc3Trigger's `steps` were the
    # only two known keys that skipped expectTag() (AUD-015's own
    # established policy) and silently dropped the field on a tag mismatch
    # instead of rejecting the file with a clear error.
    add_executable(mcb_tag_mismatch_rejection_test
        test/tag_mismatch_rejection_test.cpp
    )
    target_link_libraries(mcb_tag_mismatch_rejection_test PRIVATE Mcb)
    add_test(NAME mcb_tag_mismatch_rejection COMMAND mcb_tag_mismatch_rejection_test)
    set_tests_properties(mcb_tag_mismatch_rejection PROPERTIES TIMEOUT 30 LABELS "format")

    # SYS-W1-01: Mc3Validation diagnostics for the MCB reader -- proves the
    # structured warning/error surface itself is populated (not just that
    # clamped/rejected VALUES are correct, already covered by
    # mcb_roundtrip's AUD-015/AUD-017 tests).
    add_executable(mcb_validation_test
        test/mcb_validation_test.cpp
    )
    target_link_libraries(mcb_validation_test PRIVATE Mcb)
    add_test(NAME mcb_validation COMMAND mcb_validation_test)
    set_tests_properties(mcb_validation PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W11-05: property-based (randomized, seeded) MCB round-trip test --
    # the McbReader/McbWriter differential half of this task, sharing the
    # same random-document generator mc3_random_roundtrip_test (mc3/test/)
    # uses for the XML side. See mcb_random_roundtrip_test.cpp's header
    # comment for the rationale.
    add_executable(mcb_random_roundtrip_test
        test/mcb_random_roundtrip_test.cpp
    )
    target_include_directories(mcb_random_roundtrip_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../mc3/test"
    )
    target_link_libraries(mcb_random_roundtrip_test PRIVATE Mcb)
    add_test(NAME mcb_random_roundtrip COMMAND mcb_random_roundtrip_test)
    set_tests_properties(mcb_random_roundtrip PROPERTIES TIMEOUT 30 LABELS "format")

    # 2026-07-20 audit finding #1 (P0): McbReader.cpp had no document-wide
    # aggregate budget, unlike Mc3XmlParser.cpp/Mc3JsonParser.cpp's own
    # DocumentBudget -- a ~20MB crafted file could force construction of
    # millions of heap objects via real (not truncated) data. New
    # DocumentBudget mirrors the XML/JSON parsers' constants exactly.
    add_executable(mcb_document_budget_test
        test/document_budget_test.cpp
    )
    target_link_libraries(mcb_document_budget_test PRIVATE Mcb)
    add_test(NAME mcb_document_budget COMMAND mcb_document_budget_test)
    set_tests_properties(mcb_document_budget PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W14-25 (2026-07-20): real zlib compression for MCB_FLAG_COMPRESSED
    # -- round-trip correctness, actual size reduction, and the zip-bomb
    # defenses (claimed uncompressed/compressed size sanity ceilings,
    # truncated/corrupted payload).
    add_executable(mcb_compression_test
        test/compression_test.cpp
    )
    target_link_libraries(mcb_compression_test PRIVATE Mcb)
    add_test(NAME mcb_compression COMMAND mcb_compression_test)
    set_tests_properties(mcb_compression PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W11-05: opt-in libFuzzer harness (see the MESHCRAFT_FUZZ block near
    # the top of this file). Deliberately NOT registered as a ctest test --
    # a corpus-driven deep-fuzzing target for longer, occasional/manual or
    # CI-scheduled runs, not a fast per-commit gate; only builds under Clang.
    if(MESHCRAFT_FUZZ)
        add_executable(mcb_libfuzzer test/fuzz/mcb_libfuzzer.cpp)
        target_link_libraries(mcb_libfuzzer PRIVATE Mcb)
        target_compile_options(mcb_libfuzzer PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g)
        target_link_options(mcb_libfuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()
endif()
