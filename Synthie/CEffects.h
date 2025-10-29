#pragma once
#include <vector>
#include <cmath>

class CEffects
{
public:
    void SetSampleRate(double sr)
    {
        m_sr = sr;
        const int maxMs = 1000;
        SetLowpassHz(8000.0);
    }

    void SetGain(double g) { m_gain = g; }
    void SetWet(double w) { m_wet = std::fmax(0.0, std::fmin(1.0, w)); }
    void SetFeedback(double fb) { m_fb = std::fmax(0.0, std::fmin(0.95, fb)); }
    void SetLowpassHz(double hz)
    {
        // one-pole LPF: y += a*(x - y)
        m_lpfA = 1.0 - std::exp(-2.0 * PI * hz / m_sr);
    }

    // Process one stereo frame in place
    void Process(double frame[2]);

private:
    double m_sr = 44100.0;

    // gain
    double m_gain = 0.90;

    // LPF
    double m_lpfA = 0.0, m_lL = 0.0, m_lR = 0.0;

    double m_fb = 0.25;
    double m_wet = 0.20;
};
