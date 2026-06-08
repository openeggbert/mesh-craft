#include "MeshCraft/Renderer/SceneRenderer.hpp"

#include <Microsoft/Xna/Framework/Graphics/BufferUsage.hpp>
#include <Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp>
#include <Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp>
#include <Microsoft/Xna/Framework/MathHelper.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MeshCraft::Mc3;

namespace MeshCraft::Renderer {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SceneRenderer::SceneRenderer(GraphicsDevice& device)
    : device_(device)
{
    effect_ = std::make_unique<BasicEffect>(device_);
    effect_->VertexColorEnabled = true;

    buildUnitBox();
    buildUnitSphere(12);
    buildUnitCylinder(12);
    buildUnitCone(12);
    buildUnitPlane();
    buildWireBox();
}

// ---------------------------------------------------------------------------
// Shape builders
// ---------------------------------------------------------------------------

static uint16_t ui16(int v) { return static_cast<uint16_t>(v); }

static void storePositions(const std::vector<VertexPositionColor>& verts, RenderMesh& mesh) {
    mesh.positions.reserve(verts.size());
    for (const auto& v : verts) mesh.positions.push_back(v.Position);
}

void SceneRenderer::buildUnitBox() {
    Color c(200, 200, 200, 255);
    // 8 corners of a unit cube centered at origin
    static const float P[][3] = {
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
        { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    };
    std::vector<VertexPositionColor> verts;
    for (auto& p : P) verts.push_back({ Vector3{p[0],p[1],p[2]}, c });

    // 12 triangles (6 faces, 2 tri each)
    static const uint16_t IDX[] = {
        0,2,1, 0,3,2,  // -Z
        4,5,6, 4,6,7,  // +Z
        0,1,5, 0,5,4,  // -Y
        2,3,7, 2,7,6,  // +Y
        0,4,7, 0,7,3,  // -X
        1,2,6, 1,6,5,  // +X
    };
    unitBox_.vb = std::make_unique<VertexBuffer>(device_, 8);
    unitBox_.vb->SetData(verts.data(), 8);
    unitBox_.ib = std::make_unique<IndexBuffer>(device_, 36);
    unitBox_.ib->SetData(IDX, 36);
    unitBox_.primitiveCount = 12;
    storePositions(verts, unitBox_);
}

void SceneRenderer::buildUnitSphere(int segments) {
    Color c(200, 200, 200, 255);
    std::vector<VertexPositionColor> verts;
    std::vector<uint16_t> indices;

    int rings   = segments / 2;
    int sectors = segments;

    for (int r = 0; r <= rings; ++r) {
        float phi = std::numbers::pi_v<float> * r / rings;
        for (int s = 0; s <= sectors; ++s) {
            float theta = 2.0f * std::numbers::pi_v<float> * s / sectors;
            float x = std::sin(phi) * std::cos(theta) * 0.5f;
            float y = std::cos(phi) * 0.5f;
            float z = std::sin(phi) * std::sin(theta) * 0.5f;
            verts.push_back({ Vector3{x,y,z}, c });
        }
    }
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            int a = r * (sectors+1) + s;
            int b = a + 1;
            int c2 = (r+1) * (sectors+1) + s;
            int d  = c2 + 1;
            indices.push_back(ui16(a)); indices.push_back(ui16(c2)); indices.push_back(ui16(b));
            indices.push_back(ui16(b)); indices.push_back(ui16(c2)); indices.push_back(ui16(d));
        }
    }
    unitSphere_.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    unitSphere_.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    unitSphere_.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    unitSphere_.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    unitSphere_.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, unitSphere_);
}

void SceneRenderer::buildUnitCylinder(int segments) {
    Color c(200, 200, 200, 255);
    std::vector<VertexPositionColor> verts;
    std::vector<uint16_t> indices;

    // Top and bottom ring + center caps
    int base = 0;
    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        float x = 0.5f * std::cos(a), z = 0.5f * std::sin(a);
        verts.push_back({ Vector3{x, -0.5f, z}, c });  // bottom ring
    }
    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        float x = 0.5f * std::cos(a), z = 0.5f * std::sin(a);
        verts.push_back({ Vector3{x, 0.5f, z}, c });   // top ring
    }
    int botCenter = static_cast<int>(verts.size());
    verts.push_back({ Vector3{0,-0.5f,0}, c });
    int topCenter = static_cast<int>(verts.size());
    verts.push_back({ Vector3{0, 0.5f,0}, c });

    for (int i = 0; i < segments; ++i) {
        int j = (i+1) % segments;
        // Side quad
        indices.push_back(ui16(i)); indices.push_back(ui16(i+segments)); indices.push_back(ui16(j+segments));
        indices.push_back(ui16(i)); indices.push_back(ui16(j+segments)); indices.push_back(ui16(j));
        // Bottom cap
        indices.push_back(ui16(botCenter)); indices.push_back(ui16(j)); indices.push_back(ui16(i));
        // Top cap
        indices.push_back(ui16(topCenter)); indices.push_back(ui16(i+segments)); indices.push_back(ui16(j+segments));
    }
    unitCylinder_.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    unitCylinder_.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    unitCylinder_.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    unitCylinder_.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    unitCylinder_.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, unitCylinder_);
}

