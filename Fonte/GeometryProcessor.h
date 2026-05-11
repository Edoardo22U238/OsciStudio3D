#pragma once
#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <array>

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

struct GeoParams
{
    float rotX = 0.f, rotY = 0.f, rotZ = 0.f;
    float scale = 0.88f;
    float offX  = 0.f, offY = 0.f;
    int   maxEdges      = 5000;
    int   smoothIter    = 2;
    float lpfAlpha      = 1.0f;   // 1=bypass
    int   targetSamples = 4800;   // 48000/10Hz
    bool  artisticMode  = true;
};

class GeometryProcessor : public juce::Thread
{
public:
    GeometryProcessor();
    ~GeometryProcessor() override;

    bool loadFile (const juce::String& path);
    void setParams (const GeoParams& p);

    struct Result {
        std::vector<float> x, y;
        int nEdges = 0, nTris = 0;
        bool valid = false;
    };
    Result       getResult()    const;
    bool         hasNewResult() const noexcept { return newResult_.load(); }
    void         clearNew()           noexcept { newResult_.store(false); }
    juce::String getLastError() const;

    void run() override;

private:
    struct Edge3 { Vec3 a, b; };

    void processInternal (const GeoParams& p);
    bool loadAssimp      (const juce::String& path);
    void extractEdges    (int maxEdges);
    void buildPath       ();
    void smoothChaikin   (std::vector<float>& px, std::vector<float>& py, int iters);
    void applyLPF        (std::vector<float>& v, float alpha);
    void resampleArcLen  (std::vector<float>& px, std::vector<float>& py, int n);
    void projectTo2D     (const GeoParams& p, std::vector<float>& px, std::vector<float>& py);

    static std::array<float,9> makeRotMat (float rx, float ry, float rz);
    static Vec3 applyRot (const std::array<float,9>& R, Vec3 v);

    struct MeshData {
        std::vector<Vec3> vertices;
        std::vector<int>  indices;
    };
    MeshData           mesh_;
    std::vector<Edge3> edges_;

    mutable std::mutex paramMutex_;
    GeoParams          params_;
    bool               paramsChanged_ { false };
    bool               fileLoaded_    { false };

    mutable std::mutex resultMutex_;
    Result             result_;

    std::atomic<bool>  newResult_ { false };
    mutable std::mutex errorMutex_;
    juce::String       lastError_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeometryProcessor)
};
