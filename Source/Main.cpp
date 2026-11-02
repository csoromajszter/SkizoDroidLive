#include <JuceHeader.h>
#include "MainComponent.h"

class SkizoDroidLiveApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "SkizoDroid Live"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { mainWindow.reset(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (std::move (name), juce::Colours::black,
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            centreWithSize (1180, 760);
            setResizable (true, true);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (SkizoDroidLiveApplication)
