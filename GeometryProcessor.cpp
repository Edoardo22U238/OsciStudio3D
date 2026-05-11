#include "GeometryProcessor.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <limits>

// ── Rotation matrix (Rz * Ry * Rx) ──────────────────────────
std::array<float,9> GeometryProcessor::makeRotMat (float rx, float ry, float rz)
{
    float cx = std::cos(rx), sx = std::sin(rx);
    float cy = std::cos(ry), sy = std::sin(ry);
    float cz = std::cos(rz), sz = std::sin(rz);

    // Rx
    float Rx[9] = { 1,0,0,  0,cx,-sx,  0,sx,cx };
    // Ry
    float Ry[9] = { cy,0,sy,  0,1,0,  -sy,0,cy };
    // Rz
    float Rz2[9] = { cz,-sz,0,  sz,cz,0,  0,0,1 };

    // Rzy = Rz * Ry
    std::array<float,9> Rzy{};
    for (int r=0;r<3;++r) for (int c=0;c<3;++c) {
        Rzy[r*3+c]=0;
        for(int k=0;k<3;++k) Rzy[r*3+c]+=Rz2[r*3+k]*Ry[k*3+c];
    }
    // R = Rzy * Rx
    std::array<float,9> R{};
    for (int r=0;r<3;++r) for (int c=0;c<3;++c) {
        R[r*3+c]=0;
        for(int k=0;k<3;++k) R[r*3+c]+=Rzy[r*3+k]*Rx[k*3+c];
    }
    return R;
}

Vec3 GeometryProcessor::applyRot (const std::array<float,9>& R, Vec3 v)
{
    return { R[0]*v.x+R[1]*v.y+R[2]*v.z,
             R[3]*v.x+R[4]*v.y+R[5]*v.z,
             R[6]*v.x+R[7]*v.y+R[8]*v.z };
}

// ─────────────────────────────────────────────────────────────
GeometryProcessor::GeometryProcessor() : juce::Thread ("GeoProcessor")
{
    startThread (juce::Thread::Priority::normal);
}

GeometryProcessor::~GeometryProcessor()
{
    signalThreadShouldExit();
    notify();
    stopThread (3000);
}

bool GeometryProcessor::loadFile (const juce::String& path)
{
    {
        std::lock_guard<std::mutex> lk (paramMutex_);
        fileLoaded_    = false;
        paramsChanged_ = false;
    }
    {
        std::lock_guard<std::mutex> lk (errorMutex_);
        lastError_ = {};
    }

    if (!loadAssimp (path))
        return false;

    {
        std::lock_guard<std::mutex> lk (paramMutex_);
        fileLoaded_    = true;
        paramsChanged_ = true;
    }
    notify();
    return true;
}

void GeometryProcessor::setParams (const GeoParams& p)
{
    {
        std::lock_guard<std::mutex> lk (paramMutex_);
        params_        = p;
        paramsChanged_ = true;
    }
    notify();
}

GeometryProcessor::Result GeometryProcessor::getResult() const
{
    std::lock_guard<std::mutex> lk (resultMutex_);
    return result_;
}

juce::String GeometryProcessor::getLastError() const
{
    std::lock_guard<std::mutex> lk (errorMutex_);
    return lastError_;
}

// ── Thread loop ───────────────────────────────────────────────
void GeometryProcessor::run()
{
    while (!threadShouldExit())
    {
        GeoParams localParams;
        bool       doProcess = false;

        {
            std::lock_guard<std::mutex> lk (paramMutex_);
            if (paramsChanged_ && fileLoaded_) {
                localParams    = params_;
                paramsChanged_ = false;
                doProcess      = true;
            }
        }

        if (doProcess)
            processInternal (localParams);

        wait (20);
    }
}

// ── Assimp loader ─────────────────────────────────────────────
bool GeometryProcessor::loadAssimp (const juce::String& path)
{
    Assimp::Importer imp;
    const unsigned flags =
        aiProcess_Triangulate          |
        aiProcess_JoinIdenticalVertices|
        aiProcess_SortByPType          |
        aiProcess_GenNormals;

    const aiScene* scene = imp.ReadFile (path.toStdString(), flags);
    if (!scene || !scene->HasMeshes())
    {
        std::lock_guard<std::mutex> lk (errorMutex_);
        lastError_ = "Assimp: " + juce::String (imp.GetErrorString());
        return false;
    }

    mesh_.vertices.clear();
    mesh_.indices.clear();

    for (unsigned m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        if (!(mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE)) continue;

        const int vertBase = (int)mesh_.vertices.size();
        for (unsigned v = 0; v < mesh->mNumVertices; ++v)
            mesh_.vertices.push_back ({ mesh->mVertices[v].x,
                                        mesh->mVertices[v].y,
                                        mesh->mVertices[v].z });

        for (unsigned f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            mesh_.indices.push_back (vertBase + (int)face.mIndices[0]);
            mesh_.indices.push_back (vertBase + (int)face.mIndices[1]);
            mesh_.indices.push_back (vertBase + (int)face.mIndices[2]);
        }
    }

    // Normalize to [-1, 1]
    if (mesh_.vertices.empty()) return false;
    Vec3 mn { 1e9f,1e9f,1e9f }, mx {-1e9f,-1e9f,-1e9f };
    for (auto& v : mesh_.vertices) {
        mn.x=std::min(mn.x,v.x); mn.y=std::min(mn.y,v.y); mn.z=std::min(mn.z,v.z);
        mx.x=std::max(mx.x,v.x); mx.y=std::max(mx.y,v.y); mx.z=std::max(mx.z,v.z);
    }
    Vec3 cen { (mn.x+mx.x)*.5f,(mn.y+mx.y)*.5f,(mn.z+mx.z)*.5f };
    float range = std::max({mx.x-mn.x, mx.y-mn.y, mx.z-mn.z});
    if (range < 1e-9f) range = 1.0f;
    float s = 2.0f / range;
    for (auto& v : mesh_.vertices) {
        v.x = (v.x-cen.x)*s;
        v.y = (v.y-cen.y)*s;
        v.z = (v.z-cen.z)*s;
    }
    return true;
}

