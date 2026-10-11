#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

// موتور صدای چیپ‌ها: NES / Game Boy / Master System / C64 SID
namespace chip
{
enum Chip  { NES = 0, GameBoy, SMS, SID };
enum Wave  { Pulse = 0, Triangle, Saw, Noise, Wavetable };
enum Stage { Idle, Attack, Decay, Sustain, Release };

struct Params
{
    int   chip = 0, wave = 0, gbWave = 0, arpPattern = 0, polyphony = 4;
    float pulseWidth = 0.5f;
    bool  noiseShort = false, smooth = true;
    float attack = 0.001f, decay = 0.15f, sustain = 0.7f, release = 0.06f;
    float vibRate = 5.f, vibDepth = 0.f;      // depth in cents
    float slideSemis = 0.f, slideTime = 0.1f;
    float arpRate = 30.f;
    float velSens = 0.5f, master = 0.6f;
};

// ---------- helpers ----------
inline float noteToHz (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

// فرکانس رو به رجیستر واقعی هر چیپ گرد می‌کنه (کوک اصیل)
inline float quantizeFreq (int chipId, int wave, float f)
{
    f = juce::jlimit (8.0f, 20000.0f, f);
    switch (chipId)
    {
        case NES:
        {
            const double cpu = 1789773.0, div = (wave == Triangle) ? 32.0 : 16.0;
            double t = juce::jlimit (0.0, 2047.0, std::round (cpu / (div * f)) - 1.0);
            return (float) (cpu / (div * (t + 1.0)));
        }
        case GameBoy:
        {
            const double base = (wave == Wavetable) ? 65536.0 : 131072.0;
            double x = juce::jlimit (0.0, 2047.0, std::round (2048.0 - base / f));
            return (float) (base / (2048.0 - x));
        }
        case SMS:
        {
            double n = juce::jlimit (1.0, 1023.0, std::round (3579545.0 / (32.0 * f)));
            return (float) (3579545.0 / (32.0 * n));
        }
        default: // SID (PAL clock)
        {
            double reg = juce::jlimit (1.0, 65535.0, std::round (f * 16777216.0 / 985248.0));
            return (float) (reg * 985248.0 / 16777216.0);
        }
    }
}

// سرعت کلاک نویز (نویز پیچ‌دار: ۱۶ برابر فرکانس نوت)
inline float noiseClock (int chipId, float f)
{
    const float target = f * 16.0f;
    if (chipId == NES)
    {
        static const int periods[16] = { 4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068 };
        int best = 0; float bestErr = 1e9f;
        for (int i = 0; i < 16; ++i)
        {
            const float c = 1789773.0f / (float) periods[i];
            const float err = std::abs (std::log2 (c / target));
            if (err < bestErr) { bestErr = err; best = i; }
        }
        return 1789773.0f / (float) periods[best];
    }
    return juce::jlimit (100.0f, 500000.0f, target);
}

inline float pulseWidthFor (int chipId, float pw)
{
    if (chipId == SMS) return 0.5f;
    if (chipId == SID) return pw;
    static const float d[4] = { 0.125f, 0.25f, 0.5f, 0.75f };
    float best = d[0];
    for (float v : d) if (std::abs (v - pw) < std::abs (best - pw)) best = v;
    return best;
}

// هر چیپ چه شکل‌موج‌هایی داره
inline int effectiveWave (int chipId, int wave)
{
    switch (chipId)
    {
        case NES:      return (wave == Pulse || wave == Noise) ? wave : Triangle;
        case GameBoy:  return (wave == Pulse || wave == Noise) ? wave : Wavetable;
        case SMS:      return (wave == Noise) ? Noise : Pulse;
        default:       return (wave == Wavetable) ? Triangle : wave;
    }
}

inline float gbWaveSample (int type, float phase)
{
    const int i = (int) (phase * 32.0f) & 31;
    float v;
    switch (type)
    {
        case 0:  v = (i < 16) ? (float) i / 15.0f : (float) (31 - i) / 15.0f; break;
        case 1:  v = (float) i / 31.0f; break;
        case 2:  v = 0.5f + 0.5f * std::sin ((float) i * juce::MathConstants<float>::twoPi / 32.0f); break;
        default: v = (i < 8) ? 1.0f : 0.0f; break;
    }
    return std::floor (v * 15.0f + 0.5f) / 15.0f;
}

// کوانتایز کردن ولوم مثل DAC هر چیپ
inline float quantizeLevel (int chipId, float a)
{
    a = juce::jlimit (0.0f, 1.0f, a);
    switch (chipId)
    {
        case NES: case GameBoy: return std::round (a * 15.0f) / 15.0f;
        case SMS:
        {
            if (a < 0.001f) return 0.0f;
            const int att = (int) std::round (-20.0f * std::log10 (a) / 2.0f);   // قدم‌های ۲dB
            return att >= 15 ? 0.0f : std::pow (10.0f, -(float) att * 2.0f / 20.0f);
        }
        default: return std::round (a * 255.0f) / 255.0f;
    }
}

struct ArpPattern { int n; int s[4]; };
inline const ArpPattern& arpPattern (int idx)
{
    static const ArpPattern t[] = { {1,{0,0,0,0}}, {4,{0,4,7,12}}, {4,{0,3,7,12}},
                                    {2,{0,12,0,0}}, {3,{0,7,12,0}}, {4,{0,3,6,9}} };
    return t[juce::jlimit (0, 5, idx)];
}

// ---------- LFSR نویز (هر چیپ فرمول خودش) ----------
struct Lfsr
{
    uint32_t reg = 1;

    void reset (int chipId)
    {
        reg = (chipId == NES) ? 1u : (chipId == GameBoy) ? 0x7FFFu : (chipId == SMS) ? 0x8000u : 0x7FFFF8u;
    }

    void step (int chipId, bool shortMode)
    {
        switch (chipId)
        {
            case NES:
            {
                const uint32_t fb = (reg & 1u) ^ ((reg >> (shortMode ? 6 : 1)) & 1u);
                reg = (reg >> 1) | (fb << 14);
                break;
            }
            case GameBoy:
            {
                const uint32_t fb = (reg & 1u) ^ ((reg >> 1) & 1u);
                reg = (reg >> 1) | (fb << 14);
                if (shortMode) reg = (reg & ~(1u << 6)) | (fb << 6);
                break;
            }
            case SMS:
            {
                const uint32_t fb = shortMode ? (reg & 1u) : ((reg & 1u) ^ ((reg >> 3) & 1u));
                reg = (reg >> 1) | (fb << 15);
                break;
            }
            default:
            {
                const uint32_t fb = ((reg >> 22) ^ (reg >> 17)) & 1u;
                reg = ((reg << 1) & 0x7FFFFFu) | fb;
                break;
            }
        }
        if (reg == 0) reg = 1;
    }

    float output (int chipId) const
    {
        switch (chipId)
        {
            case NES: case GameBoy: return (reg & 1u) ? 0.0f : 1.0f;
            case SMS:               return (float) (reg & 1u);
            default:
            {
                const uint32_t v = (((reg >> 20) & 1u) << 7) | (((reg >> 18) & 1u) << 6) | (((reg >> 14) & 1u) << 5)
                                 | (((reg >> 11) & 1u) << 4) | (((reg >> 9) & 1u) << 3)  | (((reg >> 5) & 1u) << 2)
                                 | (((reg >> 4) & 1u) << 1)  | ((reg >> 2) & 1u);
                return (float) v / 255.0f;
            }
        }
    }
};

// ---------- Voice ----------
struct Voice
{
    Stage    stage = Idle;
    int      note = 60;
    float    velocity = 1.0f;
    uint64_t age = 0;

    double phase = 0.0, lfsrAcc = 0.0, timeSec = 0.0, vibPhase = 0.0;
    Lfsr   lfsr;
    float  env = 0.0f, relRate = 0.0f, slideCur = 0.0f;

    void noteOn (int n, float vel, const Params& p, uint64_t order)
    {
        note = n; velocity = vel; age = order;
        stage = Attack; env = 0.0f;
        phase = 0.0; lfsrAcc = 0.0; timeSec = 0.0; vibPhase = 0.0;
        slideCur = p.slideSemis;
        lfsr.reset (p.chip);
    }

    void noteOff (const Params& p)
    {
        if (stage == Idle || stage == Release) return;
        if (env <= 1.0e-4f) { stage = Idle; return; }
        relRate = env / std::max (p.release, 0.001f);
        stage = Release;
    }

    float oscSample (const Params& p, int eff, int tbl) const
    {
        switch (eff)
        {
            case Pulse:
                return phase < (double) pulseWidthFor (p.chip, p.pulseWidth) ? 1.0f : 0.0f;
            case Triangle:
            {
                if (p.chip == NES)
                {
                    const int i = (int) (phase * 32.0) & 31;
                    return (float) (i < 16 ? 15 - i : i - 16) / 15.0f;
                }
                const float t = phase < 0.5 ? (float) phase * 2.0f : 2.0f - (float) phase * 2.0f;
                return std::floor (t * 255.0f + 0.5f) / 255.0f;
            }
            case Saw:
                return std::floor ((float) phase * 256.0f) / 255.0f;
            default: // Wavetable (Game Boy)
            {
                return gbWaveSample (tbl, (float) phase);
            }
        }
    }

    float render (const Params& p, double sr, float bendSemis)
    {
        if (stage == Idle) return 0.0f;
        const float dt = (float) (1.0 / sr);

        switch (stage)
        {
            case Attack:
                env += dt / std::max (p.attack, 0.0005f);
                if (env >= 1.0f) { env = 1.0f; stage = Decay; }
                break;
            case Decay:
                env -= dt * (1.0f - p.sustain) / std::max (p.decay, 0.001f);
                if (env <= p.sustain) { env = p.sustain; stage = Sustain; }
                break;
            case Sustain: env = p.sustain; break;
            case Release:
                env -= dt * relRate;
                if (env <= 0.0f) { env = 0.0f; stage = Idle; return 0.0f; }
                break;
            default: break;
        }

        timeSec += dt;

        float arpSemi = 0.0f;
        if (p.arpPattern > 0)
        {
            const auto& pat = arpPattern (p.arpPattern);
            arpSemi = (float) pat.s[(int) (timeSec * p.arpRate) % pat.n];
        }

        vibPhase += (double) p.vibRate * dt;
        if (vibPhase >= 1.0) vibPhase -= 1.0;
        const float vibSemi = std::sin ((float) vibPhase * juce::MathConstants<float>::twoPi) * p.vibDepth / 100.0f;

        if (slideCur != 0.0f)
        {
            const float stepAmt = std::abs (p.slideSemis) / std::max (p.slideTime, 0.001f) * dt;
            if (std::abs (slideCur) <= stepAmt) slideCur = 0.0f;
            else slideCur -= std::copysign (stepAmt, slideCur);
        }

        const int eff = effectiveWave (p.chip, p.wave);
        const float rawHz = noteToHz ((float) note + arpSemi + vibSemi + slideCur + bendSemis);

        const int os = p.smooth ? 4 : 1;
        const double dtOsc = 1.0 / (sr * (double) os);
        float sum = 0.0f;

        if (eff == Noise)
        {
            const double clk = (double) noiseClock (p.chip, rawHz);
            for (int k = 0; k < os; ++k)
            {
                lfsrAcc += clk * dtOsc;
                while (lfsrAcc >= 1.0) { lfsrAcc -= 1.0; lfsr.step (p.chip, p.noiseShort); }
                sum += lfsr.output (p.chip);
            }
        }
        else
        {
            const double f = (double) quantizeFreq (p.chip, eff, rawHz);
            int tbl = p.gbWave;
            if (p.chip == GameBoy) { if (p.wave == Triangle) tbl = 0; else if (p.wave == Saw) tbl = 1; }
            for (int k = 0; k < os; ++k)
            {
                phase += f * dtOsc;
                if (phase >= 1.0) phase -= std::floor (phase);
                sum += oscSample (p, eff, tbl);
            }
        }

        const float wave = (sum / (float) os) * 2.0f - 1.0f;
        const float vel  = (1.0f - p.velSens) + p.velSens * velocity;
        return wave * quantizeLevel (p.chip, env * vel);
    }
};
} // namespace chip
