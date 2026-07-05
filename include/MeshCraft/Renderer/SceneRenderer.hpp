#pragma once

#include <MeshCraft/Mc3/Mc3Camera.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Light.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/BasicEffect.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp>
#include <Microsoft/Xna/Framework/Graphics/Texture2D.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace MeshCraft::Renderer {

// Per-object transform/visibility/material override for animation playback.
// Any optional that is set replaces the corresponding field during rendering.
struct AnimOverride {
    std::optional<std::array<float, 3>> position;
    std::optional<std::array<float, 3>> rotation;
    std::optional<std::array<float, 3>> scale;
    std::optional<bool>                 visible;

    // Material property overrides (animated material channels)
    std::optional<std::array<float, 4>> baseColor;  // RGBA
    std::optional<float>                roughness;
    std::optional<float>                metallic;
    std::optional<std::array<float, 3>> emissive;   // RGB

    // Deform override (animated deform.x/y/z channels)
    std::optional<std::array<float, 3>> deformScale;
};

struct RenderMesh {
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer>  ib;
    std::vector<Microsoft::Xna::Framework::Vector3> positions; // for dynamic recoloring
    int primitiveCount{0};

    // UV+normal variant for texture rendering (VertexPositionNormalTexture)
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> texVB;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer>  texIB;
    int texPrimitiveCount{0};
};

// Line-list shape for edge overlay rendering
struct WireShape {
    std::vector<Microsoft::Xna::Framework::Vector3> positions; // interleaved pairs (LineList)
    int lineCount{0};
};

class SceneRenderer {
public:
    explicit SceneRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    void draw(const Mc3::Mc3Document& doc,
              const Microsoft::Xna::Framework::Matrix& view,
              const Microsoft::Xna::Framework::Matrix& projection,
              const std::vector<const Mc3::Mc3Object*>& selected);

    // Clear the CSG content-hash cache (call on document load/close to free memory).
    // Normal edits do NOT need an explicit clear — the cache auto-invalidates via hash.
    void clearCsgCache() { csgMeshCache_.clear(); csgTriCountMap_.clear(); }

    // Triangle count of the last computed CSG result for the given object ID.
    // Returns -1 if the object has not been rendered yet this session.
    int csgCachedTriCount(const std::string& objId) const {
        auto it = csgTriCountMap_.find(objId);
        return it != csgTriCountMap_.end() ? it->second : -1;
    }

    // Number of times a CSG subtree was actually re-evaluated (cache miss —
    // buildManifoldTree() called) since this SceneRenderer was constructed.
    // A static CSG scene re-drawn across many frames should see this stay
    // at 1 after the first draw, not grow with the frame count (K1).
    int csgCacheEvaluationCount() const { return csgCacheEvaluations_; }

    // Export the computed CSG result for obj to an OBJ file at path.
    // Returns true on success; on failure, err is set to a human-readable message.
    bool exportCsgMesh(const Mc3::Mc3Object& obj, const Mc3::Mc3Document& doc,
                       const std::string& path, std::string& err);

    // Replace the per-object animation overrides used during the next draw() call.
    void setAnimOverrides(std::unordered_map<std::string, AnimOverride> overrides) {
        animOverrides_ = std::move(overrides);
    }

    // Render translate gizmo (X/Y/Z axis lines + cube tips) for a selected object
    void drawGizmo(const Mc3::Mc3Object* obj,
                   const Microsoft::Xna::Framework::Matrix& view,
                   const Microsoft::Xna::Framework::Matrix& projection,
                   float gizmoLength,
                   bool localSpace = false);

    // Render scale gizmo (X/Y/Z axis lines + flat-square tips) for a selected object
    void drawScaleGizmo(const Mc3::Mc3Object* obj,
                        const Microsoft::Xna::Framework::Matrix& view,
                        const Microsoft::Xna::Framework::Matrix& projection,
                        float gizmoLength,
                        bool localSpace = false);

    // Render rotate gizmo (X/Y/Z circles) for a selected object
    void drawRotateGizmo(const Mc3::Mc3Object* obj,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         float gizmoLength,
                         bool localSpace = false);

    // Render a single object's bounding box (wireframe)
    void drawObjectWireframe(const Mc3::Mc3Object& obj,
                             const Microsoft::Xna::Framework::Matrix& view,
                             const Microsoft::Xna::Framework::Matrix& projection,
                             Microsoft::Xna::Framework::Color color);

