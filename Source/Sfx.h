#pragma once
#include "ChipEngine.h"

// مولد افکت‌های آرکید / آتاری (شلیک، لیزر، انفجار، پاورآپ، ...). با موج و نویز و کوانتایز همون چیپ
namespace chip
{
struct SfxR   // بازه‌ی پارامترها (مشترک بین موتور، پارامترها و مولد تصادفی)
{
    static constexpr float pitchLo = -24.0f, pitchHi = 48.0f, slideLim = 300.0f, accelLim = 1500.0f, stepHi = 240.0f;
    static constexpr float attackLo = 0.001f, attackHi = 0.2f, holdHi = 1.0f, decayLo = 0.02f, decayHi = 2.0f;
    static constexpr float dutyLo = 0.05f, dutyHi = 0.95f, dutySweepLim = 2.0f, vibDepthHi = 12.0f;
    static constexpr float vibRateLo = 0.1f, vibRateHi = 30.0f, jumpLim = 24.0f, jumpTimeHi = 0.5f, retrigHi = 40.0f, volHi = 1.5f;
};

struct SfxParams
{
    int   wave = 0;                                   // 0 Pulse، 1 Triangle، 2 Saw، 3 Noise
    float pitch = 18.0f, key = 1.0f;                  // pitch: نیم‌پرده بالاتر از C4؛ key: چقدر کلید روی پیچ اثر بذاره
    float slide = -170.0f, accel = 0.0f, step = 60.0f; // نیم‌پرده بر ثانیه، شتاب، آپدیت پله‌ای پیچ (۰ = پیوسته)
    float attack = 0.001f, hold = 0.01f, decay = 0.16f;
    float duty = 0.5f, dutySweep = 0.0f;
    float vibDepth = 0.0f, vibRate = 8.0f, jump = 0.0f, jumpTime = 0.05f;
    float noise = 0.0f, retrig = 0.0f, vol = 1.0f;
};

struct SfxVoice
{
    bool     active = false, held = false;
    int      note = 60;
    float    vel = 1.0f, heldSem = 0.0f;
    uint64_t age = 0;
    double   t = 0.0, vibPhase = 0.0, stepAcc = 1.0;
    OscState osc, nosc;

    void trigger (int n, float v, const Params& p, uint64_t order)
    {
        note = n; vel = v; age = order;
        active = true; held = true;
        t = 0.0; vibPhase = 0.0; stepAcc = 1.0; heldSem = 0.0f;
        osc.reset (p.chip);
        nosc.reset (p.chip);
    }

    void noteOff() { held = false; }