// ── Edge extraction with deduplication ───────────────────────
void GeometryProcessor::extractEdges (int maxEdges)
{
    edges_.clear();

    // Hash helper: pack two int keys
    struct EdgeKey {
        int a, b;
        bool operator== (const EdgeKey& o) const { return a==o.a && b==o.b; }
    };
    struct EKH {
        size_t operator()(const EdgeKey& k) const {
            return std::hash<long long>()( (long long)k.a * 100003LL + k.b );
        }
    };
    std::unordered_map<EdgeKey, bool, EKH> seen;
    seen.reserve ((size_t)mesh_.indices.size());

    const int nTris = (int)mesh_.indices.size() / 3;
    for (int t = 0; t < nTris; ++t)
    {
        int idx[3] = { mesh_.indices[t*3], mesh_.indices[t*3+1], mesh_.indices[t*3+2] };
        for (int e = 0; e < 3; ++e)
        {
            int va = idx[e], vb = idx[(e+1)%3];
            if (va > vb) std::swap (va, vb);
            EdgeKey key { va, vb };
            if (seen.emplace (key, true).second)
            {
                edges_.push_back ({ mesh_.vertices[va], mesh_.vertices[vb] });
                if ((int)edges_.size() >= maxEdges * 2) goto done; // hard cap
            }
        }
    }
    done:

    // If too many, decimate uniformly
    if ((int)edges_.size() > maxEdges)
    {
        std::vector<Edge3> tmp;
        tmp.reserve ((size_t)maxEdges);
        float step = (float)edges_.size() / (float)maxEdges;
        for (int i = 0; i < maxEdges; ++i)
            tmp.push_back (edges_[(int)(i*step)]);
        edges_ = std::move (tmp);
    }
}

// ── Greedy nearest-neighbor path ordering ────────────────────
void GeometryProcessor::buildPath()
{
    const int n = (int)edges_.size();
    if (n == 0) return;

    // Use a flat endpoint list and a spatial grid (64^3) for lookups
    struct Pt { float x, y, z; int edgeIdx; bool isB; };
    std::vector<Pt> pts;
    pts.reserve (n * 2);
    for (int i = 0; i < n; ++i) {
        pts.push_back ({ edges_[i].a.x, edges_[i].a.y, edges_[i].a.z, i, false });
        pts.push_back ({ edges_[i].b.x, edges_[i].b.y, edges_[i].b.z, i, true  });
    }

    std::vector<bool> usedEdge (n, false);
    std::vector<bool> usedPt   (pts.size(), false);

    // Reorder edges_ in-place according to nearest-neighbor
    std::vector<Edge3> ordered;
    ordered.reserve (n);

    // Start with edge 0
    float cx = edges_[0].a.x, cy = edges_[0].a.y, cz = edges_[0].a.z;
    usedEdge[0] = true;
    ordered.push_back ({ edges_[0].a, edges_[0].b });
    cx = edges_[0].b.x; cy = edges_[0].b.y; cz = edges_[0].b.z;

    for (int iter = 1; iter < n; ++iter)
    {
        float bestDist = std::numeric_limits<float>::max();
        int   bestIdx  = -1;
        bool  bestFlip = false;

        for (int i = 0; i < n; ++i)
        {
            if (usedEdge[i]) continue;
            auto& e = edges_[i];
            float da = (e.a.x-cx)*(e.a.x-cx)+(e.a.y-cy)*(e.a.y-cy)+(e.a.z-cz)*(e.a.z-cz);
            float db = (e.b.x-cx)*(e.b.x-cx)+(e.b.y-cy)*(e.b.y-cy)+(e.b.z-cz)*(e.b.z-cz);
            if (da < bestDist) { bestDist=da; bestIdx=i; bestFlip=false; }
            if (db < bestDist) { bestDist=db; bestIdx=i; bestFlip=true; }
        }
        if (bestIdx < 0) break;
        usedEdge[bestIdx] = true;
        if (!bestFlip) {
            ordered.push_back ({ edges_[bestIdx].a, edges_[bestIdx].b });
            cx=edges_[bestIdx].b.x; cy=edges_[bestIdx].b.y; cz=edges_[bestIdx].b.z;
        } else {
            ordered.push_back ({ edges_[bestIdx].b, edges_[bestIdx].a });
            cx=edges_[bestIdx].a.x; cy=edges_[bestIdx].a.y; cz=edges_[bestIdx].a.z;
        }
    }
    edges_ = std::move (ordered);
}

