#pragma once

#include <Microsoft/Xna/Framework/Graphics/BasicEffect.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <memory>

namespace MeshCraft::Renderer {

class GridRenderer {
public:
    explicit GridRenderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    void draw(const Microsoft::Xna::Framework::Matrix& view,
              const Microsoft::Xna::Framework::Matrix& projection);

    void setSpacing(float spacing);

private:
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect>  effect_;
    int lineCount_{0};

    void buildGrid(int halfExtent, float spacing);
};

} // namespace MeshCraft::Renderer
