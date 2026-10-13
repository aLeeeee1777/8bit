#include "PluginEditor.h"

ChipSynthEditor::ChipSynthEditor (ChipSynthProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    auto make = [this] (const char* title, int page, int col) -> Panel&
    {
        panels.push_back (std::make_unique<Panel> (title));
        panels.back()->page = page;
        panels.back()->col = col;
        addAndMakeVisible (*panels.back());
        return *panels.back();
    };

    // ================= صفحه ۰: SOUND =================
    auto& sound = make ("SOUND", 0, 0);
    combo (sound, "mode", "Mode (Synth / Drums / Sampler)");
    combo (sound, "chip", "Chip");
    combo (sound, "wave", "Waveform");
    combo (sound, "gbwave", "Wave Table");
    knob  (sound, "pw", "Pulse Width");
    knob  (sound, "pwmdepth", "PWM Depth");
    knob  (sound, "pwmrate", "PWM Rate");
    toggle (sound, "noiseshort", "Noise short mode");
    toggle (sound, "smooth", "Anti-alias (4x)");
    toggle (sound, "authentic", "Authentic tuning");

    auto& env = make ("ENVELOPE", 0, 0);
    knob (env, "attack", "Attack");
    knob (env, "decay", "Decay");
    knob (env, "sustain", "Sustain");
    knob (env, "release", "Release");

    auto& filter = make ("FILTER", 0, 1);
    combo (filter, "fmode", "Mode", 130);
    knob  (filter, "cutoff", "Cutoff");
    knob  (filter, "reso", "Resonance");
    knob  (filter, "fenv", "Env Amount");

    auto& outP = make ("OUTPUT", 0, 1);
    knob (outP, "poly", "Polyphony");
    knob (outP, "velsens", "Velocity");
    knob (outP, "master", "Master");

    // ================= صفحه ۱: LAYERS & UNISON =================
    auto& osc1 = make ("OSC 1  +  UNISON", 1, 0);
    knob (osc1, "o1level", "Osc 1 Level");
    knob (osc1, "unison", "Unison Voices");
    knob (osc1, "unidetune", "Detune (cents)");
    knob (osc1, "unispread", "Stereo Spread");

    auto addLayer = [this, &make] (const char* title, const char* pre, int col)
    {
        auto& pn = make (title, 1, col);
        const juce::String k (pre);
        toggle (pn, k + "on", "Enabled");
        toggle (pn, k + "short", "Noise short mode");
        combo  (pn, k + "chip", "Chip");
        combo  (pn, k + "wave", "Waveform");
        combo  (pn, k + "gb", "Wave Table");
        knob   (pn, k + "pw", "Pulse Width");
        knob   (pn, k + "level", "Level");
        knob   (pn, k + "semi", "Semitones");
        knob   (pn, k + "fine", "Fine");
    };
    addLayer ("OSC 2  (extra layer)", "l2", 0);
    addLayer ("OSC 3  (extra layer)", "l3", 1);

    // ================= صفحه ۲: PITCH & MOD =================
    auto& pitch = make ("PITCH", 2, 0);
    combo (pitch, "penvshape", "Pitch Env Shape", 140);
    combo (pitch, "vibshape", "Vibrato Shape", 140);
    knob (pitch, "transpose", "Transpose");
    knob (pitch, "fine", "Fine Tune");
    knob (pitch, "bendrange", "Bend Range");
    knob (pitch, "penvamt", "Pitch Env");
    knob (pitch, "penvtime", "Env Time");
    knob (pitch, "glide", "Glide");
    knob (pitch, "slide", "Slide");
    knob (pitch, "slidetime", "Slide Time");
    knob (pitch, "vibrate", "Vib Rate");
    knob (pitch, "vibdepth", "Vib Depth");
    knob (pitch, "vibdelay", "Vib Delay");

    auto& mod = make ("MODULATION & NOISE", 2, 1);
    knob (mod, "sync", "Hard Sync");
    knob (mod, "ring", "Ring Mod");
    knob (mod, "nmpitch", "Noise>Pitch");
    knob (mod, "nmfilter", "Noise>Filter");
    knob (mod, "nmrate", "Noise Rate");
    knob (mod, "nmmix", "Noise Mix");

    auto& crush = make ("CRUSH & DRIVE", 2, 1);
    knob  (crush, "bits", "Bit Depth");
    knob  (crush, "srate", "Sample Rate");
    knob  (crush, "drive", "Drive");
    combo (crush, "dtype", "Drive Type", 100);

    // ================= صفحه ۳: MATRIX =================
    auto& lfos = make ("LFO 1 / LFO 2 / MOD ENV  (per note)", 3, 0);
    combo (lfos, "lfo1shape", "LFO 1 Shape", 130);
    knob  (lfos, "lfo1rate", "LFO 1 Rate");
    knob  (lfos, "lfo1delay", "LFO 1 Fade-in");
    combo (lfos, "lfo2shape", "LFO 2 Shape", 130);
    knob  (lfos, "lfo2rate", "LFO 2 Rate");
    knob  (lfos, "lfo2delay", "LFO 2 Fade-in");
    knob  (lfos, "menvatk", "ModEnv Atk");
    knob  (lfos, "menvdec", "ModEnv Dec");

    auto& glfo = make ("GLOBAL LFO A / B  (free-running)", 3, 0);
    combo (glfo, "glfoashape", "LFO A Shape", 130);
    knob  (glfo, "glfoarate", "LFO A Rate");
    combo (glfo, "glfobshape", "LFO B Shape", 130);
    knob  (glfo, "glfobrate", "LFO B Rate");

    auto& vm = make ("VOICE MATRIX  (source > destination, amount)", 3, 1);
    for (int i = 1; i <= 8; ++i)
    {
        const juce::String n (i);
        slot (vm, "m" + n + "src", "m" + n + "dst", "m" + n + "amt");
    }
    auto& gm = make ("GLOBAL MATRIX  (crush, FX, master)", 3, 1);
    for (int i = 1; i <= 4; ++i)
    {
        const juce::String n (i);
        slot (gm, "g" + n + "src", "g" + n + "dst", "g" + n + "amt");
    }

    // ================= صفحه ۴: ARP & STEPS =================
    auto& arpP = make ("ARPEGGIATOR (held notes)", 4, 0);
    combo (arpP, "arpmode", "Mode", 130);
    combo (arpP, "arpsync", "Clock", 130);
    combo (arpP, "arpdiv", "Rate (sync)", 130);
    knob  (arpP, "arphz", "Free Hz");
    knob  (arpP, "arpspeed", "Speed x");
    knob  (arpP, "arpswing", "Swing");
    knob  (arpP, "arpgate", "Gate");
    knob  (arpP, "arpoct", "Octaves");
    knob  (arpP, "arpramp", "Speed Ramp");
    knob  (arpP, "arpramptime", "Ramp Time");
    knob  (arpP, "arplen", "Pattern Len");
    toggle (arpP, "arplatch", "Latch (hold notes)");

    auto& chipArp = make ("CHIP ARP (per-voice, frame rate)", 4, 0);
    combo (chipArp, "arp", "Pattern", 160);
    knob  (chipArp, "arprate", "Rate Hz");

    auto& steps = make ("STEP SEQUENCER", 4, 1);
    steps.add (std::make_unique<StepGrid> (proc.apvts), 440, 182);

    // ================= صفحه ۵: DRUMS =================
    auto& kit = make ("DRUM KIT  (Mode = Drums)", 5, 0);
    combo (kit, "mode", "Mode", 140);
    combo (kit, "chip", "Chip flavor", 200);
    knob  (kit, "drumvol", "Volume");
    toggle (kit, "noiseshort", "Metallic noise (short mode)");
    toggle (kit, "authentic", "Authentic (stepped, 4-bit)");

    auto& kick = make ("KICK & SNARE", 5, 0);
    knob (kick, "kicktune", "Kick Tune");
    knob (kick, "kickdecay", "Kick Decay");
    knob (kick, "kicksweep", "Kick Punch");
    knob (kick, "kickclick", "Kick Click");
    knob (kick, "kickdrive", "Kick Drive");
    knob (kick, "kicksub", "Kick Sub");
    knob (kick, "snaretune", "Snare Tune");
    knob (kick, "snaretone", "Snare Tone");
    knob (kick, "snaredecay", "Snare Decay");

    auto& hats = make ("HATS & TOMS", 5, 1);
    knob (hats, "hatdecay", "Hat Decay");
    knob (hats, "ohdecay", "Open Hat Decay");
    knob (hats, "tomtune", "Tom Tune");

    auto& pads = make ("PAD MAP  (FL Studio note names)", 5, 1);
    juce::StringArray padLines;
    padLines.add ("C3 Kick      C#3 Rim       D3 Snare     D#3 Clap");
    padLines.add ("E3 Snare 2   F3 Tom 1      F#3 Closed Hat   G3 Tom 2");
    padLines.add ("G#3 Pedal Hat   A3 Tom 3   A#3 Open Hat     B3 Tom 4");
    padLines.add ("C4 Tom 5      C#4 Crash     D4 Tom 6     D#4 Ride");
    padLines.add ("G#4 Cowbell");
    padLines.add ("(MIDI 36-51 and 56, General MIDI layout)");
    pads.add (std::make_unique<InfoBox> (padLines), 440, padLines.size() * 20 + 4);

    // ================= صفحه ۶: SAMPLER =================
    auto& smp = make ("SAMPLER  (Mode = Sampler)", 6, 0);
    smp.add (std::make_unique<SamplerLoader> (proc), 440, 58);
    combo (smp, "mode", "Mode", 140);
    combo (smp, "smploop", "Loop", 140);
    knob  (smp, "smproot", "Root Note");
    knob  (smp, "smpstart", "Start");
    knob  (smp, "smpend", "End");
    knob  (smp, "smpgain", "Gain");
    knob  (smp, "smpbits", "Bits (DAC)");
    knob  (smp, "smprate", "Rate (DAC)");

    auto& smpInfo = make ("HOW TO USE", 6, 1);
    juce::StringArray sl;
    sl.add ("1) Load a wav/aiff/flac/ogg file (max 30 s).");
    sl.add ("2) Set Mode = Sampler. Play it on the keyboard.");
    sl.add ("3) Root Note = the key that plays it at original pitch.");
    sl.add ("4) Bits / Rate = chip-DAC lo-fi (try 4 bits, 8000 Hz).");
    sl.add ("5) Envelope (SOUND tab), Crush, Reverb and FX all apply.");
    sl.add ("The sample path is saved inside your FL project.");
    smpInfo.add (std::make_unique<InfoBox> (sl), 440, sl.size() * 20 + 4);

    // ================= صفحه ۷: FX =================
    auto& fxP = make ("CHORUS & DELAY", 7, 0);
    knob (fxP, "chorusmix", "Chorus");
    knob (fxP, "chorusrate", "Ch Rate");
    knob (fxP, "chorusdepth", "Ch Depth");
    knob (fxP, "delaymix", "Delay");
    knob (fxP, "delaytime", "Dly Time");
    knob (fxP, "delayfb", "Dly Fdbk");

    auto& rev = make ("REVERB  (Lo-Fi = crushed, dark tail)", 7, 1);
    knob (rev, "revmix", "Reverb");
    knob (rev, "revsize", "Size");
    knob (rev, "revdecay", "Decay");
    knob (rev, "revdamp", "Damping");
    knob (rev, "revpre", "Pre-delay");
    knob (rev, "revlofi", "Lo-Fi");
    knob (rev, "revwidth", "Width");

    // ================= صفحه ۸: SFX (تفنگ پیکسلی، لیزر، انفجار) =================
    auto& sfx = make ("PIXEL SFX  (Mode = SFX)", 8, 0);
    combo (sfx, "mode", "Mode", 130);
    combo (sfx, "chip", "Chip flavor", 200);
    combo (sfx, "sfxwave", "Waveform", 130);
    knob  (sfx, "sfxpitch", "Pitch");
    knob  (sfx, "sfxkey", "Key Track");
    knob  (sfx, "sfxslide", "Slide");
    knob  (sfx, "sfxaccel", "Slide Accel");
    knob  (sfx, "sfxstep", "Step Rate");
    knob  (sfx, "sfxattack", "Attack");
    knob  (sfx, "sfxhold", "Hold");
    knob  (sfx, "sfxdecay", "Decay");
    knob  (sfx, "sfxduty", "Duty");
    knob  (sfx, "sfxdutysweep", "Duty Sweep");
    knob  (sfx, "sfxvibdepth", "Vib Depth");
    knob  (sfx, "sfxvibrate", "Vib Rate");
    knob  (sfx, "sfxjump", "Pitch Jump");
    knob  (sfx, "sfxjumptime", "Jump Time");
    knob  (sfx, "sfxnoise", "Noise");
    knob  (sfx, "sfxretrig", "Retrigger Hz");
    knob  (sfx, "sfxvol", "Volume");

    auto& gen = make ("GENERATOR  (click, then play a key)", 8, 1);
    gen.add (std::make_unique<GenButtons> (proc), 440, 76);

    auto& sfxInfo = make ("HOW TO USE", 8, 1);
    juce::StringArray fl;
    fl.add ("Shoot / Explosion / ... = generate a new sound.");
    fl.add ("Mutate = small random change of the current one.");
    fl.add ("Play any key (MIDI or on-screen) to fire it.");
    fl.add ("Key Track 0 = every key fires the same sound.");
    fl.add ("Retrigger Hz > 0 = machine gun while key is held.");
    fl.add ("Step Rate = pitch updates like old consoles (60 Hz).");
    fl.add ("Slide = how fast the pitch drops (the 'pew').");
    fl.add ("Chip flavor, Crush, Reverb and FX shape the result.");
    sfxInfo.add (std::make_unique<InfoBox> (fl), 440, fl.size() * 20 + 4);

    // ================= تب‌ها =================
    static const char* tabNames[numPages] = { "SOUND", "LAYERS", "PITCH & MOD", "MATRIX", "ARP & STEPS", "DRUMS", "SAMPLER", "FX", "SFX" };
    for (size_t i = 0; i < tabs.size(); ++i)
    {
        tabs[i].setButtonText (tabNames[i]);
        tabs[i].setClickingTogglesState (true);
        tabs[i].setRadioGroupId (1001);
        tabs[i].setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3b6ea5));
        const int idx = (int) i;
        tabs[i].onClick = [this, idx] { showPage (idx); };
        addAndMakeVisible (tabs[i]);
    }
    tabs[0].setToggleState (true, juce::dontSendNotification);

    // ================= منوی پریست =================
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Presets...");
    presetBox.onChange = [this] { proc.applyPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    // ================= کیبورد روی صفحه =================
    keyboard.setAvailableRange (24, 96);
    keyboard.setKeyWidth (16.0f);
    addAndMakeVisible (keyboard);

    // هیچ کنترلی فوکوس کیبورد نگیره تا کلیدها به FL برسه (تایپینگ کیبورد / کپس‌لاک)
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    for (auto& pnl : panels) disableFocus (*pnl);
    for (auto& t : tabs) disableFocus (t);
    disableFocus (presetBox);
    keyboard.setWantsKeyboardFocus (true);
    keyboard.setMouseClickGrabsKeyboardFocus (true);

    setSize (960, 700);
    setSize (960, layoutAll (false) + 84);
    showPage (0);
}

