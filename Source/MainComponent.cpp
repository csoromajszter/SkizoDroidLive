#include "MainComponent.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
int barsToItemId (int bars)
{
    if (bars == 1) return 1;
    if (bars == 2) return 2;
    if (bars == 4) return 3;
    if (bars == 8) return 4;
    if (bars == 16) return 5;
    return 3;
}

int itemIdToBars (int itemId)
{
    switch (itemId)
    {
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        case 4: return 8;
        case 5: return 16;
        default: break;
    }

    return 4;
}
}

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();
    initialiseDefaults();

    logoDrawable = juce::Drawable::createFromImageData (
        BinaryData::skizodroid_logo_header_png,
        BinaryData::skizodroid_logo_header_pngSize);

    titleLabel.setText ("SkizoDroid Live", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    projectLabel.setText ("NO PROJECT", juce::dontSendNotification);
    projectLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (projectLabel);

    selectedSceneLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (selectedSceneLabel);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    bpmSlider.onValueChange = [this]
    {
        if (! loadingProjectUi && projectFile.existsAsFile())
            saveProject();
    };
    addAndMakeVisible (bpmSlider);

    sceneNameLabel.setText ("SCENE NAME", juce::dontSendNotification);
    sceneNameLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (sceneNameLabel);

    sceneNameEditor.setTextToShowWhenEmpty ("Scene name", juce::Colours::grey);
    sceneNameEditor.onTextChange = [this]
    {
        if (loadingProjectUi)
            return;

        sceneNames[selectedScene] = sceneNameEditor.getText().trim();
        refreshSceneButtons();
        refreshSongUi();

        if (projectFile.existsAsFile())
            saveProject();
    };
    addAndMakeVisible (sceneNameEditor);

    sceneBarsLabel.setText ("LENGTH", juce::dontSendNotification);
    sceneBarsLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (sceneBarsLabel);

    sceneBarsBox.addItem ("1 bar", 1);
    sceneBarsBox.addItem ("2 bars", 2);
    sceneBarsBox.addItem ("4 bars", 3);
    sceneBarsBox.addItem ("8 bars", 4);
    sceneBarsBox.addItem ("16 bars", 5);
    sceneBarsBox.onChange = [this]
    {
        if (loadingProjectUi)
            return;

        sceneBars[selectedScene] = itemIdToBars (sceneBarsBox.getSelectedId());
        refreshSceneButtons();
        updateStatus();

        if (projectFile.existsAsFile())
            saveProject();
    };
    addAndMakeVisible (sceneBarsBox);

    newProjectButton.onClick = [this] { newProject(); };
    openProjectButton.onClick = [this] { openProject(); };
    saveProjectButton.onClick = [this] { saveProject(); };
    saveAsProjectButton.onClick = [this] { saveProjectAs(); };

    addAndMakeVisible (newProjectButton);
    addAndMakeVisible (openProjectButton);
    addAndMakeVisible (saveProjectButton);
    addAndMakeVisible (saveAsProjectButton);

    playButton.onClick = [this] { startPlayback(); };
    stopButton.onClick = [this] { stopAll(); };
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    copySceneButton.onClick = [this] { copySelectedScene(); };
    pasteSceneButton.onClick = [this] { pasteSceneToSelected(); };
    clearSceneButton.onClick = [this] { clearSelectedScene(); };

    addAndMakeVisible (copySceneButton);
    addAndMakeVisible (pasteSceneButton);
    addAndMakeVisible (clearSceneButton);

    copySceneButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (44, 52, 68));
    pasteSceneButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (44, 52, 68));
    clearSceneButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (68, 44, 44));

    patternModeButton.onClick = [this] { setPerformanceMode (false); };
    songModeButton.onClick = [this] { setPerformanceMode (true); };
    addToSongButton.onClick = [this] { addSelectedSceneToSong(); };
    removeSongStepButton.onClick = [this] { removeSelectedSongStep(); };
    holdSongStepButton.onClick = [this] { toggleSelectedSongStepHold(); };
    continueSongButton.onClick = [this] { continueSong(); };
    clearSongButton.onClick = [this] { clearSong(); };
    prevSongPageButton.onClick = [this]
    {
        if (songPage > 0)
        {
            --songPage;
            refreshSongUi();
        }
    };
    nextSongPageButton.onClick = [this]
    {
        const int maxPage = juce::jmax (0, (juce::jmax (songStepCount, 1) - 1) / visibleSongSteps);
        if (songPage < maxPage)
        {
            ++songPage;
            refreshSongUi();
        }
    };

    for (auto* button : { &patternModeButton, &songModeButton, &addToSongButton,
                           &removeSongStepButton, &holdSongStepButton, &continueSongButton,
                           &clearSongButton, &prevSongPageButton, &nextSongPageButton })
        addAndMakeVisible (*button);

    patternModeButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (70, 70, 88));
    songModeButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (38, 38, 44));
    holdSongStepButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (86, 64, 34));
    continueSongButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (42, 72, 48));
    clearSongButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (68, 44, 44));

    songPageLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (songPageLabel);

    for (int i = 0; i < visibleSongSteps; ++i)
    {
        songStepButtons[i].onClick = [this, i]
        {
            selectSongStep (songPage * visibleSongSteps + i);
        };
        addAndMakeVisible (songStepButtons[i]);
    }

    for (int i = 0; i < numTracks; ++i)
    {
        trackNameEditors[i].setText (trackNames[i], false);
        trackNameEditors[i].setJustification (juce::Justification::centred);
        trackNameEditors[i].onTextChange = [this, i]
        {
            if (loadingProjectUi)
                return;

            const auto newName = trackNameEditors[i].getText().trim();
            trackNames[i] = newName.isNotEmpty() ? newName : ("T" + juce::String (i + 1));
            refreshTrackButtons();

            if (projectFile.existsAsFile())
                saveProject();
        };
        addAndMakeVisible (trackNameEditors[i]);

        loadButtons[i].onClick = [this, i] { chooseLoopForTrack (i); };
        loadButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (42, 42, 48));
        addAndMakeVisible (loadButtons[i]);

        clearButtons[i].setButtonText ("CLEAR");
        clearButtons[i].onClick = [this, i] { clearClipForTrack (i); };
        clearButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (68, 44, 44));
        addAndMakeVisible (clearButtons[i]);

        mixer.addInputSource (&tracks[i].transport, false);
    }

    for (int i = 0; i < numScenes; ++i)
    {
        sceneButtons[i].onClick = [this, i]
        {
            if (playing && ! songMode)
                triggerScene (i);
            else
                selectSceneForEditing (i);
        };

        addAndMakeVisible (sceneButtons[i]);
    }

    setSize (1500, 980);
    setAudioChannels (0, 2);

    refreshTrackNameEditors();
    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    refreshSongUi();
    updateProjectLabel();
    updateStatus();
}

