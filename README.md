# ChipSynth v0.6 + ChipFX — ابزار کامل چیپ‌تیون (VST3)

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

## جدید در v0.6 — افکت‌های آرکید و آتاری (تب SFX)
**تفنگ پیکسلی، لیزر، انفجار، پاورآپ، پرش، سکه...** با مود **SFX** (Mode = SFX):
- **مولد (مثل sfxr):** دکمه‌های Shoot / Explosion / Power-up / Hit / Jump / Blip / Random یه صدای جدید می‌سازن، و Mutate یه تغییر کوچیک تصادفی روی صدای فعلی می‌ده. بعد هر کلیدی بزنی (MIDI یا کیبورد روی صفحه) شلیک می‌شه.
- **پارامترها:** Pitch، Slide (سرعت افت پیچ = همون «پیو»)، Slide Accel، **Step Rate** (آپدیت پله‌ای پیچ مثل فریم کنسول)، Attack/Hold/Decay، Duty و Duty Sweep، ویبراتو، Pitch Jump (صدای سکه/پاورآپ)، Noise، **Retrigger Hz** (مسلسل تا وقتی کلید نگه داشته می‌شه)، Key Track (۰ = همه‌ی کلیدها یه صدا رو شلیک می‌کنن)
- موج و نویز و پله‌های ولوم از **چیپ انتخابی** میاد (Chip flavor): NES، Atari TIA، POKEY، SID و ...
- Crush، ریورب و افکت‌ها روی SFX هم اعمال می‌شن.
- ۱۶ پریست SFX: Atari Shot، Pixel Blaster، Rapid Fire، Heavy Cannon، Laser Pew، Ricochet، POKEY Zap، 8-bit Explosion، Atari Boom، Machine Gun، Hit/Hurt، Jump، Power-up، Coin، Siren، Gritty Blast
- مجموعاً ۹۴ پریست

## جدید در v0.5
**دو پلاگین:** `ChipSynth.vst3` (ساز) و `ChipFX.vst3` (افکت وکال و میکس).

### ChipSynth
- **ریورب** استریو با Size/Decay/Damping/Pre-delay/Width و **Lo-Fi** (دم ریورب بیت‌کراش و تیره می‌شه)
- **لایه‌ها:** تا ۳ اسیلاتور، هرکدوم چیپ/موج/لول/ترنسپوز/فاین جدا (تب LAYERS)
- **Unison** تا ۵ صدا با دیتیون و پهنای استریو
- **ماتریکس مدولاسیون:** ۲ LFO + ModEnv برای هر نت؛ ۸ اسلات (منبع ← مقصد ← مقدار) روی Pitch، Cutoff، Resonance، PW، Level، Sync، Ring، Noise Mix، لول لایه‌ها
- **ماتریکس گلوبال:** ۲ LFO آزاد + ۴ اسلات روی Bit Depth، Sample Rate، Drive، Chorus، Delay، Reverb و Master (مثلاً بیت‌کراش متحرک)
- **سمپلر (Mode = Sampler):** فایل wav/aiff/flac/ogg (حداکثر ۳۰ ثانیه) روی کیبورد، Root Note، Loop، Start/End، و DAC لو-فای (Bits / Rate). مسیر فایل داخل پروژه‌ی FL ذخیره می‌شه.
- **کیک قوی‌تر:** Kick Click، Kick Sub (ساب سینوسی)، Kick Drive + ۱۶ پریست کیک (Kick - ... و Synth Kick - ...)
- ۷۸ پریست

### ChipFX (افکت)
پیچ‌شیفتر (±۲۴ نیم‌پرده)، بیت‌کراشر + دیستورشن (Soft/Hard/Fold)، فیلتر LP/BP/HP، گیت ریتمیک سینک به BPM (۷ الگو)، **استاتر** (تکرار آخرین تکه؛ پارامتر Stutter رو با اتوماسیون FL روشن/خاموش کن)، دیلی پینگ‌پونگ (آزاد یا سینک)، ریورب Lo-Fi. بدون تنظیم، کاملاً شفافه (مثل bypass). رابطش فعلاً لیست ساده‌ی خود JUCE هست.

### رفع باگ
دیلی/کورس نسخه‌های قبلی (۰.۲ تا ۰.۴) یه باگ نادر داشت: وقتی یه عدد اعشاری گرد می‌شد، ممکن بود یه اسپایک بلند تو خروجی بیاد. تو این نسخه درست شد.

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
با GitHub Actions (فایل .github/workflows/build.yml): همه‌ی فایل‌ها رو آپلود کن (پوشه‌ی Source/FX هم باشه)، تب Actions ← آخرین run ← Artifacts. دو خروجی می‌ده: ChipSynth-VST3-Windows و ChipFX-VST3-Windows.
یا روی ویندوز با: cmake -B build ؛ cmake --build build --config Release
خروجی: build\ChipSynth_artefacts\Release\VST3\ChipSynth.vst3

## لایسنس JUCE
JUCE تحت AGPLv3 یا لایسنس تجاری/شخصی عرضه می‌شه. برای استفاده‌ی شخصی مشکلی نیست؛ برای فروش شرایطش رو چک کن.
