#include "MeshCraft/Editor/LuaScriptRunner.hpp"

#include "MeshCraft/EditorCommandAlgorithms.hpp"
#include "MeshCraft/Editor/LuaMemoryBudget.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <vector>

using namespace MeshCraft::Mc3;

namespace MeshCraft::Editor {

namespace {

bool finite(float value) { return std::isfinite(value); }

void requireFinite(const std::array<float, 3>& values, const char* field)
{
    for (const float value : values) {
        if (!finite(value))
            throw std::runtime_error(std::string(field) + " must contain only finite values");
    }
}

Mc3Document deepCopyDocumentForScript(const Mc3Document& source)
{
    Mc3Document copy = source;
    copy.objects.clear();
    for (const auto& object : source.objects) {
        if (!object) throw std::runtime_error("document contains a null root object");
        copy.objects.push_back(MeshCraft::deepCopyObjectAlg(*object));
    }
    copy.definitions.clear();
    for (const auto& [key, object] : source.definitions) {
        if (!object) throw std::runtime_error("document definition '" + key + "' is null");
        copy.definitions[key] = MeshCraft::deepCopyObjectAlg(*object);
    }
    return copy;
}

void collectObjectsById(std::vector<std::shared_ptr<Mc3Object>>& objects,
                        const std::string& id, std::vector<Mc3Object*>& matches,
                        int depth = 0)
{
    if (depth > 256) throw std::runtime_error("object nesting exceeds 256 levels");
    for (const auto& object : objects) {
        if (!object) throw std::runtime_error("document contains a null object");
        if (object->id == id) matches.push_back(object.get());
        collectObjectsById(object->children, id, matches, depth + 1);
    }
}

Mc3Object* resolveTransactionalTarget(Mc3Document& source, Mc3Document& working,
                                      Mc3Object* target)
{
    if (!target) return nullptr;
    if (target->id.empty())
        throw std::runtime_error("script target needs a non-empty unique object id");

    std::vector<Mc3Object*> sourceMatches;
    collectObjectsById(source.objects, target->id, sourceMatches);
    if (sourceMatches.size() != 1 || sourceMatches.front() != target)
        throw std::runtime_error("script target is detached or its object id is ambiguous");

    std::vector<Mc3Object*> workingMatches;
    collectObjectsById(working.objects, target->id, workingMatches);
    if (workingMatches.size() != 1)
        throw std::runtime_error("could not resolve the script target in the transaction copy");
    return workingMatches.front();
}

void validateScriptObjectTree(const std::vector<std::shared_ptr<Mc3Object>>& objects,
                              const Mc3Document& document,
                              std::unordered_set<const Mc3Object*>& visited,
                              int depth = 0)
{
    if (depth > 256) throw std::runtime_error("validation rejected object nesting beyond 256 levels");
    for (const auto& object : objects) {
        if (!object) throw std::runtime_error("validation rejected a null object");
        if (!visited.insert(object.get()).second)
            throw std::runtime_error("validation rejected a repeated/cyclic object reference");
        requireFinite(object->transform.position, "object position");
        requireFinite(object->transform.rotation, "object rotation");
        requireFinite(object->transform.scale, "object scale");
        requireFinite(object->transform.pivot, "object pivot");
        if (!object->material.empty() && !document.materials.count(object->material))
            throw std::runtime_error("validation rejected unknown material: " + object->material);
        if (!object->materialOverride.empty() && !document.materials.count(object->materialOverride))
            throw std::runtime_error("validation rejected unknown material override: " +
                                     object->materialOverride);
        for (const auto& [_, state] : object->states) {
            if (state.position) requireFinite(*state.position, "state position");
            if (state.rotation) requireFinite(*state.rotation, "state rotation");
            if (state.scale) requireFinite(*state.scale, "state scale");
        }
        validateScriptObjectTree(object->children, document, visited, depth + 1);
    }
}

void validateScriptResult(const Mc3Document& document)
{
    std::unordered_set<const Mc3Object*> visited;
    validateScriptObjectTree(document.objects, document, visited);
    for (const auto& [_, definition] : document.definitions) {
        if (!definition) throw std::runtime_error("validation rejected a null definition");
        validateScriptObjectTree({definition}, document, visited);
    }

    // Re-use the format validator as the final whole-document validation
    // boundary. The semantic checks above additionally reject values that Lua
    // can create directly but a parse-time sanitizer would otherwise repair.
    Mc3Validation validation;
    document.validate(validation);
    if (validation.hasErrors()) {
        for (const auto& entry : validation.entries) {
            if (entry.severity == Mc3ValidationSeverity::Error)
                throw std::runtime_error("validation rejected script result: " + entry.message);
        }
    }
}

// No equivalent already exists on Mc3Document itself (MeshCraftApplication's
// own flatFindById/flatFindByName are private members of that CNA-coupled
// class, not reachable from here). Two full passes, not id-or-name checked
// per level while descending -- an id match anywhere in the tree must win
// over a shallower name match, and checking both per-level would get that
// priority wrong for a deep id match vs. a shallow name match.
Mc3Object* findByIdRecursive(std::vector<std::shared_ptr<Mc3Object>>& list, const std::string& id) {
    for (auto& o : list) {
        if (!o) continue;
        if (o->id == id) return o.get();
        if (auto* found = findByIdRecursive(o->children, id)) return found;
    }
    return nullptr;
}

Mc3Object* findByNameRecursive(std::vector<std::shared_ptr<Mc3Object>>& list, const std::string& name) {
    for (auto& o : list) {
        if (!o) continue;
        if (o->name == name) return o.get();
        if (auto* found = findByNameRecursive(o->children, name)) return found;
    }
    return nullptr;
}

Mc3Object* findByIdOrName(Mc3Document& doc, const std::string& key) {
    if (auto* o = findByIdRecursive(doc.objects, key)) return o;
    return findByNameRecursive(doc.objects, key);
}

// The `def` global -- R103/R104 compose-time socket-placement API,
// unchanged from mesh-world's own Mc3ScriptRunner::PlacementApi except
// `target` is nullable here (mesh-world's compose-time caller always has
// a real target; MeshCraft's own call sites -- the Scripts tab's preview
// button, a trigger's run-script step -- don't always have one).
struct PlacementApi {
    Mc3Object*    target; // may be nullptr
    Mc3Document*  doc;

