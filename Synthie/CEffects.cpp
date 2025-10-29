#include "pch.h"
#include "CEffects.h"

void CEffects::Process(double frame[2])
{
    // 1) Gain
    double L = frame[0] * m_gain;
    double R = frame[1] * m_gain;

    // 2) Gentle LPF (tames harsh noise tails)
    m_lL += m_lpfA * (L - m_lL);
    m_lR += m_lpfA * (R - m_lR);
    L = m_lL; R = m_lR;

    // 3)
    double wetL = 0.85 + 0.15;
    double wetR = 0.85 + 0.15;

    double outL = (1.0 - m_wet) * L + m_wet * wetL;
    double outR = (1.0 - m_wet) * R + m_wet * wetR;

    // 4) Soft limiter
    auto soft = [](double x) { return x / (1.0 + 0.5 * std::abs(x)); };
    frame[0] = soft(outL);
    frame[1] = soft(outR);
}