// Unit test for earClipPolygon — the extrude cap triangulator.
//
// Regression for concave cross-sections (Star / arbitrary Custom): a triangle
// fan covers the concave notches and overlaps outside the outline, so the
// summed triangle area exceeds the polygon area. Ear-clipping produces a valid
// triangulation whose triangle areas sum exactly to the polygon area.

#include "MeshBuilder.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

using mc3togltf::earClipPolygon;
using Pt = std::array<float, 2>;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

static float polygonArea(const std::vector<Pt>& p) {
    float a = 0.0f;
    for (size_t i = 0; i < p.size(); ++i) {
        const Pt& u = p[i];
        const Pt& v = p[(i + 1) % p.size()];
        a += u[0] * v[1] - v[0] * u[1];
    }
    return std::fabs(a) * 0.5f;
}

static float triArea(const Pt& a, const Pt& b, const Pt& c) {
    return std::fabs((b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0])) * 0.5f;
}

// True if q lies strictly inside triangle (a,b,c).
static bool strictlyInside(const Pt& q, const Pt& a, const Pt& b, const Pt& c) {
    auto cross = [](const Pt& p0, const Pt& p1, const Pt& p2) {
        return (p1[0]-p0[0])*(p2[1]-p0[1]) - (p1[1]-p0[1])*(p2[0]-p0[0]);
    };
    float d1 = cross(a, b, q), d2 = cross(b, c, q), d3 = cross(c, a, q);
    bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    if (neg && pos) return false;          // outside
    return std::fabs(d1) > 1e-4f && std::fabs(d2) > 1e-4f && std::fabs(d3) > 1e-4f;
}

static void testPolygon(const std::vector<Pt>& poly, const std::string& name) {
    auto tris = earClipPolygon(poly);
    check(tris.size() == poly.size() - 2,
          name + ": produces exactly n-2 triangles");

    float sum = 0.0f;
    for (const auto& t : tris) sum += triArea(poly[t[0]], poly[t[1]], poly[t[2]]);
    float area = polygonArea(poly);
    check(std::fabs(sum - area) < area * 1e-3f,
          name + ": triangle areas sum to the polygon area (no overlap/overcount)");

    // No polygon vertex lies strictly inside any output triangle.
    bool anyInside = false;
    for (const auto& t : tris)
        for (size_t v = 0; v < poly.size(); ++v)
            if (v != t[0] && v != t[1] && v != t[2] &&
                strictlyInside(poly[v], poly[t[0]], poly[t[1]], poly[t[2]]))
                anyInside = true;
    check(!anyInside, name + ": no vertex is strictly inside a triangle");
}

static std::vector<Pt> makeStar(int points, float rOuter, float rInner) {
    std::vector<Pt> p;
    const float pi = std::numbers::pi_v<float>;
    for (int i = 0; i < points * 2; ++i) {
        float ang = pi * i / points;
        float r = (i % 2 == 0) ? rOuter : rInner;
        p.push_back({std::cos(ang) * r, std::sin(ang) * r});
    }
    return p;
}

int main() {
    // Convex square — trivial, must still work.
    testPolygon({{0,0}, {1,0}, {1,1}, {0,1}}, "square");

    // Concave 5-point star (the built-in Star cross-section shape).
    testPolygon(makeStar(5, 1.0f, 0.4f), "star-5");

    // A sharper concave star to stress the reflex-vertex handling.
    testPolygon(makeStar(6, 1.0f, 0.2f), "star-6-sharp");

    // Concave "arrow"/L-ish custom polygon.
    testPolygon({{0,0}, {2,0}, {2,1}, {1,1}, {1,2}, {0,2}}, "L-shape");

    if (failures == 0) { std::cout << "All ear-clip tests passed.\n"; return 0; }
    std::cerr << failures << " ear-clip test(s) failed.\n";
    return 1;
}
