#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// سمپلر / حالت DAC: یک سمپل روی کیبورد، با کیفیت چیپی (بیت و سمپل‌ریت پایین)
namespace chip
{
struct SampleData
{
    std::vector<float> data;          // مونو
    double sampleRate = 44100.0;
};

struct SampleParams
{
    int   rootNote = 60, loop = 0;    // loop: 0 one-shot، 1 لوپ
    float start = 0.0f, end = 1.0f, gain = 1.0f;
    float bits = 16.0f, rate = 44100.0f;
    float attack = 0.001f, decay = 0.1f, sustain = 1.0f, release = 0.1f;
};

struct SampleVoice
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage    stage = Idle;
    int      note = 60;
    float    vel = 1.0f, env = 0.0f, relRate = 0.0f, hold = 0.0f;
    double   pos = 0.0, acc = 0.0;
    uint64_t age = 0;

    void trigger (int n, float v, const SampleData* d, const SampleParams& p, uint64_t order)
    {
        if (d == nullptr || d->data.empty()) return;
        note = n; vel = v; age = order;
        pos = (double) p.start * (double) d->data.size();
        acc = 0.0; hold = 0.0f; env = 0.0f;
        stage = Attack;
    }

    void noteOff (const SampleParams& p)
    {
        if (stage == Idle || stage == Release) return;
        relRate = env / std::max (p.release, 0.001f);
        stage = Release;
    }

    float render (const SampleData* d, const SampleParams& p, double sr)
    {
        if (stage == Idle) return 0.0f;
        if (d == nullptr || d->data.empty()) { stage = Idle; return 0.0f; }
        const float dt = (float) (1.0 / sr);

        switch (stage)
        {
            case Attack:  env += dt / std::max (p.attack, 0.0005f); if (env >= 1.0f) { env = 1.0f; stage = Decay; } break;
            case Decay:   env -= dt * (1.0f - p.sustain) / std::max (p.decay, 0.001f); if (env <= p.sustain) { env = p.sustain; stage = Sustain; } break;
            case Sustain: env = p.sustain; break;
            case Release: env -= dt * relRate; if (env <= 0.0f) { env = 0.0f; stage = Idle; return 0.0f; } break;
            default: break;
        }

        const double n = (double) d->data.size();
        const double s0 = std::min (std::max ((double) p.start, 0.0), 0.999) * n;
        const double s1 = std::min (std::max ((double) p.end, (double) p.start + 0.002), 1.0) * n;
        const double ratio = std::pow (2.0, (double) (note - p.rootNote) / 12.0) * d->sampleRate / sr;

        float edge = 1.0f;
        if (p.loop == 0)
        {
            if (pos >= s1 - 1.0) { stage = Idle; return 0.0f; }
            edge = (float) std::min (1.0, (s1 - 1.0 - pos) / std::max (1.0, ratio * 48.0));   // فید کوتاه آخر
        }
        else if (pos >= s1 - 1.0)
            pos = s0 + std::fmod (pos - s0, std::max (2.0, s1 - 1.0 - s0));

        const int i0 = (int) pos;
        const int i1 = std::min (i0 + 1, (int) d->data.size() - 1);
        const float fr = (float) (pos - (double) i0);
        float x = d->data[(size_t) i0] + fr * (d->data[(size_t) i1] - d->data[(size_t) i0]);
        pos += ratio;

        // DAC چیپی: سمپل‌ریت و بیت پایین
        if ((double) p.rate < sr * 0.999)
        {
            acc += (double) p.rate / sr;
            if (acc >= 1.0)
            {
                acc -= std::floor (acc);
                hold = x;
                if (p.bits < 15.99f)
                {
                    const float lv = std::pow (2.0f, std::max (1.0f, p.bits) - 1.0f);
                    hold = std::round (std::max (-1.0f, std::min (1.0f, x)) * lv) / lv;
                }
            }
            x = hold;
        }
        else if (p.bits < 15.99f)
        {
            const float lv = std::pow (2.0f, std::max (1.0f, p.bits) - 1.0f);
            x = std::round (std::max (-1.0f, std::min (1.0f, x)) * lv) / lv;
        }
        return x * env * vel * p.gain * edge;
    }
};
} // namespace chip
