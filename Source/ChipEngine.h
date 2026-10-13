#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

// موتور صدای چیپ‌ها — v0.2
// NES / Game Boy / Master System / C64 SID / Atari POKEY / Atari 2600 TIA / AY-3-8910 / PC Engine
namespace chip
{
enum Chip  { NES = 0, GameBoy, SMS, SID, POKEY, TIA, AY, PCE, NumChips };
enum Wave  { Pulse = 0, Triangle, Saw, Noise, Wavetable };
enum Stage { Idle, Attack, Decay, Sustain, Release };
enum PolyMode { PolyNone = 0, PolyPure, Poly4, Poly5Toggle, Poly5Direct, Poly9, Poly17 };

struct Layer   // لایه‌ی اسیلاتور ۲ و ۳ (لایه‌ی ۱ همون پارامترهای اصلیه)
{
    bool  on = false, noiseShort = false;
    int   chip = 0, wave = 0, gbWave = 0, semis = 0;
    float pw = 0.5f, level = 0.7f, fine = 0.0f;
};
struct ModSlot { int src = 0, dst = 0; float amt = 0.0f; };
struct LfoSet  { int shape = 0; float rate = 2.0f, delay = 0.0f; };

struct Params
{
    int   chip = 0, wave = 0, gbWave = 0, arpPattern = 0, polyphony = 4, filterMode = 0;
    float pulseWidth = 0.5f;
    bool  noiseShort = false, smooth = true, authentic = true;
    float attack = 0.001f, decay = 0.15f, sustain = 0.8f, release = 0.08f;
    float cutoff = 4000.0f, resonance = 0.2f, filterEnv = 0.0f;
    float syncRatio = 1.0f, ringRatio = 0.0f;
    float vibRate = 5.0f, vibDepth = 0.0f;
    int   transpose = 0, vibShape = 0, penvShape = 0;
    float fineTune = 0.0f, bendRange = 2.0f, penvAmt = 0.0f, penvTime = 0.15f, glide = 0.0f, vibDelay = 0.0f;
    float nmPitch = 0.0f, nmFilter = 0.0f, nmRate = 30.0f, nmMix = 0.0f;
    float pwmRate = 4.0f, pwmDepth = 0.0f;
    Layer layer[2];
    float level1 = 1.0f;
    int   unison = 1;
    float uniDetune = 12.0f, uniSpread = 0.6f;
    LfoSet lfo[2];
    float menvAtk = 0.005f, menvDec = 0.5f;
    ModSlot slot[8];
    float slideSemis = 0.0f, slideTime = 0.1f;
    float arpRate = 30.0f;
    float velSens = 0.3f, master = 0.8f;
};

// ---------- helpers ----------
inline float noteToHz (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

// فرکانس رو به رجیستر واقعی هر چیپ گرد می‌کنه (کوک اصیل)
inline float quantizeFreq (int chipId, int wave, float f, bool authentic)
{
    f = juce::jlimit (8.0f, 20000.0f, f);
    if (! authentic) return f;

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
        case SID:   // PAL clock
        {
            double reg = juce::jlimit (1.0, 65535.0, std::round (f * 16777216.0 / 985248.0));
            return (float) (reg * 985248.0 / 16777216.0);
        }
        case POKEY: // حالت ۱۶ بیتی (کانال‌های لینک‌شده، کلاک 1.79MHz)
        {
            double n = juce::jlimit (0.0, 65535.0, std::round (1789790.0 / (2.0 * f)) - 7.0);
            return (float) (1789790.0 / (2.0 * (n + 7.0)));
        }
        case TIA:   // تقسیم‌کننده‌ی ۵ بیتی (فقط ۳۲ پله) روی ۳ کلاک پایه
        {
            static const double bases[3] = { 15700.0, 5233.3, 506.5 };
            double best = f, bestErr = 1.0e9;
            for (double b : bases)
            {
                const double n  = juce::jlimit (0.0, 31.0, std::round (b / f) - 1.0);
                const double fq = b / (n + 1.0);
                const double err = std::abs (std::log2 (fq / f));
                if (err < bestErr) { bestErr = err; best = fq; }
            }
            return (float) best;
        }
        case AY:
        {
            if (wave == Triangle || wave == Saw)   // فرکانس انولوپ سخت‌افزاری
            {
                double ep = juce::jlimit (1.0, 65535.0, std::round (1789773.0 / (256.0 * f)));
                return (float) (1789773.0 / (256.0 * ep));
            }
            double tp = juce::jlimit (1.0, 4095.0, std::round (1789773.0 / (16.0 * f)));
            return (float) (1789773.0 / (16.0 * tp));
        }
        default:    // PCE
        {
            double n = juce::jlimit (1.0, 4095.0, std::round (3579545.0 / (32.0 * f)));
            return (float) (3579545.0 / (32.0 * n));
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
        int best = 0; float bestErr = 1.0e9f;
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
    if (chipId == SMS || chipId == AY) return 0.5f;
    if (chipId == SID || chipId == PCE) return pw;
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
        case SID:      return (wave == Wavetable) ? Triangle : wave;
        default:       return wave;   // POKEY / TIA / AY / PCE همه‌ی پنج موج
    }
}

// POKEY و TIA: «دیستورشن» با شمارنده‌های چندجمله‌ای (poly counters)
inline int polyModeFor (int chipId, int wave)
{
    if (chipId != POKEY && chipId != TIA) return PolyNone;
    switch (wave)
    {
        case Pulse:    return PolyPure;
        case Triangle: return Poly4;
        case Saw:      return Poly5Toggle;
        case Noise:    return chipId == POKEY ? Poly17 : Poly9;
        default:       return chipId == POKEY ? Poly9 : Poly5Direct;
    }
}

inline uint32_t stepPoly (uint32_t r, int bits, int tap)
{
    const uint32_t fb = (r ^ (r >> tap)) & 1u;
    return (r >> 1) | (fb << (bits - 1));
}

inline float gbWaveSample (int type, float phase, float levels)
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
    return std::floor (v * levels + 0.5f) / levels;
}

inline float ayLevel (int i) { return i <= 0 ? 0.0f : std::pow (2.0f, -(float) (15 - i) * 0.5f); }

// کوانتایز کردن ولوم مثل DAC هر چیپ
inline float quantizeLevel (int chipId, float a)
{
    a = juce::jlimit (0.0f, 1.0f, a);
    switch (chipId)
    {
        case NES: case GameBoy: case POKEY: case TIA:
            return std::round (a * 15.0f) / 15.0f;
        case PCE:
            return std::round (a * 31.0f) / 31.0f;
        case SMS:
        {
            if (a < 0.001f) return 0.0f;
            const int att = (int) std::round (-20.0f * std::log10 (a) / 2.0f);   // قدم‌های ۲dB
            return att >= 15 ? 0.0f : std::pow (10.0f, -(float) att * 2.0f / 20.0f);
        }
        case AY:
        {
            if (a < 0.003f) return 0.0f;
            return ayLevel ((int) std::round (15.0f + 2.0f * std::log2 (a)));   // قدم‌های ۳dB
        }
        default: return std::round (a * 255.0f) / 255.0f;
    }
}

struct ArpPattern { int n; int s[8]; };
inline const ArpPattern& arpPattern (int idx)
{
    static const ArpPattern t[] = {
        {1,{0,0,0,0,0,0,0,0}},          // Off
        {4,{0,4,7,12,0,0,0,0}},         // Major
        {4,{0,3,7,12,0,0,0,0}},         // Minor
        {2,{0,12,0,0,0,0,0,0}},         // Octave
        {3,{0,7,12,0,0,0,0,0}},         // Power
        {4,{0,3,6,9,0,0,0,0}},          // Diminished
        {4,{0,4,7,11,0,0,0,0}},         // Maj7
        {4,{0,3,7,10,0,0,0,0}},         // Min7
        {4,{0,5,7,12,0,0,0,0}},         // Sus4
        {3,{0,12,24,0,0,0,0,0}},        // 3 Octaves
        {6,{0,3,5,7,10,12,0,0}},        // Minor Pentatonic
        {6,{0,2,4,6,8,10,0,0}},         // Whole-tone
        {6,{0,4,7,12,7,4,0,0}}          // Major Up-Down
    };
    return t[juce::jlimit (0, 12, idx)];
}

// ---------- LFSR نویز (هر چیپ فرمول خودش) ----------
struct Lfsr
{
    uint32_t reg = 1;

