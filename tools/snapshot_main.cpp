// Headless GUI snapshot: renders the RackStrip editor to a PNG without a DAW.
// Build: see build_snapshot.sh
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class SnapshotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "RackStripSnapshot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        proc = std::make_unique<RackStripProcessor>();
        proc->setRateAndBufferSizeDetails (48000.0, 512);

        // pretend some gain reduction is happening so meters are lit
        for (int i = 0; i < 200; ++i)
            if (auto* ed = dynamic_cast<RackStripEditor*> (proc->createEditor()))
            {
                ed->setSize (1376, 768);
                auto img = ed->createComponentSnapshot (ed->getLocalBounds());
                juce::PNGImageFormat png;
                juce::FileOutputStream out (juce::File::getCurrentWorkingDirectory().getChildFile ("snapshot.png"));
                png.writeImageToStream (img, out);
                delete ed;
                juce::Logger::writeToLog ("wrote snapshot.png " + juce::String (img.getWidth()) + "x" + juce::String (img.getHeight()));
                break;
            }
        quit();
    }
    void shutdown() override { proc.reset(); }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    std::unique_ptr<RackStripProcessor> proc;
};

START_JUCE_APPLICATION (SnapshotApp)
