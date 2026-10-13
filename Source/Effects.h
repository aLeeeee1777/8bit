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
        if (rp >= (float) n) rp -= (float) n;
        int i0 = (int) rp;
        if (i0 >= n) i0 = n - 1;
        const float fr = std::min (1.0f, std::max (0.0f, rp - (float) i0));
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

    void process (float in, float& outL, float& outR, const FxParams& p) { processStereo (in, in, outL, outR, p); }

    void processStereo (float inL, float inR, float& outL, float& outR, const FxParams& p)
    {
        const float twoPi = 6.28318530718f;
        const float ms = 0.001f * (float) sampleRate;

        chL.push (inL);
        chR.push (inR);
        lfo += (double) p.chorusRate / sampleRate;
        if (lfo >= 1.0) lfo -= 1.0;

        const float dl = (10.0f + p.chorusDepth * std::sin (twoPi * (float) lfo)) * ms;
        const float dr = (10.0f + p.chorusDepth * std::sin (twoPi * (float) (lfo + 0.5))) * ms;
        const float norm = 1.0f / (1.0f + 0.5f * p.chorusMix);
        const float l = (inL + p.chorusMix * chL.read (dl)) * norm;
        const float r = (inR + p.chorusMix * chR.read (dr)) * norm;

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

struct ReverbParams
{
    float mix = 0.0f, size = 0.5f, decay = 0.6f, damp = 0.5f, predelay = 0.0f, lofi = 0.0f, width = 1.0f;
};

// ریورب استریو (Schroeder/Freeverb) + حالت Lo-Fi: دم ریورب بیت‌کراش و کم‌نمونه می‌شه
class Reverb
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        static const int cT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int aT[4] = { 556, 441, 341, 225 };
        const double k = sr / 44100.0;
        for (int i = 0; i < 8; ++i)
        {
            combBase[i] = (int) (cT[i] * k);
            combL[i].init ((int) (cT[i] * k * 1.55) + 8);
            combR[i].init ((int) ((cT[i] + 23) * k * 1.55) + 8);
        }
        for (int i = 0; i < 4; ++i)
        {
            apL[i].init ((int) (aT[i] * k) + 1);
            apR[i].init ((int) ((aT[i] + 23) * k) + 1);
        }
        pdL.init ((int) (sr * 0.3)); pdR.init ((int) (sr * 0.3));
        crL.prepare (sr); crR.prepare (sr);
        lastSize = -1.0f;
        setSize (0.5f);
    }

    void process (float inL, float inR, float& outL, float& outR, const ReverbParams& p)
    {
        if (p.mix < 0.001f) { outL = inL; outR = inR; return; }
        if (std::abs (p.size - lastSize) > 1.0e-4f) setSize (p.size);

        pdL.push (inL); pdR.push (inR);
        const float pd = p.predelay * 0.001f * (float) sampleRate;
        const float xl = pd > 1.0f ? pdL.read (pd) : inL;
        const float xr = pd > 1.0f ? pdR.read (pd) : inR;
        const float x = (xl + xr) * 0.015f;

        const float fb   = 0.7f + 0.28f * std::max (0.0f, std::min (1.0f, p.decay));
        const float damp = std::min (0.95f, std::max (0.0f, p.damp) * 0.9f + p.lofi * 0.2f);

        float wl = 0.0f, wr = 0.0f;
        for (int i = 0; i < 8; ++i) { wl += combL[i].process (x, fb, damp); wr += combR[i].process (x, fb, damp); }
        for (int i = 0; i < 4; ++i) { wl = apL[i].process (wl); wr = apR[i].process (wr); }
        wl *= 3.0f; wr *= 3.0f;

        if (p.lofi > 0.001f)
        {
            CrushParams cp;
            cp.bits = 16.0f - 10.0f * p.lofi;
            cp.rate = (float) sampleRate * (1.0f - 0.85f * p.lofi);
            wl = crL.process (wl, cp);
            wr = crR.process (wr, cp);
        }

        const float a = 0.5f * (1.0f + p.width), b = 0.5f * (1.0f - p.width);
        outL = inL * (1.0f - 0.3f * p.mix) + (wl * a + wr * b) * p.mix;
        outR = inR * (1.0f - 0.3f * p.mix) + (wr * a + wl * b) * p.mix;
    }

private:
    struct Comb
    {
        std::vector<float> buf; int idx = 0, len = 8; float store = 0.0f;
        void init (int maxLen) { buf.assign ((size_t) std::max (maxLen, 16), 0.0f); idx = 0; len = (int) buf.size(); store = 0.0f; }
        float process (float x, float fb, float damp)
        {
            const float out = buf[(size_t) idx];
            store = out * (1.0f - damp) + store * damp;
            buf[(size_t) idx] = x + store * fb;
            if (++idx >= len) idx = 0;
            return out;
        }
    };
    struct Allpass
    {
        std::vector<float> buf; int idx = 0;
        void init (int n) { buf.assign ((size_t) std::max (n, 2), 0.0f); idx = 0; }
        float process (float x)
        {
            const float b = buf[(size_t) idx];
            buf[(size_t) idx] = x + b * 0.5f;
            if (++idx >= (int) buf.size()) idx = 0;
            return b - x;
        }
    };

    void setSize (float size)
    {
        lastSize = size;
        const double sc = 0.5 + (double) std::max (0.0f, std::min (1.0f, size));   // ۰٫۵× تا ۱٫۵×
        for (int i = 0; i < 8; ++i)
        {
            combL[i].len = std::min ((int) combL[i].buf.size(), std::max (16, (int) (combBase[i] * sc)));
            combR[i].len = std::min ((int) combR[i].buf.size(), std::max (16, (int) ((combBase[i] + 23) * sc)));
        }
    }

    double sampleRate = 44100.0;
    float lastSize = -1.0f;
    int combBase[8] {};
    Comb combL[8], combR[8];
    Allpass apL[4], apR[4];
    DelayLine pdL, pdR;
    Crusher crL, crR;
};
} // namespace chip
