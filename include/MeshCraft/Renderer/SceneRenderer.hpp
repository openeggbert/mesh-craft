#pragma once

#include <MeshCraft/Mc3/Mc3Camera.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/Mc3/Mc3Light.hpp>
#include <MeshCraft/Mc3/Mc3Object.hpp>
#include <MeshCraft/AssetLodAlgorithms.hpp>
#include <MeshCraft/PointSpotLightingAlgorithms.hpp>
#include <Microsoft/Xna/Framework/Color.hpp>
#include <Microsoft/Xna/Framework/Graphics/BasicEffect.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp>
#include <Microsoft/Xna/Framework/Graphics/SamplerState.hpp>
#include <Microsoft/Xna/Framework/Graphics/ShaderEffect.hpp>
#include <Microsoft/Xna/Framework/Graphics/Texture2D.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace MeshCraft::Editor { struct WalkCollider; }

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
    // CPU mirror of texVB. It permits per-object UV projection to upload a
    // temporary mapped buffer without reading data back from the GPU.
    std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionNormalTexture> texturedVertices;
    int texPrimitiveCount{0};
};

// One cached CSG vertex buffer is shared by per-material index ranges. This
// avoids multiplying generated Manifold vertex data merely to use different
// child materials in the viewport.
struct CsgPreviewMaterialRange {
    std::string materialId;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indexBuffer;
    int primitiveCount{0};
};

struct CsgPreviewCacheEntry {
    RenderMesh mesh;
    std::vector<CsgPreviewMaterialRange> materialRanges;
    std::string warning;
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
    void clearCsgCache() {
        csgMeshCache_.clear();
        csgTriCountMap_.clear();
        // Callers use this on document replacement as well as CSG edits.
        // Resetting LOD hysteresis then avoids carrying a prior document's
        // instance IDs into a newly loaded scene.
        assetLodPreviousTiers_.clear();
        assetLodSelectionMap_.clear();
    }

    // Triangle count of the last computed CSG result for the given object ID.
    // Returns -1 if the object has not been rendered yet this session.
    int csgCachedTriCount(const std::string& objId) const {
        auto it = csgTriCountMap_.find(objId);
        return it != csgTriCountMap_.end() ? it->second : -1;
    }

    // STAB-0672: a human-readable reason the CSG preview for the given
    // object ID may be incomplete/empty (unsupported Mesh/Extrude content
    // anywhere in the subtree, or the subtree exceeds the CSG depth limit),
    // or an empty string if the last-rendered result had no such issue.
    // "Tris: 0" alone doesn't distinguish a genuinely empty boolean (e.g. an
    // Intersection of non-overlapping shapes) from one that silently
    // dropped content it can't represent.
    std::string csgWarning(const std::string& objId) const {
        auto it = csgWarningMap_.find(objId);
        return it != csgWarningMap_.end() ? it->second : std::string();
    }

    // Number of times a CSG subtree was actually re-evaluated (cache miss —
    // buildManifoldTree() called) since this SceneRenderer was constructed.
    // A static CSG scene re-drawn across many frames should see this stay
    // at 1 after the first draw, not grow with the frame count (K1).
    int csgCacheEvaluationCount() const { return csgCacheEvaluations_; }

    // Current entry count of the CSG mesh cache. Bounded: the cache is
    // cleared entirely once it would exceed 128 entries (STAB-0216) — not an
    // LRU eviction down to a fixed size, so this can transiently read as high
    // as 129 (128 existing + 1 just-inserted, checked before the *next*
    // insertion) before the next unique CSG subtree triggers a full clear.
    int csgMeshCacheSize() const { return static_cast<int>(csgMeshCache_.size()); }

    // LOD tier (0=full, 1=mid, 2=low) picked for the given object ID on
    // its last draw, based on camera distance (G8). Returns -1 if the
    // object has not been rendered yet this session (STAB-0525).
    int lastLodLevel(const std::string& objId) const {
        auto it = lodLevelMap_.find(objId);
        return it != lodLevelMap_.end() ? it->second : -1;
    }

