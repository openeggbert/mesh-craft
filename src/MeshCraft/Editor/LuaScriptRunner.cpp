#include "MeshCraft/Editor/LuaScriptRunner.hpp"

#include "MeshCraft/Mc3/Mc3Object.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <array>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

using namespace MeshCraft::Mc3;

namespace MeshCraft::Editor {

namespace {

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

    std::string get_name() const { return obj->name; }
    std::string get_id()   const { return obj->id; }

    std::tuple<float, float, float> get_position() const {
        return {obj->transform.position[0], obj->transform.position[1], obj->transform.position[2]};
    }
    void set_position(float x, float y, float z) { obj->transform.position = {x, y, z}; }

    std::tuple<float, float, float> get_rotation() const {
        return {obj->transform.rotation[0], obj->transform.rotation[1], obj->transform.rotation[2]};
    }
    void set_rotation(float x, float y, float z) { obj->transform.rotation = {x, y, z}; }

    std::tuple<float, float, float> get_scale() const {
        return {obj->transform.scale[0], obj->transform.scale[1], obj->transform.scale[2]};
    }
    void set_scale(float x, float y, float z) { obj->transform.scale = {x, y, z}; }

    bool get_visible() const { return obj->visible; }
    void set_visible(bool v) { obj->visible = v; }

    std::string get_material() const { return obj->material; }
    void set_material(const std::string& m) { obj->material = m; }
};

// The `scene` global itself -- broader than mesh-world's scope, added
// per explicit request for read/write object property access beyond
// just socket placement.
struct SceneApi {
    Mc3Document* doc;

    sol::object find(const std::string& key, sol::this_state ts) const {
        Mc3Object* found = findByIdOrName(*doc, key);
        if (!found) return sol::nil;
        return sol::make_object(ts, ObjectHandle{found});
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
std::string LuaScriptRunner::run(const std::string& source, Mc3Document& doc, Mc3Object* target) {
    if (source.empty()) return ""; // matches Mc3Script::hasSource()'s own "empty is a no-op" convention

    try {
        sol::state lua;
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

        PlacementApi placementApi{target, &doc};
        lua["def"] = &placementApi;

        SceneApi sceneApi{&doc};
        lua["scene"] = &sceneApi;

        auto result = lua.safe_script(source, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            return std::string("Lua error: ") + err.what();
        }
        return "";
    } catch (const std::exception& e) {
        return std::string("LuaScriptRunner error: ") + e.what();
    }
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace MeshCraft::Editor
