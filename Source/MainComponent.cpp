#include "MainComponent.h"

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();

    titleLabel.setText ("SKIZODROID LIVE", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    statusLabel.setText ("READY", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0);
    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (
        juce::Slider::TextBoxRight,
        false,
        70,
        24);

    addAndMakeVisible (bpmSlider);

    playButton.onClick = [this]
    {
        startPlayback();
    };

    stopButton.onClick = [this]
    {
        stopAll();
    };

    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);

    const std::array<juce::String, numTracks> trackNames =
    {
        "DRUM",
        "BASS",
        "SYNTH",
        "NOISE"
    };

    for (int i = 0; i < numTracks; ++i)
    {
        loadButtons[i].setButtonText ("LOAD " + trackNames[i]);

        loadButtons[i].onClick = [this, i]
        {
            chooseLoopForTrack (i);
        };

        addAndMakeVisible (loadButtons[i]);

        mixer.addInputSource (&tracks[i].transport, false);
    }

    for (int i = 0; i < numScenes; ++i)
    {
        sceneButtons[i].setButtonText (
            "SCENE " + juce::String (i + 1));

        sceneButtons[i].onClick = [this, i]
        {
            triggerScene (i);
        };

        addAndMakeVisible (sceneButtons[i]);
    }

    setSize (1100, 700);

    setAudioChannels (0, 2);

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

void MainComponent::prepareToPlay (
    int samplesPerBlockExpected,
    double sampleRate)
{
    currentSampleRate = sampleRate;

    mixer.prepareToPlay (
        samplesPerBlockExpected,
        sampleRate);
}

void MainComponent::getNextAudioBlock (
    const juce::AudioSourceChannelInfo& bufferToFill)
{
    mixer.getNextAudioBlock (bufferToFill);

    if (! playing)
        return;

    samplesSinceBarStart += bufferToFill.numSamples;

    const auto samplesPerBar = getSamplesPerBar();

    if (samplesSinceBarStart >= samplesPerBar)
    {
        samplesSinceBarStart =
            std::fmod (samplesSinceBarStart, samplesPerBar);

        if (queuedScene >= 0)
        {
            const int sceneToLaunch = queuedScene;
            queuedScene = -1;

            juce::MessageManager::callAsync (
                [safeThis =
                     juce::Component::SafePointer<MainComponent> (this),
                 sceneToLaunch]
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
    g.fillAll (juce::Colour::fromRGB (16, 16, 18));

    g.setColour (juce::Colour::fromRGB (45, 45, 50));

    g.drawRoundedRectangle (
        getLocalBounds()
            .toFloat()
            .reduced (20.0f),
        10.0f,
        2.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (30);

    titleLabel.setBounds (
        area.removeFromTop (55));

    statusLabel.setBounds (
        area.removeFromTop (35));

    area.removeFromTop (10);

    auto transportArea =
        area.removeFromTop (55);

    playButton.setBounds (
        transportArea.removeFromLeft (120).reduced (5));

    stopButton.setBounds (
        transportArea.removeFromLeft (120).reduced (5));

    bpmLabel.setBounds (
        transportArea.removeFromLeft (70));

    bpmSlider.setBounds (
        transportArea.removeFromLeft (250).reduced (5));

    area.removeFromTop (20);

    auto loadArea =
        area.removeFromTop (80);

    const int loadWidth =
        loadArea.getWidth() / numTracks;

    for (int i = 0; i < numTracks; ++i)
    {
        loadButtons[i].setBounds (
            loadArea
                .removeFromLeft (loadWidth)
                .reduced (8));
    }

    area.removeFromTop (30);

    auto sceneArea =
        area.removeFromTop (180);

    const int sceneWidth =
        sceneArea.getWidth() / numScenes;

    for (int i = 0; i < numScenes; ++i)
    {
        sceneButtons[i].setBounds (
            sceneArea
                .removeFromLeft (sceneWidth)
                .reduced (8));
    }
}

void MainComponent::chooseLoopForTrack (int trackIndex)
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose an audio loop",
        juce::File(),
        "*.wav;*.aif;*.aiff");

    const auto flags =
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (
        flags,
        [this, trackIndex]
        (const juce::FileChooser& chooser)
        {
            const auto file =
                chooser.getResult();

            if (! file.existsAsFile())
                return;

            std::unique_ptr<juce::AudioFormatReader> reader (
                formatManager.createReaderFor (file));

            if (reader == nullptr)
            {
                statusLabel.setText (
                    "Could not load audio file",
                    juce::dontSendNotification);

                return;
            }

            auto& track = tracks[trackIndex];

            track.transport.stop();
            track.transport.setSource (nullptr);
            track.readerSource.reset();

            const double sourceSampleRate =
                reader->sampleRate;

            auto newSource =
                std::make_unique<
                    juce::AudioFormatReaderSource> (
                        reader.release(),
                        true);

            newSource->setLooping (true);

            track.transport.setSource (
                newSource.get(),
                0,
                nullptr,
                sourceSampleRate);

            track.readerSource =
                std::move (newSource);

            track.loadedFile = file;

            loadButtons[trackIndex].setButtonText (
                file.getFileNameWithoutExtension());

            updateStatus();
        });
}

void MainComponent::triggerScene (int sceneIndex)
{
    if (! playing)
    {
        startSceneNow (sceneIndex);
        return;
    }

    queuedScene = sceneIndex;

    updateStatus();
}

void MainComponent::startSceneNow (int sceneIndex)
{
    currentScene = sceneIndex;
    queuedScene = -1;

    samplesSinceBarStart = 0.0;

    for (auto& track : tracks)
    {
        if (track.readerSource != nullptr)
        {
            track.transport.stop();
            track.transport.setPosition (0.0);
            track.transport.start();
        }
    }

    playing = true;

    updateStatus();
}

void MainComponent::startPlayback()
{
    startSceneNow (currentScene);
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

    updateStatus();
}

void MainComponent::updateStatus()
{
    juce::String text;

    if (playing)
    {
        text =
            "PLAYING  |  SCENE "
            + juce::String (currentScene + 1);
    }
    else
    {
        text = "STOPPED";
    }

    if (queuedScene >= 0)
    {
        text +=
            "  |  NEXT: SCENE "
            + juce::String (queuedScene + 1);
    }

    statusLabel.setText (
        text,
        juce::dontSendNotification);
}

double MainComponent::getSamplesPerBar() const
{
    const double bpm =
        bpmSlider.getValue();

    const double secondsPerBeat =
        60.0 / bpm;

    const double secondsPerBar =
        secondsPerBeat * 4.0;

    return secondsPerBar *
           currentSampleRate;
}
