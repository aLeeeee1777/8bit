#include "PluginProcessor.h"
#include "PluginEditor.h"

ChipSynthProcessor::ChipSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < 16; ++i)
    {
        const juce::String n (i + 1);
        stepOnP[(size_t) i]  = apvts.getRawParameterValue ("s" + n + "on");
        stepAccP[(size_t) i] = apvts.getRawParameterValue ("s" + n + "acc");
        stepTrP[(size_t) i]  = apvts.getRawParameterValue ("s" + n + "tr");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ChipSynthProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addChoice = [&] (const String& id, const char* name, StringArray items, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    auto addFloat = [&] (const String& id, const char* name, float lo, float hi, float def, float skew = 1.0f)
    { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                         NormalisableRange<float> (lo, hi, 0.0f, skew), def)); };

    auto addBool = [&] (const String& id, const String& name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    auto addInt = [&] (const String& id, const String& name, int lo, int hi, int def)
    { layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };

    // --- صدا ---
    addChoice ("mode", "Mode", { "Synth", "Drums" }, 0);
    addChoice ("chip", "Chip", { "NES (2A03)", "Game Boy (DMG)", "Master System (SN76489)", "C64 (SID 6581)",
                                 "Atari POKEY", "Atari 2600 (TIA)", "AY-3-8910 / YM2149", "PC Engine (HuC6280)" }, 0);
    addChoice ("wave", "Waveform", { "Pulse", "Triangle", "Saw", "Noise", "Wavetable" }, 0);
    addChoice ("gbwave", "Wave Table", { "Triangle", "Saw", "Sine", "Pulse 25%" }, 0);
    addFloat  ("pw", "Pulse Width", 0.05f, 0.95f, 0.5f);
    addFloat  ("pwmrate", "PWM Rate", 0.1f, 20.0f, 4.0f, 0.5f);
    addFloat  ("pwmdepth", "PWM Depth", 0.0f, 1.0f, 0.0f);
    addBool   ("noiseshort", "Noise Short Mode", false);
    addBool   ("smooth", "Anti-alias (4x)", true);
    addBool   ("authentic", "Authentic Tuning", true);

    // --- انولوپ ---
    addFloat ("attack",  "Attack",  0.001f, 2.0f, 0.001f, 0.4f);
    addFloat ("decay",   "Decay",   0.001f, 2.0f, 0.15f, 0.4f);
    addFloat ("sustain", "Sustain", 0.0f,   1.0f, 0.8f);
    addFloat ("release", "Release", 0.001f, 3.0f, 0.08f, 0.4f);

    // --- فیلتر ---
    addChoice ("fmode", "Filter", { "Off", "Low-pass", "Band-pass", "High-pass" }, 0);
    addFloat  ("cutoff", "Cutoff", 40.0f, 16000.0f, 4000.0f, 0.35f);
    addFloat  ("reso", "Resonance", 0.0f, 1.0f, 0.2f);
    addFloat  ("fenv", "Env Amount", -4.0f, 4.0f, 0.0f);

    // --- مدولاسیون ---
    addFloat ("sync", "Hard Sync", 1.0f, 8.0f, 1.0f);
    addFloat ("ring", "Ring Mod", 0.0f, 4.0f, 0.0f);
    addFloat ("nmpitch",  "Noise>Pitch",  0.0f, 12.0f, 0.0f);
    addFloat ("nmfilter", "Noise>Filter", 0.0f, 4.0f, 0.0f);
    addFloat ("nmrate",   "Noise Rate",   1.0f, 4000.0f, 30.0f, 0.3f);
    addFloat ("nmmix",    "Noise Mix",    0.0f, 1.0f, 0.0f);

    // --- پیچ ---
    addInt   ("transpose", "Transpose", -24, 24, 0);
    addFloat ("fine", "Fine Tune", -100.0f, 100.0f, 0.0f);
    addInt   ("bendrange", "Bend Range", 1, 24, 2);
    addFloat ("penvamt", "Pitch Env", -48.0f, 48.0f, 0.0f);
    addFloat ("penvtime", "Pitch Env Time", 0.005f, 2.0f, 0.15f, 0.4f);
    addChoice ("penvshape", "Env Shape", { "Exp", "Linear", "Stepped" }, 0);
    addFloat ("glide", "Glide", 0.0f, 1.5f, 0.0f, 0.5f);
    addFloat ("slide", "Slide", -12.0f, 12.0f, 0.0f);
    addFloat ("slidetime", "Slide Time", 0.005f, 1.0f, 0.1f, 0.5f);
    addChoice ("vibshape", "Vib Shape", { "Sine", "Triangle", "Square (trill)", "Saw", "Random" }, 0);
    addFloat ("vibrate", "Vib Rate", 0.1f, 30.0f, 5.0f, 0.5f);
    addFloat ("vibdepth", "Vib Depth", 0.0f, 1200.0f, 0.0f, 0.5f);
    addFloat ("vibdelay", "Vib Delay", 0.0f, 3.0f, 0.0f, 0.5f);

    // --- Crush ---
    addFloat  ("bits", "Bit Depth", 1.0f, 16.0f, 16.0f);
    addFloat  ("srate", "Sample Rate", 500.0f, 44100.0f, 44100.0f, 0.4f);
    addFloat  ("drive", "Drive", 0.0f, 1.0f, 0.0f);
    addChoice ("dtype", "Drive Type", { "Soft", "Hard", "Fold" }, 0);

    // --- آرپژ (از روی نت‌های نگه‌داشته‌شده) ---
    addChoice ("arpmode", "Arpeggiator", { "Off", "Up", "Down", "Up-Down", "Random", "Played", "Chord", "Converge" }, 0);
    addChoice ("arpsync", "Arp Clock", { "Host Sync", "Free (Hz)" }, 0);
    addChoice ("arpdiv", "Arp Rate", { "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" }, 5);
    addFloat  ("arphz", "Arp Hz", 0.5f, 120.0f, 12.0f, 0.4f);
    addFloat  ("arpspeed", "Speed x", 0.25f, 4.0f, 1.0f, 0.43f);
    addFloat  ("arpswing", "Swing", 0.0f, 0.6f, 0.0f);
    addFloat  ("arpgate", "Gate", 0.05f, 1.0f, 0.6f);
    addInt    ("arpoct", "Octaves", 1, 4, 1);
    addFloat  ("arpramp", "Speed Ramp", -2.0f, 2.0f, 0.0f);
    addFloat  ("arpramptime", "Ramp Time", 0.2f, 8.0f, 2.0f, 0.5f);
    addInt    ("arplen", "Pattern Length", 1, 16, 16);
    addBool   ("arplatch", "Latch (hold)", false);

    for (int i = 1; i <= 16; ++i)
    {
        const String n (i);
        addBool ("s" + n + "on",  "Step " + n + " On",  true);
        addBool ("s" + n + "acc", "Step " + n + " Accent", false);
        addInt  ("s" + n + "tr",  "Step " + n + " Transpose", -12, 12, 0);
    }

    // --- Chip Arp (فریم‌ریتی، روی هر صدا) ---
    addChoice ("arp", "Chip Arp", { "Off", "Major", "Minor", "Octave", "Power (5th)", "Diminished", "Maj7", "Min7",
                                    "Sus4", "3 Octaves", "Minor Pent.", "Whole-tone", "Major Up-Down" }, 0);
    addFloat  ("arprate", "Chip Arp Rate", 4.0f, 200.0f, 30.0f, 0.5f);

    // --- افکت ---
    addFloat ("chorusmix", "Chorus", 0.0f, 1.0f, 0.0f);
    addFloat ("chorusrate", "Ch Rate", 0.1f, 5.0f, 0.8f);
    addFloat ("chorusdepth", "Ch Depth", 0.0f, 8.0f, 3.0f);
    addFloat ("delaytime", "Dly Time", 20.0f, 1000.0f, 300.0f, 0.5f);
    addFloat ("delayfb", "Dly Fdbk", 0.0f, 0.9f, 0.35f);
    addFloat ("delaymix", "Delay", 0.0f, 1.0f, 0.0f);

    // --- درام ---
    addFloat ("kicktune", "Kick Tune", -12.0f, 12.0f, 0.0f);
    addFloat ("kickdecay", "Kick Decay", 0.05f, 0.8f, 0.28f);
    addFloat ("kicksweep", "Kick Punch", 0.0f, 48.0f, 24.0f);
    addFloat ("snaretune", "Snare Tune", -12.0f, 12.0f, 0.0f);
    addFloat ("snaretone", "Snare Tone", 0.0f, 1.0f, 0.5f);
    addFloat ("snaredecay", "Snare Decay", 0.05f, 0.6f, 0.18f);
    addFloat ("hatdecay", "Hat Decay", 0.01f, 0.2f, 0.035f);
    addFloat ("ohdecay", "Open Hat Decay", 0.1f, 1.2f, 0.35f);
    addFloat ("tomtune", "Tom Tune", -12.0f, 12.0f, 0.0f);
    addFloat ("drumvol", "Drum Volume", 0.0f, 1.0f, 0.9f);

    // --- خروجی ---
    addInt   ("poly", "Polyphony", 1, 8, 4);
    addFloat ("velsens", "Velocity", 0.0f, 1.0f, 0.3f);
    addFloat ("master", "Master", 0.0f, 1.0f, 0.8f);

    return layout;
}

