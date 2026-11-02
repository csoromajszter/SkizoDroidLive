#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

class MainComponent : public juce::AudioAppComponent,
                      public juce::Button::Listener,
                      public juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected,
                        double sampleRate) override;

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void buttonClicked (juce::Button* button) override;

    void timerCallback() override;

private:
    struct LoopTrack
    {
        juce::TextButton loadButton;
        juce::TextButton muteButton;
        juce::Slider volumeSlider;

        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        juce::AudioTransportSource transportSource;

        juce::File loadedFile;
    };

    static constexpr int numTracks = 4;
    static constexpr int numScenes = 4;

    std::array<LoopTrack, numTracks> tracks;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };

    juce::Label bpmLabel;
    juce::Slider bpmSlider;

    std::array<juce::TextButton, numScenes> sceneButtons;

    juce::Label currentSceneLabel;

    juce::AudioFormatManager formatManager;

    bool isPlaying = false;

    int currentScene = 0;
    int pendingScene = -1;

    double currentSampleRate = 44100.0;
    double samplesSinceBarStart = 0.0;

    void loadLoopForTrack (int trackIndex);
    void startPlayback();
    void stopPlayback();
    void requestSceneChange (int sceneIndex);
    void applySceneChange (int sceneIndex);

    double getSamplesPerBar() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
