#include "MeshCraft/Renderer/SceneRenderer.hpp"
#include "MeshCraft/CoordinateSystemAlgorithms.hpp"
#include "MeshCraft/Renderer/PrimitiveTessellationAlg.hpp"

#include <Microsoft/Xna/Framework/Graphics/BufferUsage.hpp>
#include <Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp>
#include <Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp>
#include <Microsoft/Xna/Framework/MathHelper.hpp>
#include <Microsoft/Xna/Framework/Vector2.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace MeshCraft::Mc3;
using namespace MeshCraft::Renderer;

namespace MeshCraft::Renderer {

// ---------------------------------------------------------------------------
// Extrude path mesh generation
// ---------------------------------------------------------------------------

namespace {

Matrix coordinateSystemRootMatrixForEdges(const Mc3Document& doc) {
    return usesRightHandedZUpAlg(doc.coordinateSystem)
        ? Matrix::CreateRotationX(-std::numbers::pi_v<float> / 2.0f)
        : Matrix::getIdentityProperty();
}

struct ExtFrame { Vector3 pos, tan, nor, bi; };

static Vector3 vn3(Vector3 v) { return Vector3::Normalize(v); }

static Vector3 ptransportNor(Vector3 nor, Vector3 newTan) {
    Vector3 n = nor - newTan * Vector3::Dot(nor, newTan);
    float len = n.Length();
    return (len < 1e-7f) ? nor : (n / len);
}

static ExtFrame initExtFrame(Vector3 startPos, Vector3 tan) {
    Vector3 worldUp = (std::abs(tan.Y) < 0.9f) ? Vector3{0.0f,1.0f,0.0f} : Vector3{1.0f,0.0f,0.0f};
    Vector3 nor = vn3(worldUp - tan * Vector3::Dot(worldUp, tan));
    return { startPos, tan, nor, Vector3::Cross(tan, nor) };
}

static Vector3 evalCubicBezier(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    float mt = 1.0f - t;
    return p0*(mt*mt*mt) + p1*(3.0f*mt*mt*t) + p2*(3.0f*mt*t*t) + p3*(t*t*t);
}

static Vector3 evalCubicBezierTan(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    float mt = 1.0f - t;
    Vector3 d = (p1-p0)*(3.0f*mt*mt) + (p2-p1)*(6.0f*mt*t) + (p3-p2)*(3.0f*t*t);
    float len = d.Length();
    return (len < 1e-7f) ? vn3(p3-p0) : d / len;
}

static std::vector<ExtFrame> makePathFrames(const Mc3ExtrudePath& path, int segs) {
    const float pi2 = 2.0f * std::numbers::pi_v<float>;
    std::vector<ExtFrame> fr;
    fr.reserve(segs + 1);

    switch (path.type) {

    case ExtrudePathType::Line: {
        Vector3 dir, nor, bi;
        if      (path.axis == "x") { dir={1,0,0}; nor={0,1,0}; bi={0,0,1}; }
        else if (path.axis == "z") { dir={0,0,1}; nor={0,1,0}; bi={-1,0,0}; }
        else                        { dir={0,1,0}; nor={1,0,0}; bi={0,0,1}; }
        for (int i = 0; i <= segs; ++i) {
            float t = static_cast<float>(i) / segs * path.length;
            fr.push_back({ dir * t, dir, nor, bi });
        }
        break;
    }

    case ExtrudePathType::Arc: {
        float R     = path.arcRadius;
        float total = path.arcAngle * (std::numbers::pi_v<float> / 180.0f);
        // Arc starts at origin, sweeps in XZ plane around center (R,0,0)
        for (int i = 0; i <= segs; ++i) {
            float theta = total * i / segs;
            float c = std::cos(theta), s = std::sin(theta);
            Vector3 pos = { R*(1.0f - c), 0.0f, R*s };
            Vector3 tan = { s, 0.0f, c };          // normalised tangent
            Vector3 nor = { c, 0.0f, -s };         // toward arc center
            Vector3 bi  = Vector3::Cross(tan, nor);
            fr.push_back({ pos, tan, nor, bi });
        }
        break;
    }

    case ExtrudePathType::Helix: {
        float R     = path.helixRadius;
        float H     = path.helixHeight;
        float omega = path.helixTurns * pi2;
        for (int i = 0; i <= segs; ++i) {
            float t     = static_cast<float>(i) / segs;
            float theta = omega * t;
            Vector3 pos = { R * std::cos(theta), H * t, R * std::sin(theta) };
            Vector3 tan = vn3({ -R * omega * std::sin(theta), H,
                                  R * omega * std::cos(theta) });
            if (fr.empty()) {
                fr.push_back(initExtFrame(pos, tan));
            } else {
                Vector3 nor = ptransportNor(fr.back().nor, tan);
                fr.push_back({ pos, tan, nor, Vector3::Cross(tan, nor) });
            }
        }
        break;
    }

    case ExtrudePathType::Polyline: {
        const auto& pts = path.points;
        if (pts.size() < 2) {
            fr.push_back(initExtFrame({0,0,0},{0,1,0}));
            fr.push_back(initExtFrame({0,1,0},{0,1,0}));
            break;
        }
        int numSeg  = static_cast<int>(pts.size()) - 1;
        int perSeg  = std::max(1, segs / numSeg);
        for (int seg = 0; seg < numSeg; ++seg) {
            const auto& a = pts[seg].position;
            const auto& b = pts[seg+1].position;
            Vector3 A = {a[0],a[1],a[2]}, B = {b[0],b[1],b[2]};
            Vector3 tan = vn3(B - A);
            int start = (seg == 0) ? 0 : 1;
            for (int s = start; s <= perSeg; ++s) {
                float t   = static_cast<float>(s) / perSeg;
                Vector3 p = A + (B - A) * t;
                if (fr.empty()) fr.push_back(initExtFrame(p, tan));
                else {
                    Vector3 nor = ptransportNor(fr.back().nor, tan);
                    fr.push_back({ p, tan, nor, Vector3::Cross(tan, nor) });
                }
            }
        }
        break;
    }

    case ExtrudePathType::Bezier: {
        const auto& pts = path.points;
        if (pts.size() < 2) {
            fr.push_back(initExtFrame({0,0,0},{0,1,0}));
            fr.push_back(initExtFrame({0,1,0},{0,1,0}));
            break;
        }
        int numCurves    = static_cast<int>(pts.size()) - 1;
        int segsPerCurve = std::max(2, segs / numCurves);
        for (int seg = 0; seg < numCurves; ++seg) {
            const auto& pp0 = pts[seg];
            const auto& pp1 = pts[seg+1];
            Vector3 P0 = {pp0.position[0], pp0.position[1], pp0.position[2]};
            Vector3 P3 = {pp1.position[0], pp1.position[1], pp1.position[2]};
            Vector3 c0 = {pp0.controlIn[0], pp0.controlIn[1], pp0.controlIn[2]};
            Vector3 c1 = {pp1.controlIn[0], pp1.controlIn[1], pp1.controlIn[2]};
            Vector3 P1 = (c0.Length() > 1e-6f) ? (P0 + c0) : Vector3::Lerp(P0, P3, 1.0f/3.0f);
            Vector3 P2 = (c1.Length() > 1e-6f) ? (P3 - c1) : Vector3::Lerp(P0, P3, 2.0f/3.0f);
            int start = (seg == 0) ? 0 : 1;
            for (int s = start; s <= segsPerCurve; ++s) {
                float t     = static_cast<float>(s) / segsPerCurve;
                Vector3 pos = evalCubicBezier(P0, P1, P2, P3, t);
                Vector3 tan = evalCubicBezierTan(P0, P1, P2, P3, t);
                if (fr.empty()) fr.push_back(initExtFrame(pos, tan));
                else {
                    Vector3 nor = ptransportNor(fr.back().nor, tan);
                    fr.push_back({ pos, tan, nor, Vector3::Cross(tan, nor) });
                }
            }
        }
        break;
    }

    } // switch
    return fr;
}

struct Pt2 { float u, v; };

static std::vector<Pt2> makeProfile(const Mc3CrossSection& cs) {
    std::vector<Pt2> pts;
    switch (cs.type) {
    case CrossSectionType::Rect: {
        float hu = cs.width  * 0.5f, hv = cs.height * 0.5f;
        pts = { {-hu,-hv},{hu,-hv},{hu,hv},{-hu,hv} };
        break;
    }
    case CrossSectionType::Circle: {
        int N = std::max(4, cs.segments);
        for (int i = 0; i < N; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / N;
            pts.push_back({ cs.radius * std::cos(a), cs.radius * std::sin(a) });
        }
        break;
    }
    case CrossSectionType::Polygon: {
        int N = std::max(3, cs.sides);
        for (int i = 0; i < N; ++i) {
            float a = 2.0f * std::numbers::pi_v<float> * i / N;
            pts.push_back({ cs.radius * std::cos(a), cs.radius * std::sin(a) });
        }
        break;
    }
    case CrossSectionType::Star: {
        int N = std::max(3, cs.sides);
        float outerR = cs.radius;
        float innerR = (cs.innerRadius > 0.0f && cs.innerRadius < cs.radius)
            ? cs.innerRadius : cs.radius * 0.5f;
        const float pi2 = 2.0f * std::numbers::pi_v<float>;
        const float offset = -std::numbers::pi_v<float> / 2.0f; // start from top
        for (int i = 0; i < N; ++i) {
            float aOuter = pi2 * i / N + offset;
            float aInner = aOuter + pi2 / (2 * N);
            pts.push_back({ outerR * std::cos(aOuter), outerR * std::sin(aOuter) });
            pts.push_back({ innerR * std::cos(aInner), innerR * std::sin(aInner) });
        }
        break;
    }
    case CrossSectionType::Custom:
        for (const auto& p : cs.customPoints)
            pts.push_back({ p.x, p.y });
        if (pts.empty())
            pts = { {-0.5f,-0.5f},{0.5f,-0.5f},{0.5f,0.5f},{-0.5f,0.5f} };
        break;
    }
    return pts;
}

} // anonymous namespace

void SceneRenderer::drawExtrudeDynamic(const Mc3Extrude& ex,
                                        const Matrix& world,
                                        const Matrix& view, const Matrix& proj,
                                        Color color, bool depthPass)
{
    auto fallback = [&] {
        if (depthPass) drawDepthMesh(unitBox_, world, view, proj);
        else drawMesh(unitBox_, world, view, proj, color);
    };
    int pathSegs = std::max(3, ex.segments);
    auto frames  = makePathFrames(ex.path, pathSegs);
    auto profile = makeProfile(ex.crossSection);

    int M = static_cast<int>(frames.size());
    int N = static_cast<int>(profile.size());
    if (M < 2 || N < 3) { fallback(); return; }

    // AUD-073: radius==0 is a legal document value (the parser only rejects
    // negatives) -- without the radius>1e-6f guard, a hollow cross-section
    // with radius=0 and innerRadius>0 (a nonsensical shape anyway: the
    // "inner" radius would be larger than the "outer" one) made
    // innerScale = innerRadius/radius evaluate to +inf below, and
    // profile[j].u/.v are already 0 when radius==0 (makeProfile scales a
    // unit circle/polygon by radius), so inf*0 produced NaN vertex
    // positions in the live viewport. Treating a near-zero-radius
    // cross-section as solid (not hollow) avoids the degenerate case
    // entirely rather than trying to define what "hollow" even means when
    // the outer radius is zero.
    bool hollow = (ex.crossSection.innerRadius > 0.0f) &&
                  (ex.crossSection.radius > 1e-6f) &&
                  (ex.crossSection.type == CrossSectionType::Circle ||
                   ex.crossSection.type == CrossSectionType::Polygon);

    // AUD-072: path segments (M) and cross-section points (N) are each
    // independently capped at parse time (kMaxTessellation=4096,
    // Mc3XmlParser.cpp), but nothing previously bounded their PRODUCT
    // before allocating -- ex.segments=4096 with a circular cross-section
    // segments=4096 built the full ~16.8M-vertex buffer (trig-computing
    // every ring point) every single frame, only to discard it at the
    // numVerts>65535 bailout that used to live at the very end of this
    // function. Compute the same vertex-count formula the ring-building
    // loops below actually produce (hollow doubles it: an inner ring is
    // built in addition to the outer one) and bail to a placeholder
    // BEFORE doing any of that work, not after -- matching AUD-064's own
    // fix to the neighboring drawGridDynamic() in this same file.
    long long vertBudget = hollow ? 2LL * M * N : static_cast<long long>(M) * N + 2;
    if (vertBudget > 65535) { fallback(); return; }

    float innerScale = hollow ? (ex.crossSection.innerRadius / ex.crossSection.radius) : 0.0f;

    float twistRad     = ex.twist * (std::numbers::pi_v<float> / 180.0f);
    float twistPerStep = (M > 1) ? twistRad / (M - 1) : 0.0f;

    std::vector<VertexPositionColor> verts;
    verts.reserve(hollow ? 2*M*N : M*N + 2);

    // Outer rings
    for (int i = 0; i < M; ++i) {
        const auto& f = frames[i];
        float angle = twistPerStep * i;
        float ca = std::cos(angle), sa = std::sin(angle);
        for (int j = 0; j < N; ++j) {
            float u = profile[j].u * ca - profile[j].v * sa;
            float v = profile[j].u * sa + profile[j].v * ca;
            verts.push_back({ f.pos + f.nor * u + f.bi * v, color });
        }
    }

    int innerBase = static_cast<int>(verts.size());
    int botCtrIdx = -1, topCtrIdx = -1;

    if (hollow) {
        // Inner rings (scaled to innerRadius)
        for (int i = 0; i < M; ++i) {
            const auto& f = frames[i];
            float angle = twistPerStep * i;
            float ca = std::cos(angle), sa = std::sin(angle);
            for (int j = 0; j < N; ++j) {
                float u = profile[j].u * innerScale * ca - profile[j].v * innerScale * sa;
                float v = profile[j].u * innerScale * sa + profile[j].v * innerScale * ca;
                verts.push_back({ f.pos + f.nor * u + f.bi * v, color });
            }
        }
    } else {
        botCtrIdx = static_cast<int>(verts.size());
        verts.push_back({ frames[0].pos,   color });
        topCtrIdx = static_cast<int>(verts.size());
        verts.push_back({ frames[M-1].pos, color });
    }

    std::vector<uint16_t> indices;
    indices.reserve(hollow ? (M-1)*N*12 + (ex.caps ? N*12 : 0)
                           : (M-1)*N*6  + (ex.caps ? N*6  : 0));

    auto oVi = [N](int ring, int pt) -> uint16_t {
        return static_cast<uint16_t>(ring * N + pt % N);
    };
    auto iVi = [N, innerBase](int ring, int pt) -> uint16_t {
        return static_cast<uint16_t>(innerBase + ring * N + pt % N);
    };

    // Outer wall
    for (int i = 0; i < M-1; ++i) {
        for (int j = 0; j < N; ++j) {
            int j1 = (j+1) % N;
            indices.push_back(oVi(i,   j));
            indices.push_back(oVi(i+1, j));
            indices.push_back(oVi(i,   j1));
            indices.push_back(oVi(i+1, j));
            indices.push_back(oVi(i+1, j1));
            indices.push_back(oVi(i,   j1));
        }
    }

    if (hollow) {
        // Inner wall (reversed winding so normals face inward)
        for (int i = 0; i < M-1; ++i) {
            for (int j = 0; j < N; ++j) {
                int j1 = (j+1) % N;
                indices.push_back(iVi(i,   j));
                indices.push_back(iVi(i,   j1));
                indices.push_back(iVi(i+1, j));
                indices.push_back(iVi(i+1, j));
                indices.push_back(iVi(i,   j1));
                indices.push_back(iVi(i+1, j1));
            }
        }
        if (ex.caps) {
            // Bottom annular cap (facing -path direction)
            for (int j = 0; j < N; ++j) {
                int j1 = (j+1) % N;
                indices.push_back(oVi(0, j));
                indices.push_back(oVi(0, j1));
                indices.push_back(iVi(0, j1));
                indices.push_back(oVi(0, j));
                indices.push_back(iVi(0, j1));
                indices.push_back(iVi(0, j));
            }
            // Top annular cap (facing +path direction)
            for (int j = 0; j < N; ++j) {
                int j1 = (j+1) % N;
                indices.push_back(oVi(M-1, j));
                indices.push_back(iVi(M-1, j));
                indices.push_back(iVi(M-1, j1));
                indices.push_back(oVi(M-1, j));
                indices.push_back(iVi(M-1, j1));
                indices.push_back(oVi(M-1, j1));
            }
        }
    } else {
        if (ex.caps) {
            for (int j = 0; j < N; ++j) {
                indices.push_back(static_cast<uint16_t>(botCtrIdx));
                indices.push_back(oVi(0, (j+1)%N));
                indices.push_back(oVi(0, j));
            }
            for (int j = 0; j < N; ++j) {
                indices.push_back(static_cast<uint16_t>(topCtrIdx));
                indices.push_back(oVi(M-1, j));
                indices.push_back(oVi(M-1, (j+1)%N));
            }
        }
    }

    if (indices.empty()) { fallback(); return; }

    int numVerts = static_cast<int>(verts.size());
    int numTris  = static_cast<int>(indices.size()) / 3;

    // Defensive backstop, not the primary guard anymore: the vertBudget
    // check above already bails before any allocation once numVerts would
    // exceed this same bound, so this should be unreachable in practice.
    // Left in place as a cheap belt-and-suspenders check against the
    // uint16_t index buffer wrapping, in case the two vertex-count
    // formulas ever drift out of sync with each other.
    if (numVerts > 65535) { fallback(); return; }

    if (depthPass) {
        drawDepthTriangles(verts, indices, world, view, proj);
        return;
    }

    VertexBuffer tmpVB(device_, numVerts);
    tmpVB.SetData(verts.data(), numVerts);

    IndexBuffer tmpIB(device_, static_cast<int>(indices.size()));
    tmpIB.SetData(indices.data(), static_cast<int>(indices.size()));

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&tmpVB);
    device_.SetIndexBuffer(&tmpIB);
    device_.DrawIndexedPrimitives(
        Graphics::PrimitiveType::TriangleList,
        0, 0, numVerts, 0, numTris);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}
