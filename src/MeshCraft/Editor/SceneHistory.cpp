#include "MeshCraft/Editor/SceneHistory.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace MeshCraft::Editor {
namespace {

constexpr std::size_t kMapNodeOverhead = sizeof(void*) * 3;

void appendString(std::ostringstream& out, std::string_view value) {
    out << value.size() << ':' << value << '|';
}

template <typename T, std::size_t N>
void appendArray(std::ostringstream& out, const std::array<T, N>& values) {
    for (const auto& value : values) out << value << ',';
    out << '|';
}

void appendStrings(std::ostringstream& out, const std::vector<std::string>& values) {
    out << values.size() << '|';
    for (const auto& value : values) appendString(out, value);
}

void appendAssetMetadata(std::ostringstream& out, const Mc3::Mc3AssetMetadata& metadata) {
    appendString(out, metadata.category);
    appendString(out, metadata.subcategory);
    appendStrings(out, metadata.semanticTags);
    appendStrings(out, metadata.styleTags);
    appendStrings(out, metadata.regionTags);
    appendStrings(out, metadata.periodTags);
    appendArray(out, metadata.nominalSize);
    appendArray(out, metadata.boundsMin);
    appendArray(out, metadata.boundsMax);
    appendString(out, metadata.facing);
    for (const auto& [name, socket] : metadata.sockets) {
        appendString(out, name);
        appendArray(out, socket);
    }
    appendStrings(out, metadata.materialSlots);
    appendString(out, metadata.collisionProxy);
    appendArray(out, metadata.clearanceVolume);
    for (const auto& [tier, definition] : metadata.lods) {
        appendString(out, tier);
        appendString(out, definition);
    }
    out << metadata.instancingEligible << '|' << metadata.maxVisibilityDistanceM << '|'
        << metadata.selectionWeight << '|';
    appendString(out, metadata.shadowPolicy);
    appendString(out, metadata.license);
    appendString(out, metadata.provenance);
    appendString(out, metadata.sourceGeneratorOrHash);
    appendString(out, metadata.semanticVersion);
}

void appendPrimitive(std::ostringstream& out, const Mc3::Mc3Primitive& primitive) {
    out << static_cast<int>(primitive.primitiveType) << '|';
    appendArray(out, primitive.size);
    out << primitive.radius << '|' << primitive.height << '|' << primitive.segments << '|';
    appendString(out, primitive.axis);
    out << primitive.majorRadius << '|' << primitive.minorRadius << '|'
        << primitive.subdivisionsX << '|' << primitive.subdivisionsZ << '|';
}

void appendExtrude(std::ostringstream& out, const Mc3::Mc3Extrude& extrude) {
    const auto& section = extrude.crossSection;
    out << static_cast<int>(section.type) << '|' << section.width << '|' << section.height << '|'
        << section.radius << '|' << section.innerRadius << '|' << section.sides << '|'
        << section.segments << '|';
    for (const auto& point : section.customPoints) out << point.x << ',' << point.y << '|';
    const auto& path = extrude.path;
    out << static_cast<int>(path.type) << '|' << path.length << '|';
    appendString(out, path.axis);
    out << path.arcRadius << '|' << path.arcAngle << '|' << path.helixRadius << '|'
        << path.helixHeight << '|' << path.helixTurns << '|';
    for (const auto& point : path.points) {
        appendArray(out, point.position);
        appendArray(out, point.controlIn);
    }
    out << extrude.twist << '|' << extrude.segments << '|' << extrude.smooth << '|' << extrude.caps << '|';
}

std::string ownObjectSignature(const Mc3::Mc3Object& object) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << static_cast<int>(object.type) << '|';
    appendString(out, object.name);
    appendString(out, object.id);
    appendArray(out, object.transform.position);
    appendArray(out, object.transform.rotation);
    appendArray(out, object.transform.scale);
    appendArray(out, object.transform.pivot);
    if (object.deform) appendArray(out, object.deform->scale);
    out << object.deform.has_value() << '|';
    appendString(out, object.material);
    out << object.visible << '|';
    appendString(out, object.collision);
    appendString(out, object.layer);
    appendStrings(out, object.tags);
    if (object.primitive) appendPrimitive(out, *object.primitive);
    out << object.primitive.has_value() << '|';
    if (object.csgOperation) out << static_cast<int>(object.csgOperation->csgType) << '|';
    out << object.csgOperation.has_value() << '|';
    if (object.extrude) appendExtrude(out, *object.extrude);
    out << object.extrude.has_value() << '|';
    appendString(out, object.definition);
    appendStrings(out, object.variantDefinitions);
    appendString(out, object.meshSource);
    appendString(out, object.materialOverride);
    out << object.isCutter << '|';
    for (const auto& [stateName, state] : object.states) {
        appendString(out, stateName);
        out << state.position.has_value() << state.rotation.has_value() << state.scale.has_value()
            << state.visible.has_value() << state.material.has_value() << '|';
        if (state.position) appendArray(out, *state.position);
        if (state.rotation) appendArray(out, *state.rotation);
        if (state.scale) appendArray(out, *state.scale);
        if (state.visible) out << *state.visible << '|';
        if (state.material) appendString(out, *state.material);
    }
    if (object.uvMapping) {
        out << static_cast<int>(object.uvMapping->projection) << '|' << object.uvMapping->scaleU << '|'
            << object.uvMapping->scaleV << '|' << object.uvMapping->offsetU << '|'
            << object.uvMapping->offsetV << '|' << object.uvMapping->rotation << '|';
    }
    out << object.uvMapping.has_value() << '|';
    for (const auto& [key, value] : object.metadata) {
        appendString(out, key);
        appendString(out, value);
    }
    if (object.assetMetadata) appendAssetMetadata(out, *object.assetMetadata);
    out << object.assetMetadata.has_value() << '|';
    appendString(out, object.scriptId);
    return out.str();
}

