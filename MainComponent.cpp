#include "MainComponent.h"
#include <cmath>

static const juce::Colour BG  { 0xff050a05 };
static const juce::Colour PNL { 0xff0a140a };
static const juce::Colour GRN { 0xff00ff41 };
static const juce::Colour DIM { 0xff226622 };

// ─────────────────────────────────────────────────────────────
MainComponent::MainComponent()
{
    setSize (1440, 880);

    // Scope renderer fills left portion
    addAndMakeVisible (scopeRenderer_);

    // Right panel
    buildControlPanel();
    addAndMakeVisible (controlsViewport_);

    // Start geometry thread
    geoProcessor_.startThread (juce::Thread::Priority::normal);

    // Audio: 0 inputs, 2 outputs
    setAudioChannels (0, 2);

    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    geoProcessor_.stopThread (3000);
    shutdownAudio();
}

// ─────────────────────────────────────────────────────────────
// Audio AppComponent
// ─────────────────────────────────────────────────────────────
void MainComponent::prepareToPlay (int, double sr)
{
    sampleRate_ = sr;
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (info.buffer->getNumChannels() < 2)
    { info.clearActiveBufferRegion(); return; }
    audioEngine_.fill (info);
}

void MainComponent::releaseResources() {}

// ─────────────────────────────────────────────────────────────
// Timer: poll geometry result, handle auto-rotation
// ─────────────────────────────────────────────────────────────
void MainComponent::timerCallback()
{
    // Auto-rotation
    const auto nowMs = juce::Time::currentTimeMillis();
    const float dt   = (float)(nowMs - lastTimerMs_) * 0.001f;
    lastTimerMs_ = nowMs;
    const float speed = (float)sldRotSpeed_->getValue();

    bool rotChanged = false;
    if (tglAutoX_.getToggleState()) { autoRotX_ += dt * 30.f * speed; rotChanged = true; }
    if (tglAutoY_.getToggleState()) { autoRotY_ += dt * 45.f * speed; rotChanged = true; }
    if (tglAutoZ_.getToggleState()) { autoRotZ_ += dt * 20.f * speed; rotChanged = true; }

    if (rotChanged) updateGeoParams();

    // New geometry result?
    if (geoProcessor_.hasNewResult())
    {
        geoProcessor_.clearNew();
        auto res = geoProcessor_.getResult();
        if (res.valid)
        {
            lastX_ = res.x;
            lastY_ = res.y;
            audioEngine_.setPath (res.x, res.y);
            scopeRenderer_.updatePath (res.x, res.y);
        }
        updateStatusLabel();
    }

    // Update Hz display
    const float hz = audioEngine_.currentHz();
    lblHz_.setText (juce::String (hz, 1) + " Hz", juce::dontSendNotification);
    lblHz_.setColour (juce::Label::textColourId, hz < 10.f ? juce::Colours::orange : GRN);
}

// ─────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────
void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    const int panelW = 290;
    scopeRenderer_.setBounds (bounds.removeFromLeft (bounds.getWidth() - panelW));
    controlsViewport_.setBounds (bounds);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (BG);
}

// ─────────────────────────────────────────────────────────────
// Controls panel builder
// ─────────────────────────────────────────────────────────────
juce::Slider* MainComponent::makeSlider (const juce::String& name,
                                          double mn, double mx, double def,
                                          const juce::String& unit)
{
    auto* s = new juce::Slider (juce::Slider::LinearHorizontal,
                                  juce::Slider::TextBoxRight);
    s->setName (name);
    s->setRange (mn, mx);
    s->setValue (def, juce::dontSendNotification);
    s->setTextValueSuffix (unit);
    s->addListener (this);
    s->setColour (juce::Slider::thumbColourId,     GRN);
    s->setColour (juce::Slider::trackColourId,     DIM);
    s->setColour (juce::Slider::textBoxTextColourId, GRN);
    s->setColour (juce::Slider::backgroundColourId, PNL);
    controlsContent_.addAndMakeVisible (s);
    return s;
}