// ---------------------------------------------------------------------------
// Edge overlay
// ---------------------------------------------------------------------------

void SceneRenderer::drawWireShape(const WireShape& wire,
                                   const Matrix& world, const Matrix& view, const Matrix& proj,
                                   Color color)
{
    if (wire.positions.empty()) return;
    int n = static_cast<int>(wire.positions.size());
    std::vector<VertexPositionColor> verts(n);
    for (int i = 0; i < n; ++i)
        verts[i] = { wire.positions[i], color };

    VertexBuffer vb(device_, n);
    vb.SetData(verts.data(), n);

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&vb);
    device_.DrawPrimitives(Graphics::PrimitiveType::LineList, 0, wire.lineCount);
    device_.SetVertexBuffer(nullptr);
}

void SceneRenderer::drawSilhouetteWireShape(const WireShape& wire,
                                            const Matrix& world, const Matrix& view, const Matrix& proj,
                                            const Vector3& lightDirection)
{
    if (wire.positions.size() < 2) return;
    const Vector3 origin = Vector3::Transform(Vector3::Zero, world);
    const Vector3 camera{camPosX_, camPosY_, camPosZ_};
    std::vector<VertexPositionColor> visible;
    visible.reserve(wire.positions.size());

    for (size_t i = 0; i + 1 < wire.positions.size(); i += 2) {
        const Vector3 a = Vector3::Transform(wire.positions[i], world);
        const Vector3 b = Vector3::Transform(wire.positions[i + 1], world);
        const Vector3 midpoint{(a.X + b.X) * 0.5f, (a.Y + b.Y) * 0.5f, (a.Z + b.Z) * 0.5f};
        Vector3 normal{midpoint.X - origin.X, midpoint.Y - origin.Y, midpoint.Z - origin.Z};
        Vector3 toCamera{camera.X - midpoint.X, camera.Y - midpoint.Y, camera.Z - midpoint.Z};
        if (normal.Length() < 1e-5f || toCamera.Length() < 1e-5f) continue;
        normal = Vector3::Normalize(normal);
        toCamera = Vector3::Normalize(toCamera);

        // P4: an edge whose estimated face direction is strongly facing away
        // or toward the camera is interior clutter. Keep the transition band:
        // this is the silhouette approximation for the prebuilt primitive
        // cages (their line-only representation has no full triangle graph).
        const float facing = std::abs(Vector3::Dot(normal, toCamera));
        if (facing > 0.55f) continue;

        // P3: darken edges turned away from the authored directional light,
        // rather than painting every segment opaque black. The vector points
        // along the light rays, hence the negation for surface-to-light.
        const float ndl = std::max(0.0f, Vector3::Dot(normal, Vector3{-lightDirection.X, -lightDirection.Y, -lightDirection.Z}));
        const int shade = static_cast<int>(45.0f + 145.0f * ndl);
        const Color color(shade, shade, shade, 210);
        visible.push_back({a, color});
        visible.push_back({b, color});
    }
    if (!visible.empty()) drawLineList(visible, view, proj);
}