    // Render a free-floating wireframe sphere at an arbitrary world position
    // and radius (e.g. proportional-editing falloff radius indicator)
    void drawWireSphereAt(const Microsoft::Xna::Framework::Vector3& center,
                          float radius,
                          const Microsoft::Xna::Framework::Matrix& view,
                          const Microsoft::Xna::Framework::Matrix& projection,
                          Microsoft::Xna::Framework::Color color);

    // Render scene-level gizmos (lights / cameras)
    void drawLightGizmos(const std::vector<Mc3::Mc3Light>& lights,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection);

    void drawCameraGizmos(const std::vector<Mc3::Mc3Camera>& cameras,
                          const Microsoft::Xna::Framework::Matrix& view,
                          const Microsoft::Xna::Framework::Matrix& projection);

    void drawCsgGizmos(const Mc3::Mc3Document& doc,
                       const Microsoft::Xna::Framework::Matrix& view,
                       const Microsoft::Xna::Framework::Matrix& projection);

    // Draw black edge lines over all visible objects (wireframe overlay)
    void drawEdgeOverlay(const Mc3::Mc3Document& doc,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection);

    // Render only objects with non-zero emissive_color, using emissive as draw color (for bloom)
    void drawEmissivePass(const Mc3::Mc3Document& doc,
                          const Microsoft::Xna::Framework::Matrix& view,
                          const Microsoft::Xna::Framework::Matrix& projection);

    // Number of emissive objects drawn in the last drawEmissivePass call
    int emissiveDrawCount() const { return emissiveDrawCount_; }

    // Compute vertex and triangle counts for all visible objects in the scene
    void scenePolyStats(const Mc3::Mc3Document& doc, int& totalVerts, int& totalTris) const;

    // Compute vertex and triangle counts for a single object (including children)
    void objectPolyStats(const Mc3::Mc3Object& obj, int& verts, int& tris) const;

    // Compute the world-space matrix for a specific object (accumulates parent transforms)
    Microsoft::Xna::Framework::Matrix computeObjectWorldMatrix(
        const Mc3::Mc3Object& obj, const Mc3::Mc3Document& doc) const;

private:
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;

    // Pre-built unit shapes (full quality)
    RenderMesh unitBox_;
    RenderMesh unitSphere_;
    RenderMesh unitCylinder_;
    RenderMesh unitCone_;
    RenderMesh unitPlane_;
    RenderMesh unitTorus_;
    RenderMesh unitCapsule_;
    RenderMesh unitIcoSphere_;

    // LOD variants: L1 = half segments, L2 = quarter segments (G8)
    RenderMesh unitSphereL1_;    // 16 seg
    RenderMesh unitSphereL2_;    // 6 seg
    RenderMesh unitCylinderL1_;  // 12 seg
    RenderMesh unitCylinderL2_;  // 6 seg
    RenderMesh unitConeL1_;      // 12 seg
    RenderMesh unitConeL2_;      // 6 seg
    RenderMesh unitTorusL1_;     // 16/8 seg
    RenderMesh unitTorusL2_;     // 8/4 seg
    RenderMesh unitCapsuleL1_;   // 8 seg
    RenderMesh unitCapsuleL2_;   // 4 seg

    // Camera world position stored by draw() for use in drawObject() LOD (G8)
    float camPosX_{0.0f}, camPosY_{0.0f}, camPosZ_{0.0f};

    // Wire box for selection highlight
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> wireBoxVB_;
    int wireBoxLineCount_{0};
    int emissiveDrawCount_{0};

    // Pre-built wire shapes for edge overlay
    WireShape wireShapeBox_;
    WireShape wireShapeSphere_;
    WireShape wireShapeCylinder_;
    WireShape wireShapeCone_;
    WireShape wireShapePlane_;
    WireShape wireShapeTorus_;
    WireShape wireShapeCapsule_;
    WireShape wireShapeDisk_;
    WireShape wireShapeGrid_;