void SceneRenderer::buildUnitCone(int segments) {
    Color c(200, 200, 200, 255);
    std::vector<VertexPositionColor> verts;
    std::vector<uint16_t> indices;

    for (int i = 0; i < segments; ++i) {
        float a = 2.0f * std::numbers::pi_v<float> * i / segments;
        verts.push_back({ Vector3{0.5f*std::cos(a), -0.5f, 0.5f*std::sin(a)}, c });
    }
    int apex   = static_cast<int>(verts.size());
    verts.push_back({ Vector3{0, 0.5f, 0}, c });
    int botCtr = static_cast<int>(verts.size());
    verts.push_back({ Vector3{0,-0.5f,0}, c });

    for (int i = 0; i < segments; ++i) {
        int j = (i+1) % segments;
        // Side
        indices.push_back(ui16(i)); indices.push_back(ui16(apex)); indices.push_back(ui16(j));
        // Bottom cap
        indices.push_back(ui16(botCtr)); indices.push_back(ui16(j)); indices.push_back(ui16(i));
    }
    unitCone_.vb = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    unitCone_.vb->SetData(verts.data(), static_cast<int>(verts.size()));
    unitCone_.ib = std::make_unique<IndexBuffer>(device_, static_cast<int>(indices.size()));
    unitCone_.ib->SetData(indices.data(), static_cast<int>(indices.size()));
    unitCone_.primitiveCount = static_cast<int>(indices.size()) / 3;
    storePositions(verts, unitCone_);
}

void SceneRenderer::buildUnitPlane() {
    Color c(200, 200, 200, 255);
    std::vector<VertexPositionColor> verts = {
        { Vector3{-0.5f, 0.0f, -0.5f}, c },
        { Vector3{ 0.5f, 0.0f, -0.5f}, c },
        { Vector3{ 0.5f, 0.0f,  0.5f}, c },
        { Vector3{-0.5f, 0.0f,  0.5f}, c },
    };
    static const uint16_t IDX[] = { 0,1,2, 0,2,3 };
    unitPlane_.vb = std::make_unique<VertexBuffer>(device_, 4);
    unitPlane_.vb->SetData(verts.data(), 4);
    unitPlane_.ib = std::make_unique<IndexBuffer>(device_, 6);
    unitPlane_.ib->SetData(IDX, 6);
    unitPlane_.primitiveCount = 2;
    storePositions(verts, unitPlane_);
}

void SceneRenderer::buildWireBox() {
    // 12 edges of a unit box
    Color c(255, 165, 0, 255); // orange for selection
    static const float P[][3] = {
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
        { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    };
    std::vector<VertexPositionColor> verts;
    auto edge = [&](int a, int b) {
        verts.push_back({ Vector3{P[a][0],P[a][1],P[a][2]}, c });
        verts.push_back({ Vector3{P[b][0],P[b][1],P[b][2]}, c });
    };
    edge(0,1); edge(1,2); edge(2,3); edge(3,0);
    edge(4,5); edge(5,6); edge(6,7); edge(7,4);
    edge(0,4); edge(1,5); edge(2,6); edge(3,7);

    wireBoxLineCount_ = 12;
    wireBoxVB_ = std::make_unique<VertexBuffer>(device_, static_cast<int>(verts.size()));
    wireBoxVB_->SetData(verts.data(), static_cast<int>(verts.size()));
}

// ---------------------------------------------------------------------------
// Gizmo
// ---------------------------------------------------------------------------

void SceneRenderer::drawGizmo(const Mc3Object* obj,
                               const Matrix& view, const Matrix& proj,
                               float gizmoLength)
{
    if (!obj) return;

    float px = obj->transform.position[0];
    float py = obj->transform.position[1];
    float pz = obj->transform.position[2];
    float L  = gizmoLength;

    // 3 axis lines (6 verts)
    VertexPositionColor lineVerts[6] = {
        { {px,   py, pz}, Color(210, 60,  60,  255) },  // X from
        { {px+L, py, pz}, Color(210, 60,  60,  255) },  // X to
        { {px, py,   pz}, Color(60,  210, 60,  255) },  // Y from
        { {px, py+L, pz}, Color(60,  210, 60,  255) },  // Y to
        { {px, py, pz  }, Color(60,  60,  210, 255) },  // Z from
        { {px, py, pz+L}, Color(60,  60,  210, 255) },  // Z to
    };

    VertexBuffer lineVB(device_, 6);
    lineVB.SetData(lineVerts, 6);

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&lineVB);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, 3);
    device_.SetVertexBuffer(nullptr);

    // Small cube at each axis tip as a drag handle
    float hs = L * 0.10f;
    Color tipCols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };
    float tips[3][3] = { {px+L, py, pz}, {px, py+L, pz}, {px, py, pz+L} };
    for (int i = 0; i < 3; ++i) {
        Matrix world = Matrix::CreateScale(hs) *
                       Matrix::CreateTranslation({tips[i][0], tips[i][1], tips[i][2]});
        drawMesh(unitBox_, world, view, proj, tipCols[i]);
    }
}

