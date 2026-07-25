#include "MeshCraft/Editor/ObjectLockState.hpp"

#include <cstdio>

using MeshCraft::Editor::ObjectLockState;

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (condition) std::printf("PASS: %s\n", message);
    else { std::printf("FAIL: %s\n", message); ++failures; }
}
} // namespace

int main() {
    ObjectLockState locks;
    check(locks.empty(), "starts empty");
    check(!locks.isLocked("cube"), "unknown object starts unlocked");

    locks.lock("cube");
    locks.lock("cube");
    check(locks.isLocked("cube") && locks.ids().size() == 1,
          "locking is idempotent");

    locks.lock("light");
    check(locks.ids().size() == 2 && locks.isLocked("light"),
          "tracks multiple object ids");

    locks.unlock("missing");
    check(locks.ids().size() == 2, "unlocking an unknown id is harmless");
    locks.unlock("cube");
    check(!locks.isLocked("cube") && locks.isLocked("light"),
          "unlocking removes only the requested id");

    locks.toggle("light");
    check(locks.empty(), "toggle unlocks a locked object");
    locks.toggle("camera");
    check(locks.isLocked("camera"), "toggle locks an unlocked object");

    return failures == 0 ? 0 : 1;
}