chip::Params ChipSynthProcessor::readParams() const
{
    chip::Params p;
    p.chip       = (int) getParam ("chip");
    p.wave       = (int) getParam ("wave");
    p.gbWave     = (int) getParam ("gbwave");
    p.pulseWidth = getParam ("pw");
    p.pwmRate    = getParam ("pwmrate");
    p.pwmDepth   = getParam ("pwmdepth");
    p.noiseShort = getParam ("noiseshort") > 0.5f;
    p.smooth     = getParam ("smooth") > 0.5f;
    p.authentic  = getParam ("authentic") > 0.5f;
    p.attack     = getParam ("attack");
    p.decay      = getParam ("decay");
    p.sustain    = getParam ("sustain");
    p.release    = getParam ("release");
    p.filterMode = (int) getParam ("fmode");
    p.cutoff     = getParam ("cutoff");
    p.resonance  = getParam ("reso");
    p.filterEnv  = getParam ("fenv");
    p.syncRatio  = getParam ("sync");
    p.ringRatio  = getParam ("ring");
    p.nmPitch    = getParam ("nmpitch");
    p.nmFilter   = getParam ("nmfilter");
    p.nmRate     = getParam ("nmrate");
    p.nmMix      = getParam ("nmmix");
    p.transpose  = (int) getParam ("transpose");
    p.fineTune   = getParam ("fine");
    p.bendRange  = getParam ("bendrange");
    p.penvAmt    = getParam ("penvamt");
    p.penvTime   = getParam ("penvtime");
    p.penvShape  = (int) getParam ("penvshape");
    p.glide      = getParam ("glide");
    p.slideSemis = getParam ("slide");
    p.slideTime  = getParam ("slidetime");
    p.vibShape   = (int) getParam ("vibshape");
    p.vibRate    = getParam ("vibrate");
    p.vibDepth   = getParam ("vibdepth");
    p.vibDelay   = getParam ("vibdelay");
    p.arpPattern = (int) getParam ("arp");
    p.arpRate    = getParam ("arprate");
    p.polyphony  = (int) getParam ("poly");
    p.velSens    = getParam ("velsens");
    p.master     = getParam ("master");
    return p;
}