    void place_instance(const std::string& childId, const std::string& definitionRef,
                         std::array<float, 3> position) const {
        if (!target)
            throw std::runtime_error(
                "def:place()/place_at() need a target object -- select one first, or run "
                "this script from its own Properties panel Script field");
        if (!doc->definitions.count(definitionRef))
            throw std::runtime_error(
                "unknown definition (not resolved/imported): " + definitionRef);
        requireFinite(position, "placement position");

        auto instance                = std::make_shared<Mc3Object>();
        instance->type               = ObjectType::Instance;
        instance->name               = childId;
        instance->id                 = childId;
        instance->definition         = definitionRef;
        instance->transform.position = position;
        target->children.push_back(instance);
    }

    void place(const std::string& childId, const std::string& definitionRef,
               const std::string& socketName) const {
        if (!target)
            throw std::runtime_error(
                "def:place() needs a target object -- select one first, or run this "
                "script from its own Properties panel Script field");
        if (!target->assetMetadata.has_value())
            throw std::runtime_error("target has no assetMetadata (no sockets declared)");

        const auto socket_it = target->assetMetadata->sockets.find(socketName);
        if (socket_it == target->assetMetadata->sockets.end())
            throw std::runtime_error("unknown socket: " + socketName);

        place_instance(childId, definitionRef, socket_it->second);
    }

    void place_at(const std::string& childId, const std::string& definitionRef,
                  float x, float y, float z) const {
        place_instance(childId, definitionRef, {x, y, z});
    }

    bool has_socket(const std::string& socketName) const {
        return target && target->assetMetadata.has_value() &&
               target->assetMetadata->sockets.count(socketName) > 0;
    }
};

// The `scene` global's per-object handle. Holds a non-owning pointer --
// valid for the whole script execution (doc/its objects outlive the call,
// and no exposed method here adds/removes objects, so no handle can
// dangle mid-script).
struct ObjectHandle {
    Mc3Object* obj;
    const Mc3Document* doc;

    std::string get_name() const { return obj->name; }
    std::string get_id()   const { return obj->id; }

    std::tuple<float, float, float> get_position() const {
        return {obj->transform.position[0], obj->transform.position[1], obj->transform.position[2]};
    }
    void set_position(float x, float y, float z) {
        const std::array<float, 3> value{x, y, z};
        requireFinite(value, "position");
        obj->transform.position = value;
    }

    std::tuple<float, float, float> get_rotation() const {
        return {obj->transform.rotation[0], obj->transform.rotation[1], obj->transform.rotation[2]};
    }
    void set_rotation(float x, float y, float z) {
        const std::array<float, 3> value{x, y, z};
        requireFinite(value, "rotation");
        obj->transform.rotation = value;
    }

    std::tuple<float, float, float> get_scale() const {
        return {obj->transform.scale[0], obj->transform.scale[1], obj->transform.scale[2]};
    }
    void set_scale(float x, float y, float z) {
        const std::array<float, 3> value{x, y, z};
        requireFinite(value, "scale");
        obj->transform.scale = value;
    }

