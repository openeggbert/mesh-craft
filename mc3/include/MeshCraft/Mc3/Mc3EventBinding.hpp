#pragma once

#include <string>

namespace MeshCraft::Mc3 {

// A document-level event-to-behaviour connection.  The source is the id of
// an ordinary object or an Area; the target is deliberately a named MC3
// trigger or scene state rather than executable code.  This keeps authored
// event graphs serialisable and lets an editor preview dispatch safely.
enum class EventBindingEvent { Enter, Exit, Click, Timer };
enum class EventBindingTarget { Trigger, SceneState };

struct Mc3EventBinding {
    std::string id;
    std::string sourceObjectId;
    EventBindingEvent event{EventBindingEvent::Enter};
    EventBindingTarget targetType{EventBindingTarget::Trigger};
    std::string targetId;
    bool enabled{true};
    float cooldown{0.0f};
    bool once{false};
    // Used only by Timer bindings. Values <= 0 are treated as one second by
    // the simulator so malformed authored data cannot create an unbounded
    // per-frame dispatch loop.
    float interval{1.0f};
};

} // namespace MeshCraft::Mc3
