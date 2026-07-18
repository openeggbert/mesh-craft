#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraft/AiAssistant.hpp"
#include "MeshCraft/ModelRegistry.hpp"
#include "AiResponseAlgorithms.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <MeshCraft/TempFile.hpp>
#include <imgui.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace MeshCraft {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* kStrictXmlRules =
    "\n\n---\n\n"
    "STRICT XML RULES (violations cause parse errors — follow exactly):\n"
    "  1. name/id attributes: ONLY ASCII letters, digits, underscores, hyphens.\n"
    "     NO Czech/accented chars, NO spaces. Use name=\"dum_prvni_patro\" NOT name=\"dum prvni patro\".\n"
    "  2. Inside any attribute value: escape & as &amp;  < as &lt;  > as &gt;\n"
    "  3. Use ONLY straight double-quotes (\") — never curly/smart quotes.\n"
    "  4. Numeric attributes (position, rotation, scale, size, radius): space-separated numbers only.\n"
    "  5. EVERY opening tag MUST end with > before any child or sibling element.\n"
    "     WRONG: <group name=\"x\" position=\"0 0 0\"\n     <child/>  (missing > on group)\n"
    "     RIGHT:  <group name=\"x\" position=\"0 0 0\"><child/></group>\n"
    "  6. Modify the provided scene XML — do NOT return an empty document.\n"
    "  7. Return ONLY valid XML starting with <?xml — no prose, no markdown fences.";

// Load the MC3 format specification from gen.md (used as AI system prompt).
// Searches a few candidate paths relative to the binary / working directory.
static std::string buildSystemPrompt() {
    namespace fs = std::filesystem;
    // Candidate locations: cwd, parent of cwd, source tree absolute path
    std::vector<fs::path> candidates = {
        "gen.md",
        "../gen.md",
        "../../gen.md",
    };
    // Also try next to the executable if we can find it via /proc/self/exe
    {
        std::error_code ec;
        auto exe = fs::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            candidates.push_back(exe.parent_path() / "gen.md");
            candidates.push_back(exe.parent_path().parent_path() / "gen.md");
        }
    }
    for (const auto& p : candidates) {
        std::ifstream f(p);
        if (!f) continue;
        std::string spec(std::istreambuf_iterator<char>(f), {});
        if (spec.size() > 500)   // sanity check: real file is ~10KB
            return spec + kStrictXmlRules;
    }
    // Fallback if gen.md not found
    return
        "You are a 3D scene editor assistant for the MC3 XML format (version 0.3).\n"
        "Apply the user's instruction to the provided scene XML and return a complete valid mc3.xml.\n"
        "Use <box>, <sphere>, <cylinder>, <cone>, <plane>, <group>, <instance>, <definitions>.\n"
        + std::string(kStrictXmlRules);
}

// Temp file is always removed, even if saveToFile()/read throws partway
// through (STAB-0392 — see the identical fix in AiResponseAlgorithms.hpp's
// parseXmlAlg for the matching leak on the response-parsing side).
static std::string serializeScene(const Mc3::Mc3Document& doc) {
    namespace fs = std::filesystem;
    auto tmp = uniqueTempPath("mc_ai_scene", ".mc3.xml");
    std::error_code ec;
    try {
        doc.saveToFile(tmp);
        std::ifstream f(tmp);
        std::string xml(std::istreambuf_iterator<char>(f), {});
        fs::remove(tmp, ec);
        return xml;
    } catch (...) {
        fs::remove(tmp, ec);
        throw;
    }
}

// Builds exactly the XML that a click of "Send" would transmit for the
// current scope selection — shared by the pre-Send byte-count label
// (STAB-0409) and the Send button handler itself, so they can never disagree.
static std::string buildOutgoingSceneXml(const Mc3::Mc3Document& document,
                                          Editor::SelectionManager& selection,
                                          int scopeSel) {
    if (scopeSel == 1) {
        Mc3::Mc3Document selDoc;
        selDoc.version = document.version;
        selDoc.model   = "selection";
        for (const auto& obj : selection.selection())
            selDoc.objects.push_back(obj);
        selDoc.materials   = document.materials;
        selDoc.definitions = document.definitions;
        return serializeScene(selDoc);
    }
    return serializeScene(document);
}

