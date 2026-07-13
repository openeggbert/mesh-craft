#pragma once

#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Camera.hpp"
#include "MeshCraft/Mc3/Mc3EmbedGltf.hpp"
#include "MeshCraft/Mc3/Mc3Environment.hpp"
#include "MeshCraft/Mc3/Mc3Light.hpp"
#include "MeshCraft/Mc3/Mc3LoadPolicy.hpp"
#include "MeshCraft/Mc3/Mc3Material.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"
#include "MeshCraft/Mc3/Mc3Music.hpp"
#include "MeshCraft/Mc3/Mc3SceneState.hpp"
#include "MeshCraft/Mc3/Mc3Script.hpp"
#include "MeshCraft/Mc3/Mc3Sound.hpp"
#include "MeshCraft/Mc3/Mc3Trigger.hpp"
#include "MeshCraft/Mc3/Mc3SvgTexture.hpp"
#include "MeshCraft/Mc3/Mc3Texture.hpp"
#include "MeshCraft/Mc3/Mc3Validation.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

// R110 -- present only on a `.mc3lib.xml`/`.mc3lib.json` reusable-definition
// library document (mesh_world_revival.md §7/§8), absent on an ordinary
// scene/model document. Referenced elsewhere as "mc3lib://<namespace>@<version>".
struct Mc3LibraryInfo {
    std::string libraryNamespace;  // e.g. "city-core"
    std::string version;           // semver "major.minor.patch", e.g. "3.2.1"
    std::string contentHash;       // "sha256:<64 lowercase hex chars>"
};

// R101 -- a single <import>/import entry (mesh_world_revival.md §7):
// pulls a `.mc3lib` library into this document under a local alias
// (`importNamespace`), so instances can reference its definitions as
// "<importNamespace>:<definitionId>". `source` is a "mc3lib://<library-
// name>@<version>" URI -- the library's OWN name/version (as declared in
// its own `library->libraryNamespace`/`library->version`), which may
// differ from the local alias this document chooses to import it under.
// `hash`, if non-empty, is verified against the resolved library's own
// `computeLibraryContentHash()` (see Mc3ImportResolver).
struct Mc3Import {
    std::string importNamespace;  // local alias, e.g. "city"
    std::string source;           // "mc3lib://city-core@3.2.1"
    std::string hash;             // "sha256:...", optional (empty = not pinned)
};

class Mc3Document {
public:
    std::string version{"0.3"};
    std::string model;
    std::string unit{"meter"};
    std::string coordinateSystem{"right_handed_y_up"};
    std::string rotationUnits{"degrees"};  // degrees | radians
    std::string eulerOrder{"XYZ"};         // XYZ | XZY | YXZ | YZX | ZXY | ZYX

    // Opaque key/value pass-through store (mirrors <metadata> in the XSD).
    std::map<std::string, std::string> metadata;

    // New-style key-value metadata (mirrors <meta><metaentry key="…" value="…"/> in the XSD).
    std::map<std::string, std::string> meta;

    // Directory of the source .mc3.xml file — used for resolving relative asset paths
    std::filesystem::path sourcePath;

    // R110 -- set only for .mc3lib.xml/.mc3lib.json library documents.
    std::optional<Mc3LibraryInfo> library;

    // R101 -- libraries this document imports (see Mc3Import above / the
    // resolver in Mc3ImportResolver.hpp). Empty for an ordinary document
    // (and typically for a library itself, though nothing prevents a
    // library from importing another one).
    std::vector<Mc3Import> imports;

    // Files referenced via <include file="..."/> — preserved so the writer
    // can re-emit them instead of inlining the included content.
    std::vector<std::string> includes;

    // IDs of definitions/materials/textures that were loaded from included
    // files.  The writer skips these so they are not duplicated into the
    // main file.
    std::set<std::string> includedDefs;
    std::set<std::string> includedMaterials;
    std::set<std::string> includedTextures;
    std::set<std::string> includedEmbeds;

    std::optional<Mc3Environment> environment;
    std::vector<Mc3Light>   lights;
    std::vector<Mc3Camera>  cameras;
    std::string defaultCamera;

