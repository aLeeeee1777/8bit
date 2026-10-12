#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// آرپژیاتور v0.3: مودهای جهت، Chord، Latch، استپ‌سکوئنسر ۱۶ تایی، سوینگ، رمپ سرعت
namespace chip
{
struct Arp
{
    enum Mode { Off = 0, Up, Down, UpDown, Random, Played, Chord, Converge };

    std::array<int, 16> held {};
    int count = 0;
    int index = -1;
    uint32_t rng = 22695477u;
    std::array<uint8_t, 128> isDown {};
    int physDown = 0;

    void clear()     { count = 0; index = -1; isDown.fill (0); physDown = 0; }
    void clearHeld() { count = 0; index = -1; }

    void press (int note, bool latch)
    {
        note = std::max (0, std::min (127, note));
        if (latch && physDown == 0) clearHeld();           // با Latch، اولین نت بعد از رها شدن همه، آکورد قبلی رو پاک می‌کنه
        if (! isDown[(size_t) note]) { isDown[(size_t) note] = 1; ++physDown; }
        for (int i = 0; i < count; ++i)
            if (held[(size_t) i] == note) return;
        if (count == 0) index = -1;
        if (count < (int) held.size()) held[(size_t) count++] = note;
    }

    void release (int note, bool latch)
    {
        note = std::max (0, std::min (127, note));
        if (isDown[(size_t) note]) { isDown[(size_t) note] = 0; --physDown; }
        if (latch) return;
        for (int i = 0; i < count; ++i)
            if (held[(size_t) i] == note)
            {
                for (int j = i; j < count - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
                --count;
                return;
            }
    }

    // نت(های) استپ بعدی رو توی out می‌ریزه و تعدادشون رو برمی‌گردونه (۰ = چیزی نگه داشته نشده)
    int next (int mode, int octaves, std::array<int, 16>& out)
    {
        if (count == 0) return 0;
        octaves = std::max (1, std::min (4, octaves));

        std::array<int, 16> base = held;
        if (mode != Played) std::sort (base.begin(), base.begin() + count);
        if (mode == Down)   std::reverse (base.begin(), base.begin() + count);

        std::array<int, 128> seq {};
        int n = 0;
        auto addOctave = [&] (int o)
        {
            for (int i = 0; i < count; ++i)
            {
                const int note = base[(size_t) i] + 12 * o;
                if (note <= 127 && n < 100) seq[(size_t) n++] = note;
            }
        };

        if (mode == Chord)
        {
            addOctave (0);
            const int c = std::min (n, 16);
            for (int i = 0; i < c; ++i) out[(size_t) i] = seq[(size_t) i];
            return c;
        }

        if (mode == Down) for (int o = octaves - 1; o >= 0; --o) addOctave (o);
        else              for (int o = 0; o < octaves; ++o)      addOctave (o);

        if (mode == UpDown && n > 2)
        {
            const int top = n;
            for (int i = top - 2; i >= 1; --i) seq[(size_t) n++] = seq[(size_t) i];
        }
        if (mode == Converge && n > 2)   // پایین‌ترین، بالاترین، دومین پایین، دومین بالا، ...
        {
            std::array<int, 128> tmp = seq;
            int lo = 0, hi = n - 1, k = 0;
            while (lo <= hi)
            {
                seq[(size_t) k++] = tmp[(size_t) lo++];
                if (lo <= hi) seq[(size_t) k++] = tmp[(size_t) hi--];
            }
        }
        if (n == 0) return 0;

        if (mode == Random)
        {
            rng = rng * 1664525u + 1013904223u;
            index = (int) ((rng >> 16) % (uint32_t) n);
        }
        else
            index = (index + 1) % n;

        out[0] = seq[(size_t) index];
        return 1;
    }
};

struct ArpSettings
{
    int    mode = 0, octaves = 1, patLen = 16;
    double baseStep = 11025.0;            // سمپل در هر استپ، با سرعت ×۱
    float  speed = 1.0f, swing = 0.0f, gate = 0.6f;
    float  ramp = 0.0f, rampTime = 2.0f;  // ramp: اکتاو سرعت (مثبت = شتاب‌گیری)
    bool   latch = false;
    std::array<bool, 16>  stepOn {};
    std::array<bool, 16>  stepAcc {};
    std::array<int, 16>   stepTr {};

    ArpSettings() { stepOn.fill (true); stepAcc.fill (false); stepTr.fill (0); }
};

class ArpRunner
{
public:
    Arp arp;

    void reset()
    {
        arp.clear();
        clock = 0.0; stepLen = 0.0; gateLen = 0.0; rampTimer = 0.0;
        counter = 0; noteCount = 0; startPending = false;
    }

    void press (int note, float vel, bool latch)
    {
        if (arp.count == 0 || (latch && arp.physDown == 0))
        {
            startPending = true;
            counter = 0;
            rampTimer = 0.0;
        }
        arp.press (note, latch);
        lastVel = vel;
    }

    template <class Off>
    void release (int note, bool latch, Off&& off)
    {
        arp.release (note, latch);
        if (arp.count == 0) closeAll (off);
    }

    template <class Off>
    void closeAll (Off&& off)
    {
        for (int i = 0; i < noteCount; ++i) off (active[(size_t) i]);
        noteCount = 0;
    }

    // هر سمپل یک بار صدا زده می‌شه
    template <class On, class Off>
    void tick (const ArpSettings& s, double sr, On&& on, Off&& off)
    {
        if (arp.count > 0) rampTimer += 1.0;

        if (startPending || clock >= stepLen)
        {
            if (startPending) { clock = 0.0; startPending = false; }
            else              { clock -= stepLen; }
            beginStep (s, sr, on, off);
        }
        if (noteCount > 0 && clock >= gateLen) closeAll (off);
        clock += 1.0;
    }

private:
    template <class On, class Off>
    void beginStep (const ArpSettings& s, double sr, On&& on, Off&& off)
    {
        closeAll (off);
        const int len = std::max (1, std::min (16, s.patLen));
        const int idx = counter % len;

        if (s.stepOn[(size_t) idx] && arp.count > 0)
        {
            std::array<int, 16> out {};
            const int n = arp.next (s.mode, s.octaves, out);
            const float vel = s.stepAcc[(size_t) idx] ? std::min (1.0f, lastVel + 0.3f) : lastVel;
            for (int i = 0; i < n; ++i)
            {
                const int note = std::max (0, std::min (127, out[(size_t) i] + s.stepTr[(size_t) idx]));
                on (note, vel);
                active[(size_t) noteCount++] = note;
            }
        }

        const double rampFactor = std::exp2 ((double) s.ramp * std::min (1.0, rampTimer / std::max (1.0, sr * (double) s.rampTime)));
        double stepSamples = s.baseStep / ((double) std::max (0.05f, s.speed) * rampFactor);
        stepSamples *= (counter % 2 == 0) ? (1.0 + (double) s.swing) : (1.0 - (double) s.swing);
        stepLen = std::max (16.0, stepSamples);
        gateLen = std::max (8.0, stepLen * (double) s.gate);
        ++counter;
    }

    std::array<int, 16> active {};
    int    noteCount = 0, counter = 0;
    double clock = 0.0, stepLen = 0.0, gateLen = 0.0, rampTimer = 0.0;
    bool   startPending = false;
    float  lastVel = 0.8f;
};
} // namespace chip
