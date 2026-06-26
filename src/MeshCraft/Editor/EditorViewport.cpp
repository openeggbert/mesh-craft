#include "MeshCraft/Editor/EditorViewport.hpp"

namespace MeshCraft::Editor {

PickRay EditorViewport::pickRay(int mx, int my, int vX, int vY, int vW, int vH) const
{
    float ndcX = ((mx - vX) / static_cast<float>(vW)) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((my - vY) / static_cast<float>(vH)) * 2.0f;
    float asp  = static_cast<float>(vW) / static_cast<float>(vH);

    return {
        .origin    = camera_.position(),
        .direction = camera_.screenRayDirection(ndcX, ndcY, asp),
    };
}

} // namespace MeshCraft::Editor