    void buildUnitBox();
    void buildUnitSphere(int segments, RenderMesh& target);
    void buildUnitCylinder(int segments, RenderMesh& target);
    void buildUnitCone(int segments, RenderMesh& target);
    void buildUnitPlane();
    void buildUnitTorus(int ringSeg, int tubeSeg, RenderMesh& target);
    void buildUnitCapsule(int segments, RenderMesh& target);
    void buildUnitIcoSphere(int subdivisions);
    void buildWireBox();
    void buildWireShapes(int segments);

    void drawObject(const Mc3::Mc3Object& obj,
                    const Mc3::Mc3Document& doc,
                    const Microsoft::Xna::Framework::Matrix& parentWorld,
                    const Microsoft::Xna::Framework::Matrix& view,
                    const Microsoft::Xna::Framework::Matrix& projection,
                    const std::vector<const Mc3::Mc3Object*>& selected,
                    int depth = 0);

    void drawEmissiveObject(const Mc3::Mc3Object& obj,
                            const Mc3::Mc3Document& doc,
                            const Microsoft::Xna::Framework::Matrix& parentWorld,
                            const Microsoft::Xna::Framework::Matrix& view,
                            const Microsoft::Xna::Framework::Matrix& projection,
                            int depth = 0);

    void drawObjectEdges(const Mc3::Mc3Object& obj,
                         const Mc3::Mc3Document& doc,
                         const Microsoft::Xna::Framework::Matrix& parentWorld,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         int depth = 0);

    void drawWireShape(const WireShape& wire,
                       const Microsoft::Xna::Framework::Matrix& world,
                       const Microsoft::Xna::Framework::Matrix& view,
                       const Microsoft::Xna::Framework::Matrix& projection,
                       Microsoft::Xna::Framework::Color color);

    void drawExtrudeDynamic(const Mc3::Mc3Extrude& ex,
                            const Microsoft::Xna::Framework::Matrix& world,
                            const Microsoft::Xna::Framework::Matrix& view,
                            const Microsoft::Xna::Framework::Matrix& projection,
                            Microsoft::Xna::Framework::Color color);

    void drawDiskDynamic(float outerR, float innerR, int segments,
                         const Microsoft::Xna::Framework::Matrix& world,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         Microsoft::Xna::Framework::Color color);

    void drawGridDynamic(float sizeX, float sizeZ, int subX, int subZ,
                         const Microsoft::Xna::Framework::Matrix& world,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         Microsoft::Xna::Framework::Color color);

    void drawLineList(const std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor>& verts,
                      const Microsoft::Xna::Framework::Matrix& view,
                      const Microsoft::Xna::Framework::Matrix& projection);

    void drawMesh(const RenderMesh& mesh,
                  const Microsoft::Xna::Framework::Matrix& world,
                  const Microsoft::Xna::Framework::Matrix& view,
                  const Microsoft::Xna::Framework::Matrix& projection,
                  Microsoft::Xna::Framework::Color color);

    void drawMeshTextured(const RenderMesh& mesh,
                          const Microsoft::Xna::Framework::Matrix& world,
                          const Microsoft::Xna::Framework::Matrix& view,
                          const Microsoft::Xna::Framework::Matrix& projection,
                          Microsoft::Xna::Framework::Color color,
                          Microsoft::Xna::Framework::Graphics::Texture2D* tex);

    Microsoft::Xna::Framework::Graphics::Texture2D* loadOrGetTexture(const std::string& absPath);
    const RenderMesh* loadOrGetMesh(const std::string& absPath);

    Microsoft::Xna::Framework::Matrix objectWorldMatrix(const Mc3::Mc3Transform& t) const;
    Microsoft::Xna::Framework::Color  materialColor(const std::string& matId,
                                                     const Mc3::Mc3Document& doc) const;
    bool isSelected(const Mc3::Mc3Object& obj, const std::vector<const Mc3::Mc3Object*>& sel) const;

    std::map<std::string, Microsoft::Xna::Framework::Graphics::Texture2D> textureCache_;
    std::map<std::string, RenderMesh> meshCache_;
    std::unordered_map<std::size_t, RenderMesh> csgMeshCache_;
    std::unordered_map<std::string, int> csgTriCountMap_;   // obj.id → last rendered tri count (K4)
    int csgCacheEvaluations_{0};   // cache-miss count (STAB-0522)
    std::unordered_map<std::string, AnimOverride> animOverrides_;
};

} // namespace MeshCraft::Renderer