    std::map<std::string, Mc3Texture>    textures;
    std::map<std::string, Mc3SvgTexture> svgTextures;
    std::map<std::string, Mc3EmbedGltf>  embeds;
    std::map<std::string, Mc3Script>     scripts;
    std::map<std::string, Mc3Sound>      sounds;
    std::map<std::string, Mc3Music>      musicTracks;
    std::map<std::string, Mc3Trigger>    triggers;
    std::map<std::string, Mc3SceneState> sceneStates;
    std::map<std::string, Mc3Material>   materials;
    std::map<std::string, std::shared_ptr<Mc3Object>> definitions;
    std::vector<std::shared_ptr<Mc3Object>> objects;
    std::map<std::string, Mc3Action> actions;

    // Load from MC3 XML (.mc3.xml). The default policy is trusted (permissive,
    // processes <include>) — pass Mc3LoadPolicy::untrusted() for AI/imported
    // content that must not open arbitrary local files.
    static Mc3Document loadFromFile(const std::filesystem::path& path);
    static Mc3Document loadFromFile(const std::filesystem::path& path,
                                    const Mc3LoadPolicy& policy);

    // SYS-W1-01: same as above, but additionally populates `validation` with a
    // warning/error entry for every clamp/default/rejection the parse
    // performs (main document AND any merged <include> files) -- an ADDITIVE
    // side-channel; the throw-on-hard-rejection behavior of the overloads
    // above is unchanged. See Mc3Validation.hpp for the entry shape.
    static Mc3Document loadFromFile(const std::filesystem::path& path,
                                    const Mc3LoadPolicy& policy,
                                    Mc3Validation& validation);

    // Parse MC3 XML already held in memory (no temp file). `sourceDir` is the
    // directory that relative resource/include paths resolve against. Intended
    // for untrusted content — the default policy here is untrusted().
    static Mc3Document loadFromString(const std::string& xml,
                                      const std::filesystem::path& sourceDir = {},
                                      const Mc3LoadPolicy& policy = Mc3LoadPolicy::untrusted());

    // SYS-W1-01: validation-capturing counterpart, same semantics as the
    // loadFromFile overload above.
    static Mc3Document loadFromString(const std::string& xml,
                                      const std::filesystem::path& sourceDir,
                                      const Mc3LoadPolicy& policy,
                                      Mc3Validation& validation);

    // R109 -- semantic mc3.json counterparts of the XML loaders/saver above.
    // Both formats parse into this SAME Mc3Document AST (see
    // mesh_world_revival.md §4.3): mc3.json is not a mechanical XML mirror,
    // it's a genuinely semantic JSON document (arrays for vectors, nested
    // objects for transform/primitive/extrude/...), produced/consumed by
    // Mc3JsonWriter/Mc3JsonParser. Does not process <include>-equivalent
    // merging -- `includes` round-trips as a plain string list either way.
    static Mc3Document loadFromJsonFile(const std::filesystem::path& path);
    static Mc3Document loadFromJsonFile(const std::filesystem::path& path,
                                        const Mc3LoadPolicy& policy);
    static Mc3Document loadFromJsonString(const std::string& json,
                                          const std::filesystem::path& sourceDir = {},
                                          const Mc3LoadPolicy& policy = Mc3LoadPolicy::untrusted());

    // Save to XML
    void saveToFile(const std::filesystem::path& path) const;

    // SYS-W1-01: re-validates this document's CURRENT in-memory state --
    // useful right before an expensive/consequential operation (export,
    // save, first render of a newly-swapped-in document) on a document that
    // may have been built programmatically (e.g. a world generator using the
    // builder methods below) or mutated in place after loading (e.g. editor
    // UI edits), neither of which goes through Mc3XmlParser's load-time
    // checks. Implemented by round-tripping through the same XML writer/
    // parser the validating loadFromFile/loadFromString overloads above use
    // (see Mc3Document.cpp), so it reports exactly the diagnostics a fresh
    // load of this content would produce. Never throws -- a revalidation
    // failure is itself reported as an error entry, not propagated, since
    // this is a diagnostic side-channel, not a gate. O(document size): not
    // free, so callers should not run it once per frame.
    void validate(Mc3Validation& validation) const;

    // Save to mc3.json (R109).
    void saveToJsonFile(const std::filesystem::path& path) const;

