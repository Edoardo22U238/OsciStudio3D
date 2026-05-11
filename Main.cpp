#include <JuceHeader.h>
#include "MainComponent.h"

class OsciApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()    override { return "OsciStudio3D"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed()          override { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow_ = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override { mainWindow_.reset(); }

    void systemRequestedQuit() override
    {
        quit();
    }

    // ── Inner window ─────────────────────────────────────────
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : juce::DocumentWindow (name,
                  juce::Colour (0xff050a05),
                  juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (800, 600, 3840, 2160);

            centreWithSize (1440, 880);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

private:
    std::unique_ptr<MainWindow> mainWindow_;
};

START_JUCE_APPLICATION (OsciApplication)
