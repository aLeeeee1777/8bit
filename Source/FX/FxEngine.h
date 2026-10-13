#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "../Effects.h"
#include "../ChipEngine.h"   // Svf

// ChipFX: افکت لو-فای/نویزی برای وکال و کل میکس — پیچ‌شیفتر، بیت‌کراش، فیلتر، گیت ریتمیک، استاتر، دیلی، ریورب
namespace chip
{
// پیچ‌شیفتر دو-تپ با کراس‌فید (لو-فای و مناسب وکال)
class PitchShifter
{
public:
    void prepare (double sr) { buf.assign ((size_t) (sr * 0.4), 0.0f); w = 0; phase = 0.0f; sampleRate = sr; }

    float process (float x, float ratio, float winMs)
    {
        const int n = (int) buf.size();
        buf[(size_t) w] = x;
        const float win = std::min ((float) n - 4.0f, std::max (64.0f, winMs * 0.001f * (float) sampleRate));
        const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
        const float a = read (phase * win), b = read (p2 * win);
        const float sA = std::sin (3.14159265f * phase), sB = std::sin (3.14159265f * p2);
        const float out = a * sA * sA + b * sB * sB;
        phase += (1.0f - ratio) / win;
        phase -= std::floor (phase);
        if (++w >= n) w = 0;
        return out;
    }

private:
    float read (float d) const
    {
        const int n = (int) buf.size();
        d = std::max (1.0f, std::min (d, (float) n - 2.0f));
        float rp = (float) w - d;
        if (rp < 0.0f) rp += (float) n;
        if (rp >= (float) n) rp -= (float) n;
        int i0 = (int) rp; if (i0 >= n) i0 = n - 1;
        int i1 = i0 + 1;   if (i1 >= n) i1 -= n;
        const float fr = std::min (1.0f, std::max (0.0f, rp - (float) i0));
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
    std::vector<float> buf;
    int w = 0;
    float phase = 0.0f;
    double sampleRate = 44100.0;
};

struct FxEngineParams
{
    float inGain = 1.0f, outGain = 1.0f, mix = 1.0f;
    float pitchSemis = 0.0f, pitchFine = 0.0f, pitchMix = 0.0f, pitchWin = 40.0f;
    CrushParams crush;
    int   fmode = 0; float cutoff = 8000.0f, reso = 0.2f;
    int   gatePattern = 0, gateDiv = 1; float gateDepth = 1.0f, gateSmooth = 0.2f;
    int   stutterDiv = 0;
    float dTime = 300.0f, dFb = 0.35f, dMix = 0.0f;
    ReverbParams rev;
};

class ChipFxEngine
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (int c = 0; c < 2; ++c)
        {
            ps[c].prepare (sr); crush[c].prepare (sr); svf[c] = Svf();
            ring[c].assign ((size_t) (sr * 4.0), 0.0f);
        }
        dL.init ((int) (sr * 1.2) + 8); dR.init ((int) (sr * 1.2) + 8);
        reverb.prepare (sr);
        wp = 0; engaged = false; gateGain = 1.0f;
    }

    static const std::array<uint16_t, 8>& patterns()
    {
        // بیت ۰ = استپ ۱. ۱ = باز
        static const std::array<uint16_t, 8> p = { 0xFFFF, 0x5555, 0xB6B6, 0x7777, 0x4949, 0x0F0F, 0xBBBB, 0x1111 };
        return p;
    }