MainComponent::~MainComponent()
{
    shutdownAudio();

    for (auto& track : tracks)
    {
        track.transport.stop();
        track.transport.setSource (nullptr);
        track.readerSource.reset();
    }

    mixer.removeAllInputs();
}

void MainComponent::initialiseDefaults()
{
    const std::array<juce::String, numTracks> defaults
    {
        "DRUMS", "BASS", "SYNTH A", "SYNTH B",
        "NOISE", "ATMOS", "FX", "VOCAL",
        "TRACK 9", "TRACK 10", "TRACK 11", "TRACK 12",
        "TRACK 13", "TRACK 14", "TRACK 15", "TRACK 16"
    };

    trackNames = defaults;

    for (int i = 0; i < numScenes; ++i)
    {
        sceneNames[i] = "Scene " + juce::String (i + 1);
        sceneBars[i] = 4;
    }
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    mixer.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    mixer.getNextAudioBlock (bufferToFill);

    if (! playing)
        return;

    samplesSinceSceneStart += bufferToFill.numSamples;
    const auto samplesPerScene = getSamplesPerScene (currentScene);

    if (samplesPerScene > 0.0 && samplesSinceSceneStart >= samplesPerScene)
    {
        samplesSinceSceneStart = std::fmod (samplesSinceSceneStart, samplesPerScene);

        if (songMode && currentSongStep >= 0)
        {
            const bool stepIsHold = songSteps[currentSongStep].hold;
            if (! stepIsHold || releaseHoldAtBoundary)
            {
                releaseHoldAtBoundary = false;
                juce::MessageManager::callAsync (
                    [safeThis = juce::Component::SafePointer<MainComponent> (this)]
                    {
                        if (safeThis != nullptr)
                            safeThis->advanceSongAtBoundary();
                    });
            }
        }
        else if (queuedScene >= 0)
        {
            const int sceneToLaunch = queuedScene;
            queuedScene = -1;

            juce::MessageManager::callAsync (
                [safeThis = juce::Component::SafePointer<MainComponent> (this), sceneToLaunch]
                {
                    if (safeThis != nullptr)
                        safeThis->startSceneNow (sceneToLaunch);
                });
        }
    }
}

