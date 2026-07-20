// SYS-W14-18 (2026-07-20) -- LuaScriptRunner is the class backing MeshCraft's
// new scripting support: Mc3Object::scriptId/doc.scripts (type "lua")
// parsed/serialized/round-tripped/were editable for a while, but nothing
// ever interpreted a script's source. Unlike the CNA-coupled UI wiring
// (the Scripts tab's "Run Script" button, the Triggers tab's run-script
// step), LuaScriptRunner itself is CNA-free -- this exercises it directly
// with REAL Lua execution, not a mirrored control-flow stand-in.

#include "MeshCraft/Editor/LuaScriptRunner.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <cstdio>
#include <memory>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Editor;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) std::printf("PASS: %s\n", msg.c_str());
    else      { std::printf("FAIL: %s\n", msg.c_str()); ++failures; }
}

static std::shared_ptr<Mc3Object> makeObj(const std::string& id, const std::string& name) {
    auto o = std::make_shared<Mc3Object>();
    o->id = id;
    o->name = name;
    return o;
}

int main() {
    LuaScriptRunner runner;

    // --- Empty source is a legitimate no-op, not an error. ---
    {
        Mc3Document doc;
        std::string err = runner.run("", doc, nullptr);
        check(err.empty(), "Empty source: no-op, returns \"\"");
    }

    // --- A plain valid script (no API calls) succeeds. ---
    {
        Mc3Document doc;
        std::string err = runner.run("local x = 1 + 2", doc, nullptr);
        check(err.empty(), "Plain valid script: succeeds (got: " + err + ")");
    }

    // --- Lua syntax error is reported, not crashed. ---
    {
        Mc3Document doc;
        std::string err = runner.run("this is not valid lua (((", doc, nullptr);
        check(!err.empty(), "Syntax error: reported as a non-empty error string");
        check(err.find("Lua error") != std::string::npos,
              "Syntax error: message identifies it as a Lua error (got: " + err + ")");
    }

    // --- Lua runtime error (calling a nil global) is reported. ---
    {
        Mc3Document doc;
        std::string err = runner.run("this_function_does_not_exist()", doc, nullptr);
        check(!err.empty(), "Runtime error: reported as a non-empty error string (got: " + err + ")");
    }

    // --- Sandbox: os/io/debug/package are nil; require() throws a clear
    // sandbox-specific message rather than doing anything. ---
    {
        Mc3Document doc;
        std::string err = runner.run("if os ~= nil then error('os leaked') end", doc, nullptr);
        check(err.empty(), "Sandbox: os is nil, not leaked into the script (got: " + err + ")");
    }
    {
        Mc3Document doc;
        std::string err = runner.run("if io ~= nil then error('io leaked') end", doc, nullptr);
        check(err.empty(), "Sandbox: io is nil, not leaked into the script (got: " + err + ")");
    }
    {
        Mc3Document doc;
        std::string err = runner.run("require('anything')", doc, nullptr);
        check(!err.empty(), "Sandbox: require() is blocked, not silently allowed");
        check(err.find("not allowed") != std::string::npos,
              "Sandbox: require()'s error names the reason (got: " + err + ")");
    }

    // --- Infinite loop is aborted by the instruction budget, not left to
    // hang the process forever. ---
    {
        Mc3Document doc;
        std::string err = runner.run("while true do end", doc, nullptr);
        check(!err.empty(), "Infinite loop: aborted, not hung (test process is still alive to check this)");
        check(err.find("budget") != std::string::npos || err.find("instruction") != std::string::npos,
              "Infinite loop: error message names the actual reason (got: " + err + ")");
    }

    // --- scene:find() by id, by name, and the not-found case. ---
    {
        Mc3Document doc;
        auto root = makeObj("root_id", "RootName");
        auto child = makeObj("child_id", "ChildName");
        root->children.push_back(child);
        doc.objects.push_back(root);

        std::string err = runner.run(
            "local a = scene:find('root_id'); if a == nil then error('id lookup failed') end\n"
            "local b = scene:find('ChildName'); if b == nil then error('name lookup failed') end\n"
            "local c = scene:find('nope'); if c ~= nil then error('should be nil') end\n",
            doc, nullptr);
        check(err.empty(), "scene:find(): id lookup, name lookup, and not-found-is-nil all correct "
              "(got: " + err + ")");
    }

    // --- ObjectHandle position/rotation/scale/visible/material actually
    // mutate the real Mc3Object, not a detached copy. ---
    {
        Mc3Document doc;
        auto obj = makeObj("obj1", "Obj1");
        doc.objects.push_back(obj);

        std::string err = runner.run(
            "local h = scene:find('obj1')\n"
            "h:set_position(1.0, 2.0, 3.0)\n"
            "h:set_rotation(10.0, 20.0, 30.0)\n"
            "h:set_scale(2.0, 2.0, 2.0)\n"
            "h:set_visible(false)\n"
            "h:set_material('brick')\n",
            doc, nullptr);
        check(err.empty(), "Property read/write script: runs without error (got: " + err + ")");
        check(obj->transform.position[0] == 1.0f && obj->transform.position[1] == 2.0f &&
              obj->transform.position[2] == 3.0f, "set_position() mutated the real object's transform");
        check(obj->transform.rotation[0] == 10.0f && obj->transform.rotation[1] == 20.0f &&
              obj->transform.rotation[2] == 30.0f, "set_rotation() mutated the real object's transform");
        check(obj->transform.scale[0] == 2.0f && obj->transform.scale[1] == 2.0f &&
              obj->transform.scale[2] == 2.0f, "set_scale() mutated the real object's transform");
        check(!obj->visible, "set_visible(false) mutated the real object");
        check(obj->material == "brick", "set_material() mutated the real object");
    }

    // --- get_* round-trips what was set directly in C++ (not just what
    // the script itself wrote). ---
    {
        Mc3Document doc;
        auto obj = makeObj("obj2", "Obj2");
        obj->transform.position = {5.0f, 6.0f, 7.0f};
        obj->visible = true;
        doc.objects.push_back(obj);

        std::string err = runner.run(
            "local h = scene:find('obj2')\n"
            "local x,y,z = h:get_position()\n"
            "if x ~= 5.0 or y ~= 6.0 or z ~= 7.0 then error('position mismatch') end\n"
            "if h:get_visible() ~= true then error('visible mismatch') end\n"
            "if h.name ~= 'Obj2' then error('name mismatch') end\n"
            "if h.id ~= 'obj2' then error('id mismatch') end\n",
            doc, nullptr);
        check(err.empty(), "get_position()/get_visible()/.name/.id read the real object's "
              "C++-set values correctly (got: " + err + ")");
    }

    // --- def:place()/place_at()/has_socket() with target == nullptr
    // reports a clear error instead of crashing or silently no-op'ing. ---
    {
        Mc3Document doc;
        std::string err = runner.run("def:place('child', 'some_def', 'socket1')", doc, nullptr);
        check(!err.empty(), "def:place() with no target: reports an error, doesn't crash");
        check(err.find("target") != std::string::npos,
              "def:place() with no target: error names the actual reason (got: " + err + ")");
    }

    // --- def:place() with a real target + valid socket + resolvable
    // definitionRef creates the Instance child, mirroring mesh-world's
    // own Mc3ScriptRunner contract exactly. ---
    {
        Mc3Document doc;
        auto target = makeObj("wall1", "Wall1");
        target->assetMetadata = Mc3AssetMetadata{};
        target->assetMetadata->sockets["door_socket"] = {1.0f, 0.0f, 0.5f};
        doc.definitions["door.simple"] = makeObj("door.simple", "door.simple");

        std::string err = runner.run(
            "def:place('front_door', 'door.simple', 'door_socket')", doc, target.get());
        check(err.empty(), "def:place() with a valid socket+definition: succeeds (got: " + err + ")");
        check(target->children.size() == 1, "def:place(): exactly one child was placed");
        if (!target->children.empty()) {
            auto& placed = target->children[0];
            check(placed->type == ObjectType::Instance, "def:place(): placed child is an Instance");
            check(placed->definition == "door.simple", "def:place(): Instance references the right definition");
            check(placed->transform.position[0] == 1.0f && placed->transform.position[2] == 0.5f,
                  "def:place(): Instance positioned at the socket's own coordinates");
        }
    }

    // --- def:place_at() places at raw coordinates, independent of any socket. ---
    {
        Mc3Document doc;
        auto target = makeObj("wall2", "Wall2");
        doc.definitions["module.a"] = makeObj("module.a", "module.a");

        std::string err = runner.run(
            "def:place_at('m1', 'module.a', 4.0, 0.0, 0.0)", doc, target.get());
        check(err.empty(), "def:place_at(): succeeds without any assetMetadata/sockets required "
              "(got: " + err + ")");
        check(target->children.size() == 1 &&
              target->children[0]->transform.position[0] == 4.0f,
              "def:place_at(): Instance placed at the raw coordinates given");
    }

    // --- def:has_socket() reflects the target's real assetMetadata. ---
    {
        Mc3Document doc;
        auto target = makeObj("wall3", "Wall3");
        target->assetMetadata = Mc3AssetMetadata{};
        target->assetMetadata->sockets["s1"] = {0.0f, 0.0f, 0.0f};

        std::string err = runner.run(
            "if not def:has_socket('s1') then error('s1 should exist') end\n"
            "if def:has_socket('s2') then error('s2 should not exist') end\n",
            doc, target.get());
        check(err.empty(), "def:has_socket(): correctly reflects the target's real sockets "
              "(got: " + err + ")");
    }

    // --- def:place() with an unresolved definitionRef reports a clear
    // error (not a dangling/silent reference). ---
    {
        Mc3Document doc;
        auto target = makeObj("wall4", "Wall4");
        target->assetMetadata = Mc3AssetMetadata{};
        target->assetMetadata->sockets["s1"] = {0.0f, 0.0f, 0.0f};

        std::string err = runner.run(
            "def:place('c', 'never_defined', 's1')", doc, target.get());
        check(!err.empty(), "def:place() with an unresolved definitionRef: reports an error");
        check(err.find("unknown definition") != std::string::npos,
              "def:place() with an unresolved definitionRef: names the reason (got: " + err + ")");
        check(target->children.empty(),
              "def:place() with an unresolved definitionRef: no dangling child was created anyway");
    }

    if (failures == 0) { std::printf("All LuaScriptRunner tests passed.\n"); return 0; }
    std::fprintf(stderr, "%d LuaScriptRunner test(s) failed.\n", failures);
    return 1;
}
