#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <memory>

class MainComponent : public juce::AudioAppComponent, private juce::Timer
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
    static constexpr int visibleSongSteps = 64;

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

    // Each bank owns its own WAV readers, transports and mixer.
    // The inactive bank is constructed on the UI thread before its launch boundary.
    struct PlaybackBank
    {
        std::array<LoopTrack, numTracks> tracks;
        juce::MixerAudioSource mixer;
        int sceneIndex = -1;
        PlaybackBank();
        ~PlaybackBank();
        void prepare (int blockSize, double sampleRate);
        void start();
        void stop();
    };

    // Decode and prepare the next scene on a worker rather than on the UI/audio threads.
    class BankBuildJob;
    juce::ThreadPool preloadPool { 1 };
    std::atomic<juce::uint64> bankRequestSerial { 0 };
    int loadingScene = -1;
    int loadingStep = -1;
    bool loadingForPlay = false;

    std::shared_ptr<PlaybackBank> activeBank;
    std::shared_ptr<PlaybackBank> stagedBank;
    std::shared_ptr<PlaybackBank> fadingBank;
    juce::CriticalSection audioBankLock;
    juce::TimeSliceThread diskReadThread { "SkizoDroid WAV read-ahead" };
    int playbackBlockSize = 512;
    int stagedSceneIndex = -1;
    int stagedSongStepIndex = -1;
    int activeSongStepAudio = -1;
    int fadeSamplesRemaining = 0;
    int fadeTotalSamples = 1;
    bool boundaryHandoffPending = false;
    bool preloadRetryPending = false;
    juce::AudioBuffer<float> fadeScratch;

    std::array<SceneClipMap, numScenes> scenes;
    std::array<juce::String, numScenes> sceneNames;
    std::array<int, numScenes> sceneBars;
    std::array<SongStep, maxSongSteps> songSteps;

    juce::AudioFormatManager formatManager;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label projectLabel;
    juce::Label selectedSceneLabel;
    juce::Label bpmLabel;
    juce::Label sceneNameLabel;
    juce::Label sceneBarsLabel;
    juce::Label songPageLabel;

    juce::Slider bpmSlider;
    juce::TextEditor sceneNameEditor;
    juce::ComboBox sceneBarsBox;

    juce::TextButton newProjectButton { "NEW" };
    juce::TextButton openProjectButton { "OPEN" };
    juce::TextButton saveProjectButton { "SAVE" };
    juce::TextButton saveAsProjectButton { "SAVE AS" };
    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton patternModeButton { "PATTERN" };
    juce::TextButton songModeButton { "SONG" };

    juce::TextButton copySceneButton { "COPY" };
    juce::TextButton pasteSceneButton { "PASTE" };
    juce::TextButton clearSceneButton { "CLEAR SCENE" };
    juce::TextButton addToSongButton { "ADD TO SONG" };

    juce::TextButton assignSceneButton { "ASSIGN SCENE" };
    juce::TextButton cancelSceneAssignButton { "CANCEL" };
    juce::TextButton removeSongStepButton { "REMOVE" };
    juce::TextButton holdSongStepButton { "HOLD: OFF" };
    juce::TextButton continueSongButton { "CONTINUE" };
    juce::TextButton clearSongButton { "CLEAR SONG" };
    juce::TextButton prevSongPageButton { "<" };
    juce::TextButton nextSongPageButton { ">" };

    std::array<juce::Label, numTracks> trackLabels;
    std::array<juce::TextButton, numTracks> loadButtons;
    std::array<juce::TextButton, numTracks> clearButtons;
    std::array<juce::TextButton, numScenes> sceneButtons;
    std::array<juce::TextButton, numScenes> previewButtons;
    std::array<juce::TextButton, numScenes> chooseButtons;
    std::array<juce::TextButton, visibleSongSteps> songStepButtons;

    std::unique_ptr<juce::Drawable> logoDrawable;
    juce::Rectangle<int> headerLogoBounds;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::ApplicationProperties appProperties;
    juce::File lastUsedDirectory;
    bool saveInProgress = false;

    juce::File projectFile;
    juce::File workingFolder;
    juce::File audioFolder;

    SceneClipMap copiedScene;
    int copiedSceneBars = 4;
    bool hasCopiedSceneClipboard = false;

    bool songMode = false;
    bool choosingSongScene = false;
    bool releaseHoldAtBoundary = false;
    int songStepCount = 0;
    int selectedSongStep = -1;
    int currentSongStep = -1;
    int songPage = 0;
    int queuedSongStep = -1;
    int previewSceneIndex = -1;
    bool previewMode = false;

    std::atomic<bool> playing { false };
    // The audio callback publishes the scene actually reaching the output.
    std::atomic<int> audibleScene { -1 };
    std::atomic<int> audibleSongStep { -1 };
    int lastUiAudibleScene = -1;
    int lastUiAudibleSongStep = -1;
    bool loadingProjectUi = false;

    int selectedScene = 0;
    int currentScene = 0;
    int queuedScene = -1;

    double currentSampleRate = 44100.0;
    double samplesSinceSceneStart = 0.0;
    // Prevent duplicate queued boundary hand-offs while the UI thread processes a scene change.
    std::atomic<bool> transitionCallbackScheduled { false };


    void initialiseDefaults();
    void createFreshWorkingFolder();
    void initialiseAppSettings();
    void rememberDirectory (const juce::File& fileOrFolder);
    juce::File getLastUsedDirectory() const;
    bool writeProjectXml();
    bool readProjectXml();
    bool packWorkingFolderToProject (const juce::File& targetFile);
    bool unpackProjectToWorkingFolder (const juce::File& sourceFile);

    void newProject();
    void openProject();
    void saveProject();
    void saveProjectWithFeedback();
    bool performProjectSave (const juce::File& targetFile);
    void setSaveButtonBusy (bool busy, bool failed = false);
    void saveProjectAs();
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
    void updateModeVisibility();
    void addSelectedSceneToSong();
    void beginAssignSceneToSongStep();
    void cancelAssignSceneToSongStep();
    void assignSceneToSelectedSongStep (int sceneIndex);
    void toggleScenePreview (int sceneIndex);
    void stopScenePreview();
    void completeQueuedTransition();
    void shrinkSongTail();
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

    void triggerScene (int sceneIndex);
    void startSceneNow (int sceneIndex);
    std::shared_ptr<PlaybackBank> buildPlaybackBank (int sceneIndex,
        const SceneClipMap& clipFiles, int blockSize, double sampleRate,
        juce::uint64 serial);
    void requestBank (int sceneIndex, int songStep, bool startImmediately);
    void receiveBuiltBank (juce::uint64 serial, int sceneIndex, int songStep,
                           bool startImmediately, std::shared_ptr<PlaybackBank> bank);
    void activatePreparedBank (int sceneIndex, int songStep, std::shared_ptr<PlaybackBank> bank);
    void prepareUpcomingScene();
    void clearAudioBanks();
    void finishAudioHandoffOnUi();
    void renderBankWithFade (const juce::AudioSourceChannelInfo& block);
    int findFollowingSongStep (int fromIndex) const;

    void startPlayback();
    void stopAll();

    void timerCallback() override;
    void updateStatus();
    double getSamplesPerBar() const;
    double getSamplesPerScene (int sceneIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
