#include "MainComponent.h"
#include <cmath>

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();

    titleLabel.setText ("SKIZODROID LIVE  V0.2", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    selectedSceneLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (selectedSceneLabel);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    addAndMakeVisible (bpmSlider);

    playButton.onClick = [this] { startPlayback(); };
    stopButton.onClick = [this] { stopAll(); };
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    for (int i = 0; i < numTracks; ++i)
    {
        loadButtons[i].onClick = [this, i] { chooseLoopForTrack (i); };
        addAndMakeVisible (loadButtons[i]);
        mixer.addInputSource (&tracks[i].transport, false);
    }

    for (int i = 0; i < numScenes; ++i)
    {
        sceneButtons[i].setButtonText (juce::String (i + 1));

        sceneButtons[i].onClick = [this, i]
        {
            if (juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown())
                selectSceneForEditing (i);
            else
                triggerScene (i);
        };

        sceneButtons[i].onStateChange = [this, i]
        {
            if (sceneButtons[i].isDown())
                selectSceneForEditing (i);
        };

        addAndMakeVisible (sceneButtons[i]);
    }

    setSize (1380, 900);
    setAudioChannels (0, 2);

    refreshTrackButtons();
    refreshSceneButtons();
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

    samplesSinceBarStart += bufferToFill.numSamples;
    const auto samplesPerBar = getSamplesPerBar();

    if (samplesSinceBarStart >= samplesPerBar)
    {
        samplesSinceBarStart = std::fmod (samplesSinceBarStart, samplesPerBar);

        if (queuedScene >= 0)
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
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (24);

    titleLabel.setBounds (area.removeFromTop (48));
    statusLabel.setBounds (area.removeFromTop (30));

    auto transport = area.removeFromTop (48);
    playButton.setBounds (transport.removeFromLeft (110).reduced (4));
    stopButton.setBounds (transport.removeFromLeft (110).reduced (4));
    bpmLabel.setBounds (transport.removeFromLeft (60));
    bpmSlider.setBounds (transport.removeFromLeft (240).reduced (4));
    selectedSceneLabel.setBounds (transport.reduced (8, 0));

    area.removeFromTop (12);

    auto trackArea = area.removeFromTop (180);
    const int trackColumns = 8;
    const int trackRows = 2;
    const int trackWidth = trackArea.getWidth() / trackColumns;
    const int trackHeight = trackArea.getHeight() / trackRows;

    for (int i = 0; i < numTracks; ++i)
    {
        const int col = i % trackColumns;
        const int row = i / trackColumns;
        loadButtons[i].setBounds (trackArea.getX() + col * trackWidth + 4,
                                  trackArea.getY() + row * trackHeight + 4,
                                  trackWidth - 8,
                                  trackHeight - 8);
    }

    area.removeFromTop (12);

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

void MainComponent::selectSceneForEditing (int sceneIndex)
{
    if (sceneIndex < 0 || sceneIndex >= numScenes)
        return;

    selectedScene = sceneIndex;
    refreshTrackButtons();
    refreshSceneButtons();
    updateStatus();
}

void MainComponent::chooseLoopForTrack (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= numTracks)
        return;

    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose audio for Scene " + juce::String (selectedScene + 1)
            + ", Track " + juce::String (trackIndex + 1),
        juce::File(),
        "*.wav;*.aif;*.aiff");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (
        flags,
        [this, trackIndex] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (! file.existsAsFile())
                return;

            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

            if (reader == nullptr)
            {
                statusLabel.setText ("Could not load audio file", juce::dontSendNotification);
                return;
            }

            scenes[selectedScene][trackIndex] = file;
            refreshTrackButtons();
            refreshSceneButtons();
            updateStatus();
        });
}

void MainComponent::refreshTrackButtons()
{
    for (int i = 0; i < numTracks; ++i)
    {
        const auto& file = scenes[selectedScene][i];

        if (file.existsAsFile())
            loadButtons[i].setButtonText ("T" + juce::String (i + 1) + "  " + file.getFileNameWithoutExtension());
        else
            loadButtons[i].setButtonText ("T" + juce::String (i + 1) + "  LOAD WAV");
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

        juce::String text = juce::String (i + 1);
        if (clipCount > 0)
            text += "\n" + juce::String (clipCount) + " clips";

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

void MainComponent::triggerScene (int sceneIndex)
{
    selectSceneForEditing (sceneIndex);

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
    samplesSinceBarStart = 0.0;

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
    updateStatus();
}

void MainComponent::startPlayback()
{
    startSceneNow (selectedScene);
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
    samplesSinceBarStart = 0.0;

    refreshSceneButtons();
    updateStatus();
}

void MainComponent::updateStatus()
{
    juce::String text = playing
        ? "PLAYING  |  SCENE " + juce::String (currentScene + 1)
        : "STOPPED";

    if (queuedScene >= 0)
        text += "  |  NEXT: SCENE " + juce::String (queuedScene + 1);

    statusLabel.setText (text, juce::dontSendNotification);

    selectedSceneLabel.setText (
        "EDIT SCENE: " + juce::String (selectedScene + 1)
            + "   |   click scene = launch/select   |   load WAVs into T1-T16",
        juce::dontSendNotification);
}

double MainComponent::getSamplesPerBar() const
{
    const double bpm = bpmSlider.getValue();
    const double secondsPerBeat = 60.0 / bpm;
    const double secondsPerBar = secondsPerBeat * 4.0;
    return secondsPerBar * currentSampleRate;
}
