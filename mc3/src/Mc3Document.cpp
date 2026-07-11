#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "Mc3XmlParser.hpp"
#include "Mc3XmlWriter.hpp"

namespace MeshCraft::Mc3 {

Mc3Document Mc3Document::loadFromFile(const std::filesystem::path& path) {
    return loadFromFile(path, Mc3LoadPolicy::trusted());
}

Mc3Document Mc3Document::loadFromFile(const std::filesystem::path& path,
                                     const Mc3LoadPolicy& policy) {
    Internal::Mc3XmlParser parser;
    return parser.parse(path, policy);
}

Mc3Document Mc3Document::loadFromString(const std::string& xml,
                                        const std::filesystem::path& sourceDir,
                                        const Mc3LoadPolicy& policy) {
    Internal::Mc3XmlParser parser;
    return parser.parseString(xml, sourceDir, policy);
}

void Mc3Document::saveToFile(const std::filesystem::path& path) const {
    Internal::Mc3XmlWriter writer;
    writer.write(*this, path);
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