void SceneRenderer::drawObjectEdges(const Mc3Object& obj, const Mc3Document& doc,
                                     const Matrix& parentWorld, const Matrix& view, const Matrix& proj,
                                     int depth)
{
    if (!obj.visible) return;
    if (depth > 16) return;

    Matrix world = objectWorldMatrix(obj.transform) * parentWorld;
    Color edgeColor(0, 0, 0, 220);
    Vector3 edgeLightDirection{0.0f, -1.0f, 0.0f};
    for (const auto& light : doc.lights) {
        if (light.type == LightType::Directional) {
            Vector3 candidate{light.direction[0], light.direction[1], light.direction[2]};
            if (candidate.Length() > 1e-5f) edgeLightDirection = Vector3::Normalize(candidate);
            break;
        }
    }
    auto drawOverlay = [&](const WireShape& shape, const Matrix& matrix) {
        drawSilhouetteWireShape(shape, matrix, view, proj, edgeLightDirection);
    };

    // World-space scale of each local axis from the world matrix row magnitudes.
    float rowMagX = std::sqrt(world.M11*world.M11 + world.M12*world.M12 + world.M13*world.M13);
    float rowMagY = std::sqrt(world.M21*world.M21 + world.M22*world.M22 + world.M23*world.M23);
    float rowMagZ = std::sqrt(world.M31*world.M31 + world.M32*world.M32 + world.M33*world.M33);
    float maxScale = std::max({rowMagX, rowMagY, rowMagZ});
    // Push ~0.25 world units outward, irrespective of object world-space scale.
    // Clamped to [1.003, 1.02]: lower bound keeps the old minimum for tiny objects;
    // upper bound keeps the cage visually close to the solid on large objects.
    float kPush = std::clamp(1.0f + 0.25f / std::max(maxScale, 0.001f), 1.003f, 1.02f);

    Matrix deform = obj.deform
        ? Matrix::CreateScale({obj.deform->scale[0], obj.deform->scale[1], obj.deform->scale[2]})
        : Matrix::getIdentityProperty();

    switch (obj.type) {
    case ObjectType::Box:
    case ObjectType::Cube: {
        float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float sy = obj.primitive ? obj.primitive->size[1] : 1.0f;
        float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        Matrix m = deform * Matrix::CreateScale({sx*kPush, sy*kPush, sz*kPush}) * world;
        drawOverlay(wireShapeBox_, m);
        break;
    }
    case ObjectType::Sphere: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        Matrix m = deform * Matrix::CreateScale({r*kPush, r*kPush, r*kPush}) * world;
        drawOverlay(wireShapeSphere_, m);
        break;
    }
    case ObjectType::Cylinder: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        Matrix m = deform * Matrix::CreateScale({r*kPush, h*kPush, r*kPush}) * world;
        drawOverlay(wireShapeCylinder_, m);
        break;
    }
    case ObjectType::Cone: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height         : 1.0f;
        Matrix m = deform * Matrix::CreateScale({r*kPush, h*kPush, r*kPush}) * world;
        drawOverlay(wireShapeCone_, m);
        break;
    }
    case ObjectType::Plane: {
        float w = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float d = obj.primitive ? obj.primitive->size[2] : 1.0f;
        Matrix m = deform * Matrix::CreateScale({w*kPush, 1.0f, d*kPush}) * world;
        drawOverlay(wireShapePlane_, m);
        break;
    }
    case ObjectType::Torus: {
        float R = obj.primitive ? obj.primitive->majorRadius : 0.35f;
        float r = obj.primitive ? obj.primitive->minorRadius : 0.15f;
        float sxz = R / 0.35f * kPush;
        float sy  = r / 0.15f * kPush;
        Matrix m = deform * Matrix::CreateScale({sxz, sy, sxz}) * world;
        drawOverlay(wireShapeTorus_, m);
        break;
    }
    case ObjectType::Capsule: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float h = obj.primitive ? obj.primitive->height : 1.0f;
        float sxz = r * kPush;
        float sy  = (h + r) / 2.0f * kPush;
        Matrix m = deform * Matrix::CreateScale({sxz, sy, sxz}) * world;
        drawOverlay(wireShapeCapsule_, m);
        break;
    }
    case ObjectType::Disk: {
        float r = obj.primitive ? obj.primitive->radius : 0.5f;
        float s = r * 2.0f * kPush;
        Matrix m = deform * Matrix::CreateScale({s, 1.0f, s}) * world;
        drawOverlay(wireShapeDisk_, m);
        break;
    }
    case ObjectType::Grid: {
        float sx = obj.primitive ? obj.primitive->size[0] : 1.0f;
        float sz = obj.primitive ? obj.primitive->size[2] : 1.0f;
        Matrix m = deform * Matrix::CreateScale({sx*kPush, 1.0f, sz*kPush}) * world;
        drawOverlay(wireShapeGrid_, m);
        break;
    }
    case ObjectType::IcoSphere: {
        float r = obj.primitive ? obj.primitive->radius * 2.0f : 1.0f;
        float s = r * kPush;
        Matrix m = deform * Matrix::CreateScale({s, s, s}) * world;
        drawOverlay(wireShapeSphere_, m);
        break;
    }
    case ObjectType::Group:
    case ObjectType::Area:
    case ObjectType::Union:
    case ObjectType::Intersection:
    case ObjectType::Difference:
        for (const auto& child : obj.children)
            drawObjectEdges(*child, doc, world, view, proj, depth + 1);
        break;
    case ObjectType::Instance: {
        // Keep the overlay in lockstep with the main pass: a culled asset
        // must not leave an otherwise invisible wireframe silhouette.
        const auto selectedLod = assetLodSelectionMap_.find(obj.id);
        if (selectedLod != assetLodSelectionMap_.end() && selectedLod->second.culled) return;
        const std::string& definitionKey = selectedLod != assetLodSelectionMap_.end()
            ? selectedLod->second.definitionId : obj.resolvedInstanceDefinitionKey();
        auto it = doc.definitions.find(definitionKey);
        if (it != doc.definitions.end() && it->second)
            drawObjectEdges(*it->second, doc, world, view, proj, depth + 1);
        else
            drawOverlay(wireShapeBox_, world);
        break;
    }
    case ObjectType::Extrude: {
        if (!obj.extrude) { drawWireShape(wireShapeBox_, deform * world, view, proj, edgeColor); break; }
        const auto& ex = obj.extrude.value();

        // Cap segments for overlay quality (no need to match the solid mesh exactly)
        int pathSegs = std::max(3, std::min(ex.segments, 20));
        auto frames  = makePathFrames(ex.path, pathSegs);
        auto profile = makeProfile(ex.crossSection);

        int M = static_cast<int>(frames.size());
        int N = static_cast<int>(profile.size());
        if (M < 2 || N < 2) { drawWireShape(wireShapeBox_, deform * world, view, proj, edgeColor); break; }

        // AUD-073: same radius>1e-6f guard as drawExtrudeDynamic() above --
        // radius==0 is legal, and without this guard innerScale below
        // evaluates to +inf, producing NaN edge-overlay vertex positions.
        bool hollow = (ex.crossSection.innerRadius > 0.0f) &&
                      (ex.crossSection.radius > 1e-6f) &&
                      (ex.crossSection.type == CrossSectionType::Circle ||
                       ex.crossSection.type == CrossSectionType::Polygon);

        // 2026-07-20 audit F5: unlike its solid-mesh sibling
        // drawExtrudeDynamic() (AUD-072), this function had no
        // vertex-budget guard at all -- see
        // extrudeEdgeOverlayExceedsVertexBudgetAlg()'s own doc comment
        // (PrimitiveTessellationAlg.hpp) for the full reasoning. Bail to a
        // placeholder wireframe before the ring-building loop below does
        // any of that work.
        if (extrudeEdgeOverlayExceedsVertexBudgetAlg(M, N, hollow)) {
            drawWireShape(wireShapeBox_, deform * world, view, proj, edgeColor);
            break;
        }

        float innerScale = hollow ? (ex.crossSection.innerRadius / ex.crossSection.radius) : 0.0f;

        float twistRad     = ex.twist * (std::numbers::pi_v<float> / 180.0f);
        float twistPerStep = (M > 1) ? twistRad / (M - 1) : 0.0f;
        Matrix localToWorld = deform * world;

        auto wposScaled = [&](int i, int j, float scale) -> Vector3 {
            const auto& f = frames[i];
            float angle = twistPerStep * i;
            float ca = std::cos(angle), sa = std::sin(angle);
            float u = (profile[j].u * ca - profile[j].v * sa) * scale;
            float v = (profile[j].u * sa + profile[j].v * ca) * scale;
            return Vector3::Transform(f.pos + f.nor * u + f.bi * v, localToWorld);
        };

        auto wpos = [&](int i, int j) -> Vector3 {
            return wposScaled(i, j, kPush);
        };
        auto wposInner = [&](int i, int j) -> Vector3 {
            return wposScaled(i, j, innerScale);
        };

        std::vector<VertexPositionColor> lines;

        // Profile rings — show up to 20 evenly-spaced rings (always include first and last)
        int ringStep = std::max(1, (M - 1) / 19);
        for (int i = 0; i < M; i += ringStep) {
            for (int j = 0; j < N; ++j) {
                lines.push_back({ wpos(i, j),       edgeColor });
                lines.push_back({ wpos(i, (j+1)%N), edgeColor });
            }
            if (hollow) {
                for (int j = 0; j < N; ++j) {
                    lines.push_back({ wposInner(i, j),       edgeColor });
                    lines.push_back({ wposInner(i, (j+1)%N), edgeColor });
                }
            }
        }
        if ((M - 1) % ringStep != 0) {
            for (int j = 0; j < N; ++j) {
                lines.push_back({ wpos(M-1, j),       edgeColor });
                lines.push_back({ wpos(M-1, (j+1)%N), edgeColor });
            }
            if (hollow) {
                for (int j = 0; j < N; ++j) {
                    lines.push_back({ wposInner(M-1, j),       edgeColor });
                    lines.push_back({ wposInner(M-1, (j+1)%N), edgeColor });
                }
            }
        }

        // Spine lines along the path — min(N, 8) evenly-spaced profile points
        int spineN = std::min(N, 8);
        for (int s = 0; s < spineN; ++s) {
            int j = s * N / spineN;
            for (int i = 0; i < M-1; ++i) {
                lines.push_back({ wpos(i,   j), edgeColor });
                lines.push_back({ wpos(i+1, j), edgeColor });
                if (hollow) {
                    lines.push_back({ wposInner(i,   j), edgeColor });
                    lines.push_back({ wposInner(i+1, j), edgeColor });
                }
            }
        }

        if (!lines.empty()) drawLineList(lines, view, proj);
        break;
    }
    default:
        drawWireShape(wireShapeBox_, world, view, proj, edgeColor);
        break;
    }
}

