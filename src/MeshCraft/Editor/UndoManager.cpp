#include "MeshCraft/Editor/UndoManager.hpp"
#include "MeshCraft/EditorAlgorithms.hpp"

namespace MeshCraft::Editor {

void UndoManager::push(Mc3::Mc3Document doc, std::vector<std::string> selectionIds) {
    pushWithCapAlg(undoStack_, std::move(doc), kMax);
    pushWithCapAlg(undoSelectionStack_, std::move(selectionIds), kMax);
    redoStack_.clear();
    redoSelectionStack_.clear();
}

std::optional<UndoManager::Entry> UndoManager::undo(Mc3::Mc3Document currentDoc,
                                                      std::vector<std::string> currentSelectionIds) {
    if (undoStack_.empty()) return std::nullopt;
    pushWithCapAlg(redoStack_, std::move(currentDoc), kMax);
    pushWithCapAlg(redoSelectionStack_, std::move(currentSelectionIds), kMax);
    Entry result;
    result.doc = std::move(undoStack_.back());
    undoStack_.pop_back();
    result.selectionIds = std::move(undoSelectionStack_.back());
    undoSelectionStack_.pop_back();
    return result;
}

std::optional<UndoManager::Entry> UndoManager::redo(Mc3::Mc3Document currentDoc,
                                                      std::vector<std::string> currentSelectionIds) {
    if (redoStack_.empty()) return std::nullopt;
    pushWithCapAlg(undoStack_, std::move(currentDoc), kMax);
    pushWithCapAlg(undoSelectionStack_, std::move(currentSelectionIds), kMax);
    Entry result;
    result.doc = std::move(redoStack_.back());
    redoStack_.pop_back();
    result.selectionIds = std::move(redoSelectionStack_.back());
    redoSelectionStack_.pop_back();
    return result;
}

std::optional<UndoManager::Entry> UndoManager::jumpTo(int stepsAgo, Mc3::Mc3Document currentDoc,
                                                        std::vector<std::string> currentSelectionIds) {
    const int n = static_cast<int>(undoStack_.size());
    const int i = n - stepsAgo;
    if (stepsAgo < 1 || i < 0 || i >= n) return std::nullopt;

    redoStack_.push_back(std::move(currentDoc));
    redoSelectionStack_.push_back(std::move(currentSelectionIds));
    for (int j = n - 1; j > i; --j) {
        redoStack_.push_back(std::move(undoStack_[static_cast<size_t>(j)]));
        redoSelectionStack_.push_back(std::move(undoSelectionStack_[static_cast<size_t>(j)]));
    }
    while (static_cast<int>(redoStack_.size()) > kMax)
        redoStack_.erase(redoStack_.begin());
    while (static_cast<int>(redoSelectionStack_.size()) > kMax)
        redoSelectionStack_.erase(redoSelectionStack_.begin());

    Entry result;
    result.doc = std::move(undoStack_[static_cast<size_t>(i)]);
    result.selectionIds = std::move(undoSelectionStack_[static_cast<size_t>(i)]);
    undoStack_.resize(static_cast<size_t>(i));
    undoSelectionStack_.resize(static_cast<size_t>(i));
    return result;
}

void UndoManager::popUndoWithoutApplying() {
    if (!undoStack_.empty()) undoStack_.pop_back();
    if (!undoSelectionStack_.empty()) undoSelectionStack_.pop_back();
}

void UndoManager::clear() {
    undoStack_.clear();
    redoStack_.clear();
    undoSelectionStack_.clear();
    redoSelectionStack_.clear();
}

} // namespace MeshCraft::Editor
