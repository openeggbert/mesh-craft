#pragma once
#include <array>
#include <string>
#include <vector>

namespace MeshCraft::Mc3 {

// ------------------------------------------------------------------
// Cross-section types
// ------------------------------------------------------------------

enum class CrossSectionType { Rect, Circle, Polygon, Custom, Star };

struct Mc3CrossSection {
    CrossSectionType type{CrossSectionType::Rect};

    // Rect
    float width{0.3f};
    float height{0.3f};

    // Circle
    float radius{0.1f};
    float innerRadius{0.0f}; // > 0 → hollow (pipe)

    // Polygon
    int sides{6};

    // Common quality hint
    int segments{32};

    // Custom: closed 2-D polygon (CCW)
    struct Point2D { float x, y; };
    std::vector<Point2D> customPoints;
};

// ------------------------------------------------------------------
// Path types
// ------------------------------------------------------------------

enum class ExtrudePathType { Line, Arc, Helix, Polyline, Bezier };

struct Mc3PathPoint {
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 3> controlIn{0.0f, 0.0f, 0.0f}; // bezier only
};

struct Mc3ExtrudePath {
    ExtrudePathType type{ExtrudePathType::Line};

    // Line
    float length{1.0f};
    std::string axis{"y"};

    // Arc
    float arcRadius{1.0f};
    float arcAngle{180.0f}; // degrees

    // Helix
    float helixRadius{0.5f};
    float helixHeight{2.0f};
    float helixTurns{4.0f};

    // Polyline / Bezier
    std::vector<Mc3PathPoint> points;
};

// ------------------------------------------------------------------
// Combined extrude parameters
// ------------------------------------------------------------------

struct Mc3Extrude {
    Mc3CrossSection crossSection;
    Mc3ExtrudePath  path;

    float twist{0.0f};    // degrees of twist over the full path
    int   segments{32};   // path subdivisions
    bool  smooth{true};
    bool  caps{true};
};

} // namespace MeshCraft::Mc3