void MainComponent::releaseResources()
{
    mixer.releaseResources();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (14, 14, 16));
    g.setColour (juce::Colour::fromRGB (46, 46, 52));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (16.0f), 10.0f, 2.0f);

    if (logoDrawable != nullptr)
        logoDrawable->drawWithin (g, headerLogoBounds.toFloat(), juce::RectanglePlacement::centred, 1.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (20);

    auto header = area.removeFromTop (56);
    headerLogoBounds = header.removeFromLeft (74).reduced (0, 4);
    titleLabel.setBounds (header.removeFromLeft (330));
    header.removeFromLeft (16);
    selectedSceneLabel.setBounds (header);

    statusLabel.setBounds (area.removeFromTop (24));

    auto projectRow = area.removeFromTop (40);
    newProjectButton.setBounds (projectRow.removeFromLeft (120).reduced (3));
    openProjectButton.setBounds (projectRow.removeFromLeft (80).reduced (3));
    saveProjectButton.setBounds (projectRow.removeFromLeft (80).reduced (3));
    saveAsProjectButton.setBounds (projectRow.removeFromLeft (90).reduced (3));
    projectLabel.setBounds (projectRow.reduced (8, 0));

    auto transport = area.removeFromTop (40);
    playButton.setBounds (transport.removeFromLeft (90).reduced (3));
    stopButton.setBounds (transport.removeFromLeft (90).reduced (3));
    bpmLabel.setBounds (transport.removeFromLeft (48));
    bpmSlider.setBounds (transport.removeFromLeft (210).reduced (3));

    auto sceneEditRow = area.removeFromTop (40);
    sceneNameLabel.setBounds (sceneEditRow.removeFromLeft (95));
    sceneNameEditor.setBounds (sceneEditRow.removeFromLeft (240).reduced (3));
    sceneBarsLabel.setBounds (sceneEditRow.removeFromLeft (65));
    sceneBarsBox.setBounds (sceneEditRow.removeFromLeft (100).reduced (3));
    copySceneButton.setBounds (sceneEditRow.removeFromLeft (115).reduced (3));
    pasteSceneButton.setBounds (sceneEditRow.removeFromLeft (120).reduced (3));
    clearSceneButton.setBounds (sceneEditRow.removeFromLeft (120).reduced (3));

    area.removeFromTop (6);

    auto songControlRow = area.removeFromTop (38);
    patternModeButton.setBounds (songControlRow.removeFromLeft (120).reduced (3));
    songModeButton.setBounds (songControlRow.removeFromLeft (110).reduced (3));
    addToSongButton.setBounds (songControlRow.removeFromLeft (120).reduced (3));
    removeSongStepButton.setBounds (songControlRow.removeFromLeft (95).reduced (3));
    holdSongStepButton.setBounds (songControlRow.removeFromLeft (105).reduced (3));
    continueSongButton.setBounds (songControlRow.removeFromLeft (100).reduced (3));
    clearSongButton.setBounds (songControlRow.removeFromLeft (105).reduced (3));
    prevSongPageButton.setBounds (songControlRow.removeFromLeft (36).reduced (3));
    songPageLabel.setBounds (songControlRow.removeFromLeft (90));
    nextSongPageButton.setBounds (songControlRow.removeFromLeft (36).reduced (3));

    auto songStepRow = area.removeFromTop (54);
    const int songCellW = songStepRow.getWidth() / visibleSongSteps;
    for (int i = 0; i < visibleSongSteps; ++i)
    {
        songStepButtons[i].setBounds (songStepRow.getX() + i * songCellW + 2,
                                      songStepRow.getY() + 2,
                                      songCellW - 4,
                                      songStepRow.getHeight() - 4);
    }

    area.removeFromTop (4);

    auto trackArea = area.removeFromTop (154);
    const int trackColumns = 8;
    const int trackRows = 2;
    const int trackWidth = trackArea.getWidth() / trackColumns;
    const int trackHeight = trackArea.getHeight() / trackRows;

    for (int i = 0; i < numTracks; ++i)
    {
        const int col = i % trackColumns;
        const int row = i / trackColumns;

        auto cell = juce::Rectangle<int> (trackArea.getX() + col * trackWidth + 4,
                                          trackArea.getY() + row * trackHeight + 4,
                                          trackWidth - 8,
                                          trackHeight - 8);

        trackNameEditors[i].setBounds (cell.removeFromTop (24));
        cell.removeFromTop (3);
        clearButtons[i].setBounds (cell.removeFromBottom (22));
        cell.removeFromBottom (3);
        loadButtons[i].setBounds (cell);
    }

    area.removeFromTop (6);

    auto grid = area.reduced (4);
    const int cellW = grid.getWidth() / sceneColumns;
    const int cellH = grid.getHeight() / sceneRows;

    for (int i = 0; i < numScenes; ++i)
    {
        const int col = i % sceneColumns;
        const int row = i / sceneColumns;

        sceneButtons[i].setBounds (grid.getX() + col * cellW + 3,
                                   grid.getY() + row * cellH + 3,
                                   cellW - 6,
                                   cellH - 6);
    }
}

void MainComponent::resetProjectData()
{
    stopAll();

    for (auto& scene : scenes)
        for (auto& file : scene)
            file = {};

    initialiseDefaults();
    selectedScene = 0;
    currentScene = 0;
    queuedScene = -1;
    songStepCount = 0;
    selectedSongStep = -1;
    currentSongStep = -1;
    songPage = 0;
    releaseHoldAtBoundary = false;
    for (auto& step : songSteps)
        step = {};
    bpmSlider.setValue (120.0, juce::dontSendNotification);

    refreshTrackNameEditors();
    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    refreshSongUi();
}