// extractXml() / repairXml() / parseXml() moved to AiResponseAlgorithms.hpp
// (as extractXmlAlg/repairXmlAlg/parseXmlAlg) so they can be unit-tested
// headlessly — this file calls the same functions, not a duplicate.

// ---------------------------------------------------------------------------
// drawAiPanel
// ---------------------------------------------------------------------------

void MeshCraftApplication::drawAiPanel() {
    if (!showAiPanel_) return;

    aiAssistant_.poll();

    // Auto-populate aiPendingDoc_ as soon as a response arrives.
    // This runs once per response (guarded by: pending not yet set and no error yet recorded).
    if (aiAssistant_.isDone() && !aiAssistant_.hasError()
            && !aiPendingDoc_.has_value() && aiValidationError_.empty()) {
        if (aiAssistant_.wasTruncated()) {
            aiValidationError_ =
                "AI response was cut off by the token limit (max_tokens="
                + std::to_string(aiAssistant_.maxTokens) + ").\n"
                "The generated scene was too large. "
                "Increase Max Tokens in the AI panel (up to 64000) or simplify the prompt.";
        } else {
            AiResponseParseResultAlg parseResult =
                validateAndParseAiResponseAlg(aiAssistant_.result());
            if (parseResult.doc.has_value()) {
                aiPendingDoc_ = std::move(parseResult.doc);
            } else {
                std::string errMsg = parseResult.errorMessage;
                // Extract line number from tinyxml2 message ("Line number=NNN") and show that line
                auto lineTag = errMsg.find("Line number=");
                if (lineTag != std::string::npos) {
                    int lineNo = std::atoi(errMsg.c_str() + lineTag + 12);
                    if (lineNo > 0) {
                        const std::string& raw = aiAssistant_.result();
                        int cur = 1;
                        std::string badLine;
                        std::istringstream ss(raw);
                        for (std::string ln; std::getline(ss, ln); ) {
                            if (cur++ == lineNo) { badLine = ln; break; }
                        }
                        if (!badLine.empty()) {
                            if (badLine.size() > 200) badLine = badLine.substr(0, 197) + "...";
                            errMsg += "\nLine content: " + badLine;
                        }
                    }
                }
                aiValidationError_ = errMsg;
            }
        }
    }

    // Pre-fill API key from environment if buffer is empty
    if (aiApiKeyBuf_[0] == '\0') {
        const char* envKey = std::getenv("ANTHROPIC_API_KEY");
        if (envKey && *envKey)
            copyToBuf(aiApiKeyBuf_, envKey);
    }

    ImGui::SetNextWindowSize(ImVec2(520, 540), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("AI Assistant", &showAiPanel_)) {
        ImGui::End();
        return;
    }

    // ---- API key ----
    ImGui::Text("Anthropic API key:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##aikey", aiApiKeyBuf_, sizeof(aiApiKeyBuf_),
                     ImGuiInputTextFlags_Password);

    // ---- Model ----
    ImGui::Text("Model:");
    ImGui::SameLine(70);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##aimodel", aiModelBuf_, sizeof(aiModelBuf_));

    // ---- Max tokens ----
    ImGui::Text("Max tokens:");
    ImGui::SameLine(90);
    ImGui::SetNextItemWidth(-1);
    // AlwaysClamp (AUDIT-0049): without it, Ctrl+Click text entry can set
    // this outside the API's supported range, which the request would then
    // send verbatim with no other downstream validation.
    ImGui::SliderInt("##aimtok", &aiAssistant_.maxTokens, 4096, 64000, "%d", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("claude-sonnet-5 supports up to 64 000 output tokens.\n"
                          "Large scenes need 20 000+. Default: 32 000.");

    ImGui::Spacing();

    // ---- Scope ----
    ImGui::Text("Scope:");
    ImGui::SameLine(70);
    const char* scopeItems[] = {"Full scene", "Selection only"};
    ImGui::SetNextItemWidth(180);
    ImGui::Combo("##aiscope", &aiScopeSel_, scopeItems, 2);

    ImGui::Spacing();

    // ---- Prompt ----
    ImGui::Text("Prompt:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextMultiline("##aiprompt", aiPromptBuf_, sizeof(aiPromptBuf_),
                              ImVec2(-1, 100));

    ImGui::Spacing();

    // ---- Status messages ----
    bool inFlight = aiAssistant_.isInFlight();
    bool hasDone  = aiAssistant_.isDone();

    if (inFlight)
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.1f, 1.0f),
                           "Sending to Claude... (large scenes can take 2–5 min)");

    if (hasDone && aiAssistant_.hasError()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        ImGui::TextWrapped("API error: %s", aiAssistant_.errorMsg().c_str());
        ImGui::PopStyleColor();
    }

    if (!aiValidationError_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        ImGui::TextWrapped("Validation error: %s", aiValidationError_.c_str());
        ImGui::PopStyleColor();
    }

    if (aiPendingDoc_.has_value()) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "Response validated.");
        // Preview truncated raw response
        const auto& res = aiAssistant_.result();
        std::string preview = res.size() > 280 ? res.substr(0, 277) + "..." : res;
        ImGui::InputTextMultiline("##aiprev", const_cast<char*>(preview.c_str()),
                                  preview.size() + 1, ImVec2(-1, 70),
                                  ImGuiInputTextFlags_ReadOnly);

        // SYS-W14-08: preview/diff of what "Apply to Scene" would actually
        // change, shown before the user commits. aiApplyNeedsConfirmationAlg()
        // below only warns on a drastic top-level *count* drop; this shows
        // which specific objects would be added/removed/modified (by id).
        AiChangeSummaryAlg change = computeAiChangeSummaryAlg(document_, *aiPendingDoc_);
        ImGui::Spacing();
        ImGui::TextDisabled("Changes vs. current scene:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%zu added", change.added.size());
        ImGui::SameLine(); ImGui::TextDisabled(",");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.0f), "%zu removed", change.removed.size());
        ImGui::SameLine(); ImGui::TextDisabled(",");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "%zu modified*", change.modified.size());
        if (!change.added.empty() || !change.removed.empty() || !change.modified.empty()) {
            if (ImGui::TreeNode("##aidiffdetail", "Show details")) {
                constexpr size_t kMaxListed = 20;
                auto listChanges = [](const char* label, const std::vector<AiObjectChangeAlg>& items) {
                    if (items.empty()) return;
                    ImGui::Text("%s (%zu):", label, items.size());
                    for (size_t i = 0; i < items.size() && i < kMaxListed; ++i) {
                        const auto& c = items[i];
                        ImGui::BulletText("%s (%s)", c.name.empty() ? "(unnamed)" : c.name.c_str(),
                                          c.id.c_str());
                    }
                    if (items.size() > kMaxListed)
                        ImGui::TextDisabled("  ... and %zu more", items.size() - kMaxListed);
                };
                listChanges("Added", change.added);
                listChanges("Removed", change.removed);
                listChanges("Modified", change.modified);
                ImGui::TreePop();
            }
            ImGui::TextDisabled("* modified = name/type/visible/material/transform changed "
                                 "(not an exhaustive field-by-field diff)");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---- Send button ----
    bool selectionEmpty = !selection_.hasSelection();
    bool canSend = !inFlight && aiApiKeyBuf_[0] != '\0' && aiPromptBuf_[0] != '\0'
                   && !(aiScopeSel_ == 1 && selectionEmpty);

    // STAB-0409: show the size of the context about to be sent, computed via
    // the exact same buildOutgoingSceneXml() call the Send button itself
    // uses, so this can never drift out of sync with what's actually sent.
    if (!inFlight && !(aiScopeSel_ == 1 && selectionEmpty)) {
        size_t bytes = buildOutgoingSceneXml(document_, selection_, aiScopeSel_).size();
        ImGui::TextDisabled("Sending %zu bytes of scene XML to AI", bytes);
    }

    if (!canSend) ImGui::BeginDisabled();
    if (ImGui::Button("Send", ImVec2(80, 0))) {
        // Clear previous result before starting a new request
        aiAssistant_.reset();
        aiPendingDoc_.reset();
        aiValidationError_.clear();
        aiApplyConfirmPending_ = false;
        aiAssistant_.apiKey = aiApiKeyBuf_;
        aiAssistant_.model  = (aiModelBuf_[0] != '\0') ? aiModelBuf_ : "claude-sonnet-5";
        try {
            std::string sceneXml = buildOutgoingSceneXml(document_, selection_, aiScopeSel_);
            aiAssistant_.sendAsync(buildSystemPrompt(), sceneXml, aiPromptBuf_);
            setStatusMsg("AI request sent…");
        } catch (...) {
            setStatusMsg("Failed to serialize scene", true);
        }
    }
    if (!canSend) ImGui::EndDisabled();
    if (aiScopeSel_ == 1 && selectionEmpty)
        ImGui::SameLine(), ImGui::TextDisabled("(select objects first)");

    // ---- Apply to Scene — available whenever aiPendingDoc_ is valid ----
    // STAB-0395: a response with drastically fewer top-level objects than the
    // scene it would replace (e.g. 50 -> 3) is much more likely to be a
    // mistaken/truncated AI result than an intentional bulk deletion, so it
    // requires an explicit second click on "Confirm Replace" instead of
    // applying immediately.
    if (aiPendingDoc_.has_value()) {
        ImGui::SameLine();
        bool needsConfirm = aiApplyNeedsConfirmationAlg(document_.objects.size(),
                                                         aiPendingDoc_->objects.size());
        const char* label = (needsConfirm && aiApplyConfirmPending_) ? "Confirm Replace"
                                                                      : "Apply to Scene";
        if (ImGui::Button(label, ImVec2(130, 0))) {
            if (needsConfirm && !aiApplyConfirmPending_) {
                aiApplyConfirmPending_ = true;
            } else {
                pushUndo();
                document_ = *aiPendingDoc_;
                objectIndex_.invalidate();  // SYS-W5-04: wholesale document_ replacement
                modified_ = true;
                aiApplyConfirmPending_ = false;
                updateWindowTitle();
                setStatusMsg("AI response applied to scene");
                // Intentionally do NOT clear aiPendingDoc_ so "Save to Registry" stays available
            }
        }
    }
    if (aiApplyConfirmPending_ && aiPendingDoc_.has_value()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.1f, 1.0f));
        ImGui::TextWrapped(
            "Warning: this AI response has only %zu object(s), down from %zu "
            "in the current scene. Click \"Confirm Replace\" to proceed anyway.",
            aiPendingDoc_->objects.size(), document_.objects.size());
        ImGui::PopStyleColor();
    }

    // ---- Save AI result to Registry ----
    // Visible whenever aiPendingDoc_ has definitions; does NOT require registry_.isOpen().
    if (aiPendingDoc_.has_value() && !aiPendingDoc_->definitions.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Save to Registry…")) {
            // Ensure the registry is open; open the default DB if needed.
            bool regReady = registry_.isOpen();
            if (!regReady) {
                bool openThrew = false;
                try {
                    registry_.open(ModelRegistry::defaultPath());
                } catch (const std::exception& ex) {
                    setStatusMsg(std::string("Registry open failed: ") + ex.what(), true);
                    openThrew = true;
                }
                if (!openThrew) {
                    regReady = registry_.isOpen();
                    if (regReady)
                        regResultsDirty_ = true;
                    else
                        setStatusMsg("Model Registry is not available in this build", true);
                }
            }
            if (regReady) {
                showRegistryPanel_ = true;
                regSaveDlgOpen_    = true;
                const std::string& defId = aiPendingDoc_->definitions.begin()->first;
                copyToBuf(regSaveNameBuf_,   defId.c_str());
                copyToBuf(regSaveGroupBuf_,  "AI");
                copyToBuf(regSaveSourceBuf_, "ai_generated");
                regSaveDescBuf_[0]    = '\0';
                regSaveVariantBuf_[0] = '\0';
                regSaveTagsBuf_[0]    = '\0';
                regSaveDefId_  = defId;
                regSaveFromAi_ = true;
            }
        }
    }

    // ---- Reset — visible whenever there is any result state to clear ----
    if (hasDone || aiPendingDoc_.has_value() || !aiValidationError_.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            aiAssistant_.reset();
            aiPendingDoc_.reset();
            aiValidationError_.clear();
            aiApplyConfirmPending_ = false;
            // If the registry save dialog was pre-filled from this AI result, close it
            // so it cannot fall back to listing scene definitions.
            if (regSaveFromAi_) {
                regSaveDlgOpen_ = false;
                regSaveFromAi_  = false;
            }
        }
    }

    ImGui::End();
}

} // namespace MeshCraft