// ── Project 3D path to 2D ────────────────────────────────────
void GeometryProcessor::projectTo2D (const GeoParams& p,
                                      std::vector<float>& px,
                                      std::vector<float>& py)
{
    auto R = makeRotMat (p.rotX, p.rotY, p.rotZ);

    px.clear(); py.clear();
    px.reserve (edges_.size() * 2);
    py.reserve (edges_.size() * 2);

    for (auto& e : edges_)
    {
        auto ra = applyRot (R, e.a);
        auto rb = applyRot (R, e.b);

        float ax = ra.x * p.scale + p.offX;
        float ay = ra.y * p.scale + p.offY;
        float bx = rb.x * p.scale + p.offX;
        float by = rb.y * p.scale + p.offY;

        ax = std::clamp (ax, -1.f, 1.f);
        ay = std::clamp (ay, -1.f, 1.f);
        bx = std::clamp (bx, -1.f, 1.f);
        by = std::clamp (by, -1.f, 1.f);

        px.push_back (ax); py.push_back (ay);
        px.push_back (bx); py.push_back (by);
    }
}

// ── Chaikin corner-cutting ────────────────────────────────────
void GeometryProcessor::smoothChaikin (std::vector<float>& px,
                                        std::vector<float>& py, int iters)
{
    for (int it = 0; it < iters; ++it)
    {
        const size_t n = px.size();
        if (n < 3) break;
        std::vector<float> nx, ny;
        nx.reserve (2*(n-1));
        ny.reserve (2*(n-1));
        for (size_t i = 0; i+1 < n; ++i)
        {
            nx.push_back (0.75f*px[i] + 0.25f*px[i+1]);
            ny.push_back (0.75f*py[i] + 0.25f*py[i+1]);
            nx.push_back (0.25f*px[i] + 0.75f*px[i+1]);
            ny.push_back (0.25f*py[i] + 0.75f*py[i+1]);
        }
        px = std::move (nx);
        py = std::move (ny);
    }
}

// ── IIR low-pass filter ───────────────────────────────────────
void GeometryProcessor::applyLPF (std::vector<float>& v, float alpha)
{
    if (v.empty() || alpha >= 0.999f) return;
    float state = v[0];
    for (auto& s : v) {
        state += alpha * (s - state);
        s = state;
    }
}

// ── Arc-length resampling ─────────────────────────────────────
void GeometryProcessor::resampleArcLen (std::vector<float>& px,
                                         std::vector<float>& py, int n)
{
    const size_t N = px.size();
    if (N < 2 || n <= 0) return;

    std::vector<float> cd (N, 0.f);
    for (size_t i = 1; i < N; ++i)
    {
        float dx = px[i]-px[i-1], dy = py[i]-py[i-1];
        cd[i] = cd[i-1] + std::sqrt (dx*dx + dy*dy);
    }
    float total = cd.back();
    if (total < 1e-9f) return;

    std::vector<float> rx (n), ry (n);
    size_t j = 0;
    for (int i = 0; i < n; ++i)
    {
        float t = total * (float)i / (float)(n-1);
        while (j+1 < N-1 && cd[j+1] < t) ++j;
        float seg = cd[j+1] - cd[j];
        float alpha = seg > 1e-9f ? (t - cd[j]) / seg : 0.f;
        rx[i] = px[j] + alpha*(px[j+1]-px[j]);
        ry[i] = py[j] + alpha*(py[j+1]-py[j]);
    }
    px = std::move (rx);
    py = std::move (ry);
}

// ── Full pipeline ─────────────────────────────────────────────
void GeometryProcessor::processInternal (const GeoParams& p)
{
    extractEdges (p.maxEdges);

    if (!p.artisticMode)
    {
        // Geometric mode: just project, no reorder
    }
    else
    {
        // Artistic: reorder for continuity
        if ((int)edges_.size() <= 3000)
            buildPath();           // O(n^2) greedy, fine up to ~3k
    }

    std::vector<float> px, py;
    projectTo2D (p, px, py);

    if (p.smoothIter > 0)
        smoothChaikin (px, py, p.smoothIter);

    applyLPF (px, p.lpfAlpha);
    applyLPF (py, p.lpfAlpha);

    const int targetN = std::clamp (p.targetSamples, 64, AudioEngine::MAX_SAMPLES);
    resampleArcLen (px, py, targetN);

    // Store result
    {
        std::lock_guard<std::mutex> lk (resultMutex_);
        result_.x      = px;
        result_.y      = py;
        result_.nEdges = (int)edges_.size();
        result_.nTris  = (int)mesh_.indices.size() / 3;
        result_.valid  = !px.empty();
    }
    newResult_.store (true);
}
