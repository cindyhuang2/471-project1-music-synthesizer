#include "pch.h"
#include <cmath>
#include <algorithm>
#include "CDrumInstrument.h"

static inline double Sine01(double p) { return std::sin(p * 2.0 * 3.141592653589793); }
static inline double clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline double Noise01(uint32_t& s) {
    // xorshift32 -> [0,1)
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return (double)(s) * (1.0 / 4294967296.0);
}
static inline double Noise11(uint32_t& s) {
    return 2.0 * Noise01(s) - 1.0; // [-1,1]
}


CDrumInstrument::CDrumInstrument()
{
    m_duration = 0.25;
    m_time = 0.0;
    m_attack = 0.002;
    m_decay = 0.12;
    m_release = 0.10;
    m_drumType = L"kick";
    m_velocity = 0.9;
    m_pitchOffset = 0.0;
}

CDrumInstrument::~CDrumInstrument() {}

void CDrumInstrument::Start()
{
    m_time = 0.0;

    Voice v;
    v.type = m_drumType;
    v.t = 0.0;
    v.dur = m_duration;
    v.vel = m_velocity;
    v.isSynth = true;

    double ph1, ph2, ph3, ph4, ph5, ph6; // cymbal partial phases

    // Per-type envelopes (very short for hats)
    if (m_drumType == L"hihat") {
        v.atk = 0.0005; v.dec = 0.030; v.rel = 0.006; v.dur = 0.04; v.sus = 0.005;
    }
    else if (m_drumType == L"snare") {
        v.atk = 0.0008;  // very fast
        v.dec = 0.080;   // snap dies quick
        v.rel = 0.050;   // short tail
        v.sus = 0.0;
        v.dur = 0.07;

        // tonal body ~200 Hz, decays fast internally
        v.bodyPh = 0.0;
        v.bodyAmp = 0.45;                  // start level of body
        v.toneDec = std::exp(-GetSamplePeriod() * 70.0); // faster than before
        v.toneMix = 0.0; // we'll use bodyAmp instead of toneMix for the sine body

        // reset band-pass states
        v.bp_prev = 0.0;
        v.bp_hp_z = 0.0;
        v.bp_lp_z = 0.0;
    }

    else if (m_drumType == L"tom" || m_drumType == L"tom-hi" || m_drumType == L"tom-low") {
        v.atk = 0.001;  v.dec = 0.160; v.rel = 0.080; v.sus = 0.03;
        v.phaseInc = std::pow(2.0, m_pitchOffset / 12.0);
    }
    else if (m_drumType == L"cymbal")
    {
        v.atk = 0.0008;  // instant
        v.dec = 0.25;    // splash body
        v.rel = 0.90;    // long tail
        v.sus = 0.0;     // no sustain plateau
        v.dur = 0.5;  // time before release begins
        // init metallic phases (add these fields to Voice if not present)
        v.ph1 = v.ph2 = v.ph3 = v.ph4 = v.ph5 = v.ph6 = 0.0;
    }
    else { // kick
        v.atk = m_attack; v.dec = m_decay; v.rel = m_release; v.sus = 0.02;
    }

    // Seed RNG uniquely per voice: hash current time + address
    uint64_t mix = (uint64_t)(m_time * 44100.0) ^ (uint64_t)(uintptr_t)this;
    mix ^= (mix >> 33); mix *= 0xff51afd7ed558ccdULL;
    mix ^= (mix >> 33); mix *= 0xc4ceb9fe1a85ec53ULL;
    mix ^= (mix >> 33);
    v.rng = (uint32_t)mix | 1u; // keep it odd

    v.oscPh = v.auxPh = 0.0;
    v.lpf_z = v.hpf_z = v.prev = 0.0;

    if (m_voices.size() >= m_maxVoices) {
        m_voices.erase(m_voices.begin());
    }
    m_voices.push_back(v);
}