void MainComponent::newProject()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Create SkizoDroid Live project",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("SkizoDroid Song.sklive"),
        "*.sklive");

    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        auto chosen = chooser.getResult();
        if (chosen == juce::File())
            return;

        chosen = chosen.withFileExtension ("sklive");
        const auto baseName = chosen.getFileNameWithoutExtension();
        const auto container = chosen.getParentDirectory();

        projectFolder = container.getChildFile (baseName);
        projectFolder.createDirectory();

        audioFolder = projectFolder.getChildFile ("Audio");
        audioFolder.createDirectory();

        projectFile = projectFolder.getChildFile (baseName + ".sklive");

        resetProjectData();
        saveProject();
        updateProjectLabel();
        updateStatus();
    });
}

void MainComponent::openProject()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Open SkizoDroid Live project",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.sklive");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        if (! loadProjectFromFile (file))
            statusLabel.setText ("COULD NOT OPEN PROJECT", juce::dontSendNotification);
    });
}

void MainComponent::saveProject()
{
    if (! projectFile.existsAsFile() && projectFile == juce::File())
    {
        statusLabel.setText ("CREATE A PROJECT FIRST", juce::dontSendNotification);
        return;
    }

    if (saveProjectToFile (projectFile))
    {
        updateProjectLabel();
        updateStatus();
    }
    else
    {
        statusLabel.setText ("PROJECT SAVE FAILED", juce::dontSendNotification);
    }
}

void MainComponent::saveProjectAs()
{
    if (projectFile == juce::File())
    {
        newProject();
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save SkizoDroid Live project as",
        projectFile.getParentDirectory().getParentDirectory()
            .getChildFile (projectFile.getFileName()),
        "*.sklive");

    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        auto chosen = chooser.getResult();
        if (chosen == juce::File())
            return;

        chosen = chosen.withFileExtension ("sklive");
        const auto baseName = chosen.getFileNameWithoutExtension();
        const auto newProjectFolder = chosen.getParentDirectory().getChildFile (baseName);
        const auto newAudioFolder = newProjectFolder.getChildFile ("Audio");
        const auto newProjectFile = newProjectFolder.getChildFile (baseName + ".sklive");

        newProjectFolder.createDirectory();
        newAudioFolder.createDirectory();

        if (audioFolder.isDirectory())
            audioFolder.copyDirectoryTo (newAudioFolder);

        const auto oldAudioFolder = audioFolder;

        for (auto& scene : scenes)
        {
            for (auto& file : scene)
            {
                if (! file.existsAsFile())
                    continue;

                if (file.isAChildOf (oldAudioFolder))
                {
                    const auto relative = file.getRelativePathFrom (oldAudioFolder);
                    file = newAudioFolder.getChildFile (relative);
                }
            }
        }

        projectFolder = newProjectFolder;
        audioFolder = newAudioFolder;
        projectFile = newProjectFile;

        saveProject();
        refreshTrackButtons();
        refreshSceneButtons();
        updateProjectLabel();
    });
}

bool MainComponent::saveProjectToFile (const juce::File& file)
{
    if (file == juce::File())
        return false;

    juce::XmlElement root ("SkizoDroidLiveProject");
    root.setAttribute ("version", "0.4.2");
    root.setAttribute ("bpm", bpmSlider.getValue());

    auto* tracksXml = root.createNewChildElement ("Tracks");
    for (int i = 0; i < numTracks; ++i)
    {
        auto* trackXml = tracksXml->createNewChildElement ("Track");
        trackXml->setAttribute ("index", i);
        trackXml->setAttribute ("name", trackNames[i]);
    }

    auto* scenesXml = root.createNewChildElement ("Scenes");

    for (int sceneIndex = 0; sceneIndex < numScenes; ++sceneIndex)
    {
        auto* sceneXml = scenesXml->createNewChildElement ("Scene");
        sceneXml->setAttribute ("index", sceneIndex);
        sceneXml->setAttribute ("name", sceneNames[sceneIndex]);
        sceneXml->setAttribute ("bars", sceneBars[sceneIndex]);

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
        {
            const auto& clipFile = scenes[sceneIndex][trackIndex];
            if (! clipFile.existsAsFile())
                continue;

            auto* clipXml = sceneXml->createNewChildElement ("Clip");
            clipXml->setAttribute ("track", trackIndex);
            clipXml->setAttribute ("path", getStoredPathForFile (clipFile));
        }
    }

    auto* songXml = root.createNewChildElement ("Song");
    songXml->setAttribute ("stepCount", songStepCount);
    for (int i = 0; i < songStepCount; ++i)
    {
        auto* stepXml = songXml->createNewChildElement ("Step");
        stepXml->setAttribute ("index", i);
        stepXml->setAttribute ("scene", songSteps[i].sceneIndex);
        stepXml->setAttribute ("hold", songSteps[i].hold);
    }

    file.getParentDirectory().createDirectory();
    return file.replaceWithText (root.toString());
}

