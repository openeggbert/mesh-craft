#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Sha256.hpp"
#include "Mc3XmlParser.hpp"
#include "Mc3XmlWriter.hpp"
#include "Mc3JsonParser.hpp"
#include "Mc3JsonWriter.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace MeshCraft::Mc3 {

namespace {
// Self-contained temp-path helper for Mc3Document::validate() below. Not
// reusing include/MeshCraft/TempFile.hpp's uniqueTempPath() -- that header
// lives on the main editor's include path, and mc3/ must stay buildable
// standalone (CNA-free), so it deliberately does not depend on it.
std::filesystem::path uniqueValidationTempPath() {
    thread_local std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream oss;
    oss << "mc3_validate_" << std::hex << rng() << ".mc3.xml";
    return std::filesystem::temp_directory_path() / oss.str();
}
} // namespace

Mc3Document Mc3Document::loadFromFile(const std::filesystem::path& path) {
    return loadFromFile(path, Mc3LoadPolicy::trusted());
}

Mc3Document Mc3Document::loadFromFile(const std::filesystem::path& path,
                                     const Mc3LoadPolicy& policy) {
    Internal::Mc3XmlParser parser;
    return parser.parse(path, policy);
}

Mc3Document Mc3Document::loadFromFile(const std::filesystem::path& path,
                                     const Mc3LoadPolicy& policy,
                                     Mc3Validation& validation) {
    Internal::Mc3XmlParser parser;
    return parser.parse(path, policy, &validation);
}

Mc3Document Mc3Document::loadFromString(const std::string& xml,
                                        const std::filesystem::path& sourceDir,
                                        const Mc3LoadPolicy& policy) {
    Internal::Mc3XmlParser parser;
    return parser.parseString(xml, sourceDir, policy);
}

Mc3Document Mc3Document::loadFromString(const std::string& xml,
                                        const std::filesystem::path& sourceDir,
                                        const Mc3LoadPolicy& policy,
                                        Mc3Validation& validation) {
    Internal::Mc3XmlParser parser;
    return parser.parseString(xml, sourceDir, policy, &validation);
}

void Mc3Document::saveToFile(const std::filesystem::path& path) const {
    Internal::Mc3XmlWriter writer;
    writer.write(*this, path);
}

void Mc3Document::validate(Mc3Validation& validation) const {
    std::filesystem::path tmp = uniqueValidationTempPath();
    try {
        saveToFile(tmp);
        std::string xml;
        {
            std::ifstream f(tmp, std::ios::binary);
            xml.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        }
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        try {
            // sourcePath doubles as the source DIRECTORY here (see its own
            // field comment) so relative <include>/resource paths resolve
            // against the ORIGINAL document's directory, not tmp's.
            (void)loadFromString(xml, sourcePath, Mc3LoadPolicy::trusted(), validation);
        } catch (const std::exception&) {
            // A hard rejection already recorded its own entry into
            // `validation` before throwing (Mc3Validation.hpp's design
            // note) -- swallow it here since validate() is diagnostic-only
            // and must never block the caller's real save/export/render.
        }
    } catch (const std::exception& ex) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        validation.addError({}, {}, {},
                             std::string("could not revalidate document: ") + ex.what());
    }
}

// --- R109 -- mc3.json counterparts --------------------------------------

Mc3Document Mc3Document::loadFromJsonFile(const std::filesystem::path& path) {
    return loadFromJsonFile(path, Mc3LoadPolicy::trusted());
}

Mc3Document Mc3Document::loadFromJsonFile(const std::filesystem::path& path,
                                          const Mc3LoadPolicy& policy) {
    Internal::Mc3JsonParser parser;
    return parser.parse(path, policy);
}

Mc3Document Mc3Document::loadFromJsonString(const std::string& jsonText,
                                            const std::filesystem::path& sourceDir,
                                            const Mc3LoadPolicy& policy) {
    Internal::Mc3JsonParser parser;
    return parser.parseString(jsonText, sourceDir, policy);
}

Mc3Document Mc3Document::loadFromJsonFile(const std::filesystem::path& path,
                                          const Mc3LoadPolicy& policy,
                                          Mc3Validation& validation) {
    Internal::Mc3JsonParser parser;
    return parser.parse(path, policy, &validation);
}

Mc3Document Mc3Document::loadFromJsonString(const std::string& jsonText,
                                            const std::filesystem::path& sourceDir,
                                            const Mc3LoadPolicy& policy,
                                            Mc3Validation& validation) {
    Internal::Mc3JsonParser parser;
    return parser.parseString(jsonText, sourceDir, policy, &validation);
}

void Mc3Document::saveToJsonFile(const std::filesystem::path& path) const {
    Internal::Mc3JsonWriter writer;
    writer.write(*this, path);
}

// --- R110 -- .mc3lib.xml/.mc3lib.json ------------------------------------

namespace {
void requireLibraryInfo(const Mc3Document& doc) {
    if (!doc.library)
        throw std::invalid_argument(
            "Mc3Document: saveToLibraryFile/saveToLibraryJsonFile requires "
            "`library` (namespace + version) to be set -- this document has "
            "no library identity to reference it by");
}
} // namespace

