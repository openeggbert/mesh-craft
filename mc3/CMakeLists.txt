cmake_minimum_required(VERSION 3.21)
project(Mc3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# -----------------------------------------------------------------------------
# tinyxml2 — XML parser for mc3.xml I/O
# -----------------------------------------------------------------------------

include(FetchContent)

if(NOT TARGET tinyxml2 AND NOT TARGET tinyxml2::tinyxml2)
    FetchContent_Declare(
        tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG        10.0.0
    )
    set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(tinyxml2)
endif()
# Alias so Mc3 can always link tinyxml2::tinyxml2 regardless of where the target came from
if(TARGET tinyxml2 AND NOT TARGET tinyxml2::tinyxml2)
    add_library(tinyxml2::tinyxml2 ALIAS tinyxml2)
endif()

# -----------------------------------------------------------------------------
# nlohmann/json — R109: semantic mc3.json reader/writer (Mc3JsonWriter /
# Mc3JsonParser). Header-only, MIT-licensed; fetched the same way tinyxml2
# is above so mc3/ stays independently buildable/standalone.
# -----------------------------------------------------------------------------

if(NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# -----------------------------------------------------------------------------
# SYS-W11-04: opt-in genuine libFuzzer corpus-driven harnesses over the XML
# and JSON MC3 parsers, complementing the always-on, fast, seeded-mutation
# mc3_xml_mutation_fuzz_test CTest target below. OFF by default; requires
# Clang (libFuzzer/-fsanitize=fuzzer is a Clang/LLVM feature -- this
# environment has Debian clang 19.1.7, verified working). Self-contained to
# this file so enabling it never reaches ../cna or the rest of the MeshCraft
# tree, whether mc3/ is configured standalone (as CI's per-component matrix
# already does) or as a root add_subdirectory(mc3): add_link_options() here
# is directory-scoped and only applies to targets defined in THIS file.
#
#   cmake -S mc3 -B <dir> -DCMAKE_CXX_COMPILER=clang++ -DMESHCRAFT_FUZZ=ON
#   cmake --build <dir> --target mc3_xml_libfuzzer mc3_json_libfuzzer
#   <dir>/mc3_xml_libfuzzer test/fuzz/corpus/xml -max_total_time=60 -seed=1
# -----------------------------------------------------------------------------
option(MESHCRAFT_FUZZ
       "Build opt-in libFuzzer corpus-driven MC3/MCB/GLB parser harnesses. Requires Clang."
       OFF)
if(MESHCRAFT_FUZZ AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "MESHCRAFT_FUZZ requires Clang (libFuzzer); "
                    "CMAKE_CXX_COMPILER_ID is '${CMAKE_CXX_COMPILER_ID}' -- "
                    "skipping mc3_xml_libfuzzer. Reconfigure with "
                    "-DCMAKE_CXX_COMPILER=clang++ to build it.")
    set(MESHCRAFT_FUZZ OFF)
endif()
if(MESHCRAFT_FUZZ)
    # fuzzer-no-link: instruments Mc3's own object code with coverage
    # counters + ASan/UBSan (so libFuzzer's coverage-guided mutation
    # actually sees INSIDE the parser, not just the harness translation
    # unit) without pulling in libFuzzer's own main -- that only happens at
    # the harness executable's link step, via the full -fsanitize=fuzzer
    # below, mirroring how MESHCRAFT_SANITIZE separates compile-time
    # instrumentation from the one global link-time runtime pull-in (see
    # that option's comment above for the same "undefined reference to
    # __asan_*" pitfall this avoids). tinyxml2 (fetched dependency) is
    # deliberately left uninstrumented, same rationale as MESHCRAFT_SANITIZE
    # excluding vendored/sibling code.
    set(MESHCRAFT_FUZZ_COMPILE_OPTIONS -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# -----------------------------------------------------------------------------
# Mc3 — static library (MC3 model source format + XML I/O)
# -----------------------------------------------------------------------------

add_library(Mc3 STATIC
    src/Mc3Animation.cpp
    src/Mc3Document.cpp
    src/Mc3Object.cpp
    src/Mc3Material.cpp
    src/Mc3Texture.cpp
    src/Mc3Primitive.cpp
    src/Mc3CsgOperation.cpp
    src/Mc3XmlParser.cpp
    src/Mc3XmlWriter.cpp
    src/Mc3JsonWriter.cpp
    src/Mc3JsonParser.cpp
    src/Mc3Sha256.cpp
    src/Mc3ImportResolver.cpp
)

target_include_directories(Mc3
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Mc3 PRIVATE tinyxml2::tinyxml2)
# nlohmann/json is header-only and used only by Mc3 implementation files.  Do
# not put its build target in a static library's exported link interface;
# consumers do not need it once libMc3 has been built.
get_target_property(_mc3_nlohmann_json_include_dirs
                    nlohmann_json::nlohmann_json
                    INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(Mc3 SYSTEM PRIVATE ${_mc3_nlohmann_json_include_dirs})

if(MSVC)
    target_compile_options(Mc3 PRIVATE /W4)
else()
    target_compile_options(Mc3 PRIVATE -Wall -Wextra)
endif()

# SYS-W11-05: Mc3 is shipped as a genuinely relocatable static package.  The
# parser's tinyxml2 dependency is normally supplied by the current build (by
# SharpRuntime in the full editor, or by FetchContent in a standalone Mc3
# build).  When that dependency is a concrete local target, install it beside
# Mc3 so an external consumer does not need the parent editor checkout.  The
# only public header it exposes is tinyxml2.h, so replacing its build-tree
# include path with a build/install pair is safe and keeps the export free of
# an absolute sibling-repository path.
set(MC3_BUNDLE_TINYXML2 OFF)
if(TARGET tinyxml2)
    set(MC3_BUNDLE_TINYXML2 ON)
    get_target_property(_mc3_tinyxml2_source_dir tinyxml2 SOURCE_DIR)
    get_target_property(_mc3_tinyxml2_sources tinyxml2 SOURCES)
    set(_mc3_tinyxml2_header "")
    foreach(_mc3_tinyxml2_source IN LISTS _mc3_tinyxml2_sources)
        if(IS_ABSOLUTE "${_mc3_tinyxml2_source}")
            set(_mc3_tinyxml2_candidate "${_mc3_tinyxml2_source}")
        else()
            set(_mc3_tinyxml2_candidate
                "${_mc3_tinyxml2_source_dir}/${_mc3_tinyxml2_source}")
        endif()
        if(_mc3_tinyxml2_candidate MATCHES "tinyxml2\\.cpp$")
            get_filename_component(_mc3_tinyxml2_include_dir
                                   "${_mc3_tinyxml2_candidate}" DIRECTORY)
            set(_mc3_tinyxml2_header
                "${_mc3_tinyxml2_include_dir}/tinyxml2.h")
            break()
        endif()
    endforeach()
    if(NOT EXISTS "${_mc3_tinyxml2_header}")
        message(FATAL_ERROR
            "Mc3 package installation could not locate tinyxml2.h for target tinyxml2")
    endif()

    set_property(TARGET tinyxml2 PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        "$<BUILD_INTERFACE:${_mc3_tinyxml2_include_dir}>;$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
    install(FILES "${_mc3_tinyxml2_header}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            COMPONENT release)
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT release)

set(_mc3_install_targets Mc3)
if(MC3_BUNDLE_TINYXML2)
    list(APPEND _mc3_install_targets tinyxml2)
endif()
install(TARGETS ${_mc3_install_targets}
        EXPORT Mc3Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT release
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT release
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT release
        )

set(MC3_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/Mc3")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Mc3Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Mc3Config.cmake"
    INSTALL_DESTINATION "${MC3_CMAKE_INSTALL_DIR}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/Mc3ConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion)
install(EXPORT Mc3Targets
        FILE Mc3Targets.cmake
        NAMESPACE MeshCraft::
        DESTINATION "${MC3_CMAKE_INSTALL_DIR}"
        COMPONENT release)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/Mc3Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/Mc3ConfigVersion.cmake"
        DESTINATION "${MC3_CMAKE_INSTALL_DIR}"
        COMPONENT release)

# AUD-055: meshcraft_apply_sanitize() is defined by the root CMakeLists.txt
# and only visible when this file is processed via its add_subdirectory()
# (guarded, since mc3/ can also be configured standalone).
if(COMMAND meshcraft_apply_sanitize)
    meshcraft_apply_sanitize(Mc3)
endif()

# SYS-W11-04: see the MESHCRAFT_FUZZ block above -- instruments Mc3's own
# object code for libFuzzer coverage-guided fuzzing when opted in.
if(MESHCRAFT_FUZZ)
    target_compile_options(Mc3 PRIVATE ${MESHCRAFT_FUZZ_COMPILE_OPTIONS})
endif()

# -----------------------------------------------------------------------------
# Round-trip unit tests
# -----------------------------------------------------------------------------

if(BUILD_TESTING)
    # When mc3 is built standalone (top-level project) nobody has called
    # enable_testing() yet, so add_test() would silently register nothing.
    # As a subdirectory of the root MeshCraft project, the root's include(CTest)
    # already enabled testing.
    if(PROJECT_IS_TOP_LEVEL)
        enable_testing()
    endif()

    add_executable(mc3_roundtrip_test test/roundtrip_test.cpp)
    target_link_libraries(mc3_roundtrip_test PRIVATE Mc3)

    # Pass features.mc3.xml path so the test can exercise the full parser
    # CMAKE_SOURCE_DIR is the top-level project root (may differ from mc3/ source dir)
    set(MC3_FEATURES_XML "${CMAKE_SOURCE_DIR}/test/features.mc3.xml")
    if(NOT EXISTS "${MC3_FEATURES_XML}")
        set(MC3_FEATURES_XML "")
    endif()

    add_test(NAME mc3_roundtrip
             COMMAND mc3_roundtrip_test ${MC3_FEATURES_XML})
    set_tests_properties(mc3_roundtrip PROPERTIES TIMEOUT 15 LABELS "format")

    # R109: mc3.json semantic JSON round-trip (Mc3JsonWriter/Mc3JsonParser) --
    # both handwritten documents covering every AST feature and, when
    # available, the same features.mc3.xml fixture the XML round-trip test
    # above uses (load XML -> Mc3Document -> write JSON -> read JSON ->
    # compare against the XML-loaded document).
    add_executable(mc3_json_roundtrip_test test/json_roundtrip_test.cpp)
    target_link_libraries(mc3_json_roundtrip_test PRIVATE Mc3)
    add_test(NAME mc3_json_roundtrip
             COMMAND mc3_json_roundtrip_test ${MC3_FEATURES_XML})
    set_tests_properties(mc3_json_roundtrip PROPERTIES TIMEOUT 15 LABELS "format")

    # R110: .mc3lib.xml/.mc3lib.json reusable-definition-library file type --
    # sha256Hex() correctness, saveToLibraryFile{,Json} requiring `library`,
    # namespace/version/contentHash round-tripping through both surfaces, and
    # computeLibraryContentHash() stability/sensitivity/no-self-reference.
    add_executable(mc3lib_test test/mc3lib_test.cpp)
    target_link_libraries(mc3lib_test PRIVATE Mc3)
    add_test(NAME mc3lib COMMAND mc3lib_test)
    set_tests_properties(mc3lib PROPERTIES TIMEOUT 15 LABELS "format")

    # R111: structured asset metadata (Mc3Object::assetMetadata) round-trips
    # through both XML and JSON, and stays genuinely absent (not defaulted)
    # on a definition that never set it.
    add_executable(mc3_asset_metadata_test test/asset_metadata_test.cpp)
    target_link_libraries(mc3_asset_metadata_test PRIVATE Mc3)
    add_test(NAME mc3_asset_metadata COMMAND mc3_asset_metadata_test)
    set_tests_properties(mc3_asset_metadata PROPERTIES TIMEOUT 15 LABELS "format")

    # R101: Mc3ImportResolver -- basic resolution, missing-dependency error,
    # content-hash match/mismatch, cycle detection, and nested-import
    # definitions NOT being merged into the top-level caller's map.
    add_executable(mc3_import_resolver_test test/import_resolver_test.cpp)
    target_link_libraries(mc3_import_resolver_test PRIVATE Mc3)
    add_test(NAME mc3_import_resolver COMMAND mc3_import_resolver_test)
    set_tests_properties(mc3_import_resolver PROPERTIES TIMEOUT 15 LABELS "format")

    # Hostile-numeric-input test: NaN/Inf/out-of-range attributes must not
    # survive parsing into the model (finiteOr sanitization).
    add_executable(mc3_finite_input_test test/finite_input_test.cpp)
    target_link_libraries(mc3_finite_input_test PRIVATE Mc3)
    add_test(NAME mc3_finite_input COMMAND mc3_finite_input_test)
    set_tests_properties(mc3_finite_input PROPERTIES TIMEOUT 15 LABELS "format")

    # Input-budget test: tessellation counts clamped to a safe upper bound so a
    # hostile <sphere segments="100000000"/> can't drive unbounded allocation.
    add_executable(mc3_input_budget_test test/input_budget_test.cpp)
    target_link_libraries(mc3_input_budget_test PRIVATE Mc3)
    add_test(NAME mc3_input_budget COMMAND mc3_input_budget_test)
    set_tests_properties(mc3_input_budget PROPERTIES TIMEOUT 15 LABELS "format")

    # AUD-064 follow-up: the .mc3.json load path had none of the above
    # per-field tessellation clamps at all (raw get<int>(), no bound) --
    # this is the JSON-path equivalent of mc3_input_budget above.
    add_executable(mc3_json_input_budget_test test/json_input_budget_test.cpp)
    target_link_libraries(mc3_json_input_budget_test PRIVATE Mc3)
    add_test(NAME mc3_json_input_budget COMMAND mc3_json_input_budget_test)
    set_tests_properties(mc3_json_input_budget PROPERTIES TIMEOUT 15 LABELS "format")

    # F17 (2026-07-20 audit): Mc3XmlParser.cpp sorts a channel's keyframes by
    # time after parsing; the JSON path never did, and Mc3Animation.cpp's
    # evaluateChannel() assumes sorted input (std::upper_bound).
    add_executable(mc3_keyframe_sort_test test/keyframe_sort_test.cpp)
    target_link_libraries(mc3_keyframe_sort_test PRIVATE Mc3)
    add_test(NAME mc3_keyframe_sort COMMAND mc3_keyframe_sort_test)
    set_tests_properties(mc3_keyframe_sort PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W1-02: documented per-field numeric ranges (camera/material/
    # geometry/environment/animation/transform) -- a value that's finite but
    # outside its field's documented domain (fov=600, roughness=-3,
    # radius=-5, ...) must be clamped, not passed through as garbage.
    add_executable(mc3_numeric_range_test test/numeric_range_test.cpp)
    target_link_libraries(mc3_numeric_range_test PRIVATE Mc3)
    add_test(NAME mc3_numeric_range COMMAND mc3_numeric_range_test)
    set_tests_properties(mc3_numeric_range PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W1-03: document-complexity budgets beyond AUD-059's total object
    # count + total tessellation weight -- materials/textures/embeds/actions/
    # channels/keyframes counts, children-per-node breadth.
    add_executable(mc3_document_budget_test test/document_budget_test.cpp)
    target_link_libraries(mc3_document_budget_test PRIVATE Mc3)
    add_test(NAME mc3_document_budget COMMAND mc3_document_budget_test)
    set_tests_properties(mc3_document_budget PROPERTIES TIMEOUT 30 LABELS "format")

    # 2026-07-20 audit F4: the .mc3.json load path had none of
    # document_budget_test.cpp's document-wide running-total budgets at all
    # (materials/textures/embeds/actions/channels/keyframes/definitions/
    # total-object counts, max document bytes) -- this is the JSON-path
    # equivalent, mirroring that file's own coverage.
    add_executable(mc3_json_document_budget_test test/json_document_budget_test.cpp)
    target_link_libraries(mc3_json_document_budget_test PRIVATE Mc3)
    add_test(NAME mc3_json_document_budget COMMAND mc3_json_document_budget_test)
    set_tests_properties(mc3_json_document_budget PROPERTIES TIMEOUT 30 LABELS "format")

    # 2026-07-20 audit F8: editing an <include>-sourced material/texture/
    # definition/embed in the GUI editor and saving used to silently drop
    # the edit (the writer skips any id still marked in doc.includedX).
    # Proves the fix's underlying mechanism -- erasing an id from includedX
    # at edit time makes it survive a save+reload round trip -- entirely
    # through the public mc3/ document API (no ImGui/CNA dependency).
    add_executable(mc3_included_resource_edit_test test/included_resource_edit_test.cpp)
    target_link_libraries(mc3_included_resource_edit_test PRIVATE Mc3)
    add_test(NAME mc3_included_resource_edit COMMAND mc3_included_resource_edit_test)
    set_tests_properties(mc3_included_resource_edit PROPERTIES TIMEOUT 15 LABELS "format")

    # Load-policy test: untrusted content (AI/import) must not process <include>;
    # include-confinement rejects path traversal.
    add_executable(mc3_load_policy_test test/load_policy_test.cpp)
    target_link_libraries(mc3_load_policy_test PRIVATE Mc3)
    add_test(NAME mc3_load_policy COMMAND mc3_load_policy_test)
    set_tests_properties(mc3_load_policy PROPERTIES TIMEOUT 15 LABELS "format")

    # AUD-068: the .mc3.json load path's own Mc3LoadPolicy coverage --
    # mirrors mc3_load_policy above's resource-confinement cases (the
    # <include>-specific cases don't apply; see the test file's own header).
    add_executable(mc3_json_load_policy_test test/json_load_policy_test.cpp)
    target_link_libraries(mc3_json_load_policy_test PRIVATE Mc3)
    add_test(NAME mc3_json_load_policy COMMAND mc3_json_load_policy_test)
    set_tests_properties(mc3_json_load_policy PROPERTIES TIMEOUT 15 LABELS "format")

    # AUD-070: a "]]>" inside SVG inlineContent/script source used to break
    # out of its CDATA section on save, letting whatever followed it in the
    # string be parsed as real XML on the next load.
    add_executable(mc3_cdata_injection_test test/cdata_injection_test.cpp)
    target_link_libraries(mc3_cdata_injection_test PRIVATE Mc3)
    add_test(NAME mc3_cdata_injection COMMAND mc3_cdata_injection_test)
    set_tests_properties(mc3_cdata_injection PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W1-04: include bombs -- depth (bounded by Mc3LoadPolicy::
    # maxIncludeDepth) and fan-out (bounded by the new DocumentBudget::
    # chargeInclude); cycles are already covered by mc3_roundtrip's
    # include_cycle_a/b.mc3.xml fixture, not duplicated here.
    add_executable(mc3_include_bomb_test test/include_bomb_test.cpp)
    target_link_libraries(mc3_include_bomb_test PRIVATE Mc3)
    add_test(NAME mc3_include_bomb COMMAND mc3_include_bomb_test)
    set_tests_properties(mc3_include_bomb PROPERTIES TIMEOUT 30 LABELS "format")

    # SYS-W1-04: oversized inline <embed> base64 body -- must be rejected at
    # parse time by a sanity-limit ceiling, not held unbounded in memory.
    add_executable(mc3_oversized_base64_test test/oversized_base64_test.cpp)
    target_link_libraries(mc3_oversized_base64_test PRIVATE Mc3)
    add_test(NAME mc3_oversized_base64 COMMAND mc3_oversized_base64_test)
    set_tests_properties(mc3_oversized_base64 PROPERTIES TIMEOUT 30 LABELS "format")

    # SYS-W1-01: Mc3Validation diagnostics -- proves the structured
    # warning/error surface itself is populated (source path, object/field
    # identity, message, suggested repair), not just that clamped VALUES are
    # correct (already covered by mc3_input_budget above).
    add_executable(mc3_validation_test test/validation_test.cpp)
    target_link_libraries(mc3_validation_test PRIVATE Mc3)
    add_test(NAME mc3_validation COMMAND mc3_validation_test)
    set_tests_properties(mc3_validation PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W1-01: Mc3Document::validate() -- the pre-render/pre-export/save
    # integration points, for documents that never went through a parser
    # load at all (built programmatically, or mutated in place afterward).
    add_executable(mc3_document_validate_test test/document_validate_test.cpp)
    target_link_libraries(mc3_document_validate_test PRIVATE Mc3)
    add_test(NAME mc3_document_validate COMMAND mc3_document_validate_test)
    set_tests_properties(mc3_document_validate PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W1-04: duplicate object ids -- proves the current (safe,
    # non-crashing, non-data-losing) library-level behavior; see the test
    # file's header comment for why this is a documented judgment call and
    # not a policy fix.
    add_executable(mc3_duplicate_ids_test test/duplicate_ids_test.cpp)
    target_link_libraries(mc3_duplicate_ids_test PRIVATE Mc3)
    add_test(NAME mc3_duplicate_ids COMMAND mc3_duplicate_ids_test)
    set_tests_properties(mc3_duplicate_ids PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W1-04: invalid UTF-8 byte sequences in attribute values / text
    # content -- proves the current safe pass-through behavior (no crash, no
    # OOB read at EOF, bytes preserved verbatim).
    add_executable(mc3_invalid_utf8_test test/invalid_utf8_test.cpp)
    target_link_libraries(mc3_invalid_utf8_test PRIVATE Mc3)
    add_test(NAME mc3_invalid_utf8 COMMAND mc3_invalid_utf8_test)
    set_tests_properties(mc3_invalid_utf8 PROPERTIES TIMEOUT 15 LABELS "format")

    # SYS-W9-02: crash-recovery mechanism (mtime comparison + Mc3Document
    # round-trip through the ".autosave" path convention that
    # MeshCraftApplication::checkForNewerAutosave()/recoverFromAutosave() rely
    # on -- see src/MeshCraft/MeshCraftApplication_FileOps.cpp).
    add_executable(mc3_autosave_recovery_test test/autosave_recovery_test.cpp)
    target_link_libraries(mc3_autosave_recovery_test PRIVATE Mc3)
    add_test(NAME mc3_autosave_recovery COMMAND mc3_autosave_recovery_test)
    set_tests_properties(mc3_autosave_recovery PROPERTIES TIMEOUT 15 LABELS "format")

    # Seeded property-based round-trip test --
    # generates many random documents (RandomMc3DocumentGenerator.hpp) and
    # asserts XML round-trip fidelity, complementing mc3_roundtrip's fixed-
    # fixture coverage with random parameter-value/feature combinations.
    add_executable(mc3_random_roundtrip_test test/random_roundtrip_test.cpp)
    target_link_libraries(mc3_random_roundtrip_test PRIVATE Mc3)
    add_test(NAME mc3_random_roundtrip COMMAND mc3_random_roundtrip_test)
    set_tests_properties(mc3_random_roundtrip PROPERTIES TIMEOUT 30 LABELS "format")

    # SYS-W11-04: seeded mutation/random-byte fuzz harness over
    # Mc3XmlParser (via Mc3Document::loadFromString) -- the primary gap this
    # task exists to close (MCB already had mcb_corruption_test's byte-fuzz;
    # the XML parser had none). See the file's header comment for the two
    # techniques used and how this relates to the opt-in libFuzzer target
    # below.
    add_executable(mc3_xml_mutation_fuzz_test test/xml_mutation_fuzz_test.cpp)
    target_link_libraries(mc3_xml_mutation_fuzz_test PRIVATE Mc3)
    add_test(NAME mc3_xml_mutation_fuzz COMMAND mc3_xml_mutation_fuzz_test)
    set_tests_properties(mc3_xml_mutation_fuzz PROPERTIES TIMEOUT 30 LABELS "format")

    # SYS-W11-04: opt-in libFuzzer harnesses (see the MESHCRAFT_FUZZ block near
    # the top of this file). Deliberately NOT registered as a ctest test --
    # it's a corpus-driven deep-fuzzing target meant for longer, occasional/
    # manual or CI-scheduled runs (`-max_total_time=...`), not a fast
    # per-commit gate; and it only builds under Clang, whereas ctest runs
    # here are expected to work under any configured compiler.
    if(MESHCRAFT_FUZZ)
        add_executable(mc3_xml_libfuzzer test/fuzz/mc3_xml_libfuzzer.cpp)
        target_link_libraries(mc3_xml_libfuzzer PRIVATE Mc3)
        target_compile_options(mc3_xml_libfuzzer PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g)
        target_link_options(mc3_xml_libfuzzer PRIVATE -fsanitize=fuzzer,address,undefined)

        add_executable(mc3_json_libfuzzer test/fuzz/mc3_json_libfuzzer.cpp)
        target_link_libraries(mc3_json_libfuzzer PRIVATE Mc3)
        target_compile_options(mc3_json_libfuzzer PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g)
        target_link_options(mc3_json_libfuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()

    # editor_commands_test pulls in MeshCraft/EditorAlgorithms.hpp from the
    # MeshCraft editor's public include dir, which lives at the repo root
    # (include/) — outside the mc3/ subtree. Only build it when those
    # headers are available (i.e. as part of the root MeshCraft project)
    # so mc3 stays buildable standalone.
    set(MC3_EDITOR_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
    if(EXISTS "${MC3_EDITOR_INCLUDE_DIR}/MeshCraft/EditorAlgorithms.hpp")
        add_executable(mc3_commands_test
            test/editor_commands_test.cpp
            "${CMAKE_SOURCE_DIR}/src/MeshCraft/Editor/SelectionManager.cpp"
        )
        target_include_directories(mc3_commands_test PRIVATE
            ${MC3_EDITOR_INCLUDE_DIR}
        )
        target_link_libraries(mc3_commands_test PRIVATE Mc3)
        target_compile_features(mc3_commands_test PRIVATE cxx_std_23)
        add_test(NAME mc3_commands COMMAND mc3_commands_test)
        set_tests_properties(mc3_commands PROPERTIES TIMEOUT 15 LABELS "commands")
    else()
        message(STATUS "mc3: skipping mc3_commands_test — MeshCraft editor "
                       "headers not found at ${MC3_EDITOR_INCLUDE_DIR} "
                       "(standalone mc3 build)")
    endif()
endif()