bool MainComponent::loadProjectFromFile (const juce::File& file)
{
    juce::XmlDocument document (file);
    auto root = document.getDocumentElement();

    if (root == nullptr || ! root->hasTagName ("SkizoDroidLiveProject"))
        return false;

    stopAll();

    projectFile = file;
    projectFolder = file.getParentDirectory();
    audioFolder = projectFolder.getChildFile ("Audio");
    audioFolder.createDirectory();

    for (auto& scene : scenes)
        for (auto& clip : scene)
            clip = {};

    initialiseDefaults();

    loadingProjectUi = true;
    bpmSlider.setValue (root->getDoubleAttribute ("bpm", 120.0), juce::dontSendNotification);

    if (auto* tracksXml = root->getChildByName ("Tracks"))
    {
        forEachXmlChildElementWithTagName (*tracksXml, trackXml, "Track")
        {
            const int index = trackXml->getIntAttribute ("index", -1);
            if (index >= 0 && index < numTracks)
                trackNames[index] = trackXml->getStringAttribute ("name", trackNames[index]);
        }
    }

    if (auto* scenesXml = root->getChildByName ("Scenes"))
    {
        forEachXmlChildElementWithTagName (*scenesXml, sceneXml, "Scene")
        {
            const int sceneIndex = sceneXml->getIntAttribute ("index", -1);
            if (sceneIndex < 0 || sceneIndex >= numScenes)
                continue;

            sceneNames[sceneIndex] = sceneXml->getStringAttribute (
                "name", "Scene " + juce::String (sceneIndex + 1));
            sceneBars[sceneIndex] = juce::jlimit (1, 16, sceneXml->getIntAttribute ("bars", 4));

            forEachXmlChildElementWithTagName (*sceneXml, clipXml, "Clip")
            {
                const int trackIndex = clipXml->getIntAttribute ("track", -1);
                if (trackIndex < 0 || trackIndex >= numTracks)
                    continue;

                scenes[sceneIndex][trackIndex] = resolveStoredPath (
                    clipXml->getStringAttribute ("path"));
            }
        }
    }

    songStepCount = 0;
    for (auto& step : songSteps)
        step = {};

    if (auto* songXml = root->getChildByName ("Song"))
    {
        forEachXmlChildElementWithTagName (*songXml, stepXml, "Step")
        {
            const int sceneIndex = stepXml->getIntAttribute ("scene", -1);
            if (sceneIndex < 0 || sceneIndex >= numScenes || songStepCount >= maxSongSteps)
                continue;

            songSteps[songStepCount].sceneIndex = sceneIndex;
            songSteps[songStepCount].hold = stepXml->getBoolAttribute ("hold", false);
            ++songStepCount;
        }
    }

    loadingProjectUi = false;

    selectedScene = 0;
    currentScene = 0;
    queuedScene = -1;
    selectedSongStep = songStepCount > 0 ? 0 : -1;
    currentSongStep = -1;
    songPage = 0;
    releaseHoldAtBoundary = false;

    refreshTrackNameEditors();
    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    refreshSongUi();
    updateProjectLabel();
    updateStatus();

    return true;
}

juce::File MainComponent::importAudioIntoProject (const juce::File& sourceFile)
{
    if (! sourceFile.existsAsFile() || projectFile == juce::File())
        return {};

    audioFolder.createDirectory();

    auto target = audioFolder.getChildFile (sourceFile.getFileName());

    if (target.existsAsFile())
    {
        if (target.getSize() == sourceFile.getSize())
            return target;

        target = target.getNonexistentSibling (false);
    }

    if (! sourceFile.copyFileTo (target))
        return {};

    return target;
}

juce::String MainComponent::getStoredPathForFile (const juce::File& file) const
{
    if (projectFolder.isDirectory() && file.isAChildOf (projectFolder))
        return file.getRelativePathFrom (projectFolder);

    return file.getFullPathName();
}

juce::File MainComponent::resolveStoredPath (const juce::String& storedPath) const
{
    if (juce::File::isAbsolutePath (storedPath))
        return juce::File (storedPath);

    return projectFolder.getChildFile (storedPath);
}

void MainComponent::selectSceneForEditing (int sceneIndex)
{
    if (sceneIndex < 0 || sceneIndex >= numScenes)
        return;

    selectedScene = sceneIndex;
    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    updateStatus();
}

