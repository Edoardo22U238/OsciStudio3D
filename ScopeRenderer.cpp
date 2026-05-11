#include "ScopeRenderer.h"
#include <cstring>

using namespace juce::gl;

// ─────────────────────────────────────────────────────────────
// GLSL sources
// ─────────────────────────────────────────────────────────────
static const char* BEAM_VERT = R"(
attribute vec2 position;
void main() { gl_Position = vec4(position, 0.0, 1.0); }
)";
static const char* BEAM_FRAG = R"(
uniform vec3  uColor;
uniform float uIntensity;
void main() { gl_FragColor = vec4(uColor * uIntensity, uIntensity); }
)";

static const char* QUAD_VERT = R"(
attribute vec2 position;
attribute vec2 texCoord;
varying vec2 vUV;
void main() { gl_Position = vec4(position, 0.0, 1.0); vUV = texCoord; }
)";

static const char* DECAY_FRAG = R"(
varying vec2 vUV;
uniform sampler2D uTex;
uniform float uDecay;
void main() { gl_FragColor = vec4(texture2D(uTex, vUV).rgb * uDecay, 1.0); }
)";

static const char* BLUR_FRAG = R"(
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDir;
void main() {
    vec4 c = vec4(0.0);
    float w[5];
    w[0]=0.0625; w[1]=0.25; w[2]=0.375; w[3]=0.25; w[4]=0.0625;
    for (int i=0;i<5;i++)
        c += texture2D(uTex, vUV + uDir*float(i-2)*2.0)*w[i];
    gl_FragColor = c;
}
)";

static const char* COMPOSITE_FRAG = R"(
varying vec2 vUV;
uniform sampler2D uPhosphor;
uniform sampler2D uGlow;
uniform float uGlowStr;
uniform float uBrightness;
void main() {
    vec3 p = texture2D(uPhosphor, vUV).rgb;
    vec3 g = texture2D(uGlow, vUV).rgb;
    vec2 uv2 = vUV*2.0-1.0;
    float vig = 1.0 - smoothstep(0.65, 1.45, length(uv2));
    float sl  = 1.0 - 0.07*(0.5+0.5*sin(vUV.y*3.14159*1080.0));
    vec3 col  = (p + g*uGlowStr) * vig * sl * uBrightness;
    gl_FragColor = vec4(col, 1.0);
}
)";

// ─────────────────────────────────────────────────────────────
ScopeRenderer::ScopeRenderer()
{
    openGLContext_.setRenderer (this);
    openGLContext_.setComponentPaintingEnabled (false);
    openGLContext_.setContinuousRepainting (true);
    openGLContext_.attachTo (*this);

    pathBufs_[0].xy.reserve (MAX_BEAM_PTS * 2);
    pathBufs_[1].xy.reserve (MAX_BEAM_PTS * 2);
}

ScopeRenderer::~ScopeRenderer()
{
    openGLContext_.detach();
}

void ScopeRenderer::resized()
{
    // FBOs are recreated in renderOpenGL when size changes
}