int ChipSynthEditor::layoutAll (bool apply)
{
    const int top = 76, gap = 8, margin = 10;
    const int colW = (getWidth() - 2 * margin - gap) / 2;
    int maxY = top;

    for (int page = 0; page < numPages; ++page)
    {
        int y[2] = { top, top };
        for (auto& pn : panels)
        {
            if (pn->page != page) continue;
            const int c = pn->col;
            const int h = pn->doLayout (colW, apply);
            if (apply) pn->setBounds (margin + c * (colW + gap), y[c], colW, h);
            y[c] += h + gap;
        }
        maxY = std::max (maxY, std::max (y[0], y[1]));
    }
    return maxY + margin;
}

void ChipSynthEditor::showPage (int index)
{
    currentPage = index;
    for (auto& pn : panels) pn->setVisible (pn->page == index);
    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i].setToggleState ((int) i == index, juce::dontSendNotification);
}

void ChipSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b22));
    g.setColour (juce::Colours::white);
    g.setFont (22.0f);
    g.drawText ("ChipSynth  v0.6", 14, 8, 400, 30, juce::Justification::centredLeft);
}

void ChipSynthEditor::resized()
{
    layoutAll (true);
    const int tabW = (getWidth() - 20) / numPages;
    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i].setBounds (10 + (int) i * tabW, 42, tabW - 4, 28);
    presetBox.setBounds (getWidth() - 274, 8, 260, 28);
    keyboard.setBounds (10, getHeight() - 78, getWidth() - 20, 70);
}
