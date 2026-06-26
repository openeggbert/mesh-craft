#include "MeshCraft/MeshCraftApplication.hpp"
#include "MeshCraft/AiAssistant.hpp"
#include "MeshCraft/ModelRegistry.hpp"

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <imgui.h>

#include <atomic>
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

static std::atomic<int> gAiTmpCounter{0};

static std::string extractXml(const std::string& s) {
    auto pos = s.find("<?xml");
    if (pos == std::string::npos) pos = s.find("<mc3");
    return (pos == std::string::npos) ? s : s.substr(pos);
}

static std::string serializeScene(const Mc3::Mc3Document& doc) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() /
               ("mc_ai_scene_" + std::to_string(gAiTmpCounter++) + ".mc3.xml");
    doc.saveToFile(tmp);
    std::ifstream f(tmp);
    std::string xml(std::istreambuf_iterator<char>(f), {});
    std::error_code ec;
    fs::remove(tmp, ec);
    return xml;
}

// Fix common AI XML mistake: opening tag not closed with '>' before next element.
// Scans char-by-char; when inside a tag and a bare '<' appears, injects '>'.
static std::string repairXml(const std::string& xml) {
    std::string out;
    out.reserve(xml.size() + 32);
    bool inTag  = false;
    bool inVal  = false;
    char quote  = 0;
    for (size_t i = 0; i < xml.size(); ++i) {
        char c = xml[i];
        if (inTag) {
            if (inVal) {
                out += c;
                if (c == quote) inVal = false;
            } else if (c == '"' || c == '\'') {
                inVal = true; quote = c; out += c;
            } else if (c == '>') {
                inTag = false; out += c;
            } else if (c == '<') {
                // Missing '>' — inject it, then start the new tag
                out += '>';
                inTag = false;
                out += c;
                inTag = true; inVal = false;
            } else {
                out += c;
            }
        } else {
            if (c == '<') inTag = true;
            out += c;
        }
    }
    return out;
}

// Parse xml string into document; throws on malformed XML or missing <mc3>.
static Mc3::Mc3Document parseXml(const std::string& xml) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() /
               ("mc_ai_resp_" + std::to_string(gAiTmpCounter++) + ".mc3.xml");
    { std::ofstream f(tmp); f << xml; }
    auto doc = Mc3::Mc3Document::loadFromFile(tmp);
    std::error_code ec;
    fs::remove(tmp, ec);
    return doc;
}

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
        } else try {
            std::string xml = repairXml(extractXml(aiAssistant_.result()));
            if (xml.find("<mc3") == std::string::npos)
                throw std::runtime_error("Response does not contain a <mc3> root element");
            Mc3::Mc3Document parsed = parseXml(xml);
            // Reject documents that would silently wipe the scene
            if (parsed.objects.empty() && parsed.definitions.empty())
                throw std::runtime_error(
                    "AI returned an empty document (no objects, no definitions). "
                    "Not applying to avoid destroying the current scene.");
            aiPendingDoc_ = std::move(parsed);
        } catch (const std::exception& ex) {
            std::string errMsg = ex.what();
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
        } // end else try
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
    ImGui::SliderInt("##aimtok", &aiAssistant_.maxTokens, 4096, 64000, "%d");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("claude-sonnet-4-6 supports up to 64 000 output tokens.\n"
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
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---- Send button ----
    bool selectionEmpty = !selection_.hasSelection();
    bool canSend = !inFlight && aiApiKeyBuf_[0] != '\0' && aiPromptBuf_[0] != '\0'
                   && !(aiScopeSel_ == 1 && selectionEmpty);
    if (!canSend) ImGui::BeginDisabled();
    if (ImGui::Button("Send", ImVec2(80, 0))) {
        // Clear previous result before starting a new request
        aiAssistant_.reset();
        aiPendingDoc_.reset();
        aiValidationError_.clear();
        aiAssistant_.apiKey = aiApiKeyBuf_;
        aiAssistant_.model  = (aiModelBuf_[0] != '\0') ? aiModelBuf_ : "claude-sonnet-4-6";
        try {
            std::string sceneXml;
            if (aiScopeSel_ == 1) {
                Mc3::Mc3Document selDoc;
                selDoc.version = document_.version;
                selDoc.model   = "selection";
                for (const auto& obj : selection_.selection())
                    selDoc.objects.push_back(obj);
                selDoc.materials   = document_.materials;
                selDoc.definitions = document_.definitions;
                sceneXml = serializeScene(selDoc);
            } else {
                sceneXml = serializeScene(document_);
            }
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
    if (aiPendingDoc_.has_value()) {
        ImGui::SameLine();
        if (ImGui::Button("Apply to Scene", ImVec2(130, 0))) {
            pushUndo();
            document_ = *aiPendingDoc_;
            modified_ = true;
            updateWindowTitle();
            setStatusMsg("AI response applied to scene");
            // Intentionally do NOT clear aiPendingDoc_ so "Save to Registry" stays available
        }
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
