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

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int numTracks = 16;
    static constexpr int numScenes = 64;
    static constexpr int sceneColumns = 8;
    static constexpr int sceneRows = 8;

    struct LoopTrack
    {
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        juce::File loadedFile;
    };

    using SceneClipMap = std::array<juce::File, numTracks>;

    std::array<LoopTrack, numTracks> tracks;
    std::array<SceneClipMap, numScenes> scenes;
    std::array<juce::String, numScenes> sceneNames;
    std::array<int, numScenes> sceneBars;
    std::array<juce::String, numTracks> trackNames;

    juce::AudioFormatManager formatManager;
    juce::MixerAudioSource mixer;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label projectLabel;
    juce::Label selectedSceneLabel;
    juce::Label bpmLabel;
    juce::Label sceneNameLabel;
    juce::Label sceneBarsLabel;

    juce::Slider bpmSlider;
    juce::TextEditor sceneNameEditor;
    juce::ComboBox sceneBarsBox;

    juce::TextButton newProjectButton { "NEW PROJECT" };
    juce::TextButton openProjectButton { "OPEN" };
    juce::TextButton saveProjectButton { "SAVE" };
    juce::TextButton saveAsProjectButton { "SAVE AS" };

    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };

    std::array<juce::TextEditor, numTracks> trackNameEditors;
    std::array<juce::TextButton, numTracks> loadButtons;
    std::array<juce::TextButton, numScenes> sceneButtons;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::File projectFile;
    juce::File projectFolder;
    juce::File audioFolder;

    bool playing = false;
    bool loadingProjectUi = false;

    int selectedScene = 0;
    int currentScene = 0;
    int queuedScene = -1;

    double currentSampleRate = 44100.0;
    double samplesSinceSceneStart = 0.0;

    void initialiseDefaults();

    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    bool saveProjectToFile (const juce::File& file);
    bool loadProjectFromFile (const juce::File& file);
    void resetProjectData();
    void updateProjectLabel();

    juce::File importAudioIntoProject (const juce::File& sourceFile);
    juce::String getStoredPathForFile (const juce::File& file) const;
    juce::File resolveStoredPath (const juce::String& storedPath) const;

    void selectSceneForEditing (int sceneIndex);
    void chooseLoopForTrack (int trackIndex);
    void refreshTrackButtons();
    void refreshSceneButtons();
    void refreshSceneEditor();
    void refreshTrackNameEditors();

    void triggerScene (int sceneIndex);
    void startSceneNow (int sceneIndex);
    bool loadSceneIntoTransports (int sceneIndex);

    void startPlayback();
    void stopAll();

    void updateStatus();
    double getSamplesPerBar() const;
    double getSamplesPerScene (int sceneIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
