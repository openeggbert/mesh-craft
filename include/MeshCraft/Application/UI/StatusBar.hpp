#pragma once

#include <functional>
#include <string_view>

namespace MeshCraft::Application::UI {

struct StatusBarContext {
    int screenWidth;
    int screenHeight;
    int statusHeight;
    bool hasNotification;
    bool notificationIsError;
    std::string_view notificationMessage{""};
    int totalObjectCount;
    int selectedObjectCount;
    std::string_view selectedObjectName{""};
    bool hasValidation;
    int validationEntryCount;
    std::string_view validationSource{""};
    std::function<void()> openValidation;
};

class StatusBar final {
public:
    static void draw(const StatusBarContext& context);
};

} // namespace MeshCraft::Application::UI
