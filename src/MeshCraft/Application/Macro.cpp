#include "MeshCraft/Application/MeshCraftApplication.hpp"

#include <functional>
#include <string>
#include <vector>

namespace MeshCraft::Application {

// SYS-W3-01 Phase 3: MacroRecorder itself is now CNA-free (Editor/
// MacroRecorder.cpp) -- the only thing left here is wiring its callback
// Context to this class's real methods/fields, built fresh at each
// play()/save()/load() call site (see MacroRecorder.hpp's own comment for
// why per-call rather than stored).
Editor::MacroRecorder::Context MeshCraftApplication::macroContext() {
    Editor::MacroRecorder::Context ctx;
    ctx.addPrimitive = [this](Mc3::ObjectType type) { addPrimitive(type); };
    ctx.deleteSelected = [this] { deleteSelected(); };
    ctx.duplicateSelected = [this] { duplicateSelected(); };
    ctx.groupSelected = [this] { groupSelected(); };
    ctx.ungroupSelected = [this] { ungroupSelected(); };
    ctx.groupScaleSelected = [this](float factor) {
        groupScaleFactor_ = factor;
        groupScaleSelected();
    };
    ctx.batchRenameSelected = [this](const std::string& name) {
        copyToBuf(batchRenameBuf_, name);
        batchRenameSelected();
    };
    ctx.linearArrayDuplicate = [this](int count, int axis, float spacing) {
        arrayDupCount_   = count;
        arrayDupAxis_    = axis;
        arrayDupSpacing_ = spacing;
        arrayDuplicate();
    };
    ctx.hideSelected = [this] {
        pushUndo();
        for (auto& s : selection_.selection()) s->visible = false;
        modified_ = true;
        updateWindowTitle();
    };
    ctx.showAllObjects = [this] {
        pushUndo();
        std::function<void(std::vector<std::shared_ptr<Mc3::Mc3Object>>&)> showAll;
        showAll = [&](auto& list) {
            for (auto& o : list) { o->visible = true; showAll(o->children); }
        };
        showAll(document_.objects);
        modified_ = true;
        updateWindowTitle();
    };
    ctx.lockSelected = [this] {
        for (const auto& s : selection_.selection()) objectLockState_.lock(s->id);
    };
    ctx.unlockSelected = [this] {
        for (const auto& s : selection_.selection()) objectLockState_.unlock(s->id);
    };
    ctx.setStatusMsg = [this](std::string msg, bool isError, float duration) {
        setStatusMsg(std::move(msg), isError, duration);
    };
    return ctx;
}

} // namespace MeshCraft::Application