void SceneRenderer::drawScaleGizmo(const Mc3Object* obj,
                                    const Matrix& view, const Matrix& proj,
                                    float gizmoLength)
{
    if (!obj) return;

    float px = obj->transform.position[0];
    float py = obj->transform.position[1];
    float pz = obj->transform.position[2];
    float L  = gizmoLength;

    VertexPositionColor lineVerts[6] = {
        { {px,   py, pz}, Color(210, 60,  60,  255) },
        { {px+L, py, pz}, Color(210, 60,  60,  255) },
        { {px, py,   pz}, Color(60,  210, 60,  255) },
        { {px, py+L, pz}, Color(60,  210, 60,  255) },
        { {px, py, pz  }, Color(60,  60,  210, 255) },
        { {px, py, pz+L}, Color(60,  60,  210, 255) },
    };

    VertexBuffer lineVB(device_, 6);
    lineVB.SetData(lineVerts, 6);

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&lineVB);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, 3);
    device_.SetVertexBuffer(nullptr);

    // Flat-square tips (thin slab perpendicular to each axis)
    float hs = L * 0.12f;
    float thin = hs * 0.25f;
    Color tipCols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };
    float tips[3][3] = { {px+L, py, pz}, {px, py+L, pz}, {px, py, pz+L} };
    // Each tip is a flat box: thin along the axis direction, wide in the other two
    Microsoft::Xna::Framework::Vector3 tipScales[3] = {
        {thin, hs, hs},  // X tip: thin on X, square in YZ
        {hs, thin, hs},  // Y tip: thin on Y, square in XZ
        {hs, hs, thin},  // Z tip: thin on Z, square in XY
    };
    for (int i = 0; i < 3; ++i) {
        Matrix world = Matrix::CreateScale(tipScales[i]) *
                       Matrix::CreateTranslation({tips[i][0], tips[i][1], tips[i][2]});
        drawMesh(unitBox_, world, view, proj, tipCols[i]);
    }
}

void SceneRenderer::drawRotateGizmo(const Mc3Object* obj,
                                     const Matrix& view, const Matrix& proj,
                                     float gizmoLength)
{
    if (!obj) return;

    const float px = obj->transform.position[0];
    const float py = obj->transform.position[1];
    const float pz = obj->transform.position[2];
    const float L  = gizmoLength;
    const int   N  = 32;

    effect_->World      = Matrix::getIdentityProperty();
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    Color cols[3] = {
        Color(210, 60,  60,  255),
        Color(60,  210, 60,  255),
        Color(60,  60,  210, 255),
    };

    for (int ax = 0; ax < 3; ++ax) {
        std::vector<VertexPositionColor> verts(N + 1);
        for (int j = 0; j <= N; ++j) {
            float t = 2.0f * std::numbers::pi_v<float> * j / N;
            float c = std::cos(t), s = std::sin(t);
            float wx, wy, wz;
            if      (ax == 0) { wx = px;       wy = py+L*c; wz = pz+L*s; } // X: circle in YZ
            else if (ax == 1) { wx = px+L*c;   wy = py;     wz = pz+L*s; } // Y: circle in XZ
            else              { wx = px+L*c;   wy = py+L*s; wz = pz;     } // Z: circle in XY
            verts[j] = { {wx, wy, wz}, cols[ax] };
        }
        VertexBuffer circleVB(device_, N + 1);
        circleVB.SetData(verts.data(), N + 1);
        device_.SetVertexBuffer(&circleVB);
        device_.DrawPrimitives(Graphics::PrimitiveType::LineStrip, 0, N);
        device_.SetVertexBuffer(nullptr);
    }
}

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------

