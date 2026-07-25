#pragma once

#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <functional>
#include <string>
#include <vector>

namespace MeshCraft::Editor {

// SYS-W3-01 Phase 3: the macro recorder/player extracted out of
// MeshCraftApplication (H12). Deferred from Phase 1/2 because
// executeMacroStep() calls 8+ other MeshCraftApplication methods plus
// direct field access (document_.objects, selection_, objectLockState_,
// modified_/updateWindowTitle) -- unlike KeybindingManager/Preferences,
// which had no cross-domain entanglement, playback genuinely needs to call
// back into its owner. Resolved with a PropertiesContext-style callback
// struct (Context), built fresh and passed per-call rather than stored --
// matching ObjectIndex's "dependencies passed as parameters, not held"
// idiom, since play()/save()/load() are all on-demand button-press actions
// with no per-frame staleness risk. This class itself owns only the step
// list and the recording flag.
class MacroRecorder {
public:
    struct Step {
        std::string              verb;
        std::vector<std::string> args;
    };

    struct Context {
        std::function<void(Mc3::ObjectType)>          addPrimitive;
        std::function<void()>                         deleteSelected;
        std::function<void()>                         duplicateSelected;
        std::function<void()>                         groupSelected;
        std::function<void()>                         ungroupSelected;
        std::function<void(float)>                    groupScaleSelected;
        std::function<void(const std::string&)>       batchRenameSelected;
        std::function<void(int, int, float)>          linearArrayDuplicate;
        std::function<void()>                         hideSelected;
        std::function<void()>                         showAllObjects;
        std::function<void()>                         lockSelected;
        std::function<void()>                         unlockSelected;
        std::function<void(std::string, bool, float)> setStatusMsg;
    };

    [[nodiscard]] bool isRecording() const { return isRecording_; }
    [[nodiscard]] int  stepCount() const { return static_cast<int>(steps_.size()); }
    [[nodiscard]] const std::vector<Step>& steps() const { return steps_; }

    void startRecording() { isRecording_ = true; steps_.clear(); }
    void stopRecording() { isRecording_ = false; }
    void clear() { steps_.clear(); isRecording_ = false; }

    // No-op while not recording -- callers don't need to check isRecording()
    // first, mirroring the pre-extraction recordStep()'s own early-return.
    void recordStep(const std::string& verb, std::vector<std::string> args = {});

    void play(const Context& ctx);
    void save(const std::string& path, const Context& ctx) const;
    void load(const std::string& path, const Context& ctx);

private:
    void executeStep(const Step& step, const Context& ctx);

    bool             isRecording_{false};
    std::vector<Step> steps_;
};

} // namespace MeshCraft::Editor
