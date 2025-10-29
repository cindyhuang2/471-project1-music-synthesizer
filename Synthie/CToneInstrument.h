#pragma once
#include "CInstrument.h"
#include "CSineWave.h"

class CToneInstrument : public CInstrument
{
private:
    CSineWave   m_sinewave;
    double m_duration;
    double m_time;

public:
    CToneInstrument();
    ~CToneInstrument();

    virtual void Start();
    virtual bool Generate();

    void SetFreq(double f) { m_sinewave.SetFreq(f); }
    void SetAmplitude(double a) { m_sinewave.SetAmplitude(a); }
    void SetDuration(double d) { m_duration = d; }
    void SetNote(CNote* note) override;
};

