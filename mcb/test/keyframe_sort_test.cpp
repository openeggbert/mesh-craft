// F17 (2026-07-20 audit) — Mc3XmlParser.cpp sorts a channel's keyframes by
// time after parsing them (stable_sort, STAB-0468); McbReader.cpp never
// did. Mc3Animation.cpp's evaluateChannel() assumes sorted input
// (std::upper_bound over kf.time) -- an out-of-order .mcb-authored channel
// (e.g. produced by a hand-rolled tool, not this codebase's own writer,
// which preserves whatever vector order it's given) silently produced
// wrong interpolation, with no error.
//
// Builds an Mc3Document directly in C++ with keyframes declared out of
// order, round-trips it through McbWriter/McbReader (no need to hand-craft
// binary bytes -- the writer preserves vector order as-is, so an unsorted
// in-memory vector produces an unsorted on-disk stream, exactly like a
// hostile/hand-rolled writer would), and confirms the reader hands back a
// sorted vector.

#include "MeshCraft/Mcb/McbReader.hpp"
#include "MeshCraft/Mcb/McbWriter.hpp"
#include "MeshCraft/Mc3/Mc3Animation.hpp"
#include "MeshCraft/Mc3/Mc3Document.hpp"

#include <iostream>
#include <sstream>
#include <string>

using namespace MeshCraft::Mc3;
using namespace MeshCraft::Mcb;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    Mc3Document doc;
    doc.version = "0.3";
    doc.model   = "kfsort";

    Mc3Channel ch;
    ch.targetObject = "Wheel";
    ch.property     = AnimatedProperty::RotationY;
    ch.keyframes.push_back(Mc3Keyframe::linear(2.0f, 200.0f));
    ch.keyframes.push_back(Mc3Keyframe::linear(0.0f, 0.0f));
    ch.keyframes.push_back(Mc3Keyframe::linear(1.0f, 100.0f));

    Mc3Action act;
    act.name     = "Spin";
    act.duration = 2.0f;
    act.channels.push_back(std::move(ch));
    doc.actions[act.name] = std::move(act);

    std::stringstream stream;
    saveToBinary(doc, stream);
    Mc3Document reloaded = loadFromBinary(stream);

    check(reloaded.actions.count("Spin") == 1, "Sanity: action round-tripped");
    if (reloaded.actions.count("Spin")) {
        const auto& racts = reloaded.actions.at("Spin");
        check(racts.channels.size() == 1, "Sanity: one channel round-tripped");
        if (!racts.channels.empty()) {
            const auto& kfs = racts.channels.front().keyframes;
            check(kfs.size() == 3, "All 3 keyframes present after round-trip");
            bool sorted = kfs.size() == 3 &&
                          kfs[0].time == 0.0f && kfs[1].time == 1.0f && kfs[2].time == 2.0f;
            check(sorted, "Keyframes are sorted ascending by time despite "
                  "out-of-order declaration order (got times " +
                  (kfs.size() == 3 ? std::to_string(kfs[0].time) + ", " +
                   std::to_string(kfs[1].time) + ", " + std::to_string(kfs[2].time) : "?") + ")");
            bool valuesMatchTimes = kfs.size() == 3 &&
                kfs[0].value == 0.0f && kfs[1].value == 100.0f && kfs[2].value == 200.0f;
            check(valuesMatchTimes,
                  "Each keyframe's value traveled with its own time during the sort");

            float atHalf = evaluateChannel(racts.channels.front(), 0.5f);
            check(atHalf > 40.0f && atHalf < 60.0f,
                  "evaluateChannel() at t=0.5 interpolates to roughly 50, not garbage "
                  "from unsorted input (got " + std::to_string(atHalf) + ")");
        }
    }

    if (failures == 0) { std::cout << "All MCB keyframe-sort tests passed.\n"; return 0; }
    std::cerr << failures << " MCB keyframe-sort test(s) failed.\n";
    return 1;
}