std::shared_ptr<Mc3::Mc3Object> cloneObject(const std::shared_ptr<Mc3::Mc3Object>& source) {
    static thread_local int depth = 0;
    struct DepthGuard {
        DepthGuard() {
            if (++depth > 256) {
                --depth;
                throw std::runtime_error("SceneHistory: object nesting exceeds 256 levels");
            }
        }
        ~DepthGuard() { --depth; }
    } guard;
    if (!source) return {};
    auto copy = std::make_shared<Mc3::Mc3Object>(*source);
    copy->children.clear();
    for (const auto& child : source->children) copy->children.push_back(cloneObject(child));
    return copy;
}

Mc3::Mc3Document cloneDocument(const Mc3::Mc3Document& source) {
    Mc3::Mc3Document copy = source;
    copy.objects.clear();
    for (const auto& object : source.objects) copy.objects.push_back(cloneObject(object));
    copy.definitions.clear();
    for (const auto& [id, definition] : source.definitions)
        copy.definitions[id] = cloneObject(definition);
    return copy;
}

std::size_t estimateString(const std::string& value) {
    return sizeof(std::string) + value.capacity() + 1;
}

void estimateObject(const Mc3::Mc3Object* object, std::set<const Mc3::Mc3Object*>& visited,
                    std::size_t& bytes) {
    if (!object || !visited.insert(object).second) return;
    bytes += sizeof(Mc3::Mc3Object);
    bytes += estimateString(object->name) + estimateString(object->id) + estimateString(object->material) +
             estimateString(object->collision) + estimateString(object->layer) + estimateString(object->definition) +
             estimateString(object->meshSource) + estimateString(object->materialOverride) + estimateString(object->scriptId);
    for (const auto& tag : object->tags) bytes += estimateString(tag);
    for (const auto& definition : object->variantDefinitions) bytes += estimateString(definition);
    for (const auto& [name, state] : object->states) {
        bytes += kMapNodeOverhead + estimateString(name) + sizeof(state);
        if (state.material) bytes += estimateString(*state.material);
    }
    for (const auto& [key, value] : object->metadata)
        bytes += kMapNodeOverhead + estimateString(key) + estimateString(value);
    if (object->primitive) bytes += sizeof(*object->primitive) + estimateString(object->primitive->axis);
    if (object->extrude) {
        bytes += sizeof(*object->extrude) + estimateString(object->extrude->path.axis);
        bytes += object->extrude->crossSection.customPoints.capacity() * sizeof(Mc3::Mc3CrossSection::Point2D);
        bytes += object->extrude->path.points.capacity() * sizeof(Mc3::Mc3PathPoint);
    }
    if (object->assetMetadata) {
        const auto& metadata = *object->assetMetadata;
        bytes += sizeof(metadata) + estimateString(metadata.category) + estimateString(metadata.subcategory) +
                 estimateString(metadata.facing) + estimateString(metadata.collisionProxy) +
                 estimateString(metadata.shadowPolicy) + estimateString(metadata.license) +
                 estimateString(metadata.provenance) + estimateString(metadata.sourceGeneratorOrHash) +
                 estimateString(metadata.semanticVersion);
        for (const auto* tags : {&metadata.semanticTags, &metadata.styleTags, &metadata.regionTags, &metadata.periodTags,
                                 &metadata.materialSlots})
            for (const auto& tag : *tags) bytes += estimateString(tag);
    }
    bytes += object->children.capacity() * sizeof(std::shared_ptr<Mc3::Mc3Object>);
    for (const auto& child : object->children) estimateObject(child.get(), visited, bytes);
}