    // beatPos: موقعیت به کوارتر نوت (ppq)
    void process (float inL, float inR, float& outL, float& outR, const FxEngineParams& p, double beatPos, double bpm)
    {
        float x[2] = { inL * p.inGain, inR * p.inGain };
        const float dry[2] = { x[0], x[1] };
        const float ratio = std::exp2 ((p.pitchSemis + p.pitchFine / 100.0f) / 12.0f);
        const bool pitchOn = p.pitchMix > 0.001f && std::abs (ratio - 1.0f) > 1.0e-4f;

        // گیت
        float gTarget = 1.0f;
        if (p.gatePattern > 0)
        {
            static const double beats[4] = { 0.5, 0.25, 0.125, 0.0625 };   // 1/8, 1/16, 1/32, 1/64
            const double stepBeats = beats[std::max (0, std::min (3, p.gateDiv))];
            const int step = (int) std::floor (beatPos / stepBeats) & 15;
            const bool open = (patterns()[(size_t) std::min (7, p.gatePattern)] >> step) & 1u;
            gTarget = open ? 1.0f : 1.0f - std::max (0.0f, std::min (1.0f, p.gateDepth));
        }
        const float gCoef = 1.0f - std::exp (-1.0f / (float) (sampleRate * (0.0005 + 0.02 * (double) p.gateSmooth)));
        gateGain += gCoef * (gTarget - gateGain);

        // استاتر
        const bool wantStutter = p.stutterDiv > 0;
        if (wantStutter && ! engaged)
        {
            static const double sb[4] = { 1.0, 0.5, 0.25, 0.125 };   // 1/4,1/8,1/16,1/32
            sliceLen = std::max (64, (int) (60.0 / std::max (20.0, bpm) * sb[std::min (3, p.stutterDiv - 1)] * sampleRate));
            sliceLen = std::min (sliceLen, (int) ring[0].size() - 2);
            const int n = (int) ring[0].size();
            segStart = ((wp - sliceLen) % n + n) % n;
            counter = 0;
            engaged = true;
        }
        else if (! wantStutter) engaged = false;

        for (int c = 0; c < 2; ++c)
        {
            float v = x[c];
            if (pitchOn)
            {
                const float sh = ps[c].process (v, ratio, p.pitchWin);
                v = v * (1.0f - p.pitchMix) + sh * p.pitchMix;
            }
            v = crush[c].process (std::max (-1.0f, std::min (1.0f, v)), p.crush);

            if (p.fmode > 0)
            {
                const float cutoff = std::max (20.0f, std::min ((float) (0.45 * sampleRate), p.cutoff));
                const float g = std::tan (3.14159265f * cutoff / (float) sampleRate);
                const float k = 2.0f - 1.9f * std::max (0.0f, std::min (1.0f, p.reso));
                float lp, bp, hp;
                svf[c].process (v, g, k, lp, bp, hp);
                v = (p.fmode == 1) ? lp : (p.fmode == 2) ? bp * k : hp;
            }
            v *= gateGain;

            const int n = (int) ring[(size_t) c].size();
            if (! engaged) ring[(size_t) c][(size_t) wp] = v;
            else
            {
                const float fadeIn  = std::min (1.0f, (float) counter / 32.0f);
                const float fadeOut = std::min (1.0f, (float) (sliceLen - counter) / 32.0f);
                v = ring[(size_t) c][(size_t) ((segStart + counter) % n)] * std::min (fadeIn, fadeOut);
            }
            x[c] = v;
        }
        if (! engaged) { if (++wp >= (int) ring[0].size()) wp = 0; }
        else           { if (++counter >= sliceLen) counter = 0; }

        // دیلی پینگ‌پونگ
        const float ds = p.dTime * 0.001f * (float) sampleRate;
        const float el = dL.read (ds), er = dR.read (ds);
        dL.push (0.5f * (x[0] + x[1]) + er * p.dFb);
        dR.push (el * p.dFb);
        float wl = x[0] + p.dMix * el, wr = x[1] + p.dMix * er;

        // ریورب
        reverb.process (wl, wr, wl, wr, p.rev);

        outL = (dry[0] * (1.0f - p.mix) + wl * p.mix) * p.outGain;
        outR = (dry[1] * (1.0f - p.mix) + wr * p.mix) * p.outGain;
    }

private:
    double sampleRate = 44100.0;
    PitchShifter ps[2];
    Crusher crush[2];
    Svf svf[2];
    std::vector<float> ring[2];
    int wp = 0, segStart = 0, counter = 0, sliceLen = 1024;
    bool engaged = false;
    float gateGain = 1.0f;
    DelayLine dL, dR;
    Reverb reverb;
};
} // namespace chip
