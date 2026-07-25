#include "MeshCraft/Editor/StatusNotification.hpp"

#include <utility>

namespace MeshCraft::Editor {

void StatusNotification::show(std::string message, bool isError, float duration) {
    message_ = std::move(message);
    isError_ = isError;
    remainingSeconds_ = duration;
}

void StatusNotification::advance(float elapsedSeconds) {
    if (remainingSeconds_ > 0.0f) remainingSeconds_ -= elapsedSeconds;
}

} // namespace MeshCraft::Editor