    // R110 -- .mc3lib.xml/.mc3lib.json save/load: same Mc3Document AST and
    // XML/JSON writers/parsers as an ordinary scene/model file (one semantic
    // model, per mesh_world_revival.md §4.3), just conventionally named and
    // required to carry `library` (namespace + version) so a resolver always
    // has an identity to reference it by. Throws std::invalid_argument if
    // `library` is unset. Does not implicitly (re)compute contentHash --
    // call computeLibraryContentHash() first if a fresh hash is wanted.
    void saveToLibraryFile(const std::filesystem::path& path) const;
    void saveToLibraryJsonFile(const std::filesystem::path& path) const;
    static Mc3Document loadFromLibraryFile(const std::filesystem::path& path);
    static Mc3Document loadFromLibraryJsonFile(const std::filesystem::path& path);

    // R110 -- sha256 (see Mc3Sha256.hpp) of this document's canonical
    // mc3.json body (definitions/materials/textures/etc.), EXCLUDING the
    // `library` block itself (a hash covering its own hash field couldn't be
    // verified). Representation-independent: XML and JSON share one AST, so
    // this is stable regardless of which surface a library was authored in.
    // Returns the hex digest WITHOUT a "sha256:" prefix; assign it into
    // `library->contentHash` with that prefix yourself, e.g.:
    //   doc.library->contentHash = "sha256:" + doc.computeLibraryContentHash();
    std::string computeLibraryContentHash() const;

    // --- Builder / helper methods -----------------------------------------
    // These add objects to the document and return a reference to the stored
    // element so callers can chain additional modifications.

    // Add a material (key = material.name).  Returns reference to the stored material.
    Mc3Material& addMaterial(Mc3Material mat);

    // Add a texture (key = texture.name).  Returns reference to the stored texture.
    Mc3Texture& addTexture(Mc3Texture tex);

    // Add an SVG texture (key = tex.id).  Returns reference to the stored entry.
    Mc3SvgTexture& addSvgTexture(Mc3SvgTexture tex);

    // Add an embedded GLTF (key = embed.id).  Returns reference to the stored entry.
    Mc3EmbedGltf& addEmbed(Mc3EmbedGltf embed);

    // Add a script (key = script.id).  Returns reference to the stored entry.
    Mc3Script& addScript(Mc3Script script);

    // Add a sound effect (key = sound.id).  Returns reference to the stored entry.
    Mc3Sound& addSound(Mc3Sound sound);

    // Add a music track (key = music.id).  Returns reference to the stored entry.
    Mc3Music& addMusic(Mc3Music music);

    // Add a trigger (key = trigger.id).  Returns reference to the stored entry.
    Mc3Trigger& addTrigger(Mc3Trigger trigger);

    // Add a scene state (key = state.name).  Returns reference to the stored entry.
    Mc3SceneState& addSceneState(Mc3SceneState state);

    // Append an object to the root objects list.  Returns the shared_ptr (copy).
    std::shared_ptr<Mc3Object> addObject(std::shared_ptr<Mc3Object> obj);

    // Register a named definition (reusable template for <instance> elements).
    // Returns the shared_ptr stored in definitions[id].
    std::shared_ptr<Mc3Object> defineObject(std::string id,
                                             std::shared_ptr<Mc3Object> obj);

    // Append a light.  Returns reference to the stored light.
    Mc3Light& addLight(Mc3Light light);

    // Append a camera.  Returns reference to the stored camera.
    Mc3Camera& addCamera(Mc3Camera cam);

    // Add or replace a named action.  Returns reference to the stored action.
    Mc3Action& addAction(Mc3Action action);

    // Set the environment block (replaces any existing one).
    Mc3Document& setEnvironment(Mc3Environment env);

    // Convenience: set background color without touching other env fields.
    Mc3Document& setBackgroundColor(float r, float g, float b);

    // Convenience: set fog without touching other env fields.
    Mc3Document& setFog(Mc3Fog fog);

    // Set a metadata key/value pair (pass-through store, round-trips via XML).
    Mc3Document& withMetadata(std::string key, std::string value);

    // Set a meta key/value pair (new-style <meta> element).
    Mc3Document& withMeta(std::string key, std::string value);

    // Convenience setters for root-level rotation convention.
    Mc3Document& withRotationUnits(std::string units);  // "degrees" | "radians"
    Mc3Document& withEulerOrder(std::string order);     // "XYZ" | "YXZ" | etc.
};

} // namespace MeshCraft::Mc3