void ScopeRenderer::updatePath (const std::vector<float>& x,
                                  const std::vector<float>& y)
{
    const size_t n = std::min (x.size(), y.size());
    const int nxt  = 1 - activePath_.load (std::memory_order_relaxed);

    auto& buf = pathBufs_[nxt];
    buf.xy.clear();
    buf.count = std::min (n, (size_t)MAX_BEAM_PTS);

    for (size_t i = 0; i < buf.count; ++i)
    {
        buf.xy.push_back (x[i]);
        buf.xy.push_back (-y[i]); // flip Y for OpenGL
    }
    pathPending_.store (true, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────
bool ScopeRenderer::compileShaders()
{
    auto compile = [&](const char* vs, const char* fs,
                        std::unique_ptr<juce::OpenGLShaderProgram>& out) -> bool
    {
        out = std::make_unique<juce::OpenGLShaderProgram>(openGLContext_);
        if (!out->addVertexShader (vs))   { jassertfalse; return false; }
        if (!out->addFragmentShader (fs)) { jassertfalse; return false; }
        if (!out->link())                 { jassertfalse; return false; }
        return true;
    };
    return compile (BEAM_VERT, BEAM_FRAG, shaderBeam_)
        && compile (QUAD_VERT, DECAY_FRAG, shaderDecay_)
        && compile (QUAD_VERT, BLUR_FRAG,  shaderBlur_)
        && compile (QUAD_VERT, COMPOSITE_FRAG, shaderComposite_);
}

void ScopeRenderer::createFBOs (int w, int h)
{
    destroyFBOs();

    auto makeFBO = [&](GLuint& fbo, GLuint& tex)
    {
        glGenTextures (1, &tex);
        glBindTexture (GL_TEXTURE_2D, tex);
        glTexImage2D  (GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture (GL_TEXTURE_2D, 0);

        glGenFramebuffers (1, &fbo);
        glBindFramebuffer (GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glClearColor (0,0,0,1);
        glClear (GL_COLOR_BUFFER_BIT);
        glBindFramebuffer (GL_FRAMEBUFFER, 0);
    };

    makeFBO (fboPhosphor_, texPhosphor_);
    makeFBO (fboGlow_,     texGlow_);
    fboW_ = w; fboH_ = h;
}

void ScopeRenderer::destroyFBOs()
{
    if (fboPhosphor_) { glDeleteFramebuffers(1,&fboPhosphor_); fboPhosphor_=0; }
    if (texPhosphor_) { glDeleteTextures(1,&texPhosphor_);     texPhosphor_=0; }
    if (fboGlow_)     { glDeleteFramebuffers(1,&fboGlow_);     fboGlow_=0;     }
    if (texGlow_)     { glDeleteTextures(1,&texGlow_);         texGlow_=0;     }
}

// ─────────────────────────────────────────────────────────────
void ScopeRenderer::newOpenGLContextCreated()
{
    compileShaders();

    // Beam VBO
    glGenBuffers (1, &beamVbo_);
    glBindBuffer (GL_ARRAY_BUFFER, beamVbo_);
    glBufferData (GL_ARRAY_BUFFER, MAX_BEAM_PTS * 2 * sizeof(float),
                  nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    // Fullscreen quad: position(xy) + uv(xy) interleaved
    float quad[] = {
        -1,-1, 0,0,   1,-1, 1,0,   1,1, 1,1,
        -1,-1, 0,0,   1, 1, 1,1,  -1,1, 0,1
    };
    glGenBuffers (1, &quadVbo_);
    glBindBuffer (GL_ARRAY_BUFFER, quadVbo_);
    glBufferData (GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void ScopeRenderer::openGLContextClosing()
{
    destroyFBOs();
    if (beamVbo_)  { glDeleteBuffers(1,&beamVbo_);  beamVbo_=0;  }
    if (quadVbo_)  { glDeleteBuffers(1,&quadVbo_);  quadVbo_=0;  }
    shaderBeam_.reset(); shaderDecay_.reset();
    shaderBlur_.reset(); shaderComposite_.reset();
}

void ScopeRenderer::renderOpenGL()
{
    juce::OpenGLHelpers::clear (juce::Colours::black);
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;

    // Recreate FBOs if size changed
    if (fboW_ != W || fboH_ != H) createFBOs (W, H);

    // Swap pending path
    if (pathPending_.load (std::memory_order_acquire))
    {
        activePath_.store (1 - activePath_.load (std::memory_order_relaxed));
        pathPending_.store (false, std::memory_order_relaxed);
    }

    const auto& path = pathBufs_[activePath_.load()];
    const float dec  = decay.load();
    const float glow = glowStrength.load();
    const float brit = brightness.load();
    const float cr   = beamR.load(), cg = beamG.load(), cb = beamB.load();

    glViewport (0, 0, W, H);
    glEnable (GL_BLEND);

    // ── Helper: bind quad and set attribs ────────────────────
    auto bindQuad = [&](juce::OpenGLShaderProgram& sh)
    {
        glBindBuffer (GL_ARRAY_BUFFER, quadVbo_);
        auto pos = (GLuint)sh.getAttributeID ("position");
        auto uv  = (GLuint)sh.getAttributeID ("texCoord");
        glEnableVertexAttribArray (pos);
        glVertexAttribPointer (pos, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), nullptr);
        glEnableVertexAttribArray (uv);
        glVertexAttribPointer (uv, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    };

    // ── 1. Decay phosphor buffer ─────────────────────────────
    glBindFramebuffer (GL_FRAMEBUFFER, fboPhosphor_);
    glBlendFunc (GL_ONE, GL_ZERO);
    shaderDecay_->use();
    glActiveTexture (GL_TEXTURE0);
    glBindTexture   (GL_TEXTURE_2D, texPhosphor_);
    juce::OpenGLShaderProgram::Uniform (*shaderDecay_, "uTex").set (0);
    juce::OpenGLShaderProgram::Uniform (*shaderDecay_, "uDecay").set (dec);
    bindQuad (*shaderDecay_);
    glDrawArrays (GL_TRIANGLES, 0, 6);

    // ── 2. Draw beam (additive) ───────────────────────────────
    if (path.count > 1)
    {
        glBlendFunc (GL_ONE, GL_ONE);
        shaderBeam_->use();
        juce::OpenGLShaderProgram::Uniform (*shaderBeam_, "uColor").set (cr,cg,cb);
        juce::OpenGLShaderProgram::Uniform (*shaderBeam_, "uIntensity").set (1.0f);

        glBindBuffer (GL_ARRAY_BUFFER, beamVbo_);
        glBufferSubData (GL_ARRAY_BUFFER, 0,
                         (GLsizeiptr)(path.count * 2 * sizeof(float)),
                         path.xy.data());

        auto pos = (GLuint)shaderBeam_->getAttributeID ("position");
        glEnableVertexAttribArray (pos);
        glVertexAttribPointer (pos, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);
        glLineWidth (1.5f);
        glDrawArrays (GL_LINE_STRIP, 0, (GLsizei)path.count);
        glDisableVertexAttribArray (pos);
    }
    glBindFramebuffer (GL_FRAMEBUFFER, 0);

    // ── 3. Glow: horizontal blur ─────────────────────────────
    glBindFramebuffer (GL_FRAMEBUFFER, fboGlow_);
    glBlendFunc (GL_ONE, GL_ZERO);
    shaderBlur_->use();
    glActiveTexture (GL_TEXTURE0);
    glBindTexture   (GL_TEXTURE_2D, texPhosphor_);
    juce::OpenGLShaderProgram::Uniform (*shaderBlur_, "uTex").set (0);
    juce::OpenGLShaderProgram::Uniform (*shaderBlur_, "uDir").set (1.0f/W, 0.0f);
    bindQuad (*shaderBlur_);
    glDrawArrays (GL_TRIANGLES, 0, 6);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);

    // vertical blur back into glow texture (ping-pong)
    glBindFramebuffer (GL_FRAMEBUFFER, fboGlow_);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture   (GL_TEXTURE_2D, texGlow_);
    juce::OpenGLShaderProgram::Uniform (*shaderBlur_, "uDir").set (0.0f, 1.0f/H);
    glDrawArrays (GL_TRIANGLES, 0, 6);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);

    // ── 4. Composite to screen ────────────────────────────────
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    shaderComposite_->use();
    glActiveTexture (GL_TEXTURE0);
    glBindTexture   (GL_TEXTURE_2D, texPhosphor_);
    glActiveTexture (GL_TEXTURE1);
    glBindTexture   (GL_TEXTURE_2D, texGlow_);
    juce::OpenGLShaderProgram::Uniform (*shaderComposite_, "uPhosphor").set (0);
    juce::OpenGLShaderProgram::Uniform (*shaderComposite_, "uGlow").set (1);
    juce::OpenGLShaderProgram::Uniform (*shaderComposite_, "uGlowStr").set (glow);
    juce::OpenGLShaderProgram::Uniform (*shaderComposite_, "uBrightness").set (brit);
    bindQuad (*shaderComposite_);
    glDrawArrays (GL_TRIANGLES, 0, 6);

    glDisable (GL_BLEND);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindTexture (GL_TEXTURE_2D, 0);
}
