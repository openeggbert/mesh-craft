#pragma once
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Validation.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mc3togltf {

enum class OutputFormat { GLTF, GLB };

// Returns the OutputFormat implied by path's extension (.gltf / .glb, case-insensitive).
// Throws std::runtime_error for any other extension.
OutputFormat outputFormatFromPath(const std::filesystem::path& outputPath);

// Aggregate statistics populated by GltfExporter::exportDocument().
struct ExportStats {
    int objectsProcessed{0};   // MC3 objects visited (buildNode calls)
    int gltfNodes{0};          // glTF nodes written
    int uniqueMeshes{0};       // unique glTF meshes in the buffer
    int materialCount{0};      // glTF materials written
    int reusedMeshRefs{0};     // node-to-mesh assignments that reused an existing mesh
    int totalVertices{0};      // vertex count summed over unique meshes
    int totalTriangles{0};     // triangle count summed over unique meshes
    int objMeshesLoaded{0};    // external OBJ files actually parsed (cache misses)
    int csgMeshesEvaluated{0}; // CSG boolean evaluations via Manifold
    int quantizedAttributeAccessors{0}; // KHR_mesh_quantization accessors written
    int narrowedIndexAccessors{0};      // lossless UINT32 -> UINT16 index accessors
    int warnings{0};           // non-fatal warnings emitted during export
};

// A deliberately conservative preflight estimate. It is constructed from the
// same geometry/material model as export, but JSON serialization overhead is
// necessarily estimated rather than promised byte-for-byte. `estimatedTotal`
// covers the one-file GLB or the combined .gltf JSON + .bin payload; ordinary
// externally referenced texture files are not counted because export retains
// them by URI instead of copying them.
struct ExportEstimate {
    std::uint64_t estimatedTotalBytes{0};
    std::uint64_t estimatedJsonBytes{0};
    std::uint64_t estimatedBinaryBytes{0};
    std::uint64_t embeddedImageBytes{0};
    bool embedsImages{false};
};

// A compatibility/loss report associated with a source MC3 object whenever
// possible. It is intentionally structured instead of stdout-only so the GUI
// can show users exactly which object needs attention after export.
struct ExportReportEntry {
    std::string objectId;
    std::string message;
};

class GltfExporter {
public:
    // When false (default), CSG nodes (union/difference/intersection) are evaluated
    // using the Manifold library and exported as a single merged mesh.  Throws on
    // evaluation failure.
    // When true (--allow-approximate-csg / editor checkbox), Manifold evaluation is
    // skipped and children are exported as separate meshes — geometrically incorrect,
    // useful only for debugging or previewing unsupported scene graphs.
    bool allowApproximateCSG{false};

    // When false (default), external resources (texture URIs, OBJ mesh sources)
    // whose path is absolute or escapes the document root are rejected, so an
    // untrusted .mc3 cannot exfiltrate arbitrary local files into the output
    // GLB. Set true (--allow-external-resources) to permit them for trusted
    // scenes that legitimately reference files outside their own directory.
    bool allowExternalResources{false};

    // Opt-in, deterministic KHR_mesh_quantization subset: normals/tangents
    // become signed-normalized 16-bit values (component error <= 1/32767),
    // in-range [0,1] UVs become unsigned-normalized 16-bit values (error <=
    // 1/65535), and compatible indices become lossless UINT16. Positions stay
    // float32 so node transforms and shared mesh reuse retain exact behavior.
    bool quantizeMeshAttributes{false};

    // Populated after exportDocument() returns successfully.
    ExportStats stats;

    // Cleared and repopulated for every estimate/export. Entries record
    // opt-in quantization fallbacks and other represented downgrades.
    std::vector<ExportReportEntry> report;

    // SYS-W1-01 (pre-export integration point): populated by exportDocument()
    // BEFORE it builds any glTF output, by re-validating `doc`'s current
    // in-memory state (see Mc3Document::validate()). Documents reaching
    // export without ever going through a validating parse -- built
    // programmatically via the Mc3Object::make*() builders, or mutated in
    // place after loading -- get the same diagnostics a fresh load would
    // produce. Diagnostic-only: never gates the export itself.
    MeshCraft::Mc3::Mc3Validation validation;

    void exportDocument(const MeshCraft::Mc3::Mc3Document& doc,
                        const std::filesystem::path& outputPath,
                        OutputFormat format);

    // Runs the same deterministic model-building phase as export without
    // creating output files. It refreshes `stats`, `validation`, and `report`.
    ExportEstimate estimateDocument(const MeshCraft::Mc3::Mc3Document& doc,
                                    const std::filesystem::path& outputPath,
                                    OutputFormat format);
};

} // namespace mc3togltf