static juce::Label* makeLbl (juce::Component& parent, const juce::String& text,
                               bool header = false)
{
    auto* l = new juce::Label ({}, text);
    l->setColour (juce::Label::textColourId,
                  header ? juce::Colour(0xff00cc33) : juce::Colour(0xff447744));
    l->setFont (juce::Font (header ? 11.f : 10.f, juce::Font::bold));
    parent.addAndMakeVisible (l);
    return l;
}

void MainComponent::buildControlPanel()
{
    controlsContent_.setSize (280, 2200);
    controlsViewport_.setViewedComponent (&controlsContent_, false);
    controlsViewport_.setScrollBarsShown (true, false);
    controlsContent_.setColour (juce::ResizableWindow::backgroundColourId, PNL);

    // ── Transport ────────────────────────────────────────────
    for (auto* b : { &btnOpen_, &btnPlay_, &btnSettings_, &btnExport_ })
    {
        b->addListener (this);
        b->setColour (juce::TextButton::buttonColourId,   PNL);
        b->setColour (juce::TextButton::textColourOffId,  GRN);
        b->setColour (juce::TextButton::buttonOnColourId, GRN);
        controlsContent_.addAndMakeVisible (b);
    }

    // ── File label ───────────────────────────────────────────
    lblFile_.setColour (juce::Label::textColourId, GRN);
    lblFile_.setFont   (juce::Font (10.f));
    lblHz_.setColour   (juce::Label::textColourId, GRN);
    lblHz_.setFont     (juce::Font (16.f, juce::Font::bold));
    lblStatus_.setColour (juce::Label::textColourId, DIM);
    lblStatus_.setFont   (juce::Font (9.f));
    controlsContent_.addAndMakeVisible (lblFile_);
    controlsContent_.addAndMakeVisible (lblHz_);
    controlsContent_.addAndMakeVisible (lblStatus_);

    // ── Sliders ──────────────────────────────────────────────
    using S = std::unique_ptr<juce::Slider>;

    sldRotX_      = S(makeSlider("rotX",     -180,180,0,"°"));
    sldRotY_      = S(makeSlider("rotY",     -180,180,0,"°"));
    sldRotZ_      = S(makeSlider("rotZ",     -180,180,0,"°"));
    sldRotSpeed_  = S(makeSlider("rotSpeed",  0.1, 6.0, 1.0,"x"));
    sldScale_     = S(makeSlider("scale",    0.1, 1.5, 0.88));
    sldOffX_      = S(makeSlider("offX",    -0.8, 0.8, 0.0));
    sldOffY_      = S(makeSlider("offY",    -0.8, 0.8, 0.0));
    sldDetail_    = S(makeSlider("detail",   200,80000,5000));
    sldSmooth_    = S(makeSlider("smooth",     0,  5,  2));
    sldLPF_       = S(makeSlider("lpf",      0.01,1.0, 1.0));
    sldTargetHz_  = S(makeSlider("targetHz",  2,  120, 30,"Hz"));
    sldGainX_     = S(makeSlider("gainX",    0.0, 2.0, 1.0));
    sldGainY_     = S(makeSlider("gainY",    0.0, 2.0, 1.0));
    sldMaster_    = S(makeSlider("master",   0.0, 1.0, 0.8));
    sldPhase_     = S(makeSlider("phase",      0, 500, 0,"smp"));
    sldDecay_     = S(makeSlider("decay",    0.5,0.999,0.92));
    sldGlow_      = S(makeSlider("glow",     0.0, 3.0, 0.55));
    sldBrightness_= S(makeSlider("brightness",0.2,2.0,1.0));
    sldBeamR_     = S(makeSlider("beamR",    0.0, 1.0, 0.0));
    sldBeamG_     = S(makeSlider("beamG",    0.0, 1.0, 1.0));
    sldBeamB_     = S(makeSlider("beamB",    0.0, 1.0, 0.25));

    // Toggles
    for (auto* t : { &tglAutoX_,&tglAutoY_,&tglAutoZ_,&tglArtistic_,&tglGrid_ })
    {
        t->setColour (juce::ToggleButton::textColourId,  GRN);
        t->setColour (juce::ToggleButton::tickColourId,  GRN);
        t->setColour (juce::ToggleButton::tickDisabledColourId, DIM);
        controlsContent_.addAndMakeVisible (t);
    }
    tglArtistic_.setToggleState (true, juce::dontSendNotification);
    tglGrid_.setToggleState (true, juce::dontSendNotification);

    // Layout is set in a lambda so we can call it once
    auto layout = [this]()
    {
        const int W = 270, M = 4, H = 22, BH = 26;
        int y = 6;

        auto row = [&](juce::Component& c, int h=H)
            { c.setBounds(M, y, W, h); y += h + 2; };

        row (btnOpen_, BH); row (btnPlay_, BH);
        row (lblFile_, H); row (lblHz_, 28); row (lblStatus_, H);
        y += 6;

        auto hdr = [&](const juce::String& s)
            { auto* l = makeLbl(controlsContent_,s,true); l->setBounds(M,y,W,14); y+=16; };

        hdr ("── ROTATION ──────────────────");
        row (*sldRotX_); row (*sldRotY_); row (*sldRotZ_);
        row (tglAutoX_,H); row (tglAutoY_,H); row (tglAutoZ_,H);
        row (*sldRotSpeed_);
        y+=4;
        hdr ("── TRANSFORM ─────────────────");
        row (*sldScale_); row (*sldOffX_); row (*sldOffY_);
        y+=4;
        hdr ("── GEOMETRY ──────────────────");
        row (*sldDetail_); row (*sldSmooth_); row (*sldLPF_);
        row (*sldTargetHz_);
        row (tglArtistic_,H);
        y+=4;
        hdr ("── AUDIO ─────────────────────");
        row (*sldGainX_); row (*sldGainY_); row (*sldMaster_);
        row (*sldPhase_);
        row (btnSettings_, BH);
        row (btnExport_, BH);
        y+=4;
        hdr ("── RENDER ────────────────────");
        row (*sldDecay_); row (*sldGlow_); row (*sldBrightness_);
        row (*sldBeamR_); row (*sldBeamG_); row (*sldBeamB_);
        row (tglGrid_,H);

        controlsContent_.setSize (280, y + 10);
    };
    layout();
}

