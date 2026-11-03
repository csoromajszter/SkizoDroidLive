#include "MainComponent.h"
#include <BinaryData.h>
#include <cmath>
#include <atomic>

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
        default: return 4;
    }
}

juce::String shortName (const juce::File& file)
{
    auto s = file.getFileNameWithoutExtension();
    if (s.length() > 16)
        s = s.substring (0, 13) + "...";
    return s;
}
}

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();
    initialiseDefaults();
    createFreshWorkingFolder();
    initialiseAppSettings();

    logoDrawable = juce::Drawable::createFromImageData (
        BinaryData::skizodroid_logo_header_png,
        BinaryData::skizodroid_logo_header_pngSize);

    titleLabel.setText ("SkizoDroid Live", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (29.0f, juce::Font::bold));
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
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 22);
    bpmSlider.onValueChange = [this]
    {
        if (! loadingProjectUi && projectFile != juce::File())
            saveProject();
    };
    addAndMakeVisible (bpmSlider);

    sceneNameLabel.setText ("SCENE", juce::dontSendNotification);
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

        if (projectFile != juce::File())
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

        if (projectFile != juce::File())
            saveProject();
    };
    addAndMakeVisible (sceneBarsBox);

    newProjectButton.onClick = [this] { newProject(); };
    openProjectButton.onClick = [this] { openProject(); };
    saveProjectButton.onClick = [this] { saveProjectWithFeedback(); };
    saveAsProjectButton.onClick = [this] { saveProjectAs(); };
    playButton.onClick = [this] { startPlayback(); };
    stopButton.onClick = [this] { stopAll(); };
    patternModeButton.onClick = [this] { setPerformanceMode (false); };
    songModeButton.onClick = [this] { setPerformanceMode (true); };

    for (auto* button : { &newProjectButton, &openProjectButton, &saveProjectButton, &saveAsProjectButton,
                           &playButton, &stopButton, &patternModeButton, &songModeButton })
        addAndMakeVisible (*button);

    copySceneButton.onClick = [this] { copySelectedScene(); };
    pasteSceneButton.onClick = [this] { pasteSceneToSelected(); };
    clearSceneButton.onClick = [this] { clearSelectedScene(); };
    addToSongButton.onClick = [this] { addSelectedSceneToSong(); };

    for (auto* button : { &copySceneButton, &pasteSceneButton, &clearSceneButton, &addToSongButton })
        addAndMakeVisible (*button);

    clearSceneButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (68, 44, 44));

    assignSceneButton.onClick = [this] { beginAssignSceneToSongStep(); };
    cancelSceneAssignButton.onClick = [this] { cancelAssignSceneToSongStep(); };
    addAndMakeVisible (assignSceneButton);
    addAndMakeVisible (cancelSceneAssignButton);
    assignSceneButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (44, 52, 68));
    cancelSceneAssignButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (68, 44, 44));

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
        if (songPage < 1)
        {
            ++songPage;
            refreshSongUi();
        }
    };

    for (auto* button : { &removeSongStepButton, &holdSongStepButton, &continueSongButton,
                           &clearSongButton, &prevSongPageButton, &nextSongPageButton })
        addAndMakeVisible (*button);

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
        trackLabels[i].setText ("T" + juce::String (i + 1), juce::dontSendNotification);
        trackLabels[i].setJustificationType (juce::Justification::centred);
        trackLabels[i].setFont (juce::FontOptions (15.0f, juce::Font::bold));
        addAndMakeVisible (trackLabels[i]);

        loadButtons[i].onClick = [this, i] { chooseLoopForTrack (i); };
        addAndMakeVisible (loadButtons[i]);

        clearButtons[i].setButtonText ("X");
        clearButtons[i].onClick = [this, i] { clearClipForTrack (i); };
        clearButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (68, 44, 44));
        addAndMakeVisible (clearButtons[i]);

        mixer.addInputSource (&tracks[i].transport, false);
    }

    for (int i = 0; i < numScenes; ++i)
    {
        sceneButtons[i].onClick = [this, i]
        {
            if (songMode && choosingSongScene)
                return; // Name header only; use the large P/A targets below.

            if (playing && ! songMode)
                triggerScene (i);
            else
                selectSceneForEditing (i);
        };
        addAndMakeVisible (sceneButtons[i]);

        previewButtons[i].setButtonText ("P");
        previewButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (157, 42, 52));
        previewButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        previewButtons[i].onClick = [this, i] { toggleScenePreview (i); };
        addAndMakeVisible (previewButtons[i]);

        chooseButtons[i].setButtonText ("A");
        chooseButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (100, 55, 165));
        chooseButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        chooseButtons[i].onClick = [this, i] { assignSceneToSelectedSongStep (i); };
        addAndMakeVisible (chooseButtons[i]);
    }

    setSize (1500, 940);
    setAudioChannels (0, 2);

    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    refreshSongUi();
    updateModeVisibility();
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

    if (workingFolder.isDirectory())
        workingFolder.deleteRecursively();
}

void MainComponent::initialiseDefaults()
{
    for (int i = 0; i < numScenes; ++i)
    {
        sceneNames[i] = "Scene " + juce::String (i + 1);
        sceneBars[i] = 4;
    }

    for (auto& step : songSteps)
        step = {};
}

void MainComponent::createFreshWorkingFolder()
{
    if (workingFolder.isDirectory())
        workingFolder.deleteRecursively();

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("SkizoDroidLiveCache");
    root.createDirectory();

    workingFolder = root.getNonexistentChildFile ("Project", "", false);
    workingFolder.createDirectory();
    audioFolder = workingFolder.getChildFile ("Audio");
    audioFolder.createDirectory();
}


