#include "WavExporter.h"
#include <cmath>
#include <algorithm>

bool WavExporter::writeStereoWav (const juce::File&         outFile,
                                   const std::vector<float>& x,
                                   const std::vector<float>& y,
                                   double                    sampleRate,
                                   float                     gainX,
                                   float                     gainY,
                                   double                    durationSeconds)
{
    if (x.empty() || y.empty()) return false;
    const size_t patternN   = std::min (x.size(), y.size());
    const int    totalSamps = (int)(sampleRate * durationSeconds);
    if (totalSamps <= 0) return false;

    juce::WavAudioFormat wavFmt;
    auto stream = outFile.createOutputStream();
    if (!stream) return false;

    auto* writer = wavFmt.createWriterFor (
        stream.get(), sampleRate, 2, 16, {}, 0);
    if (!writer) return false;
    stream.release(); // writer owns it

    juce::AudioBuffer<float> buf (2, 8192);
    int written = 0;

    while (written < totalSamps)
    {
        const int block = std::min (8192, totalSamps - written);
        auto* L = buf.getWritePointer (0);
        auto* R = buf.getWritePointer (1);
        for (int i = 0; i < block; ++i)
        {
            size_t idx = (size_t)(written + i) % patternN;
            L[i] = std::clamp (x[idx] * gainX, -1.0f, 1.0f);
            R[i] = std::clamp (y[idx] * gainY, -1.0f, 1.0f);
        }
        if (!writer->writeFromAudioSampleBuffer (buf, 0, block))
            break;
        written += block;
    }
    delete writer;
    return true;
}
