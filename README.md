# ChipSynth — سینتی‌سایزر چیپ‌تیون (VST3)

چیپ‌ها: NES (2A03) · Game Boy (DMG) · Master System (SN76489) · C64 (SID)

## بیلد روی ویندوز
1. نصب: Visual Studio 2022 (گزینه‌ی Desktop development with C++)، CMake، Git
2. توی پوشه‌ی پروژه، Command Prompt باز کن:

    cmake -B build -G "Visual Studio 17 2022"
    cmake --build build --config Release

(بار اول JUCE دانلود می‌شه، چند دقیقه طول می‌کشه.)
3. خروجی: build\ChipSynth_artefacts\Release\VST3\ChipSynth.vst3
4. کپی کن به: C:\Program Files\Common Files\VST3\
5. FL Studio: Options ← Manage plugins ← Find installed plugins ← بعد از اسکن، ChipSynth رو از Add ← Instruments اضافه کن.

مک: `cmake -B build -G Xcode` و بعد `cmake --build build --config Release`؛ مقصد ~/Library/Audio/Plug-Ins/VST3

## هر چیپ چی داره
- NES: پالس (۴ duty)، مثلثی ۴ بیتی ۳۲ پله‌ای، نویز ۱۵ بیتی + حالت کوتاه، ولوم ۱۶ پله‌ای
- Game Boy: پالس، Wave Table ۴ بیتی ۳۲ نمونه‌ای (مثلث/اره‌ای/سینوس/پالس)، نویز ۱۵/۷ بیتی
- Master System: موج مربعی ۵۰٪، نویز (white / periodic)، ولوم ۲dB-پله‌ای
- SID: پالس با pulse width پیوسته، مثلث، اره‌ای، نویز ۲۳ بیتی، ۸ بیتی

کوک هر نت به رجیستر واقعی چیپ گرد می‌شه (برای همین نت‌های خیلی پایین از محدوده‌ی چیپ کلمپ می‌شن؛ توی FL ترنسپوز کن).
Anti-alias رو خاموش کنی صدا خشن و اصیل‌تر (aliased) می‌شه.

## لایسنس JUCE
JUCE تحت AGPLv3 یا لایسنس تجاری/شخصی عرضه می‌شه. برای استفاده‌ی شخصی مشکلی نیست؛ برای فروش شرایطش رو چک کن.

## بیلد بدون نصب چیزی (GitHub Actions)
1. توی github.com یه ریپوی جدید (private هم می‌شه) بساز و همه‌ی فایل‌های این پروژه رو (با پوشه‌ی .github) آپلود کن.
2. تب Actions ← آخرین run (سبز شد) ← پایین صفحه Artifacts ← ChipSynth-VST3-Windows رو دانلود کن.
3. زیپ رو باز کن و پوشه‌ی ChipSynth.vst3 رو بذار توی C:\Program Files\Common Files\VST3