void MainComponent::chooseLoopForTrack (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= numTracks)
        return;

    if (projectFile == juce::File())
    {
        statusLabel.setText ("CREATE OR OPEN A PROJECT FIRST", juce::dontSendNotification);
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose audio for " + sceneNames[selectedScene] + " / " + trackNames[trackIndex],
        juce::File(),
        "*.wav;*.aif;*.aiff");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (
        chooserFlags,
        [this, trackIndex] (const juce::FileChooser& chooser)
        {
            const auto sourceFile = chooser.getResult();

            if (! sourceFile.existsAsFile())
                return;

            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (sourceFile));

            if (reader == nullptr)
            {
                statusLabel.setText ("COULD NOT LOAD AUDIO FILE", juce::dontSendNotification);
                return;
            }

            const auto importedFile = importAudioIntoProject (sourceFile);

            if (! importedFile.existsAsFile())
            {
                statusLabel.setText ("AUDIO IMPORT FAILED", juce::dontSendNotification);
                return;
            }

            scenes[selectedScene][trackIndex] = importedFile;
            refreshTrackButtons();
            refreshSceneButtons();
            saveProject();
            updateStatus();
        });
}

void MainComponent::clearClipForTrack (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= numTracks)
        return;

    scenes[selectedScene][trackIndex] = {};
    refreshTrackButtons();
    refreshSceneButtons();

    if (projectFile.existsAsFile())
        saveProject();

    updateStatus();
}

void MainComponent::copySelectedScene()
{
    copiedScene = scenes[selectedScene];
    copiedSceneBars = sceneBars[selectedScene];
    hasCopiedSceneClipboard = true;
    updateStatus();
}

void MainComponent::pasteSceneToSelected()
{
    if (! hasCopiedSceneClipboard)
        return;

    scenes[selectedScene] = copiedScene;
    sceneBars[selectedScene] = copiedSceneBars;

    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();

    if (projectFile.existsAsFile())
        saveProject();

    updateStatus();
}

void MainComponent::clearSelectedScene()
{
    for (auto& clip : scenes[selectedScene])
        clip = {};

    refreshTrackButtons();
    refreshSceneButtons();

    if (projectFile.existsAsFile())
        saveProject();

    updateStatus();
}

void MainComponent::setPerformanceMode (bool useSongMode)
{
    if (playing)
        stopAll();

    songMode = useSongMode;
    currentSongStep = -1;
    releaseHoldAtBoundary = false;
    refreshSongUi();
    updateStatus();
}

void MainComponent::addSelectedSceneToSong()
{
    if (songStepCount >= maxSongSteps)
        return;

    songSteps[songStepCount].sceneIndex = selectedScene;
    songSteps[songStepCount].hold = false;
    selectedSongStep = songStepCount;
    ++songStepCount;
    songPage = selectedSongStep / visibleSongSteps;

    if (projectFile.existsAsFile())
        saveProject();

    refreshSongUi();
    updateStatus();
}

void MainComponent::removeSelectedSongStep()
{
    if (selectedSongStep < 0 || selectedSongStep >= songStepCount)
        return;

    for (int i = selectedSongStep; i < songStepCount - 1; ++i)
        songSteps[i] = songSteps[i + 1];

    songSteps[songStepCount - 1] = {};
    --songStepCount;

    if (songStepCount == 0)
        selectedSongStep = -1;
    else
        selectedSongStep = juce::jmin (selectedSongStep, songStepCount - 1);

    const int maxPage = juce::jmax (0, (juce::jmax (songStepCount, 1) - 1) / visibleSongSteps);
    songPage = juce::jmin (songPage, maxPage);

    if (projectFile.existsAsFile())
        saveProject();

    refreshSongUi();
    updateStatus();
}

void MainComponent::toggleSelectedSongStepHold()
{
    if (selectedSongStep < 0 || selectedSongStep >= songStepCount)
        return;

    songSteps[selectedSongStep].hold = ! songSteps[selectedSongStep].hold;

    if (projectFile.existsAsFile())
        saveProject();

    refreshSongUi();
    updateStatus();
}

void MainComponent::clearSong()
{
    if (playing)
        stopAll();

    for (auto& step : songSteps)
        step = {};

    songStepCount = 0;
    selectedSongStep = -1;
    currentSongStep = -1;
    songPage = 0;
    releaseHoldAtBoundary = false;

    if (projectFile.existsAsFile())
        saveProject();

    refreshSongUi();
    updateStatus();
}

void MainComponent::continueSong()
{
    if (! playing || ! songMode || currentSongStep < 0 || currentSongStep >= songStepCount)
        return;

    if (songSteps[currentSongStep].hold)
    {
        releaseHoldAtBoundary = true;
        updateStatus();
    }
}

void MainComponent::selectSongStep (int absoluteIndex)
{
    if (absoluteIndex < 0 || absoluteIndex >= songStepCount)
        return;

    selectedSongStep = absoluteIndex;
    const int sceneIndex = songSteps[absoluteIndex].sceneIndex;
    if (sceneIndex >= 0 && sceneIndex < numScenes)
        selectSceneForEditing (sceneIndex);

    refreshSongUi();
}

