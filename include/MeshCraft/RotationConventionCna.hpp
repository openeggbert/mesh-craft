#pragma once

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/RotationConventionAlgorithms.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/Quaternion.hpp>

namespace MeshCraft {

[[nodiscard]] inline Microsoft::Xna::Framework::Matrix rotationMatrixForDocumentAlg(
    const Mc3::Mc3Document& document, const std::array<float, 3>& rotation)
{
    const auto quaternion = rotationQuaternionAlg(
        rotation, document.rotationUnits, document.eulerOrder);
    return Microsoft::Xna::Framework::Matrix::CreateFromQuaternion(
        {quaternion.x, quaternion.y, quaternion.z, quaternion.w});
}

} // namespace MeshCraft
