#include <MeshCraft/RotationConventionAlgorithms.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <string_view>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* description)
{
    if (condition) std::cout << "PASS: " << description << '\n';
    else { std::cerr << "FAIL: " << description << '\n'; ++failures; }
}

float dot(const MeshCraft::RotationQuaternionAlg& a,
          const MeshCraft::RotationQuaternionAlg& b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

bool sameOrientation(const MeshCraft::RotationQuaternionAlg& a,
                     const MeshCraft::RotationQuaternionAlg& b)
{
    return std::abs(std::abs(dot(a, b)) - 1.0f) < 1.0e-4f;
}

} // namespace

int main()
{
    constexpr std::array<std::string_view, 6> orders{
        "XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"};
    constexpr std::array<float, 3> degrees{37.0f, -22.0f, 71.0f};
    constexpr float radiansPerDegree = std::numbers::pi_v<float> / 180.0f;
    const std::array<float, 3> radians{
        degrees[0] * radiansPerDegree,
        degrees[1] * radiansPerDegree,
        degrees[2] * radiansPerDegree,
    };

    for (const auto order : orders) {
        const auto degreeQuaternion = MeshCraft::rotationQuaternionAlg(degrees, "degrees", order);
        const auto radianQuaternion = MeshCraft::rotationQuaternionAlg(radians, "radians", order);
        check(sameOrientation(degreeQuaternion, radianQuaternion),
              (std::string("degrees/radians agree for ") + std::string(order)).c_str());

        const auto normalised = MeshCraft::rotationAsDegreesXYZAlg(degrees, "degrees", order);
        const auto normalisedQuaternion = MeshCraft::rotationQuaternionAlg(normalised, "degrees", "XYZ");
        check(sameOrientation(degreeQuaternion, normalisedQuaternion),
              (std::string("degrees/XYZ normalization preserves ") + std::string(order)).c_str());
    }

    const auto xyz = MeshCraft::rotationMatrix3Alg({0.0f, 90.0f, 0.0f}, "degrees", "XYZ");
    const auto forward = MeshCraft::rotateDirectionAlg({0.0f, 0.0f, -1.0f}, xyz);
    check(std::abs(forward[0] + 1.0f) < 1.0e-4f && std::abs(forward[1]) < 1.0e-4f &&
              std::abs(forward[2]) < 1.0e-4f,
          "row-vector direction rotation matches the editor forward convention");
    check(MeshCraft::normalisedEulerOrderAlg("bad") == "XYZ",
          "invalid Euler order falls back safely to XYZ");
    check(std::abs(MeshCraft::degreesInRotationUnitsAlg(180.0f, "radians") -
                       std::numbers::pi_v<float>) < 1.0e-5f,
          "gizmo degree delta converts to radians for authored radian documents");

    if (failures == 0) std::cout << "All rotation-convention algorithm tests passed.\n";
    return failures == 0 ? 0 : 1;
}