void MainComponent::initialiseAppSettings()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "SkizoDroidLive";
    options.filenameSuffix = "settings";
    options.folderName = "TrickyLabs";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    if (auto* settings = appProperties.getUserSettings())
    {
        const auto stored = settings->getValue ("lastUsedDirectory");
        if (stored.isNotEmpty())
        {
            const juce::File candidate (stored);
            if (candidate.isDirectory())
                lastUsedDirectory = candidate;
        }
    }

    if (! lastUsedDirectory.isDirectory())
        lastUsedDirectory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
}

void MainComponent::rememberDirectory (const juce::File& fileOrFolder)
{
    auto folder = fileOrFolder.isDirectory() ? fileOrFolder : fileOrFolder.getParentDirectory();
    if (! folder.isDirectory())
        return;

    lastUsedDirectory = folder;

    if (auto* settings = appProperties.getUserSettings())
    {
        settings->setValue ("lastUsedDirectory", lastUsedDirectory.getFullPathName());
        settings->saveIfNeeded();
    }
}

juce::File MainComponent::getLastUsedDirectory() const
{
    if (lastUsedDirectory.isDirectory())
        return lastUsedDirectory;

    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
}

bool MainComponent::writeProjectXml()
{
    if (! workingFolder.isDirectory())
        return false;

    juce::XmlElement root ("SkizoDroidLiveProject");
    root.setAttribute ("version", "0.9");
    root.setAttribute ("bpm", bpmSlider.getValue());

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
    for (int i = 0; i < songStepCount; ++i)
    {
        auto* stepXml = songXml->createNewChildElement ("Step");
        stepXml->setAttribute ("index", i);
        stepXml->setAttribute ("scene", songSteps[i].sceneIndex);
        stepXml->setAttribute ("hold", songSteps[i].hold);
    }

    return workingFolder.getChildFile ("project.xml").replaceWithText (root.toString());
}

bool MainComponent::readProjectXml()
{
    const auto xmlFile = workingFolder.getChildFile ("project.xml");
    if (! xmlFile.existsAsFile())
        return false;

    juce::XmlDocument document (xmlFile);
    auto root = document.getDocumentElement();
    if (root == nullptr || ! root->hasTagName ("SkizoDroidLiveProject"))
        return false;

    for (auto& scene : scenes)
        for (auto& clip : scene)
            clip = {};

    initialiseDefaults();
    songStepCount = 0;

    loadingProjectUi = true;
    bpmSlider.setValue (root->getDoubleAttribute ("bpm", 120.0), juce::dontSendNotification);

    if (auto* scenesXml = root->getChildByName ("Scenes"))
    {
        forEachXmlChildElementWithTagName (*scenesXml, sceneXml, "Scene")
        {
            const int sceneIndex = sceneXml->getIntAttribute ("index", -1);
            if (sceneIndex < 0 || sceneIndex >= numScenes)
                continue;

            sceneNames[sceneIndex] = sceneXml->getStringAttribute ("name", "Scene " + juce::String (sceneIndex + 1));
            sceneBars[sceneIndex] = juce::jlimit (1, 16, sceneXml->getIntAttribute ("bars", 4));

            forEachXmlChildElementWithTagName (*sceneXml, clipXml, "Clip")
            {
                const int trackIndex = clipXml->getIntAttribute ("track", -1);
                if (trackIndex >= 0 && trackIndex < numTracks)
                    scenes[sceneIndex][trackIndex] = resolveStoredPath (clipXml->getStringAttribute ("path"));
            }
        }
    }

    if (auto* songXml = root->getChildByName ("Song"))
    {
        int legacySequentialIndex = 0;
        forEachXmlChildElementWithTagName (*songXml, stepXml, "Step")
        {
            // Old V0.8 projects have no index and are sequential.
            const int index = stepXml->getIntAttribute ("index", legacySequentialIndex);
            ++legacySequentialIndex;
            if (index < 0 || index >= maxSongSteps)
                continue;

            const int sceneIndex = stepXml->getIntAttribute ("scene", -1);
            if (sceneIndex >= 0 && sceneIndex < numScenes)
            {
                songSteps[index].sceneIndex = sceneIndex;
                songSteps[index].hold = stepXml->getBoolAttribute ("hold", false);
                songStepCount = juce::jmax (songStepCount, index + 1);
            }
        }
    }

    loadingProjectUi = false;
    selectedScene = 0;
    currentScene = 0;
    queuedScene = -1;
    selectedSongStep = songStepCount > 0 ? 0 : -1;
    currentSongStep = -1;
    songPage = 0;
    queuedSongStep = -1;
    releaseHoldAtBoundary = false;
    choosingSongScene = false;

    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    refreshSongUi();
    return true;
}

bool MainComponent::packWorkingFolderToProject (const juce::File& targetFile)
{
    if (! writeProjectXml())
        return false;

    juce::ZipFile::Builder builder;
    juce::Array<juce::File> files;
    workingFolder.findChildFiles (files, juce::File::findFiles, true);

    for (const auto& file : files)
    {
        const auto relativePath = file.getRelativePathFrom (workingFolder).replaceCharacter ('\\', '/');
        builder.addFile (file, 0, relativePath);
    }

    targetFile.getParentDirectory().createDirectory();
    auto tempFile = targetFile.getSiblingFile (targetFile.getFileName() + ".tmp");
    tempFile.deleteFile();

    bool ok = false;
    {
        juce::FileOutputStream out (tempFile);
        if (! out.openedOk())
            return false;

        out.setPosition (0);
        out.truncate();
        ok = builder.writeToStream (out, nullptr);
        out.flush();
    } // IMPORTANT: close file before replacing/moving it on Windows

    if (! ok || ! tempFile.existsAsFile() || tempFile.getSize() == 0)
    {
        tempFile.deleteFile();
        return false;
    }

    if (targetFile.existsAsFile() && ! targetFile.deleteFile())
    {
        tempFile.deleteFile();
        return false;
    }

    return tempFile.moveFileTo (targetFile) && targetFile.existsAsFile() && targetFile.getSize() > 0;
}