    // Authored-definition LOD is intentionally independent of lastLodLevel:
    // it can swap an Instance to another definition or cull it, while the
    // older value only changes the tessellation of one primitive mesh.
    void setAssetLodConfig(AssetLodConfig config) {
        assetLodConfig_ = normaliseAssetLodConfigAlg(config);
        assetLodPreviousTiers_.clear();
        assetLodSelectionMap_.clear();
    }
    [[nodiscard]] const AssetLodConfig& assetLodConfig() const { return assetLodConfig_; }
    [[nodiscard]] std::optional<AssetLodSelection>
    lastAssetLodSelection(const std::string& objId) const {
        auto it = assetLodSelectionMap_.find(objId);
        return it != assetLodSelectionMap_.end() ? std::optional<AssetLodSelection>{it->second}
                                                 : std::nullopt;
    }
    void clearAssetLodState() {
        assetLodPreviousTiers_.clear();
        assetLodSelectionMap_.clear();
    }

    // Export the computed CSG result for obj to an OBJ file at path.
    // Returns true on success; on failure, err is set to a human-readable message.
    bool exportCsgMesh(const Mc3::Mc3Object& obj, const Mc3::Mc3Document& doc,
                       const std::string& path, std::string& err);

    // Replace the per-object animation overrides used during the next draw() call.
    void setAnimOverrides(std::unordered_map<std::string, AnimOverride> overrides) {
        animOverrides_ = std::move(overrides);
    }

    // Render translate gizmo (X/Y/Z axis lines + cube tips) for a selected object
    void drawGizmo(const Mc3::Mc3Object* obj, const Mc3::Mc3Document& doc,
                   const Microsoft::Xna::Framework::Matrix& view,
                   const Microsoft::Xna::Framework::Matrix& projection,
                   float gizmoLength,
                   bool localSpace = false);

    // Render scale gizmo (X/Y/Z axis lines + flat-square tips) for a selected object
    void drawScaleGizmo(const Mc3::Mc3Object* obj, const Mc3::Mc3Document& doc,
                        const Microsoft::Xna::Framework::Matrix& view,
                        const Microsoft::Xna::Framework::Matrix& projection,
                        float gizmoLength,
                        bool localSpace = false);

    // Render rotate gizmo (X/Y/Z circles) for a selected object
    void drawRotateGizmo(const Mc3::Mc3Object* obj, const Mc3::Mc3Document& doc,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         float gizmoLength,
                         bool localSpace = false);

    // Render a single object's bounding box (wireframe)
    void drawObjectWireframe(const Mc3::Mc3Object& obj, const Mc3::Mc3Document& doc,
                             const Microsoft::Xna::Framework::Matrix& view,
                             const Microsoft::Xna::Framework::Matrix& projection,
                             Microsoft::Xna::Framework::Color color);

    // Render a free-floating wireframe sphere at an arbitrary world position
    // and radius (e.g. proportional-editing falloff radius indicator)
    void drawWireSphereAt(const Microsoft::Xna::Framework::Vector3& center,
                          float radius,
                          const Mc3::Mc3Document& doc,
                          const Microsoft::Xna::Framework::Matrix& view,
                          const Microsoft::Xna::Framework::Matrix& projection,
                          Microsoft::Xna::Framework::Color color);

    // Draw the active world-space Walk Mode collision proxies as one bounded
    // line batch. These coordinates already include the MC3 coordinate-system
    // root, so this deliberately takes no Mc3Document and applies no second
    // root transform.
    void drawWalkCollisionDebug(std::span<const Editor::WalkCollider> colliders,
                                const Microsoft::Xna::Framework::Matrix& view,
                                const Microsoft::Xna::Framework::Matrix& projection);

    // Render scene-level gizmos (lights / cameras)
    void drawLightGizmos(const Mc3::Mc3Document& doc,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection);

