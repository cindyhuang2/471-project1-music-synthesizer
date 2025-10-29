#pragma once
#include "CInstrument.h"
#include "CAudioNode.h"
#include <memory>
#include <vector>
#include <audio/Wave.h>

class CWavePlayer;  // Forward declaration

class CDrumInstrument : public CInstrument
{
public:
    CDrumInstrument();
    virtual ~CDrumInstrument();

    virtual void Start();
    virtual bool Generate();

    struct Voice {
        std::wstring type;   // "kick"|"snare"|"tom"|"cymbal"|variants
        double t = 0.0;      // seconds since note-on
        double dur = 0.25;   // seconds (from XML beats * sec/beat)
        double vel = 0.9;    // 0..1
        double pan = 0.0;    // -1..+1 (optional)
        // Envelopes
        double atk = 0.001, dec = 0.1, rel = 0.1;
        // Source: either synth or sample
        bool   isSynth = false;
        // Sample playback
        std::shared_ptr<CWave> sample; // whole file in RAM
        double phase = 0.0;            // fractional index
        double phaseInc = 1.0;         // pitch ratio
        // Simple filters for snare/cymbal/tom body
        double lpf_z = 0.0, hpf_z = 0.0;
        // Scratch for synth oscillators
        double oscPh = 0.0, auxPh = 0.0;

        uint32_t rng = 0xA3C59AC3u;  // per-voice RNG state
        double prev = 0.0;           // for simple HPF/differentiator
        double sus = 0.02;           // sustain level per voice (drums want it tiny)
        double toneMix = 0.0;        // for snare tone mix
        double toneDec = 0.0;        // snare tone decay rate

        double ph1, ph2, ph3, ph4, ph5, ph6;

        double bp_prev = 0.0;   // for band-pass HP stage (prev noise)
        double bp_hp_z = 0.0;   // HP leak state
        double bp_lp_z = 0.0;   // LP state
        double bodyPh = 0.0;    // body sine phase (~200 Hz)
        double bodyAmp = 0.0;   // internal decay for body

    };

    double GetVoiceEnvelope(const Voice& v);

    virtual void SetNote(CNote* note);

    void AddVoice(const std::wstring& type, double durationSec, double velocity,
        double pitchSemitones = 0.0, double pan = 0.0);


private:
    double m_duration;
    double m_time;

    // Wave player that streams from file
    std::shared_ptr<CWavePlayer> m_wavePlayer;

    // Envelope for volume control
    double m_attack;
    double m_decay;
    double m_release;
    double m_velocity;
    double m_pitchOffset;

    // Which drum sound to play
    std::wstring m_drumType;

    // Calculate envelope value at current time
    double GetEnvelope();
    
    std::vector<Voice> m_voices;  // active voices
    size_t m_maxVoices = 64;      // polyphony cap

};