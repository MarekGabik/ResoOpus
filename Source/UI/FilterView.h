#pragma once

#include "Theme.h"
#include "Widgets.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <vector>

class ResoOpusProcessor;

// FILTER A and B as they really answer: the impulse response of the actual filter models (same DSP as the sound,
// with the modulated cutoff, resonance and drive) through an FFT, drawn on a log frequency scale. The result
// follows ROUTING: serial = A x B, parallel = A + B, split = drone (A) and lead (B), stereo = left (A) and right (B).
// Below it the ROUTING switch and a small diagram of the signal path.
class FilterRoutingView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FilterRoutingView (ResoOpusProcessor&);
    void update();   // editor timer: re-measures when a filter setting moved
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr double displayRate = 48000.0;
    static constexpr int fftOrder = 14, fftSize = 1 << fftOrder;

private:
    struct Setting
    {
        float cutoff = 0.0f, res = -1.0f, drive = 0.0f;
        int model = -1, mode = -1;
        bool operator== (const Setting& o) const
        {
            return std::abs (cutoff - o.cutoff) < 0.002f * cutoff && std::abs (res - o.res) < 0.002f && std::abs (drive - o.drive) < 0.002f
                   && model == o.model && mode == o.mode;
        }
    };
    void measure (const Setting&, std::vector<std::complex<float>>& out);
    juce::Rectangle<float> graphArea() const;
    juce::Rectangle<float> diagramArea() const;
    float xFor (double hz) const;
    float yFor (double db) const;
    juce::Path curve (const std::vector<std::complex<float>>& h, bool closed) const;
    void drawDiagram (juce::Graphics&, juce::Rectangle<float>) const;

    ResoOpusProcessor& proc;
    LedRadio route;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> work;
    Setting shown[2];
    std::vector<std::complex<float>> resp[2], sum;
    int routeShown = -2;
    bool bOn = false;
};