    bool get_visible() const { return obj->visible; }
    void set_visible(bool v) { obj->visible = v; }

    std::string get_material() const { return obj->material; }
    void set_material(const std::string& m) {
        if (!m.empty() && !doc->materials.count(m))
            throw std::runtime_error("unknown material: " + m);
        obj->material = m;
    }
};

// The `scene` global itself -- broader than mesh-world's scope, added
// per explicit request for read/write object property access beyond
// just socket placement.
struct SceneApi {
    Mc3Document* doc;

    sol::object find(const std::string& key, sol::this_state ts) const {
        Mc3Object* found = findByIdOrName(*doc, key);
        if (!found) return sol::nil;
        return sol::make_object(ts, ObjectHandle{found, doc});
    }
};

// Safety net beyond mesh-world's own reference implementation (which has
// none -- acceptable for an offline/CLI tool, not acceptable for an
// interactive editor). LUA_MASKCOUNT fires this hook every N VM
// instructions; a script under the budget never triggers it at all, so
// no external counter/state is needed -- the hook itself always errors
// when it fires, which only happens past the threshold.
constexpr int kInstructionBudget = 50'000'000;
void instructionBudgetHook(lua_State* L, lua_Debug*) {
    luaL_error(L, "script exceeded its execution budget (%d VM instructions) -- "
                  "likely an infinite loop", kInstructionBudget);
}

} // namespace

// GCC's -Warray-bounds= fires a false positive inside sol2's own heavily-
// templated new_usertype()/set_field() instantiations below (sibling
// calls using different-length string-literal keys confuse its
// array-bounds analysis across the inlined template chain) -- not a real
// bug, and it's sol2's own vendored header code being instantiated here,
// not a mistake in this function. Same "not our code" rationale as
// lua54's own -w/-Wno-warning suppression in CMakeLists.txt.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
std::string LuaScriptRunner::run(const std::string& source, Mc3Document& doc,
                                 Mc3Object* target,
                                 const std::function<void()>& beforeCommit) {
    if (source.empty()) return ""; // matches Mc3Script::hasSource()'s own "empty is a no-op" convention

    try {
        Mc3Document working = deepCopyDocumentForScript(doc);
        Mc3Object* workingTarget = resolveTransactionalTarget(doc, working, target);

        LuaMemoryBudget memoryBudget;
        sol::state lua(sol::default_at_panic, budgetedLuaAllocator, &memoryBudget);
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

        // Sandbox: identical discipline to mesh-world's LuaRuntime/
        // Mc3ScriptRunner -- no filesystem/process/module access.
        for (const char* g : {"io", "os", "debug", "package",
                               "dofile", "loadfile", "load", "collectgarbage"}) {
            lua[g] = sol::nil;
        }
        lua.set_function("require", [](const std::string& name) -> sol::object {
            throw std::runtime_error(
                "require is not allowed in the MeshCraft script sandbox: " + name);
        });

        lua_sethook(lua.lua_state(), instructionBudgetHook, LUA_MASKCOUNT, kInstructionBudget);

        lua.new_usertype<PlacementApi>("Mc3ScriptPlacementApi",
            sol::no_constructor,
            "place",      &PlacementApi::place,
            "place_at",   &PlacementApi::place_at,
            "has_socket", &PlacementApi::has_socket);

        lua.new_usertype<ObjectHandle>("Mc3ScriptObjectHandle",
            sol::no_constructor,
            "name",         sol::property(&ObjectHandle::get_name),
            "id",           sol::property(&ObjectHandle::get_id),
            "get_position", &ObjectHandle::get_position,
            "set_position", &ObjectHandle::set_position,
            "get_rotation", &ObjectHandle::get_rotation,
            "set_rotation", &ObjectHandle::set_rotation,
            "get_scale",    &ObjectHandle::get_scale,
            "set_scale",    &ObjectHandle::set_scale,
            "get_visible",  &ObjectHandle::get_visible,
            "set_visible",  &ObjectHandle::set_visible,
            "get_material", &ObjectHandle::get_material,
            "set_material", &ObjectHandle::set_material);

        lua.new_usertype<SceneApi>("Mc3ScriptSceneApi",
            sol::no_constructor,
            "find", &SceneApi::find);

        PlacementApi placementApi{workingTarget, &working};
        lua["def"] = &placementApi;

        SceneApi sceneApi{&working};
        lua["scene"] = &sceneApi;

        auto result = lua.safe_script(source, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            return std::string("Lua error: ") + err.what();
        }
        validateScriptResult(working);
        if (beforeCommit) beforeCommit();
        doc = std::move(working);
        return "";
    } catch (const std::exception& e) {
        return std::string("LuaScriptRunner error: ") + e.what();
    }
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace MeshCraft::Editor