    void drawCameraGizmos(const Mc3::Mc3Document& doc,
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

    // AUD-085: render the visible scene a second time into a color target,
    // encoding the hardware depth in the red channel. CNA intentionally does
    // not expose a backbuffer depth texture, so SSAO samples this pass rather
    // than reaching around the graphics abstraction with glBlitFramebuffer.
    void drawDepthPass(const Mc3::Mc3Document& doc,
                       const Microsoft::Xna::Framework::Matrix& view,
                       const Microsoft::Xna::Framework::Matrix& projection);
    [[nodiscard]] bool depthPassAvailable() const;

    // Number of emissive objects drawn in the last drawEmissivePass call
    int emissiveDrawCount() const { return emissiveDrawCount_; }

    // Compute vertex and triangle counts for all visible objects in the scene
    void scenePolyStats(const Mc3::Mc3Document& doc, int& totalVerts, int& totalTris) const;

    // Compute vertex and triangle counts for a single object (including children)
    void objectPolyStats(const Mc3::Mc3Object& obj, int& verts, int& tris) const;

    // Compute the world-space matrix for a specific object (accumulates parent transforms)
    Microsoft::Xna::Framework::Matrix computeObjectWorldMatrix(
        const Mc3::Mc3Object& obj, const Mc3::Mc3Document& doc) const;

    // 2026-07-20 audit finding #6: Mc3Camera::rotation ("alternative to
    // target") had editor UI to set it, but neither drawCameraGizmos()
    // (below) nor MeshCraftApplication's Look-Through-Camera mode ever
    // read it -- both always derived the view direction from `target`
    // instead, so a camera authored with only `rotation` (target left at
    // its {0,0,0} default) silently pointed at the origin in the live
    // preview, while mc3togltf's export already handled it correctly.
    // Converts a rotation triple (degrees, same [pitch,yaw,roll] axis
    // convention as every other rotation field in this codebase --
    // objectWorldMatrix()'s own CreateFromYawPitchRoll(rotation[1],
    // rotation[0], rotation[2])) into a forward direction, starting from
    // this project's right_handed_y_up "looks down -Z at identity
    // rotation" convention. Static (no instance state needed) so
    // MeshCraftApplication.cpp's Look-Through-Camera code can call it too.
    static Microsoft::Xna::Framework::Vector3 cameraForwardFromRotation(
        const std::array<float, 3>& rotationDegrees);

private:
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
    std::optional<Microsoft::Xna::Framework::Graphics::ShaderEffect> depthEffect_;
    // SYS-W14-33: BasicEffect has no positional-light API. This optional
    // source-GLSL effect is available only on CNA backends that implement the
    // complete named-uniform 3D contract; all others retain BasicEffect.
    std::optional<Microsoft::Xna::Framework::Graphics::ShaderEffect> pointSpotEffect_;
    struct PunctualPreviewLight {
        std::array<float, 3> position{};
        std::array<float, 3> direction{0.0f, -1.0f, 0.0f};
        std::array<float, 3> color{1.0f, 1.0f, 1.0f};
        float brightness{1.0f};
        float range{0.0f};
        float innerConeCos{1.0f};
        float outerConeCos{1.0f};
        bool spot{false};
    };
    std::array<PunctualPreviewLight, MeshCraft::kViewportPunctualLightLimit> punctualPreviewLights_{};
    int punctualPreviewLightCount_{0};
    bool punctualPreviewFallbackWarned_{false};

    // Pre-built unit shapes (full quality)
    RenderMesh unitBox_;
    RenderMesh unitSphere_;
    RenderMesh unitCylinder_;
    RenderMesh unitCone_;
    RenderMesh unitPlane_;
    RenderMesh unitTorus_;
    RenderMesh unitCapsule_;
    // AUD-062: the IcoSphere's `segments` field maps to one of the same
    // four subdivision levels the glTF exporter uses (segments / 8, clamped
    // to [1, 4]).  Keep one unit mesh for each level rather than silently
    // drawing every IcoSphere with a fixed subdivision level.
    std::array<RenderMesh, 4> unitIcoSpheres_;

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

    AssetLodConfig assetLodConfig_{};
    std::unordered_map<std::string, AssetLodTier> assetLodPreviousTiers_;
    std::unordered_map<std::string, AssetLodSelection> assetLodSelectionMap_;

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
    // majorRadius/minorRadius default to the historical fixed unit-mesh
    // ratio (0.35/0.15) -- callers that want a mesh at the object's ACTUAL
    // ratio (AUD-061) pass those explicitly; see getOrBuildTorusMesh().
    void buildUnitTorus(int ringSeg, int tubeSeg, RenderMesh& target,
                        float majorRadius = 0.35f, float minorRadius = 0.15f);
    // radius/height default to the historical fixed unit-mesh values
    // (0.5/1.0) -- callers that want a mesh at the object's ACTUAL
    // radius/height (AUD-061) pass those explicitly; see
    // getOrBuildCapsuleMesh().
    void buildUnitCapsule(int segments, RenderMesh& target,
                          float radius = 0.5f, float height = 1.0f);
    void buildUnitIcoSphere(int subdivisions, RenderMesh& target);
    const RenderMesh& icoSphereMeshForSegments(int segments) const;
    void buildWireBox();
    void buildWireShapes(int segments);

    // AUD-061: Torus/Capsule can't be correctly reproduced by scaling one
    // fixed-ratio unit mesh (see PrimitiveTessellationAlg.hpp's file header),
    // so the viewport builds one real mesh per distinct (LOD tier, actual
    // radius parameters) combination on demand, cached here so a static
    // scene with many tori/capsules does not re-tessellate every frame.
    // Bounded like csgMeshCache_ (cleared entirely past 128 entries rather
    // than LRU-evicted -- consistent with this codebase's existing
    // CSG-preview-cache precedent).
    const RenderMesh& getOrBuildTorusMesh(int ringSeg, int tubeSeg,
                                          float majorRadius, float minorRadius);
    const RenderMesh& getOrBuildCapsuleMesh(int segments, float radius, float height);

    void drawObject(const Mc3::Mc3Object& obj,
                    const Mc3::Mc3Document& doc,
                    const Microsoft::Xna::Framework::Matrix& parentWorld,
                    const Microsoft::Xna::Framework::Matrix& view,
                    const Microsoft::Xna::Framework::Matrix& projection,
                    const std::vector<const Mc3::Mc3Object*>& selected,
                    int depth = 0);

    // Maps Directional/Ambient lights to BasicEffect and collects up to eight
    // authored Point/Spot lights for the optional CNA ShaderEffect path.
    // Called once per draw, so edits in the Lights panel are immediate.
    void applyDocumentLighting(const Mc3::Mc3Document& doc);

    bool drawPunctualLightMesh(const Microsoft::Xna::Framework::Matrix& world,
                               const Microsoft::Xna::Framework::Matrix& view,
                               const Microsoft::Xna::Framework::Matrix& projection,
                               Microsoft::Xna::Framework::Color color,
                               Microsoft::Xna::Framework::Graphics::Texture2D* texture,
                               const Microsoft::Xna::Framework::Graphics::SamplerState* sampler,
                               Microsoft::Xna::Framework::Graphics::VertexBuffer* vertexBuffer,
                               Microsoft::Xna::Framework::Graphics::IndexBuffer* indexBuffer,
                               int primitiveCount);

    void drawEmissiveObject(const Mc3::Mc3Object& obj,
                            const Mc3::Mc3Document& doc,
                            const Microsoft::Xna::Framework::Matrix& parentWorld,
                            const Microsoft::Xna::Framework::Matrix& view,
                            const Microsoft::Xna::Framework::Matrix& projection,
                            int depth = 0);

    void drawDepthObject(const Mc3::Mc3Object& obj,
                         const Mc3::Mc3Document& doc,
                         const Microsoft::Xna::Framework::Matrix& parentWorld,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         int depth = 0);
    void drawDepthMesh(const RenderMesh& mesh,
                       const Microsoft::Xna::Framework::Matrix& world,
                       const Microsoft::Xna::Framework::Matrix& view,
                       const Microsoft::Xna::Framework::Matrix& projection);
    void drawDepthTriangles(const std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor>& vertices,
                            const std::vector<uint16_t>& indices,
                            const Microsoft::Xna::Framework::Matrix& world,
                            const Microsoft::Xna::Framework::Matrix& view,
                            const Microsoft::Xna::Framework::Matrix& projection);

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

    // Renderer proposals P3/P4: shade edge segments from their outward
    // direction and retain only camera-facing silhouettes for the overlay.
    void drawSilhouetteWireShape(const WireShape& wire,
                                 const Microsoft::Xna::Framework::Matrix& world,
                                 const Microsoft::Xna::Framework::Matrix& view,
                                 const Microsoft::Xna::Framework::Matrix& projection,
                                 const Microsoft::Xna::Framework::Vector3& lightDirection);

    void drawExtrudeDynamic(const Mc3::Mc3Extrude& ex,
                            const Microsoft::Xna::Framework::Matrix& world,
                            const Microsoft::Xna::Framework::Matrix& view,
                            const Microsoft::Xna::Framework::Matrix& projection,
                            Microsoft::Xna::Framework::Color color,
                            bool depthPass = false);

    void drawDiskDynamic(float outerR, float innerR, int segments,
                         const Microsoft::Xna::Framework::Matrix& world,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         Microsoft::Xna::Framework::Color color,
                         bool depthPass = false);

    void drawGridDynamic(float sizeX, float sizeZ, int subX, int subZ,
                         const Microsoft::Xna::Framework::Matrix& world,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         Microsoft::Xna::Framework::Color color,
                         bool depthPass = false);

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
                          Microsoft::Xna::Framework::Graphics::Texture2D* tex,
                          const Microsoft::Xna::Framework::Graphics::SamplerState* sampler = nullptr,
                          const Mc3::Mc3UvMapping* uvMapping = nullptr,
                          std::array<float, 3> uvGeometryScale = {1.0f, 1.0f, 1.0f},
                          Microsoft::Xna::Framework::Graphics::IndexBuffer* indexBuffer = nullptr,
                          int primitiveCount = -1);

