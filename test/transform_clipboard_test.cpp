#include "MeshCraft/Editor/TransformClipboard.hpp"
#include "MeshCraft/Mc3/Mc3Object.hpp"

#include <cstdio>

using MeshCraft::Editor::TransformClipboard;
using MeshCraft::Mc3::Mc3Object;

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (condition) std::printf("PASS: %s\n", message);
    else { std::printf("FAIL: %s\n", message); ++failures; }
}
} // namespace

int main() {
    TransformClipboard clipboard;
    Mc3Object source;
    Mc3Object target;
    target.transform.position = {9.0f, 8.0f, 7.0f};

    check(!clipboard.hasValue(), "clipboard starts empty");
    check(!clipboard.pasteTo(target) && target.transform.position[0] == 9.0f,
          "empty clipboard leaves target unchanged");

    source.transform.position = {1.0f, 2.0f, 3.0f};
    source.transform.rotation = {10.0f, 20.0f, 30.0f};
    source.transform.scale = {2.0f, 3.0f, 4.0f};
    source.transform.pivot = {5.0f, 6.0f, 7.0f};
    target.transform.pivot = {-1.0f, -2.0f, -3.0f};

    clipboard.copyFrom(source);
    check(clipboard.hasValue(), "copy marks clipboard populated");
    check(clipboard.pasteTo(target), "populated clipboard pastes");
    check(target.transform.position == source.transform.position &&
              target.transform.rotation == source.transform.rotation &&
              target.transform.scale == source.transform.scale,
          "paste transfers position, rotation, and scale exactly");
    check(target.transform.pivot[0] == -1.0f && target.transform.pivot[1] == -2.0f &&
              target.transform.pivot[2] == -3.0f,
          "paste deliberately preserves target pivot");

    return failures == 0 ? 0 : 1;
}