bool MainComponent::unpackProjectToWorkingFolder (const juce::File& sourceFile)
{
    if (! sourceFile.existsAsFile())
        return false;

    createFreshWorkingFolder();
    juce::ZipFile zip (sourceFile);
    const auto result = zip.uncompressTo (workingFolder, true);
    if (result.failed())
        return false;

    audioFolder = workingFolder.getChildFile ("Audio");
    audioFolder.createDirectory();
    return readProjectXml();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    mixer.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    const int numSamples = bufferToFill.numSamples;
    const int fadeLength = juce::jmax (1, static_cast<int> (currentSampleRate * 0.008));

    int phase = transitionFadeState.load (std::memory_order_acquire);
    if (phase == 2)
    {
        // The source hand-off is performed on the message thread.
        // Never let the audio thread render a transport being reconfigured.
        bufferToFill.clearActiveBufferRegion();
        if (startFadeIn.exchange (false))
        {
            transitionFadeState.store (3, std::memory_order_release);
            fadeSamplePosition = 0;
        }
        return;
    }

    if (! playing)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    mixer.getNextAudioBlock (bufferToFill);

    if (phase == 1 || phase == 3)
    {
        // Smooth gain ramps suppress discontinuities at STOP->PLAY transitions.
        // This is a fade-through-silence; it is NOT an overlapping crossfade.
        const int channels = bufferToFill.buffer->getNumChannels();
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float fraction = juce::jlimit (0.0f, 1.0f,
                static_cast<float> (fadeSamplePosition) / static_cast<float> (fadeLength));
            const float gain = phase == 1 ? (1.0f - fraction) : fraction;
            for (int ch = 0; ch < channels; ++ch)
                bufferToFill.buffer->getWritePointer (ch, bufferToFill.startSample)[sample] *= gain;

            ++fadeSamplePosition;
            if (fadeSamplePosition >= fadeLength)
            {
                if (phase == 1)
                {
                    // Silence the remainder of the block and arrange the
                    // actual transport change without blocking the audio thread.
                    for (int ch = 0; ch < channels; ++ch)
                        juce::FloatVectorOperations::clear (
                            bufferToFill.buffer->getWritePointer (ch, bufferToFill.startSample) + sample + 1,
                            numSamples - sample - 1);
                    transitionFadeState.store (2, std::memory_order_release);
                    if (! transitionCallbackScheduled)
                    {
                        transitionCallbackScheduled = true;
                        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
                        {
                            if (safeThis != nullptr)
                                safeThis->completeQueuedTransition();
                        });
                    }
                }
                else
                {
                    transitionFadeState.store (0, std::memory_order_release);
                    transitionCallbackScheduled = false;
                }
                break;
            }
        }
        return;
    }

    if (previewMode)
        return; // No Song Mode advancement while previewing.

    samplesSinceSceneStart += numSamples;
    const auto samplesPerScene = getSamplesPerScene (currentScene);
    if (samplesPerScene <= 0.0 || samplesSinceSceneStart < samplesPerScene)
        return;

    samplesSinceSceneStart = std::fmod (samplesSinceSceneStart, samplesPerScene);

    bool shouldChange = false;
    if (songMode && currentSongStep >= 0)
    {
        const bool held = songSteps[currentSongStep].hold;
        shouldChange = queuedSongStep >= 0 || ! held || releaseHoldAtBoundary;
    }
    else
    {
        shouldChange = queuedScene >= 0;
    }

    if (shouldChange)
    {
        fadeSamplePosition = 0;
        transitionFadeState.store (1, std::memory_order_release);
    }
}

void MainComponent::completeQueuedTransition()
{
    // The audio callback is outputting silence until this finishes.
    if (transitionFadeState.load (std::memory_order_acquire) != 2 || ! playing)
        return;

    if (songMode && currentSongStep >= 0)
    {
        releaseHoldAtBoundary = false;
        advanceSongAtBoundary();
    }
    else if (queuedScene >= 0)
    {
        const int next = queuedScene;
        queuedScene = -1;
        startSceneNow (next);
    }

    if (playing)
        startFadeIn.store (true, std::memory_order_release);
}