    Microsoft::Xna::Framework::Graphics::Texture2D* loadOrGetTexture(const std::string& absPath);
    Microsoft::Xna::Framework::Graphics::Texture2D* textureForMaterial(
        const std::string& materialId, const Mc3::Mc3Document& doc,
        std::optional<Microsoft::Xna::Framework::Graphics::SamplerState>& svgSampler);
    const RenderMesh* loadOrGetMesh(const std::string& absPath);
    const RenderMesh* loadOrGetEmbeddedMesh(const Mc3::Mc3Document& doc,
                                            const std::string& embedReference);

    Microsoft::Xna::Framework::Matrix objectWorldMatrix(const Mc3::Mc3Transform& t) const;
    Microsoft::Xna::Framework::Color  materialColor(const std::string& matId,
                                                     const Mc3::Mc3Document& doc) const;
    bool isSelected(const Mc3::Mc3Object& obj, const std::vector<const Mc3::Mc3Object*>& sel) const;

    std::map<std::string, Microsoft::Xna::Framework::Graphics::Texture2D> textureCache_;
    struct SvgCacheState {
        std::optional<std::filesystem::file_time_type> lastWriteTime;
    };
    // Successful and failed SVG rasterizations both remember the source stamp:
    // an unchanged bad file is not reparsed/logged every frame, while an edit
    // invalidates either result immediately.
    std::map<std::string, SvgCacheState> svgTextureCacheState_;
    std::map<std::string, SvgCacheState> svgTextureFailureState_;
    std::map<std::string, RenderMesh> meshCache_;
    // AUD-061: per-object-ratio Torus/Capsule mesh caches, keyed on the
    // exact parameters that determine the mesh's shape (LOD segment counts +
    // actual radii). See getOrBuildTorusMesh()/getOrBuildCapsuleMesh().
    std::map<std::tuple<int, int, float, float>, RenderMesh> torusMeshCache_;   // (ringSeg, tubeSeg, majorRadius, minorRadius)
    std::map<std::tuple<int, float, float>, RenderMesh> capsuleMeshCache_;      // (segments, radius, height)
    std::unordered_map<std::size_t, CsgPreviewCacheEntry> csgMeshCache_;
    std::unordered_map<std::string, int> csgTriCountMap_;   // obj.id → last rendered tri count (K4)
    std::unordered_map<std::string, std::string> csgWarningMap_;   // obj.id → last incomplete-preview reason, empty if none (STAB-0672)
    int csgCacheEvaluations_{0};   // cache-miss count (STAB-0522)
    std::unordered_map<std::string, int> lodLevelMap_;   // obj.id → last used LOD tier (STAB-0525)
    std::unordered_map<std::string, AnimOverride> animOverrides_;
};

} // namespace MeshCraft::Renderer