void SceneRenderer::drawDiskDynamic(float outerR, float innerR, int segments,
                                     const Matrix& world,
                                     const Matrix& view, const Matrix& proj,
                                     Color color, bool depthPass)
{
    const int segs = std::max(3, segments);
    const float pi2 = 2.0f * std::numbers::pi_v<float>;
    const bool solid = (innerR <= 0.0f || innerR >= outerR);

    std::vector<VertexPositionColor> verts;
    std::vector<uint16_t> indices;

    if (solid) {
        // Fan: center + outer ring
        int ctr = 0;
        verts.push_back({ Vector3{0.0f, 0.0f, 0.0f}, color });
        for (int i = 0; i < segs; ++i) {
            float a = pi2 * i / segs;
            verts.push_back({ Vector3{ outerR * std::cos(a), 0.0f, outerR * std::sin(a) }, color });
        }
        for (int i = 0; i < segs; ++i) {
            int j = (i + 1) % segs;
            indices.push_back(static_cast<uint16_t>(ctr));
            indices.push_back(static_cast<uint16_t>(1 + i));
            indices.push_back(static_cast<uint16_t>(1 + j));
        }
    } else {
        // Annulus: inner ring then outer ring
        for (int i = 0; i < segs; ++i) {
            float a = pi2 * i / segs;
            float ca = std::cos(a), sa = std::sin(a);
            verts.push_back({ Vector3{ innerR * ca, 0.0f, innerR * sa }, color }); // inner
            verts.push_back({ Vector3{ outerR * ca, 0.0f, outerR * sa }, color }); // outer
        }
        for (int i = 0; i < segs; ++i) {
            int j = (i + 1) % segs;
            int in0 = i * 2, out0 = i * 2 + 1;
            int in1 = j * 2, out1 = j * 2 + 1;
            indices.push_back(static_cast<uint16_t>(in0));  indices.push_back(static_cast<uint16_t>(out0)); indices.push_back(static_cast<uint16_t>(out1));
            indices.push_back(static_cast<uint16_t>(in0));  indices.push_back(static_cast<uint16_t>(out1)); indices.push_back(static_cast<uint16_t>(in1));
        }
    }

    if (indices.empty()) return;
    if (depthPass) {
        drawDepthTriangles(verts, indices, world, view, proj);
        return;
    }
    int nv = static_cast<int>(verts.size());
    int nt = static_cast<int>(indices.size()) / 3;

    VertexBuffer tmpVB(device_, nv);
    tmpVB.SetData(verts.data(), nv);
    IndexBuffer  tmpIB(device_, static_cast<int>(indices.size()));
    tmpIB.SetData(indices.data(), static_cast<int>(indices.size()));

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&tmpVB);
    device_.SetIndexBuffer(&tmpIB);
    device_.DrawIndexedPrimitives(Graphics::PrimitiveType::TriangleList, 0, 0, nv, 0, nt);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}

