#pragma once

#include <Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp>

#include <memory>
#include <string>

namespace MeshCraft::Editor {

// SYS-W3-01 Phase 7: one-shared-preview-at-a-time playback for
// Mc3Sound/Mc3Music entries (STAB-0706), extracted out of
// MeshCraftApplication. Self-contained like Preferences/KeybindingManager/
// WalkController -- no callback DI needed, and CNA-coupled where
// unavoidable (real SoundEffect/SoundEffectInstance playback). Also folds
// in a small dedup: isPlaying()'s "key matches AND instance exists AND
// state is Playing" check was previously hand-duplicated at both of its
// call sites (the sound list and the music list rows).
class AudioPreview {
public:
    ~AudioPreview();

    // Stops whatever is currently playing, clears any previous error, then
    // attempts to load srcPath and play it (looped or not) under the given
    // key. On failure (missing file, unsupported format, no audio device,
    // etc.), error() reports why and nothing plays. Loop must be set
    // before Play() -- SoundEffectInstance::setIsLoopedProperty() throws
    // once playback has started -- so it's applied here rather than being
    // toggleable afterward.
    void play(const std::string& key, const std::string& srcPath, bool loop);
    void stop();

    [[nodiscard]] bool isPlaying(const std::string& key) const;

    // The key currently associated with the preview instance (empty if
    // nothing is loaded) -- distinct from isPlaying(), which additionally
    // requires the instance to actually be in the Playing state. Used to
    // guard against removing/mutating the very entry a live preview
    // instance still references, regardless of its playback state.
    [[nodiscard]] const std::string& currentKey() const { return key_; }
    [[nodiscard]] const std::string& error() const { return error_; }

private:
    std::unique_ptr<Microsoft::Xna::Framework::Audio::SoundEffectInstance> instance_;
    std::string key_;
    std::string error_;
};

} // namespace MeshCraft::Editor
