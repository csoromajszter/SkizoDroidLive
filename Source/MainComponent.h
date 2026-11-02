#pragma once
#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int numTracks = 4;
    static constexpr int numScenes = 4;

    struct Track
    {
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        juce::File file;
        float gain = 0.85f;
    };

    void timerCallback() override;
    void chooseLoopForTrack (int trackIndex);
    void triggerScene (int sceneIndex);
    void startSceneNow (int sceneIndex);
    void stopAll();
    void updateStatus();

    juce::AudioFormatManager formatManager;
    std::array<Track, numTracks> tracks;
    juce::MixerAudioSource mixer;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label bpmLabel;
    juce::Slider bpmSlider;
    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };

    std::array<juce::TextButton, numTracks> loadButtons {
        juce::TextButton { "LOAD DRUM" },
        juce::TextButton { "LOAD BASS" },
        juce::TextButton { "LOAD SYNTH" },
        juce::TextButton { "LOAD NOISE" }
    };

    std::array<juce::TextButton, numScenes> sceneButtons {
        juce::TextButton { "SCENE A" },
        juce::TextButton { "SCENE B" },
        juce::TextButton { "SCENE C" },
        juce::TextButton { "SCENE D" }
    };

    double currentSampleRate = 44100.0;
    bool playing = false;
    int currentScene = -1;
    int queuedScene = -1;
    double playheadSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