Matrix SceneRenderer::objectWorldMatrix(const Mc3Object& obj) const {
    const auto& t = obj.transform;
    float rx = t.rotation[0] * (std::numbers::pi_v<float> / 180.0f);
    float ry = t.rotation[1] * (std::numbers::pi_v<float> / 180.0f);
    float rz = t.rotation[2] * (std::numbers::pi_v<float> / 180.0f);

    Matrix world = Matrix::CreateScale({ t.scale[0], t.scale[1], t.scale[2] });
    world = world * Matrix::CreateFromYawPitchRoll(ry, rx, rz);
    world.setTranslationProperty({ t.position[0], t.position[1], t.position[2] });
    return world;
}

Color SceneRenderer::materialColor(const std::string& matId, const Mc3Document& doc) const {
    if (!matId.empty()) {
        auto it = doc.materials.find(matId);
        if (it != doc.materials.end()) {
            const auto& bc = it->second.baseColor;
            return Color(
                static_cast<int>(std::clamp(bc[0], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[1], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[2], 0.0f, 1.0f) * 255),
                static_cast<int>(std::clamp(bc[3], 0.0f, 1.0f) * 255));
        }
    }
    return Color(180, 180, 180, 255);
}

bool SceneRenderer::isSelected(const Mc3Object& obj, const std::vector<const Mc3Object*>& sel) const {
    return std::any_of(sel.begin(), sel.end(), [&](const Mc3Object* p){ return p == &obj; });
}

void SceneRenderer::drawMesh(const RenderMesh& mesh,
                              const Matrix& world, const Matrix& view, const Matrix& proj,
                              Color color)
{
    int n = mesh.vb->VertexCount();

    // Build a temporary VB tinted with the material color (cheap for small meshes)
    std::vector<VertexPositionColor> tinted(n);
    // We don't have a way to read back VB data, so we store the original verts in RenderMesh.
    // For now, use the color directly as a uniform tint by building a new VB.
    // This works because our unit shapes have few vertices.
    for (int i = 0; i < n; ++i)
        tinted[i] = { mesh.positions[i], color };

    VertexBuffer tmpVB(device_, n);
    tmpVB.SetData(tinted.data(), n);

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
    }

    device_.SetVertexBuffer(&tmpVB);
    device_.SetIndexBuffer(mesh.ib.get());
    device_.DrawIndexedPrimitives(
        Graphics::PrimitiveType::TriangleList,
        0, 0,
        n,
        0,
        mesh.primitiveCount);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}

void SceneRenderer::drawObjectWireframe(const Mc3Object& obj,
                                         const Matrix& view, const Matrix& proj, Color color)
{
    (void)color;
    Matrix world = objectWorldMatrix(obj);

    // Scale wire box to object's bounding size
    float sx = 1.0f, sy = 1.0f, sz = 1.0f;
    if (obj.primitive) {
        const auto& p = obj.primitive.value();
        switch (obj.type) {
        case ObjectType::Box:
        case ObjectType::Cube:
            sx = p.size[0]; sy = p.size[1]; sz = p.size[2]; break;
        case ObjectType::Sphere:
            sx = sy = sz = p.radius * 2.0f; break;
        case ObjectType::Cylinder:
            sx = sz = p.radius * 2.0f; sy = p.height; break;
        case ObjectType::Cone:
            sx = sz = p.radius * 2.0f; sy = p.height; break;
        case ObjectType::Plane:
            sx = p.size[0]; sy = 0.01f; sz = p.size[2]; break;
        default: break;
        }
    }

    Matrix scaleM = Matrix::CreateScale({ sx * obj.transform.scale[0],
                                          sy * obj.transform.scale[1],
                                          sz * obj.transform.scale[2] });
    float rx = obj.transform.rotation[0] * (std::numbers::pi_v<float> / 180.0f);
    float ry = obj.transform.rotation[1] * (std::numbers::pi_v<float> / 180.0f);
    float rz = obj.transform.rotation[2] * (std::numbers::pi_v<float> / 180.0f);
    Matrix rotM = Matrix::CreateFromYawPitchRoll(ry, rx, rz);
    Matrix transM = Matrix::getIdentityProperty();
    transM.setTranslationProperty({ obj.transform.position[0],
                                    obj.transform.position[1],
                                    obj.transform.position[2] });
    Matrix wireWorld = scaleM * rotM * transM;

    effect_->World      = wireWorld;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;

    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty()) {
        pass.Apply();
    }

    device_.SetVertexBuffer(wireBoxVB_.get());
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, wireBoxLineCount_);
    device_.SetVertexBuffer(nullptr);
}