void MainComponent::releaseResources()
{
    mixer.releaseResources();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (14, 14, 16));
    g.setColour (juce::Colour::fromRGB (46, 46, 52));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (14.0f), 10.0f, 2.0f);

    if (logoDrawable != nullptr)
        logoDrawable->drawWithin (g, headerLogoBounds.toFloat(), juce::RectanglePlacement::centred, 1.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto header = area.removeFromTop (50);
    headerLogoBounds = header.removeFromLeft (64).reduced (0, 4);
    titleLabel.setBounds (header.removeFromLeft (250));
    selectedSceneLabel.setBounds (header);

    auto menuRow = area.removeFromTop (30);
    newProjectButton.setBounds (menuRow.removeFromLeft (66).reduced (2));
    openProjectButton.setBounds (menuRow.removeFromLeft (66).reduced (2));
    saveProjectButton.setBounds (menuRow.removeFromLeft (66).reduced (2));
    saveAsProjectButton.setBounds (menuRow.removeFromLeft (78).reduced (2));
    playButton.setBounds (menuRow.removeFromLeft (64).reduced (2));
    stopButton.setBounds (menuRow.removeFromLeft (64).reduced (2));
    bpmLabel.setBounds (menuRow.removeFromLeft (40));
    bpmSlider.setBounds (menuRow.removeFromLeft (185).reduced (2));
    patternModeButton.setBounds (menuRow.removeFromLeft (82).reduced (2));
    songModeButton.setBounds (menuRow.removeFromLeft (70).reduced (2));
    projectLabel.setBounds (menuRow.reduced (8, 0));

    statusLabel.setBounds (area.removeFromTop (22));

    if (! songMode)
    {
        auto sceneEditRow = area.removeFromTop (32);
        sceneNameLabel.setBounds (sceneEditRow.removeFromLeft (56));
        sceneNameEditor.setBounds (sceneEditRow.removeFromLeft (220).reduced (2));
        sceneBarsLabel.setBounds (sceneEditRow.removeFromLeft (60));
        sceneBarsBox.setBounds (sceneEditRow.removeFromLeft (92).reduced (2));
        copySceneButton.setBounds (sceneEditRow.removeFromLeft (72).reduced (2));
        pasteSceneButton.setBounds (sceneEditRow.removeFromLeft (72).reduced (2));
        clearSceneButton.setBounds (sceneEditRow.removeFromLeft (96).reduced (2));
        addToSongButton.setBounds (sceneEditRow.removeFromLeft (104).reduced (2));

        area.removeFromTop (3);
        auto trackArea = area.removeFromTop (108);
        const int trackColumns = 8;
        const int trackWidth = trackArea.getWidth() / trackColumns;
        const int trackHeight = trackArea.getHeight() / 2;

        for (int i = 0; i < numTracks; ++i)
        {
            const int col = i % 8;
            const int row = i / 8;
            auto cell = juce::Rectangle<int> (trackArea.getX() + col * trackWidth + 3,
                                              trackArea.getY() + row * trackHeight + 2,
                                              trackWidth - 6,
                                              trackHeight - 4);
            trackLabels[i].setBounds (cell.removeFromLeft (28));
            clearButtons[i].setBounds (cell.removeFromRight (26).reduced (1));
            loadButtons[i].setBounds (cell.reduced (1));
        }

        area.removeFromTop (3);
        auto grid = area.reduced (3);
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
    else
    {
        auto songControls = area.removeFromTop (34);

        if (choosingSongScene)
        {
            cancelSceneAssignButton.setBounds (songControls.removeFromLeft (82).reduced (2));
            songPageLabel.setBounds (songControls.removeFromLeft (260));

            area.removeFromTop (4);
            auto grid = area.reduced (3);
            const int cellW = grid.getWidth() / sceneColumns;
            const int cellH = grid.getHeight() / sceneRows;

            for (int i = 0; i < numScenes; ++i)
            {
                const int col = i % sceneColumns;
                const int row = i / sceneColumns;
                auto cell = juce::Rectangle<int> (grid.getX() + col * cellW + 3,
                                                   grid.getY() + row * cellH + 3,
                                                   cellW - 6, cellH - 6);
                sceneButtons[i].setBounds (cell.removeFromTop (juce::jmax (26, cell.getHeight() / 3)));
                const int leftWidth = cell.getWidth() / 2;
                previewButtons[i].setBounds (cell.removeFromLeft (leftWidth));
                chooseButtons[i].setBounds (cell);
            }
        }
        else
        {
            assignSceneButton.setBounds (songControls.removeFromLeft (108).reduced (2));
            removeSongStepButton.setBounds (songControls.removeFromLeft (82).reduced (2));
            holdSongStepButton.setBounds (songControls.removeFromLeft (96).reduced (2));
            continueSongButton.setBounds (songControls.removeFromLeft (94).reduced (2));
            clearSongButton.setBounds (songControls.removeFromLeft (96).reduced (2));
            prevSongPageButton.setBounds (songControls.removeFromLeft (34).reduced (2));
            songPageLabel.setBounds (songControls.removeFromLeft (90));
            nextSongPageButton.setBounds (songControls.removeFromLeft (34).reduced (2));

            area.removeFromTop (4);
            auto grid = area.reduced (3);
            const int cellW = grid.getWidth() / 8;
            const int cellH = grid.getHeight() / 8;

            for (int i = 0; i < visibleSongSteps; ++i)
            {
                const int col = i % 8;
                const int row = i / 8;
                songStepButtons[i].setBounds (grid.getX() + col * cellW + 3,
                                              grid.getY() + row * cellH + 3,
                                              cellW - 6,
                                              cellH - 6);
            }
        }
    }
}

void MainComponent::resetProjectData()
{
    stopAll();
    for (auto& scene : scenes)
        for (auto& clip : scene)
            clip = {};

    initialiseDefaults();
    selectedScene = 0;
    currentScene = 0;
    queuedScene = -1;
    songStepCount = 0;
    selectedSongStep = -1;
    currentSongStep = -1;
    queuedSongStep = -1;
    songPage = 0;
    queuedSongStep = -1;
    releaseHoldAtBoundary = false;
    choosingSongScene = false;
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    createFreshWorkingFolder();
    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    refreshSongUi();
}

void MainComponent::newProject()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Create SkizoDroid Live project",
        getLastUsedDirectory().getChildFile ("SkizoDroid Song.sklive"),
        "*.sklive");

    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        auto chosen = chooser.getResult();
        if (chosen == juce::File()) return;
        chosen = chosen.withFileExtension ("sklive");
        rememberDirectory (chosen);

        resetProjectData();
        projectFile = chosen;
        saveProject();
        updateProjectLabel();
        updateStatus();
    });
}