bool CDrumInstrument::Generate()
{
    const double dt = GetSamplePeriod();

    // Clear output
    m_frame[0] = 0.0;
    m_frame[1] = 0.0;

    // Process all active voices
    for (auto it = m_voices.begin(); it != m_voices.end(); )
    {
        Voice& v = *it;

        // Check if voice is done
        double tail = v.dur + 1e-3;
        if (v.rel > 0.0) tail += v.rel;

        if (v.t > tail) {
            it = m_voices.erase(it);
            continue;
        }

        // Calculate envelope for this voice
        double env = GetVoiceEnvelope(v);
        double s = 0.0;

        // Generate sound based on drum type
        if (v.type == L"snare")
        {
            // ------------- Noise crack in the MID band (? 1–4.5 kHz) -------------
            // Source: white noise
            double n = Noise11(v.rng);

            // High-pass around 1 kHz (remove low "bong", keep crack)
            const double hp_cut = 1000.0;
            const double a_hp = std::exp(-2.0 * PI * hp_cut * dt);
            double hp = (n - v.bp_prev) + a_hp * v.bp_hp_z;
            v.bp_prev = n;
            v.bp_hp_z = hp;

            // Low-pass around 4.5 kHz (avoid cymbal-ish sizzle)
            const double lp_cut = 4500.0;
            const double a_lp = std::exp(-2.0 * PI * lp_cut * dt);
            v.bp_lp_z = (1.0 - a_lp) * hp + a_lp * v.bp_lp_z;
            double midCrack = v.bp_lp_z;

            // Tiny sprinkle of high fizz only in the first ~15 ms
            double fizz = 0.0;
            if (v.t < 0.015) {
                // quick, bright burst (HP at 5 kHz then LP at 10 kHz)
                const double hp2 = 5000.0, lp2 = 10000.0;
                const double a_hp2 = std::exp(-2.0 * PI * hp2 * dt);
                const double a_lp2 = std::exp(-2.0 * PI * lp2 * dt);
                static thread_local double h2 = 0.0, l2 = 0.0;
                double nn = Noise11(v.rng);
                double hps = (nn - h2) + a_hp2 * h2; h2 = nn;
                l2 = (1.0 - a_lp2) * hps + a_lp2 * l2;
                fizz = 0.15 * l2 * std::exp(-v.t / 0.010); // very short
            }

            // ------------- Short tonal body around 180–220 Hz -------------
            const double bodyHz = 190.0; // tweak 180–220 to taste
            v.bodyPh += bodyHz * dt; if (v.bodyPh >= 1.0) v.bodyPh -= 1.0;
            v.bodyAmp *= v.toneDec;  // fast internal decay (~70 s^-1)
            double body = v.bodyAmp * Sine01(v.bodyPh);

            // Mix: mostly mid-band noise + small body + micro fizz
            s = 0.82 * midCrack   // the "crack"
                + 0.12 * body       // thump without boom
                + fizz;             // initial bright snap only
        }


        else if (v.type == L"tom" || v.type == L"tom-hi" || v.type == L"tom-lo")
        {
            // Determine base pitch based on tom type
            double basePitch = 110.0; // default mid tom (A2)

            if (v.type == L"tom-hi") {
                basePitch = 155.56; // D#3 - high tom
            }
            else if (v.type == L"tom-low") {
                basePitch = 82.41; // E2 - low tom
            }

            // Then apply pitch offset on top
            const double freq = basePitch * v.phaseInc;
            const double sweep = 80.0;
            const double tau = 0.04;
            const double f = freq + sweep * std::exp(-v.t / tau);
            v.oscPh += f * dt;
            v.oscPh -= std::floor(v.oscPh);
            s = Sine01(v.oscPh);
        }
        else if (v.type == L"hihat")
        {
            // bright noise
            double n = Noise11(v.rng);

            // Band-pass around 9 kHz: (HPF @ 4k then LPF @ 12k)
            const double hp_cut = 4000.0;
            const double lp_cut = 12000.0;
            const double ahp = std::exp(-2.0 * PI * hp_cut * dt);
            const double alp = std::exp(-2.0 * PI * lp_cut * dt);

            // simple HPF (differentiator form)
            double hp = n - v.prev + ahp * v.hpf_z;
            v.prev = n;
            v.hpf_z = hp;

            // one-pole LPF on hp to make BPF
            v.lpf_z = (1.0 - alp) * hp + alp * v.lpf_z;

            // optional faint metallic cluster (inharmonic, very quiet)
            v.oscPh += 0.123 * dt; if (v.oscPh >= 1.0) v.oscPh -= 1.0;
            v.auxPh += 0.187 * dt; if (v.auxPh >= 1.0) v.auxPh -= 1.0;
            double metal = 0.05 * (Sine01(v.oscPh * 11000.0) + Sine01(v.auxPh * 14700.0));

            s = 0.95 * v.lpf_z + metal;
        }

        else if (v.type == L"cymbal")
        {
            // White noise source (persistent RNG)
            double n = Noise11(v.rng);

            // --- High-pass (~5.5 kHz) using "differentiator + leak" ---
            const double hp_cut = 5500.0;
            const double a_hp = std::exp(-2.0 * PI * hp_cut * dt);
            double hp = (n - v.prev) + a_hp * v.hpf_z;  // high-passy
            v.prev = n;
            v.hpf_z = hp;

            // --- Low-pass (~12 kHz) to shape the band ---
            const double lp_cut = 12000.0;
            const double a_lp = std::exp(-2.0 * PI * lp_cut * dt);
            v.lpf_z = (1.0 - a_lp) * hp + a_lp * v.lpf_z;
            double band = v.lpf_z;

            // --- Metallic partial cluster (very quiet, inharmonic) ---
            // Two fast “ring-mod” pairs to add cymbal-like zing:
            v.oscPh += 0.000; if (v.oscPh >= 1.0) v.oscPh -= 1.0; // (phases unused for time; we’ll integrate below)
            v.auxPh += 0.000; if (v.auxPh >= 1.0) v.auxPh -= 1.0;

            // integrate explicit phases for metal partials
            // choose inharmonic ratios around 4–12 kHz
            static const double ratios[6] = { 4.07, 5.41, 6.80, 8.21, 9.63, 11.2 };
            // base “clang” freq:
            const double f0 = 1100.0;

            // keep a couple of running phases on the voice (add these to your Voice struct if you want more)
            v.ph1 += (f0 * ratios[0]) * dt; if (v.ph1 >= 1.0) v.ph1 -= 1.0;
            v.ph2 += (f0 * ratios[1]) * dt; if (v.ph2 >= 1.0) v.ph2 -= 1.0;
            v.ph3 += (f0 * ratios[2]) * dt; if (v.ph3 >= 1.0) v.ph3 -= 1.0;
            v.ph4 += (f0 * ratios[3]) * dt; if (v.ph4 >= 1.0) v.ph4 -= 1.0;
            v.ph5 += (f0 * ratios[4]) * dt; if (v.ph5 >= 1.0) v.ph5 -= 1.0;
            v.ph6 += (f0 * ratios[5]) * dt; if (v.ph6 >= 1.0) v.ph6 -= 1.0;

            double metal =
                0.22 * Sine01(v.ph1) +
                0.18 * Sine01(v.ph2) +
                0.16 * Sine01(v.ph3) +
                0.14 * Sine01(v.ph4) +
                0.12 * Sine01(v.ph5) +
                0.10 * Sine01(v.ph6);

            // subtle internal decay on metallics so attack is bright, tail is mostly noise
            static const double metalTau = 0.9; // seconds-ish
            const double metalEnv = std::exp(-v.t / metalTau);
            metal *= metalEnv;

            // Mix a bright, airy cymbal: mostly filtered noise + a taste of metal
            s = 0.80 * band + 0.20 * metal;
        }
        else // default: bass/kick
        {
            const double base = 55.0, sweep = 140.0, tau = 0.035;
            const double f = base + sweep * std::exp(-v.t / tau);
            v.oscPh += f * dt;
            v.oscPh -= std::floor(v.oscPh);

            double click = 0.0;
            if (v.t < 0.006) {
                v.auxPh += 1000.0 * dt;
                v.auxPh -= std::floor(v.auxPh);
                click = 0.35 * Sine01(v.auxPh);
            }
            s = 0.95 * Sine01(v.oscPh) + click;
        }

        double out = v.vel * env * s;
        out = out / (1.0 + 0.5 * std::abs(out)); // soft clip

        // Add to output frame
        m_frame[0] += out;
        m_frame[1] += out;

        // Advance voice time
        v.t += dt;
        ++it;
    }

    // Advance global time (for backwards compatibility)
    m_time += dt;

    // Continue if we have active voices
    return !m_voices.empty();
}