// ─────────────────────────────────────────────────────────────
// Listeners
// ─────────────────────────────────────────────────────────────
void MainComponent::sliderValueChanged (juce::Slider* s)
{
    const juce::String n = s->getName();

    if (n == "gainX")   { audioEngine_.gainX.store   ((float)s->getValue()); return; }
    if (n == "gainY")   { audioEngine_.gainY.store   ((float)s->getValue()); return; }
    if (n == "master")  { audioEngine_.master.store  ((float)s->getValue()); return; }
    if (n == "phase")   { audioEngine_.phaseShift.store((int)s->getValue()); return; }

    if (n == "decay")   { scopeRenderer_.decay.store       ((float)s->getValue()); return; }
    if (n == "glow")    { scopeRenderer_.glowStrength.store ((float)s->getValue()); return; }
    if (n == "brightness"){ scopeRenderer_.brightness.store ((float)s->getValue()); return; }
    if (n == "beamR")   { scopeRenderer_.beamR.store ((float)s->getValue()); return; }
    if (n == "beamG")   { scopeRenderer_.beamG.store ((float)s->getValue()); return; }
    if (n == "beamB")   { scopeRenderer_.beamB.store ((float)s->getValue()); return; }

    // Everything else triggers geometry re-process
    updateGeoParams();
}

