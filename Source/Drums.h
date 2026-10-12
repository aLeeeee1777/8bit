#pragma once
#include "ChipEngine.h"

// درام ماشین هشت‌بیتی: هر چیپ با نویز LFSR، موج و پله‌های ولوم خودش
namespace chip
{
enum DrumType { DKick = 0, DRim, DSnare, DClap, DSnare2, DTom, DClosedHat, DOpenHat, DCrash, DRide, DCowbell, DNone = -1 };

struct DrumParams
{
    float kickTune = 0.0f, kickDecay = 0.28f, kickSweep = 24.0f;
    float snareTune = 0.0f, snareTone = 0.5f, snareDecay = 0.18f;
    float hatDecay = 0.035f, openDecay = 0.35f, tomTune = 0.0f, volume = 0.9f;
    bool  noiseShort = false, authentic = true;
};

// نت MIDI → درام (چیدمان General MIDI)
inline int drumForNote (int note, int& tomIndex)
{
    tomIndex = 0;
    switch (note)
    {
        case 36: return DKick;
        case 37: return DRim;
        case 38: return DSnare;
        case 39: return DClap;
        case 40: return DSnare2;
        case 41: tomIndex = 0; return DTom;
        case 42: return DClosedHat;
        case 43: tomIndex = 1; return DTom;
        case 44: return DClosedHat;      // pedal hat (کوتاه‌تر)
        case 45: tomIndex = 2; return DTom;
        case 46: return DOpenHat;
        case 47: tomIndex = 3; return DTom;
        case 48: tomIndex = 4; return DTom;
        case 49: return DCrash;
        case 50: tomIndex = 5; return DTom;
        case 51: return DRide;
        case 56: return DCowbell;
        default: return DNone;
    }
}

inline int drumToneWave (int chipId)
{
    switch (chipId)
    {
        case NES: case SID:    return Triangle;
        case GameBoy: case PCE: return Wavetable;
        default:               return Pulse;
    }
}

// موج تُن هر چیپ (۰..۱)
inline float drumTone (int chipId, float phase)
{
    switch (chipId)
    {
        case NES:
        {
            const int i = (int) (phase * 32.0f) & 31;
            return (float) (i < 16 ? 15 - i : i - 16) / 15.0f;
        }
        case GameBoy: return gbWaveSample (2, phase, 15.0f);
        case PCE:     return gbWaveSample (0, phase, 31.0f);
        case SID:
        {
            const float t = phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
            return std::floor (t * 255.0f + 0.5f) / 255.0f;
        }
        default: return phase < 0.5f ? 1.0f : 0.0f;
    }
}

struct DrumVoice
{
    bool     active = false, choke = false;
    int      type = DNone, note = 36, tomIdx = 0;
    float    vel = 1.0f, chokeT = 0.0f;
    uint64_t age = 0;
    double   t = 0.0, phase = 0.0, phase2 = 0.0, nAcc = 0.0;
    float    nLast = 0.0f, hpLp = 0.0f;
    Lfsr     lfsr;

    void trigger (int drumType, int midiNote, int tom, float velocity, int chipId, uint64_t order)
    {
        type = drumType; note = midiNote; tomIdx = tom; vel = velocity; age = order;
        active = true; choke = false; chokeT = 0.0f;
        t = phase = phase2 = nAcc = 0.0; nLast = 0.0f; hpLp = 0.0f;
        lfsr.reset (chipId);
    }

    // نویز LFSR با کلاک دلخواه؛ اگه کلاک بالاتر از سمپل‌ریت بود میانگین می‌گیره
    float noise (int chipId, double clock, bool shortMode, double sr)
    {
        nAcc += clock / sr;
        int n = (int) nAcc;
        nAcc -= (double) n;
        if (n <= 0) return nLast;
        n = std::min (n, 64);
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) { lfsr.step (chipId, shortMode); sum += lfsr.output (chipId); }
        nLast = sum / (float) n * 2.0f - 1.0f;
        return nLast;
    }

    float highpass (float x) { hpLp += 0.22f * (x - hpLp); return x - hpLp; }

    float tone (int chipId, float f, double sr, bool authentic)
    {
        const float fq = quantizeFreq (chipId, drumToneWave (chipId), f, authentic);
        phase += (double) fq / sr;
        if (phase >= 1.0) phase -= std::floor (phase);
        return drumTone (chipId, (float) phase) * 2.0f - 1.0f;
    }

    static float square (double ph) { return ph < 0.5 ? 1.0f : -1.0f; }

