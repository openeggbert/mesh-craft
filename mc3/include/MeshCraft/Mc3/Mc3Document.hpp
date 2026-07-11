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

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

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

    // Parse MC3 XML already held in memory (no temp file). `sourceDir` is the
    // directory that relative resource/include paths resolve against. Intended
    // for untrusted content — the default policy here is untrusted().
    static Mc3Document loadFromString(const std::string& xml,
                                      const std::filesystem::path& sourceDir = {},
                                      const Mc3LoadPolicy& policy = Mc3LoadPolicy::untrusted());

    // Save to XML
    void saveToFile(const std::filesystem::path& path) const;

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
