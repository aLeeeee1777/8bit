# ChipSynth v0.4 — سینتی‌سایزر چیپ‌تیون (VST3)

## چیپ‌ها (۸ تا)
NES (2A03) · Game Boy · Master System (SN76489) · C64 SID · Atari POKEY · Atari 2600 (TIA) · AY-3-8910/YM2149 · PC Engine (HuC6280)

## شکل‌موج هر چیپ (منوی Waveform)
| چیپ | Pulse | Triangle | Saw | Noise | Wavetable |
|---|---|---|---|---|---|
| NES | ۴ duty | ۴ بیتی | ← مثلث | LFSR ۱۵ بیتی | ← مثلث |
| Game Boy | ۴ duty | ← Wavetable | ← Wavetable | ۱۵/۷ بیتی | ۴ بیتی ۳۲ نمونه |
| Master System | ۵۰٪ | ← پالس | ← پالس | white/periodic | ← پالس |
| SID | PW پیوسته | ۸ بیتی | ۸ بیتی | ۲۳ بیتی | ← مثلث |
| POKEY | pure | Poly4 (باز) | Poly5+pulse | Poly17 | Poly9 |
| TIA | pure | Poly4 | Poly5+pulse | Poly9 | Poly5 |
| AY-3-8910 | ۵۰٪ | انولوپ مثلثی | انولوپ اره‌ای | نویز ۱۷ بیتی | تون AND نویز |
| PC Engine | PW (۳۲ پله) | ۵ بیتی | ۵ بیتی | LFSR | ۵ بیتی ۳۲ نمونه |

## جدید در v0.4 — درام هشت‌بیتی و لید
**مود Drums** (تب DRUMS یا پارامتر Mode): نت‌های MIDI رو به درام تبدیل می‌کنه. نام نت‌ها طبق FL Studio:

| نت | درام | نت | درام |
|---|---|---|---|
| C3 | Kick | G#3 | Pedal Hat |
| C#3 | Rim | A3 | Tom 3 |
| D3 | Snare | A#3 | Open Hat |
| D#3 | Clap | B3 | Tom 4 |
| E3 | Snare 2 (الکترو) | C4 | Tom 5 |
| F3 | Tom 1 | C#4 | Crash |
| F#3 | Closed Hat | D4 | Tom 6 |
| G3 | Tom 2 | D#4 | Ride |
| | | G#4 | Cowbell |

(MIDI 36 تا 51 و 56، چیدمان General MIDI؛ یعنی کلیپ‌های درام MIDI آماده هم کار می‌کنن.)

- هر چیپ درام خودش رو داره: نویز LFSR همون چیپ (NES/GB/SMS/SID/…)، موج تُن چیپ، و ولوم ۴ بیتی/لگاریتمی. گزینه‌ی Authentic پیچ و ولوم رو مثل فریم کنسول (۲۴۰ بار در ثانیه، پله‌ای) آپدیت می‌کنه.
- کیک با پیچ افتان (Kick Punch)، اسنیر = تُن + نویز، هت بسته/باز با choke (هت بسته، هت باز رو می‌بُره)، کلپ چندضربه‌ای، تام‌ها با پیچ افتان.
- کراش، رایدر و کاوبل هم هست. Metallic noise (حالت کوتاه LFSR) هت‌ها رو فلزی می‌کنه.
- بعد از درام می‌تونی Crush & Drive و کورس/دیلی رو هم روی کل کیت بذاری.

**لید:** PWM (تغییر عرض پالس؛ روی NES و Game Boy پله‌ای بین duty ها می‌چرخه) + ۱۳ پریست لید برای چیپ‌های مختلف + ۱۰ کیت درام آماده.

## جدید در v0.3
- **آرپژیاتور قدرتمند:** مودهای Up/Down/Up-Down/Random/Played/Chord/Converge، تا ۴ اکتاو، Latch، سینک به BPM (۱/۲ تا ۱/۶۴ شامل triplet) یا Free Hz، ضریب سرعت ×۰.۲۵ تا ×۴، Swing، Gate، Speed Ramp (شتاب‌گیری/کندشدن)
- **استپ‌سکوئنسر ۱۶ تایی:** روشن/خاموش (rest)، اکسنت و ترنسپوز برای هر استپ، و طول پترن
- **Chip Arp:** ۱۳ الگو، سرعت تا ۲۰۰ هرتز
- **Pitch:** Transpose، Fine Tune، Bend Range، Pitch Envelope (Exp/Linear/Stepped)، Glide، Slide، ویبراتو با ۵ شکل (Sine/Tri/Square-trill/Saw/Random) و Delay
- **Noise Mod:** نویز روی پیچ و فیلتر (sample&hold) و Noise Mix
- **Crush & Drive:** Bit Depth، Sample Rate، دیستورشن (Soft/Hard/Fold)
- ۳۰ پریست از جمله Noisy Arcade، Laser Zap، Chip Kick، Trance Gate Arp، Accel Arp
- رابط ۳ تب‌ه + کیبورد روی صفحه

## امکانات v0.2
- ولوم درست‌شده (قبلاً خیلی کم بود)
- فیلتر LP/BP/HP با رزونانس و Env Amount
- Hard Sync و Ring Mod
- آرپژیاتور از روی نت‌های نگه‌داشته‌شده (Up/Down/Up-Down/Random/Played)، سینک به BPM، گیت، تا ۳ اکتاو
- Chip Arp: آرپژ سریع و فریم‌ریتی مثل آهنگ‌های کنسول
- کورس + دیلی پینگ‌پونگ
- Authentic Tuning: کوک رو به رجیستر واقعی چیپ گرد می‌کنه (خاموشش کنی کوک دقیق می‌شه)
- رابط گرافیکی اختصاصی

## بیلد
با GitHub Actions (فایل .github/workflows/build.yml): همه‌ی فایل‌ها رو آپلود کن، تب Actions ← آخرین run ← Artifacts.
یا روی ویندوز با: cmake -B build ؛ cmake --build build --config Release
خروجی: build\ChipSynth_artefacts\Release\VST3\ChipSynth.vst3

## لایسنس JUCE
JUCE تحت AGPLv3 یا لایسنس تجاری/شخصی عرضه می‌شه. برای استفاده‌ی شخصی مشکلی نیست؛ برای فروش شرایطش رو چک کن.
