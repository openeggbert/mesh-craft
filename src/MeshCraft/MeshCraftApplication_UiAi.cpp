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
#include <string>

namespace MeshCraft {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* kSystemPrompt =
    "You are a 3D scene editor assistant for the MC3 format.\n"
    "MC3 is an XML-based 3D scene format. Key elements:\n"
    "  <box>, <sphere>, <cylinder>, <cone>, <plane> — primitives with size/radius/height attributes\n"
    "  <group> — named group with child objects (position/rotation/scale)\n"
    "  <instance name=\"...\" definition=\"defId\"/> — instance of a reusable definition\n"
    "  <material id=\"...\" roughness=\"0.5\" metallic=\"0.0\"><base_color>R G B A</base_color></material>\n"
    "  <definitions><definition id=\"...\">[children]</definition></definitions> — reusable shapes\n"
    "  <light type=\"point|directional|spot\" position=\"X Y Z\"/>\n"
    "Apply the user's instruction to the provided scene.\n"
    "Return ONLY a complete, valid mc3.xml document starting with <?xml version=\"1.0\"?>.\n"
    "No prose, no markdown fences, no explanations — pure XML only.";

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
        try {
            std::string xml = extractXml(aiAssistant_.result());
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
            aiValidationError_ = ex.what();
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
                           "Sending to Claude... (may take up to 60 s)");

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
            aiAssistant_.sendAsync(kSystemPrompt, sceneXml + "\n\nTask: " + aiPromptBuf_);
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
