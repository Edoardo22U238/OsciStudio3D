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
void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    vUV = texCoord;
}
)";

static const char* DECAY_FRAG = R"(
varying vec2 vUV;
uniform sampler2D uTex;
uniform float uDecay;

void main()
{
    gl_FragColor = vec4(texture2D(uTex, vUV).rgb * uDecay, 1.0);
}
)";

static const char* BLUR_FRAG = R"(
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uDir;

void main()
{
    vec4 c = vec4(0.0);

    float w[5];
    w[0]=0.0625;
    w[1]=0.25;
    w[2]=0.375;
    w[3]=0.25;
    w[4]=0.0625;

    for (int i = 0; i < 5; ++i)
        c += texture2D(uTex, vUV + uDir * float(i - 2) * 2.0) * w[i];

    gl_FragColor = c;
}
)";

static const char* COMPOSITE_FRAG = R"(
varying vec2 vUV;

uniform sampler2D uPhosphor;
uniform sampler2D uGlow;

uniform float uGlowStr;
uniform float uBrightness;

void main()
{
    vec3 p = texture2D(uPhosphor, vUV).rgb;
    vec3 g = texture2D(uGlow,     vUV).rgb;

    vec2 uv2 = vUV * 2.0 - 1.0;

    float vig = 1.0 - smoothstep(0.65, 1.45, length(uv2));

    float sl = 1.0 - 0.07 *
        (0.5 + 0.5 * sin(vUV.y * 3.14159 * 1080.0));

    vec3 col =
        (p + g * uGlowStr) *
        vig *
        sl *
        uBrightness;

    gl_FragColor = vec4(col, 1.0);
}
)";

// ─────────────────────────────────────────────────────────────

ScopeRenderer::ScopeRenderer()
{
    openGLContext_.setRenderer(this);
    openGLContext_.setComponentPaintingEnabled(false);
    openGLContext_.setContinuousRepainting(true);
    openGLContext_.attachTo(*this);

    pathBufs_[0].xy.reserve(MAX_BEAM_PTS * 2);
    pathBufs_[1].xy.reserve(MAX_BEAM_PTS * 2);
}

ScopeRenderer::~ScopeRenderer()
{
    openGLContext_.detach();
}

void ScopeRenderer::resized()
{
}

void ScopeRenderer::updatePath(
    const std::vector<float>& x,
    const std::vector<float>& y)
{
    const size_t n =
        std::min(x.size(), y.size());

    const int nxt =
        1 - activePath_.load(std::memory_order_relaxed);

    auto& buf = pathBufs_[nxt];

    buf.xy.clear();

    buf.count =
        std::min(n, (size_t)MAX_BEAM_PTS);

    for (size_t i = 0; i < buf.count; ++i)
    {
        buf.xy.push_back(x[i]);
        buf.xy.push_back(-y[i]);
    }

    pathPending_.store(
        true,
        std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────

bool ScopeRenderer::compileShaders()
{
    auto compile =
        [&](const char* vs,
            const char* fs,
            std::unique_ptr<juce::OpenGLShaderProgram>& out)
        -> bool
    {
        out =
            std::make_unique<juce::OpenGLShaderProgram>(
                openGLContext_);

        if (!out->addVertexShader(vs))
            return false;

        if (!out->addFragmentShader(fs))
            return false;

        if (!out->link())
            return false;

        return true;
    };

    return
        compile(BEAM_VERT, BEAM_FRAG, shaderBeam_) &&
        compile(QUAD_VERT, DECAY_FRAG, shaderDecay_) &&
        compile(QUAD_VERT, BLUR_FRAG, shaderBlur_) &&
        compile(QUAD_VERT, COMPOSITE_FRAG, shaderComposite_);
}

// ─────────────────────────────────────────────────────────────

void ScopeRenderer::newOpenGLContextCreated()
{
    compileShaders();

    glGenBuffers(1, &beamVbo_);

    glBindBuffer(GL_ARRAY_BUFFER, beamVbo_);

    glBufferData(
        GL_ARRAY_BUFFER,
        MAX_BEAM_PTS * 2 * sizeof(float),
        nullptr,
        GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const float quad[] =
    {
        -1.f,-1.f, 0.f,0.f,
         1.f,-1.f, 1.f,0.f,
         1.f, 1.f, 1.f,1.f,

        -1.f,-1.f, 0.f,0.f,
         1.f, 1.f, 1.f,1.f,
        -1.f, 1.f, 0.f,1.f
    };

    glGenBuffers(1, &quadVbo_);

    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(quad),
        quad,
        GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ─────────────────────────────────────────────────────────────

void ScopeRenderer::openGLContextClosing()
{
    if (beamVbo_)
        glDeleteBuffers(1, &beamVbo_);

    if (quadVbo_)
        glDeleteBuffers(1, &quadVbo_);

    beamVbo_ = 0;
    quadVbo_ = 0;

    shaderBeam_.reset();
    shaderDecay_.reset();
    shaderBlur_.reset();
    shaderComposite_.reset();
}

// ─────────────────────────────────────────────────────────────

static void bindQuad(
    GLuint vbo,
    juce::OpenGLShaderProgram& shader)
{
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    juce::OpenGLShaderProgram::Attribute pos(
        shader,
        "position");

    juce::OpenGLShaderProgram::Attribute uv(
        shader,
        "texCoord");

    if (pos.attributeID >= 0)
    {
        glEnableVertexAttribArray(
            (GLuint)pos.attributeID);

        glVertexAttribPointer(
            (GLuint)pos.attributeID,
            2,
            GL_FLOAT,
            GL_FALSE,
            4 * sizeof(float),
            nullptr);
    }

    if (uv.attributeID >= 0)
    {
        glEnableVertexAttribArray(
            (GLuint)uv.attributeID);

        glVertexAttribPointer(
            (GLuint)uv.attributeID,
            2,
            GL_FLOAT,
            GL_FALSE,
            4 * sizeof(float),
            (void*)(2 * sizeof(float)));
    }
}

// ─────────────────────────────────────────────────────────────

void ScopeRenderer::renderOpenGL()
{
    juce::OpenGLHelpers::clear(
        juce::Colours::black);

    if (pathPending_.load(std::memory_order_acquire))
    {
        activePath_.store(
            1 - activePath_.load(),
            std::memory_order_relaxed);

        pathPending_.store(
            false,
            std::memory_order_relaxed);
    }

    const auto& path =
        pathBufs_[activePath_.load()];

    if (path.count <= 1)
        return;

    shaderBeam_->use();

    juce::OpenGLShaderProgram::Uniform(
        *shaderBeam_,
        "uColor")
        .set(0.3f, 1.0f, 0.4f);

    juce::OpenGLShaderProgram::Uniform(
        *shaderBeam_,
        "uIntensity")
        .set(1.0f);

    glBindBuffer(GL_ARRAY_BUFFER, beamVbo_);

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        path.count * 2 * sizeof(float),
        path.xy.data());

    juce::OpenGLShaderProgram::Attribute pos(
        *shaderBeam_,
        "position");

    if (pos.attributeID >= 0)
    {
        glEnableVertexAttribArray(
            (GLuint)pos.attributeID);

        glVertexAttribPointer(
            (GLuint)pos.attributeID,
            2,
            GL_FLOAT,
            GL_FALSE,
            2 * sizeof(float),
            nullptr);

        glDrawArrays(
            GL_LINE_STRIP,
            0,
            (GLsizei)path.count);

        glDisableVertexAttribArray(
            (GLuint)pos.attributeID);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