void MainComponent::openProject()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Open SkizoDroid Live project",
        getLastUsedDirectory(),
        "*.sklive");

    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile()) return;
        rememberDirectory (file);

        stopAll();
        if (! unpackProjectToWorkingFolder (file))
        {
            statusLabel.setText ("COULD NOT OPEN PROJECT", juce::dontSendNotification);
            return;
        }

        projectFile = file;
        updateProjectLabel();
        updateStatus();
    });
}

bool MainComponent::performProjectSave (const juce::File& targetFile)
{
    if (targetFile == juce::File())
        return false;

    const bool ok = packWorkingFolderToProject (targetFile);
    if (ok)
        rememberDirectory (targetFile);
    return ok;
}

void MainComponent::setSaveButtonBusy (bool busy, bool failed)
{
    saveInProgress = busy;
    saveProjectButton.setEnabled (! busy);

    if (busy)
    {
        saveProjectButton.setButtonText ("SAVING...");
        saveProjectButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (150, 35, 35));
    }
    else if (failed)
    {
        saveProjectButton.setButtonText ("SAVE FAILED");
        saveProjectButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (150, 35, 35));
    }
    else
    {
        saveProjectButton.setButtonText ("SAVE");
        saveProjectButton.removeColour (juce::TextButton::buttonColourId);
    }

    saveProjectButton.repaint();
}

void MainComponent::saveProjectWithFeedback()
{
    if (saveInProgress)
        return;

    if (projectFile == juce::File())
    {
        newProject();
        return;
    }

    setSaveButtonBusy (true);

    // Let JUCE repaint the red SAVING state before the ZIP write starts.
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safeThis == nullptr)
            return;

        const bool ok = safeThis->performProjectSave (safeThis->projectFile);
        safeThis->setSaveButtonBusy (false, ! ok);

        if (ok)
        {
            safeThis->updateProjectLabel();
            safeThis->updateStatus();
        }
        else
        {
            safeThis->statusLabel.setText ("PROJECT SAVE FAILED", juce::dontSendNotification);
        }
    });
}

void MainComponent::saveProject()
{
    if (projectFile == juce::File())
        return;

    if (! performProjectSave (projectFile))
        statusLabel.setText ("PROJECT SAVE FAILED", juce::dontSendNotification);
    else
    {
        updateProjectLabel();
        updateStatus();
    }
}

void MainComponent::saveProjectAs()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save SkizoDroid Live project as",
        projectFile != juce::File() ? projectFile : getLastUsedDirectory().getChildFile ("SkizoDroid Song.sklive"),
        "*.sklive");

    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        auto chosen = chooser.getResult();
        if (chosen == juce::File()) return;
        chosen = chosen.withFileExtension ("sklive");
        rememberDirectory (chosen);

        if (performProjectSave (chosen))
        {
            projectFile = chosen;
            updateProjectLabel();
            updateStatus();
        }
    });
}

juce::File MainComponent::importAudioIntoProject (const juce::File& sourceFile)
{
    if (! sourceFile.existsAsFile()) return {};
    audioFolder.createDirectory();

    auto target = audioFolder.getChildFile (sourceFile.getFileName());
    if (target.existsAsFile())
    {
        if (target.getSize() == sourceFile.getSize()) return target;
        target = target.getNonexistentSibling (false);
    }

    return sourceFile.copyFileTo (target) ? target : juce::File();
}

juce::String MainComponent::getStoredPathForFile (const juce::File& file) const
{
    return file.getRelativePathFrom (workingFolder).replaceCharacter ('\\', '/');
}

juce::File MainComponent::resolveStoredPath (const juce::String& storedPath) const
{
    return workingFolder.getChildFile (storedPath);
}

void MainComponent::selectSceneForEditing (int sceneIndex)
{
    if (sceneIndex < 0 || sceneIndex >= numScenes) return;
    selectedScene = sceneIndex;
    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    updateStatus();
}

void MainComponent::chooseLoopForTrack (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= numTracks) return;
    if (projectFile == juce::File())
    {
        statusLabel.setText ("CREATE OR OPEN A PROJECT FIRST", juce::dontSendNotification);
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose audio for " + sceneNames[selectedScene] + " / T" + juce::String (trackIndex + 1),
        getLastUsedDirectory(), "*.wav;*.aif;*.aiff");

    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (flags, [this, trackIndex] (const juce::FileChooser& chooser)
    {
        const auto sourceFile = chooser.getResult();
        if (! sourceFile.existsAsFile()) return;
        rememberDirectory (sourceFile);

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (sourceFile));
        if (reader == nullptr) return;

        const auto imported = importAudioIntoProject (sourceFile);
        if (! imported.existsAsFile()) return;

        scenes[selectedScene][trackIndex] = imported;
        refreshTrackButtons();
        refreshSceneButtons();
        saveProject();
    });
}

void MainComponent::clearClipForTrack (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= numTracks) return;
    scenes[selectedScene][trackIndex] = {};
    refreshTrackButtons();
    refreshSceneButtons();
    if (projectFile != juce::File()) saveProject();
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
    if (! hasCopiedSceneClipboard) return;
    scenes[selectedScene] = copiedScene;
    sceneBars[selectedScene] = copiedSceneBars;
    refreshTrackButtons();
    refreshSceneButtons();
    refreshSceneEditor();
    if (projectFile != juce::File()) saveProject();
}