void SceneRenderer::drawGridDynamic(float sizeX, float sizeZ, int subX, int subZ,
                                     const Matrix& world,
                                     const Matrix& view, const Matrix& proj,
                                     Color color, bool depthPass)
{
    subX = std::max(1, subX);
    subZ = std::max(1, subZ);

    const int cols = subX + 1;
    const int rows = subZ + 1;

    // subX/subZ are each individually capped at parse time (kMaxTessellation,
    // Mc3XmlParser.cpp), but that per-field cap doesn't stop their PRODUCT --
    // a single <grid subdivisions_x="4096" subdivisions_z="4096"/> is legal
    // per-field yet requests ~16.8M vertices, rebuilt from scratch every
    // frame with no cache (unlike other primitives). Bail out to a
    // placeholder before allocating anything, matching drawExtrudeDynamic's
    // own numVerts>65535 fallback below -- also keeps the uint16_t index
    // buffer from silently wrapping.
    if (static_cast<long long>(cols) * static_cast<long long>(rows) > 65535) {
        if (depthPass) drawDepthMesh(unitBox_, world, view, proj);
        else drawMesh(unitBox_, world, view, proj, color);
        return;
    }

    std::vector<VertexPositionColor> verts;
    verts.reserve(cols * rows);

    for (int iz = 0; iz < rows; ++iz) {
        float z = (-0.5f + static_cast<float>(iz) / subZ) * sizeZ;
        for (int ix = 0; ix < cols; ++ix) {
            float x = (-0.5f + static_cast<float>(ix) / subX) * sizeX;
            verts.push_back({ Vector3{x, 0.0f, z}, color });
        }
    }

    std::vector<uint16_t> indices;
    indices.reserve(subX * subZ * 6);
    for (int iz = 0; iz < subZ; ++iz) {
        for (int ix = 0; ix < subX; ++ix) {
            int v00 = iz * cols + ix;
            int v10 = v00 + 1;
            int v01 = v00 + cols;
            int v11 = v01 + 1;
            indices.push_back(static_cast<uint16_t>(v00));
            indices.push_back(static_cast<uint16_t>(v10));
            indices.push_back(static_cast<uint16_t>(v11));
            indices.push_back(static_cast<uint16_t>(v00));
            indices.push_back(static_cast<uint16_t>(v11));
            indices.push_back(static_cast<uint16_t>(v01));
        }
    }

    if (indices.empty()) return;
    if (depthPass) {
        drawDepthTriangles(verts, indices, world, view, proj);
        return;
    }
    int nv = static_cast<int>(verts.size());
    int nt = static_cast<int>(indices.size()) / 3;

    VertexBuffer tmpVB(device_, nv);
    tmpVB.SetData(verts.data(), nv);
    IndexBuffer  tmpIB(device_, static_cast<int>(indices.size()));
    tmpIB.SetData(indices.data(), static_cast<int>(indices.size()));

    effect_->World      = world;
    effect_->View       = view;
    effect_->Projection = proj;
    effect_->VertexColorEnabled = true;
    for (auto& pass : effect_->getCurrentTechniqueProperty()->getPassesProperty())
        pass.Apply();

    device_.SetVertexBuffer(&tmpVB);
    device_.SetIndexBuffer(&tmpIB);
    device_.DrawIndexedPrimitives(Graphics::PrimitiveType::TriangleList, 0, 0, nv, 0, nt);
    device_.SetVertexBuffer(nullptr);
    device_.SetIndexBuffer(nullptr);
}

void SceneRenderer::drawEdgeOverlay(const Mc3Document& doc,
                                     const Matrix& view, const Matrix& proj)
{
    Matrix identity = coordinateSystemRootMatrixForEdges(doc);
    for (const auto& obj : doc.objects)
        drawObjectEdges(*obj, doc, identity, view, proj);
}

} // namespace MeshCraft::Renderer
