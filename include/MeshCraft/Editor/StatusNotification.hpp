#pragma once

#include <string>

namespace MeshCraft::Editor {

// SYS-W3-01 Phase 10: a short-lived status-bar notification has no scene,
// UI, or renderer dependency. The application decides where to display it;
// this class owns only replacement and expiry semantics.
class StatusNotification {
public:
    void show(std::string message, bool isError = false, float duration = 3.0f);
    void advance(float elapsedSeconds);

    [[nodiscard]] bool active() const { return remainingSeconds_ > 0.0f; }
    [[nodiscard]] bool isError() const { return isError_; }
    [[nodiscard]] const std::string& message() const { return message_; }

private:
    std::string message_;
    float remainingSeconds_{0.0f};
    bool isError_{false};
};

} // namespace MeshCraft::Editor