    float render (const DrumParams& d, int chipId, double sr)
    {
        if (! active) return 0.0f;
        const float dt = (float) (1.0 / sr);
        t += dt;
        const float tt = (float) t;
        const float tq = d.authentic ? std::floor (tt * 240.0f) / 240.0f : tt;   // آپدیت ۲۴۰ بار در ثانیه مثل فریم کنسول

        auto env = [&] (float dec) { return std::exp (-5.0f * tq / std::max (dec, 0.005f)); };
        auto q   = [&] (float a)   { return quantizeLevel (chipId, a * vel); };

        float out = 0.0f, endT = 0.5f;

        switch (type)
        {
            case DKick:
            {
                const float semis = d.kickTune + d.kickSweep * std::exp (-tq / 0.04f);
                const float f = 50.0f * std::exp2 (semis / 12.0f);
                out = tone (chipId, f, sr, d.authentic) * q (env (d.kickDecay))
                    + noise (chipId, 20000.0, false, sr) * q (env (0.006f)) * 0.3f;
                endT = d.kickDecay * 1.25f;
                break;
            }
            case DRim:
            {
                out = tone (chipId, 1750.0f, sr, d.authentic) * q (env (0.03f)) * 0.7f
                    + noise (chipId, 30000.0, false, sr) * q (env (0.01f)) * 0.4f;
                endT = 0.05f;
                break;
            }
            case DSnare:
            {
                const float f = 190.0f * std::exp2 ((d.snareTune + 4.0f * std::exp (-tq / 0.02f)) / 12.0f);
                out = tone (chipId, f, sr, d.authentic) * q (env (d.snareDecay * 0.55f)) * d.snareTone * 0.8f
                    + noise (chipId, 14000.0, false, sr) * q (env (d.snareDecay)) * (1.0f - d.snareTone * 0.4f) * 0.8f;
                endT = d.snareDecay * 1.25f;
                break;
            }
            case DSnare2:
            {
                const float f = 240.0f * std::exp2 ((d.snareTune + 7.0f * std::exp (-tq / 0.03f)) / 12.0f);
                out = tone (chipId, f, sr, d.authentic) * q (env (d.snareDecay * 0.8f)) * 0.8f
                    + noise (chipId, 9000.0, d.noiseShort, sr) * q (env (d.snareDecay * 0.6f)) * 0.4f;
                endT = d.snareDecay * 1.1f;
                break;
            }
            case DClap:
            {
                float e;
                if (tq < 0.036f) { const float m = std::fmod (tq, 0.012f); e = m < 0.009f ? std::exp (-m / 0.005f) : 0.0f; }
                else             e = 0.9f * std::exp (-(tq - 0.036f) / 0.04f);
                out = highpass (noise (chipId, 25000.0, false, sr)) * q (e) * 0.9f;
                endT = 0.3f;
                break;
            }
            case DTom:
            {
                static const float base[6] = { 82.0f, 104.0f, 131.0f, 165.0f, 208.0f, 262.0f };
                const float f = base[juce::jlimit (0, 5, tomIdx)] * std::exp2 ((d.tomTune + 5.0f * std::exp (-tq / 0.05f)) / 12.0f);
                out = tone (chipId, f, sr, d.authentic) * q (env (0.32f))
                    + noise (chipId, 15000.0, false, sr) * q (env (0.005f)) * 0.2f;
                endT = 0.4f;
                break;
            }
            case DClosedHat:
            {
                const float dec = (note == 44) ? d.hatDecay * 0.6f : d.hatDecay;
                out = highpass (noise (chipId, 111000.0, d.noiseShort, sr)) * q (env (dec)) * 0.75f;
                endT = dec * 1.3f;
                break;
            }
            case DOpenHat:
            {
                out = highpass (noise (chipId, 111000.0, d.noiseShort, sr)) * q (env (d.openDecay)) * 0.7f;
                endT = d.openDecay * 1.3f;
                break;
            }
            case DCrash:
            {
                out = highpass (noise (chipId, 80000.0, d.noiseShort, sr)) * q (env (1.2f)) * 0.6f;
                endT = 1.4f;
                break;
            }
            case DRide:
            {
                phase  += 3300.0 / sr;  if (phase  >= 1.0) phase  -= 1.0;
                phase2 += 5100.0 / sr;  if (phase2 >= 1.0) phase2 -= 1.0;
                out = (highpass (noise (chipId, 90000.0, d.noiseShort, sr)) * 0.3f
                       + square (phase) * 0.18f + square (phase2) * 0.14f) * q (env (0.7f));
                endT = 0.85f;
                break;
            }
            case DCowbell:
            {
                phase  += 540.0 / sr;  if (phase  >= 1.0) phase  -= 1.0;
                phase2 += 810.0 / sr;  if (phase2 >= 1.0) phase2 -= 1.0;
                out = (square (phase) + square (phase2)) * 0.28f * q (env (0.25f));
                endT = 0.3f;
                break;
            }
            default: break;
        }

        if (choke)
        {
            chokeT += dt;
            const float g = 1.0f - chokeT / 0.004f;
            if (g <= 0.0f) { active = false; return 0.0f; }
            out *= g;
        }
        if (tt > endT) active = false;
        return out * d.volume;
    }
};
} // namespace chip
