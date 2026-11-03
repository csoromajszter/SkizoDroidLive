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
    static constexpr int maxSongSteps = 128;
    static constexpr int visibleSongSteps = 16;

    struct LoopTrack
    {
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        juce::File loadedFile;
    };

    using SceneClipMap = std::array<juce::File, numTracks>;

    struct SongStep
    {
        int sceneIndex = -1;
        bool hold = false;
    };

    std::array<LoopTrack, numTracks> tracks;
    std::array<SceneClipMap, numScenes> scenes;
    std::array<juce::String, numScenes> sceneNames;
    std::array<int, numScenes> sceneBars;
    std::array<juce::String, numTracks> trackNames;
    std::array<SongStep, maxSongSteps> songSteps;

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

    juce::TextButton copySceneButton { "COPY SCENE" };
    juce::TextButton pasteSceneButton { "PASTE SCENE" };
    juce::TextButton clearSceneButton { "CLEAR SCENE" };

    juce::TextButton patternModeButton { "PATTERN MODE" };
    juce::TextButton songModeButton { "SONG MODE" };
    juce::TextButton addToSongButton { "ADD TO SONG" };
    juce::TextButton removeSongStepButton { "REMOVE" };
    juce::TextButton holdSongStepButton { "HOLD: OFF" };
    juce::TextButton continueSongButton { "CONTINUE" };
    juce::TextButton clearSongButton { "CLEAR SONG" };
    juce::TextButton prevSongPageButton { "<" };
    juce::TextButton nextSongPageButton { ">" };
    juce::Label songPageLabel;

    std::array<juce::TextEditor, numTracks> trackNameEditors;
    std::array<juce::TextButton, numTracks> loadButtons;
    std::array<juce::TextButton, numTracks> clearButtons;
    std::array<juce::TextButton, numScenes> sceneButtons;
    std::array<juce::TextButton, visibleSongSteps> songStepButtons;

    std::unique_ptr<juce::Drawable> logoDrawable;
    juce::Rectangle<int> headerLogoBounds;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::File projectFile;
    juce::File projectFolder;
    juce::File audioFolder;

    SceneClipMap copiedScene;
    int copiedSceneBars = 4;
    bool hasCopiedSceneClipboard = false;

    bool songMode = false;
    bool releaseHoldAtBoundary = false;
    int songStepCount = 0;
    int selectedSongStep = -1;
    int currentSongStep = -1;
    int songPage = 0;

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
    void clearClipForTrack (int trackIndex);
    void copySelectedScene();
    void pasteSceneToSelected();
    void clearSelectedScene();

    void setPerformanceMode (bool useSongMode);
    void addSelectedSceneToSong();
    void removeSelectedSongStep();
    void toggleSelectedSongStepHold();
    void clearSong();
    void continueSong();
    void selectSongStep (int absoluteIndex);
    void refreshSongUi();
    void startSongStepNow (int songIndex);
    void advanceSongAtBoundary();

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
