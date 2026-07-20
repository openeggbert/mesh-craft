// F17 (2026-07-20 audit) — Mc3XmlParser.cpp sorts a channel's keyframes by
// time after parsing them (stable_sort, STAB-0468); Mc3JsonParser.cpp never
// did. Mc3Animation.cpp's evaluateChannel() assumes sorted input
// (std::upper_bound over kf.time) -- an out-of-order .mc3.json-authored
// channel silently produced wrong interpolation, with no error.

#include <MeshCraft/Mc3/Mc3Animation.hpp>
#include <MeshCraft/Mc3/Mc3Document.hpp>

#include <iostream>
#include <string>

using namespace MeshCraft::Mc3;

static int failures = 0;
static void check(bool cond, const std::string& msg) {
    if (cond) { std::cout << "PASS: " << msg << "\n"; }
    else      { std::cerr << "FAIL: " << msg << "\n"; ++failures; }
}

int main() {
    // Keyframes declared out of order (2.0, 0.0, 1.0) must come back sorted
    // ascending by time -- exercises the exact pattern evaluateChannel()'s
    // std::upper_bound relies on.
    const std::string js = R"json(
    {
      "version": "0.3",
      "model": "kfsort",
      "actions": [
        {
          "name": "Spin",
          "duration": 2.0,
          "channels": [
            {
              "target": "Wheel",
              "property": "rotation.y",
              "keyframes": [
                { "time": 2.0, "value": 200.0 },
                { "time": 0.0, "value": 0.0 },
                { "time": 1.0, "value": 100.0 }
              ]
            }
          ]
        }
      ]
    }
    )json";

    Mc3Document doc = Mc3Document::loadFromJsonString(js);
    check(doc.actions.count("Spin") == 1, "Sanity: action parsed");
    if (doc.actions.count("Spin")) {
        const auto& act = doc.actions.at("Spin");
        check(act.channels.size() == 1, "Sanity: one channel parsed");
        if (!act.channels.empty()) {
            const auto& kfs = act.channels.front().keyframes;
            check(kfs.size() == 3, "All 3 keyframes present after parsing");
            bool sorted = kfs.size() == 3 &&
                          kfs[0].time == 0.0f && kfs[1].time == 1.0f && kfs[2].time == 2.0f;
            check(sorted, "Keyframes are sorted ascending by time despite "
                  "out-of-order declaration order (got times " +
                  (kfs.size() == 3 ? std::to_string(kfs[0].time) + ", " +
                   std::to_string(kfs[1].time) + ", " + std::to_string(kfs[2].time) : "?") + ")");
            bool valuesMatchTimes = kfs.size() == 3 &&
                kfs[0].value == 0.0f && kfs[1].value == 100.0f && kfs[2].value == 200.0f;
            check(valuesMatchTimes,
                  "Each keyframe's value traveled with its own time during the sort "
                  "(not desynced by an unstable/partial reorder)");

            // Exercise the actual consumer: evaluateChannel() must now
            // interpolate correctly (it silently wouldn't, pre-fix, since
            // std::upper_bound over an unsorted range is undefined behavior
            // -- this is the real-world symptom the sort exists to prevent).
            float atHalf = evaluateChannel(act.channels.front(), 0.5f);
            check(atHalf > 40.0f && atHalf < 60.0f,
                  "evaluateChannel() at t=0.5 (halfway between the 0.0/0-value and "
                  "1.0/100-value keyframes) interpolates to roughly 50, not garbage "
                  "from unsorted input (got " + std::to_string(atHalf) + ")");
        }
    }

    // Already-sorted input must be unaffected (no reordering artifact).
    const std::string jsSorted = R"json(
    {
      "version": "0.3",
      "model": "kfsorted",
      "actions": [
        {
          "name": "Fade",
          "duration": 1.0,
          "channels": [
            {
              "target": "Light",
              "property": "visible",
              "keyframes": [
                { "time": 0.0, "value": 1.0 },
                { "time": 0.5, "value": 0.5 },
                { "time": 1.0, "value": 0.0 }
              ]
            }
          ]
        }
      ]
    }
    )json";
    Mc3Document doc2 = Mc3Document::loadFromJsonString(jsSorted);
    if (doc2.actions.count("Fade") && !doc2.actions.at("Fade").channels.empty()) {
        const auto& kfs = doc2.actions.at("Fade").channels.front().keyframes;
        check(kfs.size() == 3 && kfs[0].time == 0.0f && kfs[1].time == 0.5f && kfs[2].time == 1.0f,
              "Already-sorted keyframes remain in their original order");
    }

    if (failures == 0) { std::cout << "All keyframe-sort tests passed.\n"; return 0; }
    std::cerr << failures << " keyframe-sort test(s) failed.\n";
    return 1;
}