void MainComponent::clearSelectedScene()
{
    for (auto& clip : scenes[selectedScene]) clip = {};
    refreshTrackButtons();
    refreshSceneButtons();
    if (projectFile != juce::File()) saveProject();
}

void MainComponent::setPerformanceMode (bool useSongMode)
{
    if (playing) stopAll();
    songMode = useSongMode;
    choosingSongScene = false;
    currentSongStep = -1;
    releaseHoldAtBoundary = false;
    updateModeVisibility();
    refreshSongUi();
    resized();
    repaint();
    updateStatus();
}

void MainComponent::updateModeVisibility()
{
    const bool pattern = ! songMode;
    const bool songGrid = songMode && ! choosingSongScene;
    const bool sceneChooser = songMode && choosingSongScene;

    sceneNameLabel.setVisible (pattern);
    sceneNameEditor.setVisible (pattern);
    sceneBarsLabel.setVisible (pattern);
    sceneBarsBox.setVisible (pattern);
    copySceneButton.setVisible (pattern);
    pasteSceneButton.setVisible (pattern);
    clearSceneButton.setVisible (pattern);
    addToSongButton.setVisible (pattern);

    for (int i = 0; i < numTracks; ++i)
    {
        trackLabels[i].setVisible (pattern);
        loadButtons[i].setVisible (pattern);
        clearButtons[i].setVisible (pattern);
    }

    for (auto& b : sceneButtons)
    {
        b.setVisible (pattern || sceneChooser);
        b.setEnabled (true); // Chooser clicks are ignored in the onClick handler.
    }

    for (auto& b : previewButtons) b.setVisible (sceneChooser);
    for (auto& b : chooseButtons) b.setVisible (sceneChooser);

    assignSceneButton.setVisible (songGrid);
    cancelSceneAssignButton.setVisible (sceneChooser);
    removeSongStepButton.setVisible (songGrid);
    holdSongStepButton.setVisible (songGrid);
    continueSongButton.setVisible (songGrid);
    clearSongButton.setVisible (songGrid);
    prevSongPageButton.setVisible (songGrid);
    songPageLabel.setVisible (songGrid || sceneChooser);
    nextSongPageButton.setVisible (songGrid);

    for (auto& b : songStepButtons)
        b.setVisible (songGrid);
}

void MainComponent::addSelectedSceneToSong()
{
    // Keep this Pattern shortcut, but use a free slot rather than forcing
    // the user to append at the end of their song.
    int index = 0;
    while (index < maxSongSteps && songSteps[index].sceneIndex >= 0)
        ++index;
    if (index >= maxSongSteps) return;

    songSteps[index] = { selectedScene, false };
    songStepCount = juce::jmax (songStepCount, index + 1);
    selectedSongStep = index;
    songPage = index / visibleSongSteps;
    if (projectFile != juce::File()) saveProject();
    refreshSongUi();
}

void MainComponent::beginAssignSceneToSongStep()
{
    if (! songMode || playing)
        return;

    if (selectedSongStep < 0)
        selectedSongStep = 0;

    if (selectedSongStep >= maxSongSteps)
        return;

    choosingSongScene = true;
    songPageLabel.setText ("CHOOSE SCENE FOR STEP " + juce::String (selectedSongStep + 1),
                           juce::dontSendNotification);
    updateModeVisibility();
    refreshSceneButtons();
    resized();
    repaint();
    updateStatus();
}

void MainComponent::cancelAssignSceneToSongStep()
{
    if (! choosingSongScene)
        return;

    stopScenePreview();
    choosingSongScene = false;
    updateModeVisibility();
    refreshSongUi();
    resized();
    repaint();
    updateStatus();
}

void MainComponent::assignSceneToSelectedSongStep (int sceneIndex)
{
    if (! songMode || ! choosingSongScene || sceneIndex < 0 || sceneIndex >= numScenes
        || selectedSongStep < 0 || selectedSongStep >= maxSongSteps)
        return;

    stopScenePreview();
    // Preserve HOLD when replacing a previously assigned scene.
    if (songSteps[selectedSongStep].sceneIndex < 0)
        songSteps[selectedSongStep].hold = false;
    songSteps[selectedSongStep].sceneIndex = sceneIndex;
    songStepCount = juce::jmax (songStepCount, selectedSongStep + 1);

    selectedScene = sceneIndex;
    choosingSongScene = false;

    if (projectFile != juce::File()) saveProject();

    updateModeVisibility();
    refreshSongUi();
    refreshSceneButtons();
    resized();
    repaint();
    updateStatus();
}

void MainComponent::toggleScenePreview (int sceneIndex)
{
    if (! choosingSongScene || sceneIndex < 0 || sceneIndex >= numScenes)
        return;

    if (previewMode && previewSceneIndex == sceneIndex)
    {
        stopScenePreview();
        return;
    }

    stopScenePreview();
    previewMode = true;
    previewSceneIndex = sceneIndex;
    // Preview loops this scene, but never advances Song Mode.
    startSceneNow (sceneIndex);
    refreshSceneButtons();
    updateStatus();
}

void MainComponent::stopScenePreview()
{
    if (! previewMode)
        return;

    stopAll();
    previewMode = false;
    previewSceneIndex = -1;
    refreshSceneButtons();
}

void MainComponent::shrinkSongTail()
{
    while (songStepCount > 0 && songSteps[songStepCount - 1].sceneIndex < 0)
        --songStepCount;
}

void MainComponent::removeSelectedSongStep()
{
    if (selectedSongStep < 0 || selectedSongStep >= maxSongSteps) return;
    songSteps[selectedSongStep] = {}; // Never shift later steps.
    shrinkSongTail();
    if (projectFile != juce::File()) saveProject();
    refreshSongUi();
}