void MainComponent::refreshSongUi()
{
    patternModeButton.setColour (juce::TextButton::buttonColourId,
                                 ! songMode ? juce::Colour::fromRGB (70, 70, 88)
                                            : juce::Colour::fromRGB (38, 38, 44));
    songModeButton.setColour (juce::TextButton::buttonColourId,
                              songMode ? juce::Colour::fromRGB (70, 70, 88)
                                       : juce::Colour::fromRGB (38, 38, 44));

    const int maxPage = juce::jmax (0, (juce::jmax (songStepCount, 1) - 1) / visibleSongSteps);
    songPage = juce::jlimit (0, maxPage, songPage);
    songPageLabel.setText ("PAGE " + juce::String (songPage + 1), juce::dontSendNotification);

    prevSongPageButton.setEnabled (songPage > 0);
    nextSongPageButton.setEnabled (songPage < maxPage);

    for (int i = 0; i < visibleSongSteps; ++i)
    {
        const int absoluteIndex = songPage * visibleSongSteps + i;
        auto& button = songStepButtons[i];

        if (absoluteIndex < songStepCount)
        {
            const auto& step = songSteps[absoluteIndex];
            juce::String text = juce::String (absoluteIndex + 1) + ": ";
            if (step.sceneIndex >= 0 && step.sceneIndex < numScenes)
                text += sceneNames[step.sceneIndex];
            if (step.hold)
                text += "\nHOLD";

            button.setButtonText (text);
            button.setEnabled (true);
            button.setColour (juce::TextButton::buttonColourId,
                              absoluteIndex == selectedSongStep
                                  ? juce::Colour::fromRGB (64, 64, 82)
                                  : juce::Colour::fromRGB (32, 32, 38));

            if (absoluteIndex == currentSongStep && playing && songMode)
                button.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (90, 70, 40));
        }
        else
        {
            button.setButtonText ("+");
            button.setEnabled (false);
            button.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (24, 24, 28));
        }
    }

    const bool hasSelection = selectedSongStep >= 0 && selectedSongStep < songStepCount;
    removeSongStepButton.setEnabled (hasSelection);
    holdSongStepButton.setEnabled (hasSelection);
    holdSongStepButton.setButtonText (hasSelection && songSteps[selectedSongStep].hold ? "HOLD: ON" : "HOLD: OFF");
    continueSongButton.setEnabled (songMode && playing && currentSongStep >= 0
                                   && currentSongStep < songStepCount
                                   && songSteps[currentSongStep].hold);
}

void MainComponent::startSongStepNow (int songIndex)
{
    if (songIndex < 0 || songIndex >= songStepCount)
    {
        stopAll();
        return;
    }

    currentSongStep = songIndex;
    selectedSongStep = songIndex;
    releaseHoldAtBoundary = false;
    songPage = songIndex / visibleSongSteps;

    const int sceneIndex = songSteps[songIndex].sceneIndex;
    startSceneNow (sceneIndex);
    currentSongStep = songIndex;
    refreshSongUi();
    updateStatus();
}

void MainComponent::advanceSongAtBoundary()
{
    if (! songMode || currentSongStep < 0)
        return;

    const int nextStep = currentSongStep + 1;
    if (nextStep >= songStepCount)
    {
        stopAll();
        return;
    }

    startSongStepNow (nextStep);
}

void MainComponent::refreshTrackNameEditors()
{
    loadingProjectUi = true;

    for (int i = 0; i < numTracks; ++i)
        trackNameEditors[i].setText (trackNames[i], false);

    loadingProjectUi = false;
}

void MainComponent::refreshTrackButtons()
{
    for (int i = 0; i < numTracks; ++i)
    {
        const auto& file = scenes[selectedScene][i];

        if (file.existsAsFile())
            loadButtons[i].setButtonText (file.getFileNameWithoutExtension());
        else
            loadButtons[i].setButtonText ("LOAD WAV");

        clearButtons[i].setEnabled (file.existsAsFile());
    }
}

void MainComponent::refreshSceneButtons()
{
    for (int i = 0; i < numScenes; ++i)
    {
        int clipCount = 0;
        for (const auto& file : scenes[i])
            if (file.existsAsFile())
                ++clipCount;

        juce::String text = sceneNames[i];
        text += "\n" + juce::String (sceneBars[i]) + " bar";

        if (clipCount > 0)
            text += " / " + juce::String (clipCount) + " clips";

        sceneButtons[i].setButtonText (text);
        sceneButtons[i].setColour (juce::TextButton::buttonColourId,
                                   i == selectedScene
                                       ? juce::Colour::fromRGB (65, 65, 78)
                                       : juce::Colour::fromRGB (34, 34, 38));

        if (i == currentScene && playing)
            sceneButtons[i].setColour (juce::TextButton::buttonColourId,
                                       juce::Colour::fromRGB (90, 70, 40));

        if (i == queuedScene)
            sceneButtons[i].setColour (juce::TextButton::buttonColourId,
                                       juce::Colour::fromRGB (55, 75, 95));
    }
}

