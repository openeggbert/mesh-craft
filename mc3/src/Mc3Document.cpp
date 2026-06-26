#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "Mc3XmlParser.hpp"
#include "Mc3XmlWriter.hpp"

namespace MeshCraft::Mc3 {

Mc3Document Mc3Document::loadFromFile(const std::filesystem::path& path) {
    Internal::Mc3XmlParser parser;
    return parser.parse(path);
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

} // namespace MeshCraft::Mc3
