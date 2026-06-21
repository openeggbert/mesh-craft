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

// System prompt sent with every request
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

// Strip markdown fences and leading whitespace; find the start of the XML.
static std::string extractXml(const std::string& s) {
    auto pos = s.find("<?xml");
    if (pos == std::string::npos) pos = s.find("<mc3");
    return (pos == std::string::npos) ? s : s.substr(pos);
}

// Serialise the current document to a string for the AI context.
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

// Parse an mc3.xml string into a document; throws on malformed XML or missing <mc3>.
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

// Validate that the AI response contains parseable MC3 XML.
static std::string validateAiXml(const std::string& raw) {
    std::string xml = extractXml(raw);
    if (xml.find("<mc3") == std::string::npos)
        throw std::runtime_error("AI response does not contain a <mc3> root element");
    // Attempt a parse to catch malformed XML early
    parseXml(xml);
    return xml;
}

// ---------------------------------------------------------------------------
// drawAiPanel
// ---------------------------------------------------------------------------

void MeshCraftApplication::drawAiPanel() {
    if (!showAiPanel_) return;

    // Poll async result each frame
    aiAssistant_.poll();

    // Pre-fill API key from environment if buffer is empty
    if (aiApiKeyBuf_[0] == '\0') {
        const char* envKey = std::getenv("ANTHROPIC_API_KEY");
        if (envKey && *envKey)
            std::strncpy(aiApiKeyBuf_, envKey, sizeof(aiApiKeyBuf_) - 1);
    }

    ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
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
                              ImVec2(-1, 120));

    ImGui::Spacing();

    // ---- Status / controls ----
    bool inFlight = aiAssistant_.isInFlight();
    bool hasDone  = aiAssistant_.isDone();

    if (inFlight) {
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.1f, 1.0f),
                           "Sending to Claude... (may take up to 60 s)");
    }

    if (hasDone && aiAssistant_.hasError()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        ImGui::TextWrapped("Error: %s", aiAssistant_.errorMsg().c_str());
        ImGui::PopStyleColor();
    }

    if (hasDone && !aiAssistant_.hasError()) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                           "Response received.");
        const auto& res = aiAssistant_.result();
        std::string preview = res.size() > 300 ? res.substr(0, 297) + "..." : res;
        ImGui::InputTextMultiline("##aiprev", const_cast<char*>(preview.c_str()),
                                  preview.size() + 1, ImVec2(-1, 80),
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
    if (ImGui::Button("Send", ImVec2(90, 0))) {
        aiAssistant_.reset();
        aiPendingDoc_.reset();
        aiAssistant_.apiKey = aiApiKeyBuf_;
        aiAssistant_.model  = (aiModelBuf_[0] != '\0') ? aiModelBuf_ : "claude-sonnet-4-6";
        try {
            // Build context: either full scene or selection subset
            std::string sceneXml;
            if (aiScopeSel_ == 1) {
                // Selection scope: build a mini-doc from selected objects
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
            std::string userMsg = sceneXml + "\n\nTask: " + aiPromptBuf_;
            aiAssistant_.sendAsync(kSystemPrompt, userMsg);
            setStatusMsg("AI request sent…");
        } catch (...) {
            setStatusMsg("Failed to serialize scene", true);
        }
    }
    if (!canSend) ImGui::EndDisabled();
    if (aiScopeSel_ == 1 && selectionEmpty)
        ImGui::SameLine(), ImGui::TextDisabled("(select objects first)");

    // ---- Apply button ----
    if (hasDone && !aiAssistant_.hasError()) {
        ImGui::SameLine();
        if (ImGui::Button("Apply to Scene", ImVec2(130, 0))) {
            try {
                std::string validXml = validateAiXml(aiAssistant_.result());
                Mc3::Mc3Document newDoc = parseXml(validXml);
                aiPendingDoc_ = newDoc;  // store for registry integration
                pushUndo();
                document_ = std::move(newDoc);
                modified_ = true;
                updateWindowTitle();
                setStatusMsg("AI response applied to scene");
                aiAssistant_.reset();
            } catch (const std::exception& ex) {
                setStatusMsg(std::string("Apply failed: ") + ex.what(), true);
            }
        }

        // Save AI result to Registry (uses previously validated aiPendingDoc_)
        if (registry_.isOpen() && aiPendingDoc_.has_value()
            && !aiPendingDoc_->definitions.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Save AI to Registry…")) {
                showRegistryPanel_ = true;
                regSaveDlgOpen_    = true;
                // Pre-fill dialog from the AI-generated doc's first definition
                const std::string& defId = aiPendingDoc_->definitions.begin()->first;
                std::strncpy(regSaveNameBuf_, defId.c_str(), sizeof(regSaveNameBuf_) - 1);
                std::strncpy(regSaveGroupBuf_, "AI", sizeof(regSaveGroupBuf_) - 1);
                std::strncpy(regSaveSourceBuf_, "ai_generated", sizeof(regSaveSourceBuf_) - 1);
                regSaveDescBuf_[0] = '\0';
                regSaveVariantBuf_[0] = '\0';
                regSaveTagsBuf_[0] = '\0';
                regSaveDefId_    = defId;
                regSaveFromAi_   = true;
            }
        }
    }

    // ---- Reset button ----
    if (hasDone) {
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            aiAssistant_.reset();
            aiPendingDoc_.reset();
        }
    }

    ImGui::End();
}

} // namespace MeshCraft
