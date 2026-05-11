#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "GeometryProcessor.h"
#include "ScopeRenderer.h"
#include "WavExporter.h"

class MainComponent : public juce::AudioAppComponent,
                       public juce::FileDragAndDropTarget,
                       public juce::Timer,
                       private juce::Slider::Listener,
                       private juce::Button::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    // AudioAppComponent
    void prepareToPlay (int samplesPerBlock, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& buf) override;
    void releaseResources() override;

    // Component
    void paint   (juce::Graphics&) override;
    void resized () override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    // Timer
    void timerCallback() override;

private:
    // ── Helpers ──────────────────────────────────────────────
    void buildControlPanel ();
    void loadFile          (const juce::String& path);
    void updateGeoParams   ();
    void showAudioSettings ();
    void exportWav         ();
    void updateStatusLabel ();

    // Slider::Listener / Button::Listener
    void sliderValueChanged (juce::Slider* s) override;
    void buttonClicked      (juce::Button* b) override;

    juce::Slider* makeSlider (const juce::String& name,
                               double min, double max, double def,
                               const juce::String& unit = {});

    // ── Core engines ─────────────────────────────────────────
    AudioEngine       audioEngine_;
    GeometryProcessor geoProcessor_;
    ScopeRenderer     scopeRenderer_;

    // ── Controls (right panel) ───────────────────────────────
    juce::Viewport         controlsViewport_;
    juce::Component        controlsContent_;

    juce::TextButton  btnOpen_       { "Open File" };
    juce::TextButton  btnPlay_       { "Play" };
    juce::TextButton  btnSettings_   { "Audio Settings" };
    juce::TextButton  btnExport_     { "Export WAV" };

    juce::Label       lblFile_       { {}, "No file loaded" };
    juce::Label       lblStatus_     { {}, "Ready" };
    juce::Label       lblHz_         { {}, "0 Hz" };

    // Sliders
    std::unique_ptr<juce::Slider> sldRotX_, sldRotY_, sldRotZ_;
    std::unique_ptr<juce::Slider> sldScale_, sldOffX_, sldOffY_;
    std::unique_ptr<juce::Slider> sldDetail_, sldSmooth_, sldLPF_;
    std::unique_ptr<juce::Slider> sldTargetHz_;
    std::unique_ptr<juce::Slider> sldGainX_, sldGainY_, sldMaster_;
    std::unique_ptr<juce::Slider> sldPhase_;
    std::unique_ptr<juce::Slider> sldDecay_, sldGlow_, sldBrightness_;
    std::unique_ptr<juce::Slider> sldBeamR_, sldBeamG_, sldBeamB_;
    std::unique_ptr<juce::Slider> sldRotSpeed_;

    juce::ToggleButton  tglAutoX_ { "Auto X" };
    juce::ToggleButton  tglAutoY_ { "Auto Y" };
    juce::ToggleButton  tglAutoZ_ { "Auto Z" };
    juce::ToggleButton  tglArtistic_ { "Artistic Mode" };
    juce::ToggleButton  tglGrid_  { "Show Grid" };

    // ── State ────────────────────────────────────────────────
    bool     playing_    { false };
    double   sampleRate_ { 48000.0 };
    float    autoRotX_   { 0.f }, autoRotY_ { 0.f }, autoRotZ_ { 0.f };
    juce::int64 lastTimerMs_ { 0 };

    GeoParams currentParams_;

    // Stored current path for WAV export
    std::vector<float> lastX_, lastY_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
