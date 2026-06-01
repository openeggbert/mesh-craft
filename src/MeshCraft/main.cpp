#include "MeshCraft/MeshCraftApplication.hpp"

#include <Nova3D/Context.h>

int main() {
    Nova3D::Context context;
    MeshCraft::MeshCraftApplication app(&context);
    return app.Run();
}