    float render (const SfxParams& s, const Params& p, double sr)
    {
        if (! active) return 0.0f;
        const double dt = 1.0 / sr;
        const bool rapid = s.retrig > 0.5f && held;

        if (rapid && t >= 1.0 / (double) s.retrig)      // مسلسل: دوباره از اول
        {
            t = 0.0; stepAcc = 1.0;
            osc.reset (p.chip); nosc.reset (p.chip);
        }
        t += dt;

        const double total = (double) s.attack + (double) s.hold + (double) s.decay * 1.25;
        if (t >= total)
        {
            if (! rapid) { active = false; return 0.0f; }
            return 0.0f;                                  // منتظر ضربه‌ی بعدی
        }

        // --- پیچ (نیم‌پرده)
        float sem = 60.0f + s.pitch + s.key * ((float) note - 60.0f) + (float) ((double) s.slide * t + 0.5 * (double) s.accel * t * t);
        if (std::abs (s.jump) > 0.01f && t >= (double) s.jumpTime) sem += s.jump;
        vibPhase += (double) s.vibRate * dt;
        if (vibPhase >= 1.0) vibPhase -= 1.0;
        sem += std::sin ((float) vibPhase * juce::MathConstants<float>::twoPi) * s.vibDepth;

        if (s.step >= 10.0f)                              // آپدیت پله‌ای مثل فریم کنسول
        {
            stepAcc += (double) s.step * dt;
            if (stepAcc >= 1.0) { stepAcc -= std::floor (stepAcc); heldSem = sem; }
            sem = heldSem;
        }

        float hz = noteToHz (sem);
        if (hz < 22.0f && t > 0.01) { active = false; return 0.0f; }   // مثل sfxr: زیر حداقل فرکانس تموم می‌شه
        hz = juce::jlimit (22.0f, 20000.0f, hz);

        // --- ولوم: اتک، نگه‌داشتن، افت نمایی
        const float tt = (float) t;
        float env;
        if (tt < s.attack)                 env = tt / std::max (s.attack, 0.0005f);
        else if (tt < s.attack + s.hold)   env = 1.0f;
        else                               env = std::exp (-5.0f * (tt - s.attack - s.hold) / std::max (s.decay, 0.01f));

        // --- صدا
        const float pw = juce::jlimit (0.05f, 0.95f, s.duty + s.dutySweep * tt);
        float x = osc.generate (p.chip, s.wave, p.gbWave, pw, p.noiseShort, p, sr, hz, 1.0f, 0.0f) * 2.0f - 1.0f;
        if (s.noise > 0.001f)
        {
            const float nz = nosc.generate (p.chip, Noise, p.gbWave, 0.5f, p.noiseShort, p, sr, hz, 1.0f, 0.0f) * 2.0f - 1.0f;
            x = x * (1.0f - 0.6f * s.noise) + nz * s.noise;
        }
        return x * quantizeLevel (p.chip, env * vel) * s.vol;
    }
};

// ---------- مولد تصادفی (مثل sfxr) ----------
inline float sfxRand01 (uint32_t& rng)
{
    rng = rng * 1664525u + 1013904223u;
    return (float) (rng >> 8) * (1.0f / 16777216.0f);
}

inline void sfxClamp (SfxParams& s)
{
    auto cl = [] (float& v, float lo, float hi) { v = std::max (lo, std::min (hi, v)); };
    s.wave = std::max (0, std::min (3, s.wave));
    cl (s.pitch, SfxR::pitchLo, SfxR::pitchHi);   cl (s.key, 0.0f, 1.0f);
    cl (s.slide, -SfxR::slideLim, SfxR::slideLim); cl (s.accel, -SfxR::accelLim, SfxR::accelLim);
    cl (s.step, 0.0f, SfxR::stepHi);
    cl (s.attack, SfxR::attackLo, SfxR::attackHi); cl (s.hold, 0.0f, SfxR::holdHi); cl (s.decay, SfxR::decayLo, SfxR::decayHi);
    cl (s.duty, SfxR::dutyLo, SfxR::dutyHi);       cl (s.dutySweep, -SfxR::dutySweepLim, SfxR::dutySweepLim);
    cl (s.vibDepth, 0.0f, SfxR::vibDepthHi);       cl (s.vibRate, SfxR::vibRateLo, SfxR::vibRateHi);
    cl (s.jump, -SfxR::jumpLim, SfxR::jumpLim);    cl (s.jumpTime, 0.0f, SfxR::jumpTimeHi);
    cl (s.noise, 0.0f, 1.0f);                      cl (s.retrig, 0.0f, SfxR::retrigHi);
    cl (s.vol, 0.0f, SfxR::volHi);
}

// kind: 0 Shoot، 1 Explosion، 2 Power-up، 3 Hit، 4 Jump، 5 Blip، 6 Random
inline void sfxGenerate (int kind, uint32_t& rng, SfxParams& s)
{
    auto rr = [&] (float a, float b) { return a + (b - a) * sfxRand01 (rng); };
    auto chance = [&] (float p) { return sfxRand01 (rng) < p; };
    s = SfxParams();

    switch (kind)
    {
        case 0:   // شلیک / لیزر
            s.wave = chance (0.7f) ? 0 : 2;
            s.pitch = rr (12.0f, 30.0f);  s.slide = rr (-230.0f, -80.0f);  s.accel = chance (0.3f) ? rr (-300.0f, 200.0f) : 0.0f;
            s.attack = 0.001f;  s.hold = rr (0.0f, 0.04f);  s.decay = rr (0.08f, 0.3f);
            s.duty = chance (0.4f) ? 0.125f : chance (0.5f) ? 0.25f : 0.5f;  s.dutySweep = rr (-0.6f, 0.0f);
            s.noise = chance (0.4f) ? rr (0.0f, 0.15f) : 0.0f;
            s.step = chance (0.5f) ? 60.0f : 0.0f;  s.key = 0.3f;
            break;
        case 1:   // انفجار
            s.wave = 3;
            s.pitch = rr (-12.0f, 6.0f);  s.slide = rr (-40.0f, -5.0f);
            s.hold = rr (0.05f, 0.2f);  s.decay = rr (0.4f, 1.2f);
            s.noise = rr (0.4f, 1.0f);  s.step = chance (0.5f) ? 30.0f : 0.0f;  s.key = 0.2f;
            break;
        case 2:   // پاورآپ
            s.wave = 0;
            s.pitch = rr (0.0f, 12.0f);  s.slide = rr (20.0f, 80.0f);
            s.jump = chance (0.5f) ? 7.0f : 12.0f;  s.jumpTime = rr (0.05f, 0.12f);
            s.vibDepth = rr (0.0f, 0.8f);  s.vibRate = rr (4.0f, 9.0f);
            s.hold = 0.02f;  s.decay = rr (0.3f, 0.6f);  s.duty = rr (0.3f, 0.5f);  s.key = 0.5f;
            break;
        case 3:   // برخورد / آسیب
            s.wave = chance (0.5f) ? 3 : 0;
            s.pitch = rr (0.0f, 12.0f);  s.slide = rr (-150.0f, -50.0f);
            s.decay = rr (0.08f, 0.2f);  s.noise = rr (0.4f, 0.9f);  s.duty = 0.5f;  s.key = 0.3f;
            break;
        case 4:   // پرش
            s.wave = 0;
            s.pitch = rr (0.0f, 6.0f);  s.slide = rr (60.0f, 140.0f);
            s.decay = rr (0.12f, 0.25f);  s.duty = 0.5f;  s.key = 0.5f;
            break;
        case 5:   // بیپ
            s.wave = 0;
            s.pitch = rr (12.0f, 24.0f);  s.slide = 0.0f;
            s.jump = chance (0.5f) ? rr (-12.0f, 12.0f) : 0.0f;  s.jumpTime = rr (0.02f, 0.06f);
            s.decay = rr (0.04f, 0.12f);  s.duty = rr (0.2f, 0.5f);  s.key = 0.6f;
            break;
        default:  // کاملاً تصادفی
            s.wave = (int) (sfxRand01 (rng) * 4.0f);
            s.pitch = rr (-12.0f, 36.0f);  s.slide = rr (-200.0f, 200.0f);  s.accel = chance (0.3f) ? rr (-500.0f, 500.0f) : 0.0f;
            s.step = chance (0.4f) ? rr (20.0f, 90.0f) : 0.0f;
            s.hold = rr (0.0f, 0.15f);  s.decay = rr (0.05f, 0.7f);
            s.duty = rr (0.1f, 0.9f);  s.dutySweep = chance (0.4f) ? rr (-1.0f, 1.0f) : 0.0f;
            s.vibDepth = chance (0.3f) ? rr (0.0f, 6.0f) : 0.0f;  s.vibRate = rr (3.0f, 20.0f);
            s.jump = chance (0.3f) ? rr (-12.0f, 12.0f) : 0.0f;  s.jumpTime = rr (0.03f, 0.2f);
            s.noise = chance (0.4f) ? rr (0.0f, 0.8f) : 0.0f;  s.key = rr (0.0f, 1.0f);
            break;
    }
    sfxClamp (s);
}

// تغییر کوچیک تصادفی روی صدای فعلی
inline void sfxMutate (SfxParams& s, uint32_t& rng, float amount)
{
    auto m = [&] (float& v, float range) { v += (sfxRand01 (rng) * 2.0f - 1.0f) * range * amount; };
    m (s.pitch, SfxR::pitchHi - SfxR::pitchLo);  m (s.slide, SfxR::slideLim * 2.0f);  m (s.accel, SfxR::accelLim * 2.0f);
    m (s.hold, SfxR::holdHi);                    m (s.decay, 0.6f);                   m (s.duty, 0.8f);
    m (s.dutySweep, 2.0f);                       m (s.vibDepth, SfxR::vibDepthHi);    m (s.jump, SfxR::jumpLim * 2.0f);
    m (s.noise, 1.0f);
    sfxClamp (s);
}
} // namespace chip