struct FlatObject {
    std::string displayId;
    std::string signature;
};

void flattenObjects(const std::vector<std::shared_ptr<Mc3::Mc3Object>>& objects,
                    const std::string& parentPath, std::map<std::string, FlatObject>& output) {
    for (std::size_t index = 0; index < objects.size(); ++index) {
        const auto& object = objects[index];
        if (!object) continue;
        const std::string path = parentPath + '/' + std::to_string(index);
        std::string key = object->id.empty() ? "path:" + path : "id:" + object->id;
        if (output.contains(key)) key = "path:" + path;
        FlatObject flat;
        flat.displayId = object->id.empty() ? path : object->id;
        flat.signature = ownObjectSignature(*object);
        for (std::size_t childIndex = 0; childIndex < object->children.size(); ++childIndex) {
            const auto& child = object->children[childIndex];
            flat.signature += "child:";
            flat.signature += child && !child->id.empty() ? child->id : path + '/' + std::to_string(childIndex);
            flat.signature += '|';
        }
        output.emplace(std::move(key), std::move(flat));
        flattenObjects(object->children, path, output);
    }
}

std::string sceneResourceSignature(const Mc3::Mc3Document& document) {
    std::ostringstream out;
    appendString(out, document.model);
    appendString(out, document.unit);
    appendString(out, document.coordinateSystem);
    appendString(out, document.rotationUnits);
    appendString(out, document.eulerOrder);
    for (const auto& [key, value] : document.metadata) { appendString(out, key); appendString(out, value); }
    for (const auto& [key, value] : document.meta) { appendString(out, key); appendString(out, value); }
    out << document.materials.size() << '|' << document.textures.size() << '|' << document.svgTextures.size()
        << '|' << document.embeds.size() << '|' << document.actions.size() << '|' << document.lights.size()
        << '|' << document.cameras.size() << '|' << document.imports.size() << '|';
    for (const auto& [id, material] : document.materials) {
        appendString(out, id);
        appendString(out, material.name);
        appendArray(out, material.baseColor);
        appendString(out, material.baseColorTexture);
        appendString(out, material.normalTexture);
        appendString(out, material.emissiveTexture);
        appendString(out, material.metallicRoughnessTexture);
        appendString(out, material.occlusionTexture);
        out << material.roughness << '|' << material.metallic << '|' << material.normalScale << '|'
            << material.occlusionStrength << '|';
        appendArray(out, material.emissiveColor);
        appendString(out, material.alphaMode);
        out << material.alphaCutoff << '|' << material.doubleSided << '|';
    }
    return out.str();
}

} // namespace

SceneHistory::SceneHistory(std::size_t budgetBytes)
    : budgetBytes_(budgetBytes) {}

std::vector<SceneHistory::SnapshotInfo> SceneHistory::snapshots() const {
    std::vector<SnapshotInfo> infos;
    infos.reserve(snapshots_.size());
    for (const auto& snapshot : snapshots_) infos.push_back(snapshot.info);
    return infos;
}

std::size_t SceneHistory::evictFor(std::size_t requiredBytes, SnapshotKind incomingKind) {
    std::size_t evicted = 0;
    while (usedBytes_ + requiredBytes > budgetBytes_ && !snapshots_.empty()) {
        auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [](const Snapshot& snapshot) {
            return snapshot.info.kind == SnapshotKind::Automatic;
        });
        if (it == snapshots_.end()) {
            if (incomingKind == SnapshotKind::Automatic) break;
            it = snapshots_.begin();
        }
        usedBytes_ -= it->info.estimatedBytes;
        snapshots_.erase(it);
        ++evicted;
    }
    return evicted;
}

