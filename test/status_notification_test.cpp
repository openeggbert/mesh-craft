#include "MeshCraft/Editor/StatusNotification.hpp"

#include <cstdio>

using MeshCraft::Editor::StatusNotification;

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (condition) std::printf("PASS: %s\n", message);
    else { std::printf("FAIL: %s\n", message); ++failures; }
}
} // namespace

int main() {
    StatusNotification notification;
    check(!notification.active(), "starts inactive");

    notification.show("Saved", false, 2.0f);
    check(notification.active() && !notification.isError() && notification.message() == "Saved",
          "success notification stores content and duration");
    notification.advance(1.5f);
    check(notification.active(), "notification stays active before its duration elapses");
    notification.advance(0.5f);
    check(!notification.active(), "notification expires at its duration");

    notification.show("Failed", true, 3.0f);
    notification.advance(1.0f);
    notification.show("Replaced", false, 0.25f);
    check(notification.active() && !notification.isError() && notification.message() == "Replaced",
          "new notification replaces message, severity, and remaining duration");
    notification.advance(1.0f);
    check(!notification.active(), "expiry remains inactive after overshooting duration");

    notification.show("Hidden", true, 0.0f);
    check(!notification.active() && notification.isError() && notification.message() == "Hidden",
          "zero-duration notification is not displayed but preserves latest state");
    return failures == 0 ? 0 : 1;
}