void MainComponent::toggleSelectedSongStepHold()
{
    if (selectedSongStep < 0 || selectedSongStep >= songStepCount) return;
    songSteps[selectedSongStep].hold = ! songSteps[selectedSongStep].hold;
    if (projectFile != juce::File()) saveProject();
    refreshSongUi();
}

void MainComponent::clearSong()
{
    if (playing) stopAll();
    for (auto& step : songSteps) step = {};
    songStepCount = 0;
    selectedSongStep = -1;
    currentSongStep = -1;
    queuedSongStep = -1;
    songPage = 0;
    if (projectFile != juce::File()) saveProject();
    refreshSongUi();
}

void MainComponent::continueSong()
{
    if (playing && songMode && currentSongStep >= 0 && currentSongStep < songStepCount && songSteps[currentSongStep].hold)
    {
        releaseHoldAtBoundary = true;
        updateStatus();
    }
}

void MainComponent::selectSongStep (int absoluteIndex)
{
    if (absoluteIndex < 0 || absoluteIndex >= maxSongSteps)
        return;

    selectedSongStep = absoluteIndex;
    const int sceneIndex = songSteps[absoluteIndex].sceneIndex;
    if (sceneIndex >= 0 && sceneIndex < numScenes)
    {
        selectedScene = sceneIndex;
        if (songMode && playing && ! previewMode)
        {
            // Backward or forward live jump: queued until the scene boundary.
            queuedSongStep = absoluteIndex;
        }
    }

    refreshSongUi();
    updateStatus();
}

void MainComponent::refreshSongUi()
{
    patternModeButton.setColour (juce::TextButton::buttonColourId,
                                 ! songMode ? juce::Colour::fromRGB (70, 70, 88) : juce::Colour::fromRGB (38, 38, 44));
    songModeButton.setColour (juce::TextButton::buttonColourId,
                              songMode ? juce::Colour::fromRGB (70, 70, 88) : juce::Colour::fromRGB (38, 38, 44));

    songPage = juce::jlimit (0, 1, songPage);

    if (! choosingSongScene)
    {
        songPageLabel.setText ("STEPS " + juce::String (songPage * 64 + 1) + "-" + juce::String (songPage * 64 + 64),
                               juce::dontSendNotification);
    }

    prevSongPageButton.setEnabled (songPage > 0);
    nextSongPageButton.setEnabled (songPage < 1);

    for (int i = 0; i < visibleSongSteps; ++i)
    {
        const int absoluteIndex = songPage * visibleSongSteps + i;
        auto& button = songStepButtons[i];

        if (absoluteIndex < songStepCount && songSteps[absoluteIndex].sceneIndex >= 0)
        {
            const auto& step = songSteps[absoluteIndex];
            juce::String text = juce::String (absoluteIndex + 1) + "\n";
            if (step.sceneIndex >= 0 && step.sceneIndex < numScenes)
                text += sceneNames[step.sceneIndex];
            if (step.hold)
                text += "\nHOLD";

            button.setButtonText (text);
            button.setEnabled (true);
            button.setColour (juce::TextButton::buttonColourId,
                              absoluteIndex == selectedSongStep ? juce::Colour::fromRGB (64, 64, 82)
                                                                : juce::Colour::fromRGB (32, 32, 38));

            if (absoluteIndex == currentSongStep && playing && songMode)
                button.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (90, 70, 40));
        }
        else
        {
            button.setButtonText (juce::String (absoluteIndex + 1) + "\n+ SCENE");
            button.setEnabled (true);
            button.setColour (juce::TextButton::buttonColourId,
                              absoluteIndex == selectedSongStep ? juce::Colour::fromRGB (64, 64, 82)
                                                                : juce::Colour::fromRGB (28, 38, 34));
        }

        if (absoluteIndex == queuedSongStep && playing && songMode)
            button.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (60, 85, 130));
    }

    const bool hasFilledSelection = selectedSongStep >= 0 && selectedSongStep < songStepCount
                                    && songSteps[selectedSongStep].sceneIndex >= 0;
    const bool hasAssignableSelection = selectedSongStep >= 0
                                     && selectedSongStep < maxSongSteps;

    assignSceneButton.setEnabled (songMode && ! playing && hasAssignableSelection);
    removeSongStepButton.setEnabled (hasFilledSelection && ! playing);
    holdSongStepButton.setEnabled (hasFilledSelection && ! playing);
    holdSongStepButton.setButtonText (hasFilledSelection && songSteps[selectedSongStep].hold ? "HOLD: ON" : "HOLD: OFF");
    continueSongButton.setEnabled (songMode && playing && currentSongStep >= 0 && currentSongStep < songStepCount
                                   && songSteps[currentSongStep].hold);
}

void MainComponent::startSongStepNow (int songIndex)
{
    if (songIndex < 0 || songIndex >= songStepCount) { stopAll(); return; }
    if (songSteps[songIndex].sceneIndex < 0) { stopAll(); return; }
    currentSongStep = songIndex;
    selectedSongStep = songIndex;
    queuedSongStep = -1;
    releaseHoldAtBoundary = false;
    songPage = songIndex / visibleSongSteps;
    startSceneNow (songSteps[songIndex].sceneIndex);
    currentSongStep = songIndex;
    refreshSongUi();
    updateStatus();
}