SceneHistory::CaptureResult SceneHistory::capture(Mc3::Mc3Document doc,
                                                   std::vector<std::string> selectionIds,
                                                   std::string label,
                                                   SnapshotKind kind) {
    const std::size_t bytes = estimateDocumentBytes(doc) +
        selectionIds.capacity() * sizeof(std::string) +
        std::accumulate(selectionIds.begin(), selectionIds.end(), std::size_t{0},
                        [](std::size_t sum, const std::string& id) { return sum + estimateString(id); });
    if (bytes > budgetBytes_) {
        return {false, 0, 0, "Snapshot is larger than the configured history budget"};
    }
    if (kind == SnapshotKind::Automatic) {
        std::size_t reclaimableAutomaticBytes = 0;
        for (const auto& snapshot : snapshots_) {
            if (snapshot.info.kind == SnapshotKind::Automatic)
                reclaimableAutomaticBytes += snapshot.info.estimatedBytes;
        }
        // Do this feasibility check before eviction. A failed automatic
        // capture must not discard useful old automatic history merely
        // because named checkpoints currently reserve the remaining budget.
        if (usedBytes_ + bytes - reclaimableAutomaticBytes > budgetBytes_) {
            return {false, 0, 0, "Automatic history is preserving named checkpoints within the budget"};
        }
    }
    const std::size_t evicted = evictFor(bytes, kind);
    if (usedBytes_ + bytes > budgetBytes_) {
        return {false, 0, evicted, "Automatic history is preserving named checkpoints within the budget"};
    }
    Snapshot snapshot;
    snapshot.info.id = nextId_++;
    snapshot.info.kind = kind;
    snapshot.info.label = label.empty() ? (kind == SnapshotKind::Checkpoint ? "Checkpoint" : "Before edit")
                                        : std::move(label);
    snapshot.info.estimatedBytes = bytes;
    snapshot.info.objectCount = countObjects(doc);
    snapshot.doc = std::move(doc);
    snapshot.selectionIds = std::move(selectionIds);
    usedBytes_ += bytes;
    const SnapshotId id = snapshot.info.id;
    snapshots_.push_back(std::move(snapshot));
    return {true, id, evicted, {}};
}

SceneHistory::BudgetResult SceneHistory::setBudgetBytes(std::size_t bytes) {
    budgetBytes_ = bytes;
    std::size_t evicted = 0;
    while (usedBytes_ > budgetBytes_ && !snapshots_.empty()) {
        auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [](const Snapshot& snapshot) {
            return snapshot.info.kind == SnapshotKind::Automatic;
        });
        if (it == snapshots_.end()) it = snapshots_.begin();
        usedBytes_ -= it->info.estimatedBytes;
        snapshots_.erase(it);
        ++evicted;
    }
    return {evicted, usedBytes_};
}

bool SceneHistory::remove(SnapshotId id) {
    const auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [&](const Snapshot& snapshot) {
        return snapshot.info.id == id;
    });
    if (it == snapshots_.end()) return false;
    usedBytes_ -= it->info.estimatedBytes;
    snapshots_.erase(it);
    return true;
}

void SceneHistory::clear() {
    snapshots_.clear();
    usedBytes_ = 0;
}

std::optional<SceneHistory::RestoredState> SceneHistory::restore(SnapshotId id) const {
    const auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [&](const Snapshot& snapshot) {
        return snapshot.info.id == id;
    });
    if (it == snapshots_.end()) return std::nullopt;
    return RestoredState{cloneDocument(it->doc), it->selectionIds};
}

