#pragma once
#include "CAudioNode.h"

class CSineWave : public CAudioNode
{
private:
    double m_freq;
    double m_amp;
    double m_phase;

public:
    //! Start audio generation
    virtual void Start();

    //! Generate one frame of audio
    virtual bool Generate();

    //! Set the sine wave frequency
    void SetFreq(double f) { m_freq = f; }

    //! Set the sine wave amplitude
    void SetAmplitude(double a) { m_amp = a; }

	CSineWave();
	virtual ~CSineWave();
};