void MainComponent::buttonClicked (juce::Button* b)
{
    if (b == &btnOpen_)
    {
        juce::FileChooser fc ("Open 3D File", {}, "*.stl;*.obj;*.ply;*.glb;*.gltf");
        if (fc.browseForFileToOpen())
            loadFile (fc.getResult().getFullPathName());
    }
    else if (b == &btnPlay_)
    {
        playing_ = !playing_;
        audioEngine_.playing.store (playing_);
        btnPlay_.setButtonText (playing_ ? "Stop" : "Play");
    }
    else if (b == &btnSettings_) { showAudioSettings(); }
    else if (b == &btnExport_)   { exportWav(); }
}

// ─────────────────────────────────────────────────────────────
void MainComponent::loadFile (const juce::String& path)
{
    lblFile_.setText (juce::File(path).getFileName(), juce::dontSendNotification);
    lblStatus_.setText ("Loading…", juce::dontSendNotification);

    juce::Thread::launch ([this, path]()
    {
        if (!geoProcessor_.loadFile (path))
        {
            juce::MessageManager::callAsync ([this]()
            {
                lblStatus_.setText ("Load failed: " + geoProcessor_.getLastError(),
                                     juce::dontSendNotification);
            });
        }
    });
}

void MainComponent::updateGeoParams()
{
    const float DEG = juce::MathConstants<float>::pi / 180.0f;

    currentParams_.rotX = ((float)sldRotX_->getValue() + autoRotX_) * DEG;
    currentParams_.rotY = ((float)sldRotY_->getValue() + autoRotY_) * DEG;
    currentParams_.rotZ = ((float)sldRotZ_->getValue() + autoRotZ_) * DEG;
    currentParams_.scale        = (float)sldScale_->getValue();
    currentParams_.offX         = (float)sldOffX_->getValue();
    currentParams_.offY         = (float)sldOffY_->getValue();
    currentParams_.maxEdges     = (int)sldDetail_->getValue();
    currentParams_.smoothIter   = (int)sldSmooth_->getValue();
    currentParams_.lpfAlpha     = (float)sldLPF_->getValue();
    currentParams_.targetSamples= (int)(sampleRate_ / std::max(0.5, sldTargetHz_->getValue()));
    currentParams_.artisticMode = tglArtistic_.getToggleState();

    scopeRenderer_.showGrid.store (tglGrid_.getToggleState());

    geoProcessor_.setParams (currentParams_);
}

void MainComponent::updateStatusLabel()
{
    auto res = geoProcessor_.getResult();
    lblStatus_.setText (
        juce::String(res.nTris) + " tris | " +
        juce::String(res.nEdges) + " edges | " +
        juce::String(res.x.size()) + " pts",
        juce::dontSendNotification);
}

void MainComponent::showAudioSettings()
{
    juce::DialogWindow::LaunchOptions opts;
    auto* sel = new juce::AudioDeviceSelectorComponent (
        deviceManager, 0, 0, 2, 2, false, false, false, false);
    sel->setSize (500, 350);
    opts.content.setOwned (sel);
    opts.dialogTitle             = "Audio Settings";
    opts.dialogBackgroundColour  = BG;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar       = true;
    opts.launchAsync();
}

void MainComponent::exportWav()
{
    if (lastX_.empty()) return;
    juce::FileChooser fc ("Export WAV", {}, "*.wav");
    if (!fc.browseForFileToSave (true)) return;
    const bool ok = WavExporter::writeStereoWav (
        fc.getResult(), lastX_, lastY_, sampleRate_,
        (float)sldGainX_->getValue(),
        (float)sldGainY_->getValue(), 10.0);
    lblStatus_.setText (ok ? "WAV exported" : "Export failed",
                         juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────────
// Drag-and-drop
// ─────────────────────────────────────────────────────────────
bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        juce::String ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".stl" || ext == ".obj" || ext == ".ply" ||
            ext == ".glb"  || ext == ".gltf") return true;
    }
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    if (!files.isEmpty()) loadFile (files[0]);
}
