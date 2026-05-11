#pragma once
#include <JuceHeader.h>
#include <vector>
#include <mutex>
#include <atomic>

/**
 * OpenGL phosphor-persistence oscilloscope display.
 * Uses two FBOs: one for accumulation (with per-frame decay),
 * one for the glow blur pass.
 */
class ScopeRenderer : public juce::Component,
                       public juce::OpenGLRenderer
{
public:
    ScopeRenderer();
    ~ScopeRenderer() override;

    // Thread-safe path update (called from UI/geometry thread)
    void updatePath (const std::vector<float>& x,
                     const std::vector<float>& y);

    // Parameters — written from UI thread, read from GL thread
    std::atomic<float> decay        { 0.92f };
    std::atomic<float> glowStrength { 0.55f };
    std::atomic<float> brightness   { 1.0f  };
    std::atomic<float> beamR        { 0.0f  };
    std::atomic<float> beamG        { 1.0f  };
    std::atomic<float> beamB        { 0.25f };
    std::atomic<bool>  showGrid     { true  };

    // juce::OpenGLRenderer
    void newOpenGLContextCreated()  override;
    void renderOpenGL()             override;
    void openGLContextClosing()     override;

    // juce::Component
    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    juce::OpenGLContext openGLContext_;

    // ── Beam path (double-buffered) ──────────────────────────
    struct PathBuf { std::vector<float> xy; size_t count = 0; };
    PathBuf          pathBufs_[2];
    std::atomic<int> activePath_ { 0 };
    std::atomic<bool> pathPending_ { false };
    std::mutex        pathMutex_;

    // ── GL resources (created/used on GL thread) ─────────────
    // Phosphor FBO
    GLuint fboPhosphor_ = 0, texPhosphor_ = 0;
    // Glow FBO
    GLuint fboGlow_ = 0, texGlow_ = 0;

    // Beam VBO (xy interleaved, pre-allocated)
    GLuint beamVao_ = 0, beamVbo_ = 0;
    static constexpr int MAX_BEAM_PTS = 200000;

    // Quad for full-screen passes
    GLuint quadVao_ = 0, quadVbo_ = 0;

    // Shader programs
    std::unique_ptr<juce::OpenGLShaderProgram> shaderBeam_;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderDecay_;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderBlur_;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderComposite_;

    int fboW_ = 0, fboH_ = 0;

    void createFBOs   (int w, int h);
    void destroyFBOs  ();
    void drawGrid     ();
    bool compileShaders();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopeRenderer)
};