    void reset (int chipId)
    {
        switch (chipId)
        {
            case GameBoy: case PCE: reg = 0x7FFFu; break;
            case SMS:               reg = 0x8000u; break;
            case SID:               reg = 0x7FFFF8u; break;
            default:                reg = 1u; break;
        }
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
            case GameBoy: case PCE:
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
            case AY:
            {
                const uint32_t fb = (reg & 1u) ^ ((reg >> 3) & 1u);
                reg = (reg >> 1) | (fb << 16);
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
            case NES: case GameBoy: case PCE: return (reg & 1u) ? 0.0f : 1.0f;
            case SMS: case AY:                return (float) (reg & 1u);
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

// فیلتر state-variable (TPT)
struct Svf
{
    float s1 = 0.0f, s2 = 0.0f;
    void reset() { s1 = s2 = 0.0f; }
    void process (float x, float g, float k, float& lp, float& bp, float& hp)
    {
        hp = (x - (k + g) * s1 - s2) / (1.0f + g * (k + g));
        bp = g * hp + s1;
        s1 = g * hp + bp;
        lp = g * bp + s2;
        s2 = g * bp + lp;
    }
};

// ---------- LFO ----------
inline float lfoShape (int shape, float ph, float sh)
{
    switch (shape)
    {
        case 0:  return std::sin (ph * juce::MathConstants<float>::twoPi);
        case 1:  return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
        case 2:  return ph < 0.5f ? 1.0f : -1.0f;
        case 3:  return 2.0f * ph - 1.0f;
        default: return sh;
    }
}

// ---------- یک اسیلاتور (برای هر لایه / هر یونیسون) ----------
struct OscState
{
    double phase = 0.0, mphase = 0.0, rphase = 0.0, lfsrAcc = 0.0, tickAcc = 0.0;
    Lfsr   lfsr;
    uint32_t p4 = 0xFu, p5 = 0x1Fu, p9 = 0x1FFu, p17 = 0x1FFFFu;
    int    tog = 0;
    float  tickOut = 0.0f;

    void reset (int chipId)
    {
        phase = mphase = rphase = lfsrAcc = tickAcc = 0.0;
        lfsr.reset (chipId);
        p4 = 0xFu; p5 = 0x1Fu; p9 = 0x1FFu; p17 = 0x1FFFFu; tog = 0; tickOut = 0.0f;
    }

    void tick (int mode)
    {
        switch (mode)
        {
            case PolyPure:     tog ^= 1; tickOut = (float) tog; break;
            case Poly4:        p4 = stepPoly (p4, 4, 1);   tickOut = (float) (p4 & 1u); break;
            case Poly5Toggle:  p5 = stepPoly (p5, 5, 2);   if (p5 & 1u) tog ^= 1; tickOut = (float) tog; break;
            case Poly5Direct:  p5 = stepPoly (p5, 5, 2);   tickOut = (float) (p5 & 1u); break;
            case Poly9:        p9 = stepPoly (p9, 9, 4);   tickOut = (float) (p9 & 1u); break;
            default:           p17 = stepPoly (p17, 17, 3); tickOut = (float) (p17 & 1u); break;
        }
    }

    float oscSample (int chipId, int eff, int tbl, float pw) const
    {
        if (chipId == AY)
        {
            if (eff == Pulse)    return phase < 0.5 ? 1.0f : 0.0f;
            if (eff == Triangle) { const int i = (int) (phase * 32.0) & 31; return ayLevel (i < 16 ? i : 31 - i); }
            if (eff == Saw)      return ayLevel (15 - ((int) (phase * 16.0) & 15));
            return 0.0f;
        }

        switch (eff)
        {
            case Pulse:
                return phase < (double) pulseWidthFor (chipId, pw) ? 1.0f : 0.0f;
            case Triangle:
            {
                if (chipId == NES)
                {
                    const int i = (int) (phase * 32.0) & 31;
                    return (float) (i < 16 ? 15 - i : i - 16) / 15.0f;
                }
                if (chipId == PCE) return gbWaveSample (0, (float) phase, 31.0f);
                const float t = phase < 0.5 ? (float) phase * 2.0f : 2.0f - (float) phase * 2.0f;
                return std::floor (t * 255.0f + 0.5f) / 255.0f;
            }
            case Saw:
            {
                if (chipId == PCE) return gbWaveSample (1, (float) phase, 31.0f);
                return std::floor ((float) phase * 256.0f) / 255.0f;
            }
            default:
                return gbWaveSample (tbl, (float) phase, chipId == PCE ? 31.0f : 15.0f);
        }
    }

    // خروجی ۰..۱ (میانگین روی oversampling)
    float generate (int chipId, int wave, int gbWave, float pw, bool noiseShort, const Params& p,
                    double sr, float rawHz, float syncRatio, float ringRatio)
    {
        const int eff = effectiveWave (chipId, wave);
        const int pm  = polyModeFor (chipId, wave);
        const int    os    = p.smooth ? 4 : 1;
        const double dtOsc = 1.0 / (sr * (double) os);
        float sum = 0.0f;

        if (pm != PolyNone)
        {
            double rate;
            if (pm == Poly9 || pm == Poly17)
                rate = (double) noiseClock (chipId, rawHz);
            else
            {
                const double fq = (double) quantizeFreq (chipId, eff, rawHz, p.authentic);
                rate = (pm == PolyPure) ? 2.0 * fq : (pm == Poly4) ? 15.0 * fq : 31.0 * fq;
            }
            for (int k = 0; k < os; ++k)
            {
                tickAcc += rate * dtOsc;
                while (tickAcc >= 1.0) { tickAcc -= 1.0; tick (pm); }
                sum += tickOut;
            }
        }
        else if (eff == Noise)
        {
            const double clk = (double) noiseClock (chipId, rawHz);
            for (int k = 0; k < os; ++k)
            {
                lfsrAcc += clk * dtOsc;
                while (lfsrAcc >= 1.0) { lfsrAcc -= 1.0; lfsr.step (chipId, noiseShort); }
                sum += lfsr.output (chipId);
            }
        }
        else
        {
            const double f = (double) quantizeFreq (chipId, eff, rawHz, p.authentic);
            const bool syncOn = syncRatio > 1.001f;
            const bool ringOn = ringRatio > 0.02f;
            const bool ayMix  = (chipId == AY && eff == Wavetable);   // تون AND نویز
            const double carrierF = f * (syncOn ? (double) syncRatio : 1.0);
            const double clk = (double) noiseClock (chipId, rawHz);

            int tbl = gbWave;
            if (chipId == GameBoy) { if (wave == Triangle) tbl = 0; else if (wave == Saw) tbl = 1; }

            for (int k = 0; k < os; ++k)
            {
                mphase += f * dtOsc;
                if (mphase >= 1.0) { mphase -= std::floor (mphase); if (syncOn) phase = 0.0; }
                phase += carrierF * dtOsc;
                if (phase >= 1.0) phase -= std::floor (phase);

                float v;
                if (ayMix)
                {
                    lfsrAcc += clk * dtOsc;
                    while (lfsrAcc >= 1.0) { lfsrAcc -= 1.0; lfsr.step (chipId, false); }
                    v = (phase < 0.5 ? 1.0f : 0.0f) * lfsr.output (chipId);
                }
                else
                    v = oscSample (chipId, eff, tbl, pw);

                if (ringOn)
                {
                    rphase += f * (double) ringRatio * dtOsc;
                    if (rphase >= 1.0) rphase -= std::floor (rphase);
                    if (rphase >= 0.5) v = 1.0f - v;
                }
                sum += v;
            }
        }
        return sum / (float) os;
    }
};

// ---------- Voice ----------
// ماتریکس: منبع ۰ None،۱ LFO1،۲ LFO2،۳ ModEnv،۴ Noise S&H،۵ Velocity،۶ Random per note،۷ KeyTrack
//          مقصد ۰ None،۱ Pitch(±24st)،۲ Cutoff(±4oct)،۳ Reso،۴ PW،۵ Level(منفی=کم‌کردن)،
//               ۶ Sync(±4)،۷ Ring(±2)،۸ NoiseMix،۹ Layer2 level،۱۰ Layer3 level
struct Voice
{
    Stage    stage = Idle;
    int      note = 60;
    float    velocity = 1.0f;
    uint64_t age = 0;

    OscState osc1[5], osc2, osc3;
    Svf      svfL, svfR;
    double timeSec = 0.0, vibPhase = 0.0, nAcc = 0.0, pwmPhase = 0.0, lfoPh[2] = { 0.0, 0.0 };
    float  pwNow = 0.5f;
    float  env = 0.0f, relRate = 0.0f, slideCur = 0.0f, glideCur = 0.0f, vibSH = 0.0f, nVal = 0.0f;
    float  lfoSH[2] = { 0.0f, 0.0f }, menv = 0.0f, noteRand = 0.0f;
    int    menvStage = 2;
    uint32_t rng = 2463534242u;

    float rnd()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng >> 8) * (1.0f / 16777216.0f);   // [0,1)
    }

    void noteOn (int n, float vel, const Params& p, uint64_t order)
    {
        note = n; velocity = vel; age = order;
        stage = Attack; env = 0.0f;
        timeSec = 0.0; vibPhase = 0.0; nAcc = 0.0; pwmPhase = 0.0; pwNow = p.pulseWidth;
        lfoPh[0] = lfoPh[1] = 0.0; lfoSH[0] = lfoSH[1] = 0.0f;
        menv = 0.0f; menvStage = 0;
        slideCur = p.slideSemis; glideCur = 0.0f; vibSH = 0.0f; nVal = 0.0f;
        rng = 2463534242u ^ (uint32_t) (order * 2654435761ull);
        if (rng == 0) rng = 1u;
        noteRand = rnd() * 2.0f - 1.0f;
        for (auto& o : osc1) o.reset (p.chip);
        osc2.reset (p.layer[0].chip);
        osc3.reset (p.layer[1].chip);
        svfL.reset(); svfR.reset();
    }

    void noteOff (const Params& p)
    {
        if (stage == Idle || stage == Release) return;
        if (env <= 1.0e-4f) { stage = Idle; return; }
        relRate = env / std::max (p.release, 0.001f);
        stage = Release;
    }

    // نسخه‌ی مونو (برای تست‌ها)
    float render (const Params& p, double sr, float bendSemis)
    {
        float l, r;
        renderStereo (p, sr, bendSemis, l, r);
        return 0.5f * (l + r);
    }

    void renderStereo (const Params& p, double sr, float bendSemis, float& outL, float& outR)
    {
        outL = outR = 0.0f;
        if (stage == Idle) return;
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
                if (env <= 0.0f) { env = 0.0f; stage = Idle; return; }
                break;
            default: break;
        }

        timeSec += dt;

        // --- Chip arp (فریم‌ریتی)
        float arpSemi = 0.0f;
        if (p.arpPattern > 0)
        {
            const auto& pat = arpPattern (p.arpPattern);
            arpSemi = (float) pat.s[(int) (timeSec * p.arpRate) % pat.n];
        }

        // --- ویبراتو
        vibPhase += (double) p.vibRate * dt;
        if (vibPhase >= 1.0) { vibPhase -= 1.0; vibSH = rnd() * 2.0f - 1.0f; }
        const float vibFade = p.vibDelay > 0.001f ? std::min (1.0f, (float) timeSec / p.vibDelay) : 1.0f;
        const float vibSemi = lfoShape (p.vibShape, (float) vibPhase, vibSH) * p.vibDepth / 100.0f * vibFade;

        // --- اسلاید
        if (slideCur != 0.0f)
        {
            const float stepAmt = std::abs (p.slideSemis) / std::max (p.slideTime, 0.001f) * dt;
            if (std::abs (slideCur) <= stepAmt) slideCur = 0.0f;
            else slideCur -= std::copysign (stepAmt, slideCur);
        }

        // --- گلاید
        if (glideCur != 0.0f)
        {
            glideCur *= std::exp (-dt / std::max (p.glide * 0.35f, 0.001f));
            if (std::abs (glideCur) < 1.0e-3f) glideCur = 0.0f;
        }

        // --- انولوپ پیچ
        float penvSemi = 0.0f;
        if (std::abs (p.penvAmt) > 0.001f)
        {
            const float t = (float) timeSec / std::max (p.penvTime, 0.002f);
            const float e = (p.penvShape == 1) ? std::max (0.0f, 1.0f - t) : std::exp (-5.0f * t);
            penvSemi = p.penvAmt * e;
            if (p.penvShape == 2) penvSemi = std::round (penvSemi);
        }

        // --- نویز S&H
        nAcc += (double) p.nmRate * dt;
        if (nAcc >= 1.0) { nAcc -= std::floor (nAcc); nVal = rnd() * 2.0f - 1.0f; }

        // --- PWM
        pwmPhase += (double) p.pwmRate * dt;
        if (pwmPhase >= 1.0) pwmPhase -= 1.0;
        const float pwmOff = std::sin ((float) pwmPhase * juce::MathConstants<float>::twoPi) * p.pwmDepth * 0.4f;

        // --- LFO ها و ModEnv و ماتریکس
        float src[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        for (int i = 0; i < 2; ++i)
        {
            lfoPh[i] += (double) p.lfo[i].rate * dt;
            if (lfoPh[i] >= 1.0) { lfoPh[i] -= 1.0; lfoSH[i] = rnd() * 2.0f - 1.0f; }
            const float fade = p.lfo[i].delay > 0.001f ? std::min (1.0f, (float) timeSec / p.lfo[i].delay) : 1.0f;
            src[1 + i] = lfoShape (p.lfo[i].shape, (float) lfoPh[i], lfoSH[i]) * fade;
        }
        if (menvStage == 0)      { menv += dt / std::max (p.menvAtk, 0.0005f); if (menv >= 1.0f) { menv = 1.0f; menvStage = 1; } }
        else if (menvStage == 1) { menv -= dt / std::max (p.menvDec, 0.001f);  if (menv <= 0.0f) { menv = 0.0f; menvStage = 2; } }
        src[3] = menv; src[4] = nVal; src[5] = velocity; src[6] = noteRand; src[7] = ((float) note - 60.0f) / 36.0f;

        float m[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        for (const auto& sl : p.slot)
            if (sl.src > 0 && sl.src < 8 && sl.dst > 0 && sl.dst < 11) m[sl.dst] += src[sl.src] * sl.amt;

        const float semisBase = (float) note + (float) p.transpose + p.fineTune / 100.0f + arpSemi + vibSemi
                              + slideCur + glideCur + penvSemi + nVal * p.nmPitch + bendSemis + m[1] * 24.0f;
        const float pwOff = pwmOff + m[4] * 0.45f;
        pwNow = juce::jlimit (0.05f, 0.95f, p.pulseWidth + pwOff);
        const float sync = juce::jlimit (1.0f, 8.0f, p.syncRatio + m[6] * 4.0f);
        const float ring = juce::jlimit (0.0f, 4.0f, p.ringRatio + m[7] * 2.0f);

        // --- لایه ۱ + Unison (استریو)
        float L = 0.0f, R = 0.0f;
        const int N = juce::jlimit (1, 5, p.unison);
        const float norm = 1.0f / std::sqrt ((float) N);
        for (int u = 0; u < N; ++u)
        {
            const float pos = (N == 1) ? 0.0f : (2.0f * (float) u / (float) (N - 1) - 1.0f);
            const float hz  = noteToHz (semisBase + pos * p.uniDetune / 100.0f);
            const float v   = (osc1[(size_t) u].generate (p.chip, p.wave, p.gbWave, pwNow, p.noiseShort, p, sr, hz, sync, ring) * 2.0f - 1.0f)
                              * p.level1 * norm;
            const float ang = (pos * p.uniSpread + 1.0f) * 0.78539816f;   // (pan+1)*pi/4
            L += v * std::cos (ang);
            R += v * std::sin (ang);
        }

        // --- لایه ۲ و ۳
        for (int k = 0; k < 2; ++k)
        {
            const auto& ly = p.layer[k];
            if (! ly.on) continue;
            const float level = juce::jlimit (0.0f, 2.0f, ly.level + m[9 + k]);
            if (level <= 0.0f) continue;
            const float hz = noteToHz (semisBase + (float) ly.semis + ly.fine / 100.0f);
            const float lpw = juce::jlimit (0.05f, 0.95f, ly.pw + pwOff);
            auto& o = (k == 0) ? osc2 : osc3;
            const float v = (o.generate (ly.chip, ly.wave, ly.gbWave, lpw, ly.noiseShort, p, sr, hz, 1.0f, 0.0f) * 2.0f - 1.0f) * level;
            L += v; R += v;
        }

        // --- نویز سفید مخلوط
        const float nmix = juce::jlimit (0.0f, 1.0f, p.nmMix + m[8]);
        if (nmix > 0.001f)
        {
            const float nz = (rnd() * 2.0f - 1.0f) * nmix * 0.7f;
            L += nz; R += nz;
        }

        // --- فیلتر (استریو)
        if (p.filterMode > 0)
        {
            float cutoff = p.cutoff * std::exp2 (p.filterEnv * env + p.nmFilter * nVal + m[2] * 4.0f);
            cutoff = juce::jlimit (20.0f, (float) (0.45 * sr), cutoff);
            const float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr);
            const float k = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, p.resonance + m[3]);
            float lp, bp, hp;
            svfL.process (L, g, k, lp, bp, hp);
            L = (p.filterMode == 1) ? lp : (p.filterMode == 2) ? bp * k : hp;
            svfR.process (R, g, k, lp, bp, hp);
            R = (p.filterMode == 1) ? lp : (p.filterMode == 2) ? bp * k : hp;
        }

        const float vel = (1.0f - p.velSens) + p.velSens * velocity;
        const float lvl = quantizeLevel (p.chip, env * vel * juce::jlimit (0.0f, 1.0f, 1.0f + m[5]));
        outL = L * lvl;
        outR = R * lvl;
    }
};
} // namespace chip