void MainComponent::advanceSongAtBoundary()
{
    if (! songMode || currentSongStep < 0) return;

    int nextStep = -1;
    if (queuedSongStep >= 0 && queuedSongStep < maxSongSteps
        && songSteps[queuedSongStep].sceneIndex >= 0)
        nextStep = queuedSongStep;
    else
    {
        for (int i = currentSongStep + 1; i < songStepCount; ++i)
        {
            if (songSteps[i].sceneIndex >= 0)
            {
                nextStep = i;
                break;
            }
        }
    }

    queuedSongStep = -1;
    if (nextStep < 0) { stopAll(); return; }
    startSongStepNow (nextStep);
}

void MainComponent::refreshTrackButtons()
{
    for (int i = 0; i < numTracks; ++i)
    {
        const auto& file = scenes[selectedScene][i];
        loadButtons[i].setButtonText (file.existsAsFile() ? shortName (file) : "LOAD");
        clearButtons[i].setEnabled (file.existsAsFile());
    }
}

void MainComponent::refreshSceneButtons()
{
    for (int i = 0; i < numScenes; ++i)
    {
        int clipCount = 0;
        for (const auto& file : scenes[i]) if (file.existsAsFile()) ++clipCount;

        juce::String text = sceneNames[i] + "\n" + juce::String (sceneBars[i]) + " bar";
        if (clipCount > 0) text += " / " + juce::String (clipCount);
        sceneButtons[i].setButtonText (text);
        sceneButtons[i].setColour (juce::TextButton::buttonColourId,
                                   i == selectedScene ? juce::Colour::fromRGB (65, 65, 78)
                                                      : juce::Colour::fromRGB (34, 34, 38));
        if (i == currentScene && playing)
            sceneButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (90, 70, 40));
        if (i == queuedScene)
            sceneButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (55, 75, 95));
        previewButtons[i].setButtonText (previewMode && i == previewSceneIndex ? "STOP" : "P");
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
    if (sceneIndex < 0 || sceneIndex >= numScenes) return;
    selectedScene = sceneIndex;
    refreshTrackButtons();
    refreshSceneEditor();
    if (! playing) { startSceneNow (sceneIndex); return; }
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
        if (! file.existsAsFile()) continue;

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr) continue;

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
    refreshSongUi();
    updateStatus();
}

void MainComponent::startPlayback()
{
    if (choosingSongScene)
        return; // Use the red P buttons for audition in scene assignment.

    if (songMode)
    {
        int startingStep = selectedSongStep;
        if (startingStep < 0 || startingStep >= maxSongSteps
            || songSteps[startingStep].sceneIndex < 0)
        {
            startingStep = -1;
            for (int i = 0; i < songStepCount; ++i)
                if (songSteps[i].sceneIndex >= 0) { startingStep = i; break; }
        }

        if (startingStep >= 0)
            startSongStepNow (startingStep);
        else
            statusLabel.setText ("ASSIGN A SCENE TO A SONG STEP FIRST", juce::dontSendNotification);
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
    queuedSongStep = -1;
    previewMode = false;
    previewSceneIndex = -1;
    transitionFadeState.store (0);
    startFadeIn.store (false);
    currentSongStep = -1;
    releaseHoldAtBoundary = false;
    samplesSinceSceneStart = 0.0;
    refreshSceneButtons();
    refreshSongUi();
    updateStatus();
}

void MainComponent::updateProjectLabel()
{
    projectLabel.setText (projectFile == juce::File() ? "NO PROJECT" : projectFile.getFileName(), juce::dontSendNotification);
}

void MainComponent::updateStatus()
{
    if (songMode && choosingSongScene)
    {
        statusLabel.setText ("P = PREVIEW    |    A = ASSIGN    |    SONG STEP "
                             + juce::String (selectedSongStep + 1)
                             + (previewMode ? "    |    PLAYING: " + sceneNames[previewSceneIndex] : ""),
                             juce::dontSendNotification);
        return;
    }

    juce::String text;

    if (playing)
    {
        text = "PLAYING  |  " + sceneNames[currentScene]
             + "  |  " + juce::String (sceneBars[currentScene]) + " bars";

        if (songMode && currentSongStep >= 0)
            text += "  |  SONG STEP " + juce::String (currentSongStep + 1);
    }
    else
    {
        text = songMode ? "SONG MODE" : "PATTERN MODE";
    }

    if (queuedScene >= 0 && ! songMode)
        text += "  |  NEXT: " + sceneNames[queuedScene];
    if (queuedSongStep >= 0 && songMode)
        text += "  |  QUEUED STEP " + juce::String (queuedSongStep + 1);
    if (previewMode)
        text += "  |  PREVIEW " + sceneNames[previewSceneIndex];

    if (songMode && currentSongStep >= 0 && currentSongStep < songStepCount
        && songSteps[currentSongStep].hold)
    {
        text += releaseHoldAtBoundary ? "  |  HOLD RELEASE ARMED" : "  |  HOLD";
    }

    if (hasCopiedSceneClipboard && ! songMode)
        text += "  |  CLIPBOARD READY";

    statusLabel.setText (text, juce::dontSendNotification);

    selectedSceneLabel.setText (
        songMode
            ? (selectedSongStep >= 0 ? "SONG STEP: " + juce::String (selectedSongStep + 1) : "SONG")
            : ("EDIT: " + sceneNames[selectedScene] + "   |   Scene " + juce::String (selectedScene + 1)),
        juce::dontSendNotification);
}

double MainComponent::getSamplesPerBar() const
{
    return (60.0 / bpmSlider.getValue()) * 4.0 * currentSampleRate;
}

double MainComponent::getSamplesPerScene (int sceneIndex) const
{
    return getSamplesPerBar() * static_cast<double> (sceneIndex >= 0 && sceneIndex < numScenes ? sceneBars[sceneIndex] : 4);
}
