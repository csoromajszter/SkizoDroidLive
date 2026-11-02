#include "MainComponent.h"

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();

    titleLabel.setText ("SKIZODROID LIVE", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    statusLabel.setText ("STOPPED  |  Scene: -  |  Queue: -", juce::dontSendNotification);
    addAndMakeVisible (statusLabel);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    addAndMakeVisible (bpmLabel);

    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 24);
    addAndMakeVisible (bpmSlider);

    playButton.onClick = [this]
    {
        if (currentScene < 0)
            currentScene = 0;

        startSceneNow (currentScene);
    };

    stopButton.onClick = [this] { stopAll(); };
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    for (int i = 0; i < numTracks; ++i)
    {
        loadButtons[(size_t) i].onClick = [this, i] { chooseLoopForTrack (i); };
        addAndMakeVisible (loadButtons[(size_t) i]);
    }

    for (int i = 0; i < numScenes; ++i)
    {
        sceneButtons[(size_t) i].onClick = [this, i] { triggerScene (i); };
        addAndMakeVisible (sceneButtons[(size_t) i]);
    }

    setSize (1180, 760);
    setAudioChannels (0, 2);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    mixer.prepareToPlay (samplesPerBlockExpected, sampleRate);

    for (auto& track : tracks)
        track.transport.prepareToPlay (samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    mixer.getNextAudioBlock (bufferToFill);

    if (playing)
        playheadSeconds += bufferToFill.numSamples / currentSampleRate;
}

void MainComponent::releaseResources()
{
    mixer.releaseResources();
    for (auto& track : tracks)
        track.transport.releaseResources();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (14, 14, 16));

    auto area = getLocalBounds().reduced (20);
    auto header = area.removeFromTop (96);
    juce::ignoreUnused (header);

    g.setColour (juce::Colour::fromRGB (42, 42, 46));
    g.drawRoundedRectangle (area.toFloat(), 10.0f, 1.5f);

    g.setColour (juce::Colour::fromRGB (180, 180, 186));
    g.setFont (15.0f);
    g.drawText ("V0.1  •  Audio-loop prototype  •  4 tracks / 4 scenes",
                26, getHeight() - 38, getWidth() - 52, 22,
                juce::Justification::centredRight);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (24);

    auto top = area.removeFromTop (54);
    titleLabel.setBounds (top.removeFromLeft (360));
    stopButton.setBounds (top.removeFromRight (100).reduced (4));
    playButton.setBounds (top.removeFromRight (100).reduced (4));
    bpmSlider.setBounds (top.removeFromRight (170).reduced (4));
    bpmLabel.setBounds (top.removeFromRight (42));

    statusLabel.setBounds (area.removeFromTop (36));
    area.removeFromTop (16);

    auto loads = area.removeFromTop (72);
    for (auto& button : loadButtons)
        button.setBounds (loads.removeFromLeft (loads.getWidth() / (int) (&loadButtons.back() - &button + 1)).reduced (6));

    area.removeFromTop (28);

    auto grid = area.removeFromTop (300);
    const int gap = 12;
    const int w = (grid.getWidth() - gap * (numScenes - 1)) / numScenes;
    for (int i = 0; i < numScenes; ++i)
    {
        sceneButtons[(size_t) i].setBounds (grid.removeFromLeft (w));
        grid.removeFromLeft (gap);
    }
}

void MainComponent::timerCallback()
{
    if (playing && queuedScene >= 0)
    {
        const double bpm = bpmSlider.getValue();
        const double secondsPerBar = (60.0 / bpm) * 4.0;
        const double pos = std::fmod (playheadSeconds, secondsPerBar);

        // Switch close to the next bar boundary. This is a prototype clock;
        // later this will be moved to an audio-thread-safe transport clock.
        if (pos < 0.05 || secondsPerBar - pos < 0.05)
        {
            startSceneNow (queuedScene);
            queuedScene = -1;
        }
    }

    updateStatus();
}

void MainComponent::chooseLoopForTrack (int trackIndex)
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Choose WAV/AIFF loop for track",
        juce::File{},
        "*.wav;*.aif;*.aiff");

    chooser->launchAsync (juce::FileBrowserComponent::openMode |
                          juce::FileBrowserComponent::canSelectFiles,
                          [this, trackIndex, chooser] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (! file.existsAsFile())
            return;

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr)
            return;

        auto source = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
        auto& track = tracks[(size_t) trackIndex];

        mixer.removeInputSource (&track.transport);
        track.transport.stop();
        track.transport.setSource (nullptr);

        track.readerSource = std::move (source);
        track.file = file;
        track.transport.setSource (track.readerSource.get(), 0, nullptr,
                                   track.readerSource->getAudioFormatReader()->sampleRate);
        track.transport.setGain (track.gain);
        mixer.addInputSource (&track.transport, false);

        loadButtons[(size_t) trackIndex].setButtonText (file.getFileNameWithoutExtension());
    });
}

void MainComponent::triggerScene (int sceneIndex)
{
    if (! playing)
    {
        currentScene = sceneIndex;
        startSceneNow (sceneIndex);
        return;
    }

    queuedScene = sceneIndex;
}

void MainComponent::startSceneNow (int sceneIndex)
{
    currentScene = sceneIndex;
    playing = true;
    playheadSeconds = 0.0;

    // V0.1 behaviour: all loaded tracks restart together.
    // Next milestone: every scene stores a different clip per track.
    for (auto& track : tracks)
    {
        if (track.readerSource != nullptr)
        {
            track.transport.setPosition (0.0);
            track.transport.start();
        }
    }
}

void MainComponent::stopAll()
{
    for (auto& track : tracks)
        track.transport.stop();

    playing = false;
    queuedScene = -1;
    playheadSeconds = 0.0;
    updateStatus();
}

void MainComponent::updateStatus()
{
    auto sceneName = [] (int index)
    {
        if (index < 0) return juce::String ("-");
        return juce::String::charToString ((juce::juce_wchar) ('A' + index));
    };

    statusLabel.setText (
        juce::String (playing ? "PLAYING" : "STOPPED") +
        "  |  Scene: " + sceneName (currentScene) +
        "  |  Queue: " + sceneName (queuedScene),
        juce::dontSendNotification);
}
