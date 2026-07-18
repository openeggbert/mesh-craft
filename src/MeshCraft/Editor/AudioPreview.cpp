#include "MeshCraft/Editor/AudioPreview.hpp"

#include <Microsoft/Xna/Framework/Audio/SoundEffect.hpp>
#include <Microsoft/Xna/Framework/Audio/SoundState.hpp>

namespace MeshCraft::Editor {

using namespace Microsoft::Xna::Framework::Audio;

AudioPreview::~AudioPreview() { stop(); }

void AudioPreview::play(const std::string& key, const std::string& srcPath, bool loop) {
    stop();
    error_.clear();

    try {
        SoundEffect se(srcPath);
        instance_ = std::make_unique<SoundEffectInstance>(se.CreateInstance());
        instance_->setIsLoopedProperty(loop);
        instance_->Play();
        key_ = key;
    } catch (const std::exception& e) {
        instance_.reset();
        key_.clear();
        error_ = std::string("Playback failed: ") + e.what();
    }
}

void AudioPreview::stop() {
    if (instance_) {
        instance_->Stop();
        instance_.reset();
    }
    key_.clear();
}

bool AudioPreview::isPlaying(const std::string& key) const {
    return key == key_ && instance_ &&
           instance_->getStateProperty() == SoundState::Playing;
}

} // namespace MeshCraft::Editor