void SceneRenderer::drawObject(const Mc3Object& obj, const Mc3Document& doc,
                                const Matrix& parentWorld,
                                const Matrix& view, const Matrix& proj,
                                const std::vector<const Mc3Object*>& selected,
                                int depth)
{
    if (!obj.visible) return;
    if (depth > 16) return; // guard against infinite instance recursion

    Matrix world = objectWorldMatrix(obj) * parentWorld;
    Color color  = materialColor(obj.material, doc);
    bool  sel    = isSelected(obj, selected);

    switch (obj.type) {
    case ObjectType::Box:
    case ObjectType::Cube: {
        float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float sy = obj.primitive ? obj.primitive->size[1] : 1.0f;
        float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        Matrix m = Matrix::CreateScale({sx,sy,sz}) * world;
        drawMesh(unitBox_, m, view, proj, color);
        break;
    }
    case ObjectType::Sphere: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        Matrix m = Matrix::CreateScale({r,r,r}) * world;
        drawMesh(unitSphere_, m, view, proj, color);
        break;
    }
    case ObjectType::Cylinder: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        Matrix m = Matrix::CreateScale({r,h,r}) * world;
        drawMesh(unitCylinder_, m, view, proj, color);
        break;
    }
    case ObjectType::Cone: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        Matrix m = Matrix::CreateScale({r,h,r}) * world;
        drawMesh(unitCone_, m, view, proj, color);
        break;
    }
    case ObjectType::Plane: {
        float w = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float d = obj.primitive ? obj.primitive->size[2] : 1.0f;
        Matrix m = Matrix::CreateScale({w,1.0f,d}) * world;
        drawMesh(unitPlane_, m, view, proj, color);
        break;
    }
    case ObjectType::Group:
    case ObjectType::Union:
    case ObjectType::Difference:
    case ObjectType::Intersection:
    case ObjectType::Area:
        for (const auto& child : obj.children)
            drawObject(*child, doc, world, view, proj, selected, depth + 1);
        break;
    case ObjectType::Instance: {
        auto it = doc.definitions.find(obj.definition);
        if (it != doc.definitions.end() && it->second)
            drawObject(*it->second, doc, world, view, proj, selected, depth + 1);
        else
            drawMesh(unitBox_, world, view, proj, color); // definition not found
        break;
    }
    case ObjectType::Extrude: {
        if (!obj.extrude) { drawMesh(unitBox_, world, view, proj, color); break; }
        const auto& ex   = obj.extrude.value();
        const auto& cs   = ex.crossSection;
        const auto& path = ex.path;

        float len = (path.type == ExtrudePathType::Line) ? path.length : 1.0f;

        // Rotate so the unit Y-aligned shapes point along the chosen axis
        Matrix axisRot = Matrix::getIdentityProperty();
        if (path.type == ExtrudePathType::Line) {
            constexpr float pih = std::numbers::pi_v<float> * 0.5f;
            if (path.axis == "x")
                axisRot = Matrix::CreateRotationZ(pih);
            else if (path.axis == "z")
                axisRot = Matrix::CreateRotationX(-pih);
        }

        switch (cs.type) {
        case CrossSectionType::Rect:
            drawMesh(unitBox_,
                     Matrix::CreateScale({cs.width, len, cs.height}) * axisRot * world,
                     view, proj, color);
            break;
        case CrossSectionType::Circle:
        case CrossSectionType::Polygon: {
            float r = cs.radius * 2.0f;
            drawMesh(unitCylinder_,
                     Matrix::CreateScale({r, len, r}) * axisRot * world,
                     view, proj, color);
            break;
        }
        default:
            drawMesh(unitBox_, world, view, proj, color);
            break;
        }
        break;
    }
    default:
        // Mesh, Instance: render as a bounding box placeholder
        drawMesh(unitBox_, world, view, proj, color);
        break;
    }

    if (sel) {
        // Draw orange wireframe over selected object
        drawObjectWireframe(obj, view, proj, Color(255,165,0,255));
    }
}

// ---------------------------------------------------------------------------
// Public draw
// ---------------------------------------------------------------------------

void SceneRenderer::draw(const Mc3Document& doc,
                          const Matrix& view, const Matrix& proj,
                          const std::vector<const Mc3Object*>& selected)
{
    device_.SetDepthTestEnabled(true);
    device_.SetDepthWriteEnabled(true);

    Matrix identity = Matrix::getIdentityProperty();

    for (const auto& obj : doc.objects)
        drawObject(*obj, doc, identity, view, proj, selected);
}

} // namespace MeshCraft::Renderer