chip::FxParams ChipSynthProcessor::readFx() const
{
    chip::FxParams f;
    f.chorusMix   = getParam ("chorusmix");
    f.chorusRate  = getParam ("chorusrate");
    f.chorusDepth = getParam ("chorusdepth");
    f.delayTime   = getParam ("delaytime");
    f.delayFb     = getParam ("delayfb");
    f.delayMix    = getParam ("delaymix");
    return f;
}

chip::CrushParams ChipSynthProcessor::readCrush() const
{
    chip::CrushParams c;
    c.bits  = getParam ("bits");
    c.rate  = getParam ("srate");
    c.drive = getParam ("drive");
    c.type  = (int) getParam ("dtype");
    return c;
}

chip::DrumParams ChipSynthProcessor::readDrum (const chip::Params& p) const
{
    chip::DrumParams d;
    d.kickTune   = getParam ("kicktune");
    d.kickDecay  = getParam ("kickdecay");
    d.kickSweep  = getParam ("kicksweep");
    d.snareTune  = getParam ("snaretune");
    d.snareTone  = getParam ("snaretone");
    d.snareDecay = getParam ("snaredecay");
    d.hatDecay   = getParam ("hatdecay");
    d.openDecay  = getParam ("ohdecay");
    d.tomTune    = getParam ("tomtune");
    d.volume     = getParam ("drumvol");
    d.noiseShort = p.noiseShort;
    d.authentic  = p.authentic;
    return d;
}

