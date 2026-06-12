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
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace MeshCraft::Renderer {

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

    // Render translate gizmo (X/Y/Z axis lines + cube tips) for a selected object
    void drawGizmo(const Mc3::Mc3Object* obj,
                   const Microsoft::Xna::Framework::Matrix& view,
                   const Microsoft::Xna::Framework::Matrix& projection,
                   float gizmoLength);

    // Render scale gizmo (X/Y/Z axis lines + flat-square tips) for a selected object
    void drawScaleGizmo(const Mc3::Mc3Object* obj,
                        const Microsoft::Xna::Framework::Matrix& view,
                        const Microsoft::Xna::Framework::Matrix& projection,
                        float gizmoLength);

    // Render rotate gizmo (X/Y/Z circles) for a selected object
    void drawRotateGizmo(const Mc3::Mc3Object* obj,
                         const Microsoft::Xna::Framework::Matrix& view,
                         const Microsoft::Xna::Framework::Matrix& projection,
                         float gizmoLength);

    // Render a single object's bounding box (wireframe)
    void drawObjectWireframe(const Mc3::Mc3Object& obj,
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

private:
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;

    // Pre-built unit shapes
    RenderMesh unitBox_;
    RenderMesh unitSphere_;
    RenderMesh unitCylinder_;
    RenderMesh unitCone_;
    RenderMesh unitPlane_;

    // Wire box for selection highlight
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> wireBoxVB_;
    int wireBoxLineCount_{0};

    // Pre-built wire shapes for edge overlay
    WireShape wireShapeBox_;
    WireShape wireShapeSphere_;
    WireShape wireShapeCylinder_;
    WireShape wireShapeCone_;
    WireShape wireShapePlane_;

    void buildUnitBox();
    void buildUnitSphere(int segments);
    void buildUnitCylinder(int segments);
    void buildUnitCone(int segments);
    void buildUnitPlane();
    void buildWireBox();
    void buildWireShapes(int segments);

    void drawObject(const Mc3::Mc3Object& obj,
                    const Mc3::Mc3Document& doc,
                    const Microsoft::Xna::Framework::Matrix& parentWorld,
                    const Microsoft::Xna::Framework::Matrix& view,
                    const Microsoft::Xna::Framework::Matrix& projection,
                    const std::vector<const Mc3::Mc3Object*>& selected,
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

    Microsoft::Xna::Framework::Matrix objectWorldMatrix(const Mc3::Mc3Object& obj) const;
    Microsoft::Xna::Framework::Color  materialColor(const std::string& matId,
                                                     const Mc3::Mc3Document& doc) const;
    bool isSelected(const Mc3::Mc3Object& obj, const std::vector<const Mc3::Mc3Object*>& sel) const;

    std::map<std::string, Microsoft::Xna::Framework::Graphics::Texture2D> textureCache_;
};

} // namespace MeshCraft::Renderer
