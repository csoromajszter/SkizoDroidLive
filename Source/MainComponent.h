#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected,
                        double sampleRate) override;

    void getNextAudioBlock (
        const juce::AudioSourceChannelInfo& bufferToFill) override;

    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int numTracks = 4;
    static constexpr int numScenes = 4;

    struct LoopTrack
    {
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        juce::File loadedFile;
    };

    std::array<LoopTrack, numTracks> tracks;

    juce::AudioFormatManager formatManager;
    juce::MixerAudioSource mixer;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label bpmLabel;

    juce::Slider bpmSlider;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };

    std::array<juce::TextButton, numTracks> loadButtons;
    std::array<juce::TextButton, numScenes> sceneButtons;

    std::unique_ptr<juce::FileChooser> fileChooser;

    bool playing = false;

    int currentScene = 0;
    int queuedScene = -1;

    double currentSampleRate = 44100.0;
    double samplesSinceBarStart = 0.0;

    void chooseLoopForTrack (int trackIndex);

    void triggerScene (int sceneIndex);
    void startSceneNow (int sceneIndex);

    void startPlayback();
    void stopAll();

    void updateStatus();

    double getSamplesPerBar() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