void Mc3Document::saveToLibraryFile(const std::filesystem::path& path) const {
    requireLibraryInfo(*this);
    saveToFile(path);
}

void Mc3Document::saveToLibraryJsonFile(const std::filesystem::path& path) const {
    requireLibraryInfo(*this);
    saveToJsonFile(path);
}

Mc3Document Mc3Document::loadFromLibraryFile(const std::filesystem::path& path) {
    Mc3Document doc = loadFromFile(path);
    requireLibraryInfo(doc);
    return doc;
}

Mc3Document Mc3Document::loadFromLibraryJsonFile(const std::filesystem::path& path) {
    Mc3Document doc = loadFromJsonFile(path);
    requireLibraryInfo(doc);
    return doc;
}

Mc3Document Mc3Document::loadFromLibraryFile(const std::filesystem::path& path,
                                             Mc3Validation& validation) {
    Mc3Document doc = loadFromFile(path, Mc3LoadPolicy::trusted(), validation);
    requireLibraryInfo(doc);
    return doc;
}

Mc3Document Mc3Document::loadFromLibraryJsonFile(const std::filesystem::path& path,
                                                 Mc3Validation& validation) {
    Mc3Document doc = loadFromJsonFile(path, Mc3LoadPolicy::trusted(), validation);
    requireLibraryInfo(doc);
    return doc;
}

std::string Mc3Document::computeLibraryContentHash() const {
    Mc3Document contentOnly = *this;
    contentOnly.library.reset();
    Internal::Mc3JsonWriter writer;
    return sha256Hex(writer.toString(contentOnly));
}

// --- Builder / helper method implementations ------------------------------

Mc3Material& Mc3Document::addMaterial(Mc3Material mat) {
    const std::string key = mat.name;
    materials[key] = std::move(mat);
    return materials[key];
}

Mc3Texture& Mc3Document::addTexture(Mc3Texture tex) {
    const std::string key = tex.name;
    textures[key] = std::move(tex);
    return textures[key];
}

Mc3SvgTexture& Mc3Document::addSvgTexture(Mc3SvgTexture tex) {
    const std::string key = tex.id;
    svgTextures[key] = std::move(tex);
    return svgTextures[key];
}

Mc3EmbedGltf& Mc3Document::addEmbed(Mc3EmbedGltf embed) {
    const std::string key = embed.id;
    embeds[key] = std::move(embed);
    return embeds[key];
}

Mc3Script& Mc3Document::addScript(Mc3Script script) {
    const std::string key = script.id;
    scripts[key] = std::move(script);
    return scripts[key];
}

Mc3Sound& Mc3Document::addSound(Mc3Sound sound) {
    const std::string key = sound.id;
    sounds[key] = std::move(sound);
    return sounds[key];
}

Mc3Music& Mc3Document::addMusic(Mc3Music music) {
    const std::string key = music.id;
    musicTracks[key] = std::move(music);
    return musicTracks[key];
}

Mc3Trigger& Mc3Document::addTrigger(Mc3Trigger trigger) {
    const std::string key = trigger.id;
    triggers[key] = std::move(trigger);
    return triggers[key];
}

Mc3SceneState& Mc3Document::addSceneState(Mc3SceneState state) {
    const std::string key = state.name;
    sceneStates[key] = std::move(state);
    return sceneStates[key];
}

std::shared_ptr<Mc3Object> Mc3Document::addObject(std::shared_ptr<Mc3Object> obj) {
    objects.push_back(obj);
    return obj;
}

std::shared_ptr<Mc3Object> Mc3Document::defineObject(std::string id,
                                                       std::shared_ptr<Mc3Object> obj)
{
    obj->id = id;
    definitions[id] = obj;
    return obj;
}

Mc3Light& Mc3Document::addLight(Mc3Light light) {
    lights.push_back(std::move(light));
    return lights.back();
}

Mc3Camera& Mc3Document::addCamera(Mc3Camera cam) {
    cameras.push_back(std::move(cam));
    return cameras.back();
}

Mc3Action& Mc3Document::addAction(Mc3Action action) {
    const std::string key = action.name;
    actions[key] = std::move(action);
    return actions[key];
}

Mc3Document& Mc3Document::setEnvironment(Mc3Environment env) {
    environment = std::move(env);
    return *this;
}

Mc3Document& Mc3Document::setBackgroundColor(float r, float g, float b) {
    if (!environment) environment = Mc3Environment{};
    environment->backgroundColor = {r, g, b};
    return *this;
}

Mc3Document& Mc3Document::setFog(Mc3Fog fog) {
    if (!environment) environment = Mc3Environment{};
    environment->fog = std::move(fog);
    return *this;
}

Mc3Document& Mc3Document::withMetadata(std::string key, std::string value) {
    metadata[std::move(key)] = std::move(value); return *this;
}

Mc3Document& Mc3Document::withMeta(std::string key, std::string value) {
    meta[std::move(key)] = std::move(value); return *this;
}

Mc3Document& Mc3Document::withRotationUnits(std::string units) {
    rotationUnits = std::move(units); return *this;
}

Mc3Document& Mc3Document::withEulerOrder(std::string order) {
    eulerOrder = std::move(order); return *this;
}

} // namespace MeshCraft::Mc3