std::optional<SceneHistory::Diff> SceneHistory::diffAgainst(SnapshotId id,
                                                             const Mc3::Mc3Document& current) const {
    const auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [&](const Snapshot& snapshot) {
        return snapshot.info.id == id;
    });
    if (it == snapshots_.end()) return std::nullopt;

    std::map<std::string, FlatObject> before;
    std::map<std::string, FlatObject> after;
    flattenObjects(it->doc.objects, "root", before);
    flattenObjects(current.objects, "root", after);
    Diff diff;
    auto addChange = [&](ChangeKind kind, const FlatObject& object, std::string detail) {
        if (kind == ChangeKind::Added) ++diff.addedCount;
        else if (kind == ChangeKind::Removed) ++diff.removedCount;
        else ++diff.modifiedCount;
        if (diff.changes.size() < kMaxReviewChanges)
            diff.changes.push_back({kind, object.displayId, std::move(detail)});
        else
            diff.truncated = true;
    };
    for (const auto& [key, object] : before) {
        const auto currentObject = after.find(key);
        if (currentObject == after.end())
            addChange(ChangeKind::Removed, object, "present only in the reviewed snapshot");
        else if (object.signature != currentObject->second.signature)
            addChange(ChangeKind::Modified, currentObject->second, "properties or hierarchy changed");
    }
    for (const auto& [key, object] : after) {
        if (!before.contains(key)) addChange(ChangeKind::Added, object, "present only in the current scene");
    }
    diff.sceneResourcesChanged = sceneResourceSignature(it->doc) != sceneResourceSignature(current);
    if (diff.sceneResourcesChanged) {
        FlatObject scene{"[scene resources]", {}};
        addChange(ChangeKind::Modified, scene, "materials, metadata, imports, cameras, lights or other scene resources changed");
    }
    return diff;
}

std::size_t SceneHistory::estimateDocumentBytes(const Mc3::Mc3Document& doc) {
    std::size_t bytes = sizeof(Mc3::Mc3Document);
    for (const auto* value : {&doc.version, &doc.model, &doc.unit, &doc.coordinateSystem,
                              &doc.rotationUnits, &doc.eulerOrder})
        bytes += estimateString(*value);
    for (const auto* map : {&doc.metadata, &doc.meta})
        for (const auto& [key, value] : *map)
            bytes += kMapNodeOverhead + estimateString(key) + estimateString(value);
    for (const auto& import : doc.imports)
        bytes += sizeof(import) + estimateString(import.importNamespace) + estimateString(import.source) + estimateString(import.hash);
    for (const auto& include : doc.includes) bytes += estimateString(include);
    for (const auto* ids : {&doc.includedDefs, &doc.includedMaterials, &doc.includedTextures, &doc.includedEmbeds})
        for (const auto& id : *ids) bytes += kMapNodeOverhead + estimateString(id);
    auto estimateValueMap = [&](const auto& map) {
        for (const auto& [key, value] : map)
            bytes += kMapNodeOverhead + estimateString(key) + sizeof(value) * 2;
    };
    estimateValueMap(doc.textures);
    estimateValueMap(doc.svgTextures);
    estimateValueMap(doc.embeds);
    estimateValueMap(doc.scripts);
    estimateValueMap(doc.sounds);
    estimateValueMap(doc.musicTracks);
    estimateValueMap(doc.triggers);
    estimateValueMap(doc.sceneStates);
    estimateValueMap(doc.materials);
    estimateValueMap(doc.actions);
    bytes += doc.lights.capacity() * sizeof(Mc3::Mc3Light) * 2;
    bytes += doc.cameras.capacity() * sizeof(Mc3::Mc3Camera) * 2;
    bytes += doc.eventBindings.capacity() * sizeof(Mc3::Mc3EventBinding) * 2;
    std::set<const Mc3::Mc3Object*> visited;
    for (const auto& object : doc.objects) estimateObject(object.get(), visited, bytes);
    for (const auto& [id, definition] : doc.definitions) {
        bytes += kMapNodeOverhead + estimateString(id);
        estimateObject(definition.get(), visited, bytes);
    }
    return bytes;
}

std::size_t SceneHistory::countObjects(const Mc3::Mc3Document& doc) {
    std::size_t count = 0;
    std::set<const Mc3::Mc3Object*> visited;
    auto visit = [&](this auto& self, const Mc3::Mc3Object* object) -> void {
        if (!object || !visited.insert(object).second) return;
        ++count;
        for (const auto& child : object->children) self(child.get());
    };
    for (const auto& object : doc.objects) visit(object.get());
    for (const auto& [id, definition] : doc.definitions) {
        (void)id;
        visit(definition.get());
    }
    return count;
}

} // namespace MeshCraft::Editor