void ChipSynthProcessor::triggerDrum (int note, float vel, const chip::Params& p)
{
    int tom = 0;
    const int type = chip::drumForNote (note, tom);
    if (type == chip::DNone) return;

    // choke: هت بسته، هت باز رو می‌بُره؛ کیک/هت باز/کراش/رایدِ قبلی هم کوتاه قطع می‌شن
    for (auto& d : drums)
    {
        if (! d.active) continue;
        if ((type == chip::DClosedHat && d.type == chip::DOpenHat) || (d.type == type && type != chip::DTom
                                                                        && type != chip::DClosedHat && type != chip::DClap))
            d.choke = true;
    }

    chip::DrumVoice* target = nullptr;
    for (auto& d : drums) if (! d.active) { target = &d; break; }
    if (target == nullptr)
    {
        target = &drums[0];
        for (auto& d : drums) if (d.age < target->age) target = &d;
    }
    target->trigger (type, note, tom, vel, p.chip, ++drumOrder);
}

namespace
{
using PV = std::vector<std::pair<const char*, float>>;
struct PresetDef { const char* name; PV values; };

// مقدارها: ایندکس برای منوها، مقدار واقعی برای نوب‌ها، 0/1 برای تیک‌ها
// chip: 0 NES, 1 GB, 2 SMS, 3 SID, 4 POKEY, 5 TIA, 6 AY, 7 PCE
// wave: 0 Pulse, 1 Triangle, 2 Saw, 3 Noise, 4 Wavetable
const std::vector<PresetDef>& getPresets()
{
    static const std::vector<PresetDef> presets = {
        { "Init", {} },

        { "NES - Pulse Lead",       { {"chip",0},{"wave",0},{"pw",0.25f},{"vibdepth",25},{"vibrate",6},{"decay",0.2f},{"sustain",0.7f},{"poly",1} } },
        { "NES - Triangle Bass",    { {"chip",0},{"wave",1},{"sustain",1},{"release",0.05f},{"poly",1} } },
        { "NES - Noise Hit",        { {"chip",0},{"wave",3},{"noiseshort",1},{"decay",0.12f},{"sustain",0},{"release",0.05f} } },

        { "GB - Wave Bass",         { {"chip",1},{"wave",4},{"gbwave",2},{"sustain",1},{"poly",1} } },
        { "GB - Pulse Arp",         { {"chip",1},{"wave",0},{"pw",0.25f},{"arp",1},{"arprate",24},{"poly",1} } },

        { "SMS - Square Lead",      { {"chip",2},{"wave",0},{"vibdepth",15},{"chorusmix",0.3f} } },
        { "SMS - Noise Hat",        { {"chip",2},{"wave",3},{"decay",0.06f},{"sustain",0},{"release",0.03f} } },

        { "SID - Filter Pulse Bass",{ {"chip",3},{"wave",0},{"pw",0.3f},{"fmode",1},{"cutoff",500},{"reso",0.6f},{"fenv",2.5f},{"decay",0.25f},{"sustain",0.4f},{"poly",1} } },
        { "SID - Saw Lead",         { {"chip",3},{"wave",2},{"fmode",1},{"cutoff",2500},{"reso",0.4f},{"fenv",1},{"chorusmix",0.4f} } },
        { "SID - Sync Sweep",       { {"chip",3},{"wave",2},{"sync",3.2f},{"sustain",0.9f},{"chorusmix",0.3f} } },

        { "POKEY - Buzz Bass",      { {"chip",4},{"wave",1},{"sustain",1},{"poly",1} } },
        { "POKEY - Saw Buzz",       { {"chip",4},{"wave",2},{"sustain",0.9f} } },
        { "POKEY - Noise",          { {"chip",4},{"wave",3},{"decay",0.15f},{"sustain",0} } },

        { "TIA - Bass",             { {"chip",5},{"wave",1},{"sustain",1},{"poly",1} } },
        { "TIA - Lead",             { {"chip",5},{"wave",0},{"sustain",0.8f} } },

        { "AY - Pulse Arp",         { {"chip",6},{"wave",0},{"arp",2},{"arprate",30},{"poly",1} } },
        { "AY - Envelope Buzz Bass",{ {"chip",6},{"wave",2},{"sustain",1},{"poly",1} } },
        { "AY - Tone+Noise",        { {"chip",6},{"wave",4},{"decay",0.2f},{"sustain",0.4f} } },

        { "PCE - Organ",            { {"chip",7},{"wave",4},{"gbwave",2},{"sustain",0.9f},{"chorusmix",0.5f} } },
        { "PCE - Pulse Lead",       { {"chip",7},{"wave",0},{"pw",0.35f},{"vibdepth",20},{"delaymix",0.25f} } },

        // --- نویزی و خشن ---
        { "Noisy Arcade - Lead",    { {"chip",4},{"wave",2},{"smooth",0},{"sync",2.8f},{"ring",1.3f},{"fmode",2},{"cutoff",1800},{"reso",0.7f},
                                      {"fenv",1.5f},{"nmpitch",1.5f},{"nmfilter",1.2f},{"nmrate",40},{"nmmix",0.15f},{"bits",6},{"srate",11025},
                                      {"drive",0.35f},{"dtype",1},{"chorusmix",0.3f},{"delaymix",0.25f},{"poly",1},{"glide",0.04f} } },
        { "Noisy Arcade - Arp",     { {"chip",4},{"wave",1},{"smooth",0},{"fmode",2},{"cutoff",2200},{"reso",0.6f},{"nmpitch",0.8f},{"nmrate",60},
                                      {"bits",7},{"srate",14000},{"drive",0.3f},{"dtype",1},{"arpmode",1},{"arpdiv",5},{"arpoct",2},{"arpgate",0.5f},
                                      {"delaymix",0.3f},{"poly",4} } },
        { "Glitch Crush Bass",      { {"chip",0},{"wave",1},{"sustain",1},{"bits",4},{"srate",8000},{"drive",0.5f},{"dtype",2},{"poly",1} } },

        // --- پیچ ---
        { "Laser Zap",              { {"chip",0},{"wave",0},{"pw",0.125f},{"penvamt",36},{"penvtime",0.18f},{"penvshape",2},{"decay",0.25f},{"sustain",0},{"release",0.05f} } },
        { "Chip Kick (pitch env)",  { {"chip",0},{"wave",1},{"penvamt",30},{"penvtime",0.07f},{"decay",0.2f},{"sustain",0},{"release",0.05f} } },
        { "Gliding Lead",           { {"chip",2},{"wave",0},{"glide",0.12f},{"vibshape",1},{"vibdepth",30},{"vibdelay",0.3f},{"poly",1} } },

        // --- آرپژ ---
        { "Trance Gate Arp",        { {"chip",3},{"wave",0},{"pw",0.4f},{"fmode",1},{"cutoff",3000},{"reso",0.3f},{"arpmode",1},{"arpdiv",5},{"arpoct",2},
                                      {"s3on",0},{"s7on",0},{"s11on",0},{"s15on",0},{"s1acc",1},{"s5acc",1},{"s9acc",1},{"s13acc",1},{"delaymix",0.3f} } },
        { "Accel Arp (speed ramp)", { {"chip",1},{"wave",0},{"pw",0.25f},{"arpmode",3},{"arpsync",1},{"arphz",6},{"arpramp",2},{"arpramptime",3},{"arpgate",0.7f} } },

        // --- لیدها ---
        { "Lead - NES Duty Cycle",  { {"chip",0},{"wave",0},{"pw",0.5f},{"pwmdepth",0.8f},{"pwmrate",5},{"vibdepth",20},{"vibdelay",0.25f},{"sustain",0.75f},{"poly",1},{"delaymix",0.2f} } },
        { "Lead - NES Arp Lead",    { {"chip",0},{"wave",0},{"pw",0.25f},{"arp",1},{"arprate",40},{"sustain",0.8f},{"poly",1},{"delaymix",0.25f} } },
        { "Lead - GB Bright",       { {"chip",1},{"wave",0},{"pw",0.5f},{"vibdepth",18},{"vibdelay",0.3f},{"sustain",0.85f},{"poly",1},{"chorusmix",0.25f} } },
        { "Lead - GB Wave Saw",     { {"chip",1},{"wave",2},{"sustain",0.9f},{"vibdepth",15},{"vibdelay",0.2f},{"poly",1},{"delaymix",0.2f} } },
        { "Lead - SMS Arcade",      { {"chip",2},{"wave",0},{"vibdepth",25},{"vibrate",7},{"vibdelay",0.2f},{"poly",1},{"delaymix",0.3f},{"chorusmix",0.2f} } },
        { "Lead - SID PWM",         { {"chip",3},{"wave",0},{"pw",0.5f},{"pwmdepth",0.7f},{"pwmrate",3},{"fmode",1},{"cutoff",3500},{"reso",0.3f},{"chorusmix",0.5f},{"poly",1} } },
        { "Lead - SID Hard Sync",   { {"chip",3},{"wave",2},{"sync",2.5f},{"fmode",1},{"cutoff",4500},{"reso",0.2f},{"vibdepth",12},{"vibdelay",0.3f},{"poly",1},{"delaymix",0.25f} } },
        { "Lead - SID Thick Saw",   { {"chip",3},{"wave",2},{"fmode",1},{"cutoff",2800},{"reso",0.35f},{"fenv",1.2f},{"decay",0.3f},{"sustain",0.7f},{"chorusmix",0.6f},{"delaymix",0.25f} } },
        { "Lead - POKEY Distort",   { {"chip",4},{"wave",2},{"sustain",0.9f},{"vibdepth",14},{"vibdelay",0.3f},{"drive",0.25f},{"dtype",1},{"poly",1},{"delaymix",0.2f} } },
        { "Lead - AY Super",        { {"chip",6},{"wave",0},{"arp",1},{"arprate",50},{"vibdepth",12},{"vibdelay",0.2f},{"poly",1},{"chorusmix",0.3f} } },
        { "Lead - PCE Wave Saw",    { {"chip",7},{"wave",4},{"gbwave",1},{"vibdepth",16},{"vibdelay",0.3f},{"chorusmix",0.5f},{"poly",1},{"delaymix",0.25f} } },
        { "Lead - Trill",           { {"chip",0},{"wave",0},{"pw",0.25f},{"vibshape",2},{"vibrate",12},{"vibdepth",100},{"vibdelay",0.4f},{"poly",1} } },
        { "Lead - Pitch Dive",      { {"chip",2},{"wave",0},{"penvamt",12},{"penvtime",0.12f},{"glide",0.08f},{"vibdepth",20},{"vibdelay",0.25f},{"poly",1} } },

        // --- درام کیت‌ها (مود Drums: نت C3=کیک، D3=اسنیر، F#3=هت بسته، A#3=هت باز) ---
        { "Drums - NES Kit",        { {"mode",1},{"chip",0},{"noiseshort",0} } },
        { "Drums - NES Metallic",   { {"mode",1},{"chip",0},{"noiseshort",1},{"hatdecay",0.03f},{"snaredecay",0.14f} } },
        { "Drums - Game Boy Kit",   { {"mode",1},{"chip",1},{"kickdecay",0.24f},{"snaretone",0.4f} } },
        { "Drums - SMS Kit",        { {"mode",1},{"chip",2},{"kickdecay",0.3f},{"snaretone",0.35f} } },
        { "Drums - C64 Kit",        { {"mode",1},{"chip",3},{"kickdecay",0.32f},{"snaretone",0.6f},{"snaredecay",0.2f} } },
        { "Drums - Atari Kit",      { {"mode",1},{"chip",5},{"kickdecay",0.26f},{"snaretone",0.3f} } },
        { "Drums - AY Kit",         { {"mode",1},{"chip",6},{"kickdecay",0.3f} } },
        { "Drums - PC Engine Kit",  { {"mode",1},{"chip",7},{"kickdecay",0.3f},{"kicksweep",30} } },
        { "Drums - Crushed Kit",    { {"mode",1},{"chip",0},{"bits",6},{"srate",14000},{"drive",0.3f},{"dtype",1},{"kicksweep",30},{"snaretone",0.45f} } },
        { "Drums - Tight 808-bit",  { {"mode",1},{"chip",1},{"kickdecay",0.5f},{"kicksweep",18},{"kicktune",-4},{"snaredecay",0.22f},{"hatdecay",0.025f},{"ohdecay",0.45f} } },
    };
    return presets;
}
}

int ChipSynthProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String ChipSynthProcessor::getProgramName (int index)
{
    const auto& p = getPresets();
    return (index >= 0 && index < (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void ChipSynthProcessor::setCurrentProgram (int index)
{
    if (index != currentProgram) applyPreset (index);
}

void ChipSynthProcessor::applyPreset (int index)
{
    const auto& presets = getPresets();
    if (index < 0 || index >= (int) presets.size()) return;
    currentProgram = index;

    for (auto* prm : getParameters())            // اول همه چیز به حالت پیش‌فرض
        prm->setValueNotifyingHost (prm->getDefaultValue());

    for (const auto& kv : presets[(size_t) index].values)
        if (auto* prm = apvts.getParameter (kv.first))
            prm->setValueNotifyingHost (prm->convertTo0to1 (kv.second));
}

void ChipSynthProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    for (auto& v : voices) v.stage = chip::Idle;
    pitchBendNorm = 0.0f;
    lastNote = -1;
    arpRunner.reset();
    lastArpMode = 0;
    lastDrumMode = false;
    for (auto& d : drums) d.active = false;
    fx.prepare (sr);
    crusher.prepare (sr);
}

bool ChipSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void ChipSynthProcessor::noteOn (int note, float vel, const chip::Params& p)
{
    const int limit = juce::jlimit (1, (int) voices.size(), p.polyphony);
    chip::Voice* target = nullptr;

    for (int i = 0; i < limit; ++i)
        if (voices[(size_t) i].stage == chip::Idle) { target = &voices[(size_t) i]; break; }

    if (target == nullptr)   // قدیمی‌ترین صدا رو بگیر
    {
        target = &voices[0];
        for (int i = 1; i < limit; ++i)
            if (voices[(size_t) i].age < target->age) target = &voices[(size_t) i];
    }

    target->noteOn (note, vel, p, ++orderCounter);
    if (p.glide > 0.001f && lastNote >= 0)
        target->glideCur = (float) (lastNote - note);   // از نت قبلی سر می‌خوره
    lastNote = note;
}

void ChipSynthProcessor::noteOffNote (int note, const chip::Params& p)
{
    for (auto& v : voices)
        if (v.stage != chip::Idle && v.note == note) v.noteOff (p);
}

void ChipSynthProcessor::handleMidi (const juce::MidiMessage& m, const chip::Params& p, int arpMode, bool latch, bool drumMode)
{
    auto off = [this, &p] (int n) { noteOffNote (n, p); };

    if (m.isNoteOn())
    {
        if (drumMode)     triggerDrum (m.getNoteNumber(), m.getFloatVelocity(), p);
        else if (arpMode != 0) arpRunner.press (m.getNoteNumber(), m.getFloatVelocity(), latch);
        else              noteOn (m.getNoteNumber(), m.getFloatVelocity(), p);
    }
    else if (m.isNoteOff())
    {
        if (drumMode)     { /* درام‌ها one-shot هستن */ }
        else if (arpMode != 0) arpRunner.release (m.getNoteNumber(), latch, off);
        else              noteOffNote (m.getNoteNumber(), p);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) v.stage = chip::Idle;
        for (auto& d : drums) d.active = false;
        arpRunner.reset();
    }
    else if (m.isPitchWheel())
        pitchBendNorm = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
}

void ChipSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);   // نت‌های کیبورد روی صفحه

    const auto p   = readParams();
    const auto fxp = readFx();
    const auto cp  = readCrush();
    const bool drumMode = getParam ("mode") > 0.5f;
    const auto dp = readDrum (p);
    const int  arpMode = drumMode ? 0 : (int) getParam ("arpmode");
    const bool latch   = getParam ("arplatch") > 0.5f;

    // --- تنظیمات آرپژیاتور ---
    chip::ArpSettings as;
    as.mode     = arpMode;
    as.octaves  = (int) getParam ("arpoct");
    as.patLen   = (int) getParam ("arplen");
    as.speed    = getParam ("arpspeed");
    as.swing    = getParam ("arpswing");
    as.gate     = getParam ("arpgate");
    as.ramp     = getParam ("arpramp");
    as.rampTime = getParam ("arpramptime");
    as.latch    = latch;
    for (size_t i = 0; i < 16; ++i)
    {
        as.stepOn[i]  = stepOnP[i]->load()  > 0.5f;
        as.stepAcc[i] = stepAccP[i]->load() > 0.5f;
        as.stepTr[i]  = (int) std::round (stepTrP[i]->load());
    }

    if ((int) getParam ("arpsync") == 0)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
        static const double beats[10] = { 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
        const int divIdx = juce::jlimit (0, 9, (int) getParam ("arpdiv"));
        as.baseStep = 60.0 / std::max (20.0, bpm) * beats[divIdx] * sampleRate;
    }
    else
        as.baseStep = sampleRate / std::max (0.1, (double) getParam ("arphz"));

    auto arpOn  = [this, &p] (int n, float v) { noteOn (n, v, p); };
    auto arpOff = [this, &p] (int n) { noteOffNote (n, p); };

    if (arpMode != lastArpMode || drumMode != lastDrumMode)   // موقع عوض‌کردن مود، همه‌چیز رو آزاد کن
    {
        for (auto& v : voices) v.noteOff (p);
        arpRunner.reset();
        lastArpMode = arpMode;
        lastDrumMode = drumMode;
    }
    if (arpMode != 0 && ! latch && arpRunner.arp.physDown == 0 && arpRunner.arp.count > 0)
    {
        arpRunner.arp.clearHeld();          // Latch رو خاموش کردی و کلیدی پایین نیست
        arpRunner.closeAll (arpOff);
    }

    const int numSamples = buffer.getNumSamples();
    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float bend = pitchBendNorm * p.bendRange;

    auto renderRange = [&] (int from, int to)
    {
        for (int i = from; i < to; ++i)
        {
            if (arpMode != 0) arpRunner.tick (as, sampleRate, arpOn, arpOff);

            float s = 0.0f;
            for (auto& v : voices) s += v.render (p, sampleRate, bend);
            for (auto& dv : drums) s += dv.render (dp, p.chip, sampleRate);
            s *= p.master * 0.85f;
            s = crusher.process (s, cp);

            float l, r;
            fx.process (s, l, r, fxp);
            l = std::tanh (l);
            r = std::tanh (r);
            if (right != nullptr) { left[i] = l; right[i] = r; }
            else                  { left[i] = 0.5f * (l + r); }
        }
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        renderRange (pos, at);
        pos = at;
        handleMidi (meta.getMessage(), p, arpMode, latch, drumMode);
    }
    renderRange (pos, numSamples);
}

juce::AudioProcessorEditor* ChipSynthProcessor::createEditor()
{
    return new ChipSynthEditor (*this);
}

void ChipSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChipSynthProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipSynthProcessor();
}