void MainComponent::refreshSceneEditor()
{
    loadingProjectUi = true;

    sceneNameEditor.setText (sceneNames[selectedScene], false);
    sceneBarsBox.setSelectedId (barsToItemId (sceneBars[selectedScene]), juce::dontSendNotification);

    loadingProjectUi = false;
}

void MainComponent::triggerScene (int sceneIndex)
{
    if (sceneIndex < 0 || sceneIndex >= numScenes)
        return;

    selectedScene = sceneIndex;
    refreshTrackButtons();
    refreshSceneEditor();

    if (! playing)
    {
        startSceneNow (sceneIndex);
        return;
    }

    queuedScene = sceneIndex;
    refreshSceneButtons();
    updateStatus();
}

bool MainComponent::loadSceneIntoTransports (int sceneIndex)
{
    bool anyLoaded = false;

    for (int i = 0; i < numTracks; ++i)
    {
        auto& track = tracks[i];
        track.transport.stop();
        track.transport.setSource (nullptr);
        track.readerSource.reset();
        track.loadedFile = {};

        const auto& file = scenes[sceneIndex][i];
        if (! file.existsAsFile())
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr)
            continue;

        const double sourceSampleRate = reader->sampleRate;
        auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
        newSource->setLooping (true);

        track.transport.setSource (newSource.get(), 0, nullptr, sourceSampleRate);
        track.readerSource = std::move (newSource);
        track.loadedFile = file;
        anyLoaded = true;
    }

    return anyLoaded;
}

void MainComponent::startSceneNow (int sceneIndex)
{
    currentScene = sceneIndex;
    selectedScene = sceneIndex;
    queuedScene = -1;
    samplesSinceSceneStart = 0.0;

    loadSceneIntoTransports (sceneIndex);

    for (auto& track : tracks)
    {
        if (track.readerSource != nullptr)
        {
            track.transport.setPosition (0.0);
            track.transport.start();
        }
    }

    playing = true;
    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    updateStatus();
}

void MainComponent::startPlayback()
{
    if (songMode && songStepCount > 0)
    {
        const int startStep = selectedSongStep >= 0 && selectedSongStep < songStepCount
            ? selectedSongStep
            : 0;
        startSongStepNow (startStep);
    }
    else
    {
        startSceneNow (selectedScene);
    }
}

void MainComponent::stopAll()
{
    for (auto& track : tracks)
    {
        track.transport.stop();
        track.transport.setPosition (0.0);
    }

    playing = false;
    queuedScene = -1;
    currentSongStep = -1;
    releaseHoldAtBoundary = false;
    samplesSinceSceneStart = 0.0;

    refreshSceneButtons();
    refreshSongUi();
    updateStatus();
}

void MainComponent::updateProjectLabel()
{
    if (projectFile == juce::File())
    {
        projectLabel.setText ("NO PROJECT", juce::dontSendNotification);
        return;
    }

    projectLabel.setText (
        "PROJECT: " + projectFile.getFileNameWithoutExtension() + "   |   " + projectFolder.getFullPathName(),
        juce::dontSendNotification);
}

void MainComponent::updateStatus()
{
    juce::String text = songMode ? "SONG MODE" : "PATTERN MODE";
    text += playing
        ? "  |  PLAYING  |  " + sceneNames[currentScene] + "  |  " + juce::String (sceneBars[currentScene]) + " bars"
        : "  |  STOPPED";

    if (! songMode && queuedScene >= 0)
        text += "  |  NEXT: " + sceneNames[queuedScene];

    if (songMode && playing && currentSongStep >= 0 && currentSongStep < songStepCount)
    {
        text += "  |  SONG STEP " + juce::String (currentSongStep + 1) + "/" + juce::String (songStepCount);
        if (songSteps[currentSongStep].hold)
            text += releaseHoldAtBoundary ? "  |  CONTINUE ARMED" : "  |  HOLD";
    }

    if (hasCopiedSceneClipboard)
        text += "  |  CLIPBOARD READY";

    statusLabel.setText (text, juce::dontSendNotification);

    selectedSceneLabel.setText (
        "EDIT: " + sceneNames[selectedScene] + "   |   Scene " + juce::String (selectedScene + 1),
        juce::dontSendNotification);
}

double MainComponent::getSamplesPerBar() const
{
    const double bpm = bpmSlider.getValue();
    const double secondsPerBeat = 60.0 / bpm;
    const double secondsPerBar = secondsPerBeat * 4.0;
    return secondsPerBar * currentSampleRate;
}

double MainComponent::getSamplesPerScene (int sceneIndex) const
{
    if (sceneIndex < 0 || sceneIndex >= numScenes)
        return getSamplesPerBar() * 4.0;

    return getSamplesPerBar() * static_cast<double> (sceneBars[sceneIndex]);
}