double CDrumInstrument::GetVoiceEnvelope(const Voice& v)
{
    const double t = v.t;

    // Attack
    if (t < v.atk && v.atk > 1e-6) return t / v.atk;

    // Decay
    const double t1 = v.atk;
    const double t2 = v.atk + v.dec;
    if (t >= t1 && t < t2 && v.dec > 1e-6) {
        const double x = (t - t1) / v.dec;
        return (1.0 - x) * (1.0 - 0.2 * x);
    }

    // Sustain
    const double sustain = 0.08;

    // Release starts at end of nominal duration
    double rs = v.dur;
    if (t < rs) return sustain;

    if (v.rel <= 1e-6) return 0.0;
    const double xr = clamp((t - rs) / v.rel, 0.0, 1.0);
    return sustain * (1.0 - xr);
}

void CDrumInstrument::SetNote(CNote* note)
{
    // Safety check
    if (note == NULL || note->Node() == NULL) return;

    CComPtr<IXMLDOMNamedNodeMap> attributes;
    HRESULT hr = note->Node()->get_attributes(&attributes);
    if (FAILED(hr) || attributes == NULL) return;

    long len;
    attributes->get_length(&len);

    for (int i = 0; i < len; i++)
    {
        CComPtr<IXMLDOMNode> attrib;
        attributes->get_item(i, &attrib);
        if (attrib == NULL) continue;

        CComBSTR name;
        attrib->get_nodeName(&name);

        CComVariant value;
        attrib->get_nodeValue(&value);

        if (name == L"duration")
        {
            if (SUCCEEDED(value.ChangeType(VT_R8)))
                m_duration = value.dblVal;
        }
        else if (name == L"type")
        {
            if (value.vt == VT_BSTR && value.bstrVal != NULL)
                m_drumType = value.bstrVal;
        }
        else if (name == L"velocity")
        {
            if (SUCCEEDED(value.ChangeType(VT_R8)))
                m_velocity = value.dblVal;
        }
        else if (name == L"pitch")
        {
            if (SUCCEEDED(value.ChangeType(VT_R8)))
                m_pitchOffset = value.dblVal;
        }
    }
}

void CDrumInstrument::AddVoice(const std::wstring& type, double durationSec, double velocity,
    double pitchSemitones, double pan)
{
    // Optional method for programmatic voice creation
    m_drumType = type;
    m_duration = durationSec;
    m_velocity = velocity;
    m_pitchOffset = pitchSemitones;
}

// Legacy method for backwards compatibility
double CDrumInstrument::GetEnvelope()
{
    const double t = m_time;

    if (t < m_attack && m_attack > 1e-6) return t / m_attack;

    const double t1 = m_attack;
    const double t2 = m_attack + m_decay;
    if (t >= t1 && t < t2 && m_decay > 1e-6) {
        const double x = (t - t1) / m_decay;
        return (1.0 - x) * (1.0 - 0.2 * x);
    }

    const double sustain = 0.08;
    double rs = m_duration;
    if (t < rs) return sustain;

    if (m_release <= 1e-6) return 0.0;
    const double xr = clamp((t - rs) / m_release, 0.0, 1.0);
    return sustain * (1.0 - xr);
}