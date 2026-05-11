#pragma once
#include <JuceHeader.h>
#include <vector>

struct WavExporter
{
    static bool writeStereoWav (const juce::File&          outFile,
                                 const std::vector<float>&  x,
                                 const std::vector<float>&  y,
                                 double                     sampleRate,
                                 float                      gainX,
                                 float                      gainY,
                                 double                     durationSeconds);
};
