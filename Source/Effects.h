#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// کورس + دیلی پینگ‌پونگ
namespace chip
{
struct FxParams
{
    float chorusMix = 0.0f, chorusRate = 0.8f, chorusDepth = 3.0f;
    float delayTime = 300.0f, delayFb = 0.35f, delayMix = 0.0f;
};

class DelayLine
{
public:
    void init (int n) { buf.assign ((size_t) std::max (n, 8), 0.0f); w = 0; }

    void push (float x)
    {
        buf[(size_t) w] = x;
        if (++w >= (int) buf.size()) w = 0;
    }

    float read (float d) const
    {
        const int n = (int) buf.size();
        d = std::min (std::max (d, 1.0f), (float) (n - 2));
        float rp = (float) w - d;
        if (rp < 0.0f) rp += (float) n;
        int i0 = (int) rp;
        if (i0 >= n) i0 -= n;
        const float fr = rp - (float) i0;
        int i1 = i0 + 1;
        if (i1 >= n) i1 -= n;
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

private:
    std::vector<float> buf;
    int w = 0;
};

class StereoFx
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        chL.init ((int) (sr * 0.05));
        chR.init ((int) (sr * 0.05));
        dL.init ((int) (sr * 1.2) + 8);
        dR.init ((int) (sr * 1.2) + 8);
        lfo = 0.0;
    }

    void process (float in, float& outL, float& outR, const FxParams& p)
    {
        const float twoPi = 6.28318530718f;
        const float ms = 0.001f * (float) sampleRate;

        chL.push (in);
        chR.push (in);
        lfo += (double) p.chorusRate / sampleRate;
        if (lfo >= 1.0) lfo -= 1.0;

        const float dl = (10.0f + p.chorusDepth * std::sin (twoPi * (float) lfo)) * ms;
        const float dr = (10.0f + p.chorusDepth * std::sin (twoPi * (float) (lfo + 0.5))) * ms;
        const float norm = 1.0f / (1.0f + 0.5f * p.chorusMix);
        const float l = (in + p.chorusMix * chL.read (dl)) * norm;
        const float r = (in + p.chorusMix * chR.read (dr)) * norm;

        const float mono = 0.5f * (l + r);
        const float ds = p.delayTime * ms;
        const float el = dL.read (ds), er = dR.read (ds);
        dL.push (mono + er * p.delayFb);
        dR.push (el * p.delayFb);

        outL = l + p.delayMix * el;
        outR = r + p.delayMix * er;
    }

private:
    double sampleRate = 44100.0, lfo = 0.0;
    DelayLine chL, chR, dL, dR;
};

struct CrushParams
{
    float bits = 16.0f, rate = 44100.0f, drive = 0.0f;
    int   type = 0;   // 0 Soft, 1 Hard, 2 Fold
};

// بیت‌کراشر + کاهش سمپل‌ریت + دیستورشن
class Crusher
{
public:
    void prepare (double sr) { sampleRate = sr; acc = 0.0; hold = 0.0f; }

    float process (float x, const CrushParams& p)
    {
        if ((double) p.rate < sampleRate * 0.999)
        {
            acc += (double) p.rate / sampleRate;
            if (acc >= 1.0) { acc -= std::floor (acc); hold = x; }
            x = hold;
        }
        if (p.bits < 15.99f)
        {
            const float levels = std::pow (2.0f, std::max (1.0f, p.bits) - 1.0f);
            x = std::round (std::max (-1.0f, std::min (1.0f, x)) * levels) / levels;
        }
        if (p.drive > 0.001f)
        {
            const float y = x * (1.0f + p.drive * 39.0f);
            if (p.type == 0)      x = std::tanh (y);
            else if (p.type == 1) x = std::max (-1.0f, std::min (1.0f, y));
            else { const float t = 0.25f * y + 0.25f; x = 4.0f * std::abs (t - std::round (t)) - 1.0f; }
        }
        return x;
    }

private:
    double sampleRate = 44100.0, acc = 0.0;
    float  hold = 0.0f;
};
} // namespace chip
