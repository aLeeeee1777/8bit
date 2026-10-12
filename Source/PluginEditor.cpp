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
    combo (sound, "mode", "Mode (Synth / Drums)");
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

    auto& fxP = make ("EFFECTS", 0, 1);
    knob (fxP, "chorusmix", "Chorus");
    knob (fxP, "chorusrate", "Ch Rate");
    knob (fxP, "chorusdepth", "Ch Depth");
    knob (fxP, "delaymix", "Delay");
    knob (fxP, "delaytime", "Dly Time");
    knob (fxP, "delayfb", "Dly Fdbk");

    auto& outP = make ("OUTPUT", 0, 1);
    knob (outP, "poly", "Polyphony");
    knob (outP, "velsens", "Velocity");
    knob (outP, "master", "Master");

    // ================= صفحه ۱: PITCH & MOD =================
    auto& pitch = make ("PITCH", 1, 0);
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

    auto& mod = make ("MODULATION & NOISE", 1, 1);
    knob (mod, "sync", "Hard Sync");
    knob (mod, "ring", "Ring Mod");
    knob (mod, "nmpitch", "Noise>Pitch");
    knob (mod, "nmfilter", "Noise>Filter");
    knob (mod, "nmrate", "Noise Rate");
    knob (mod, "nmmix", "Noise Mix");

    auto& crush = make ("CRUSH & DRIVE", 1, 1);
    knob  (crush, "bits", "Bit Depth");
    knob  (crush, "srate", "Sample Rate");
    knob  (crush, "drive", "Drive");
    combo (crush, "dtype", "Drive Type", 100);

    // ================= صفحه ۲: ARP & STEPS =================
    auto& arpP = make ("ARPEGGIATOR (held notes)", 2, 0);
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

    auto& chipArp = make ("CHIP ARP (per-voice, frame rate)", 2, 0);
    combo (chipArp, "arp", "Pattern", 160);
    knob  (chipArp, "arprate", "Rate Hz");

    auto& steps = make ("STEP SEQUENCER", 2, 1);
    steps.add (std::make_unique<StepGrid> (proc.apvts), 440, 182);

    // ================= صفحه ۳: DRUMS =================
    auto& kit = make ("DRUM KIT  (Mode = Drums)", 3, 0);
    combo (kit, "mode", "Mode", 140);
    combo (kit, "chip", "Chip flavor", 200);
    knob  (kit, "drumvol", "Volume");
    toggle (kit, "noiseshort", "Metallic noise (short mode)");
    toggle (kit, "authentic", "Authentic (stepped, 4-bit)");

    auto& kick = make ("KICK & SNARE", 3, 0);
    knob (kick, "kicktune", "Kick Tune");
    knob (kick, "kickdecay", "Kick Decay");
    knob (kick, "kicksweep", "Kick Punch");
    knob (kick, "snaretune", "Snare Tune");
    knob (kick, "snaretone", "Snare Tone");
    knob (kick, "snaredecay", "Snare Decay");

    auto& hats = make ("HATS & TOMS", 3, 1);
    knob (hats, "hatdecay", "Hat Decay");
    knob (hats, "ohdecay", "Open Hat Decay");
    knob (hats, "tomtune", "Tom Tune");

    auto& pads = make ("PAD MAP  (FL Studio note names)", 3, 1);
    juce::StringArray lines;
    lines.add ("C3 Kick      C#3 Rim       D3 Snare     D#3 Clap");
    lines.add ("E3 Snare 2   F3 Tom 1      F#3 Closed Hat   G3 Tom 2");
    lines.add ("G#3 Pedal Hat   A3 Tom 3   A#3 Open Hat     B3 Tom 4");
    lines.add ("C4 Tom 5      C#4 Crash     D4 Tom 6     D#4 Ride");
    lines.add ("G#4 Cowbell");
    lines.add ("(MIDI 36-51 and 56, General MIDI layout)");
    pads.add (std::make_unique<InfoBox> (lines), 440, lines.size() * 20 + 4);

    // ================= تب‌ها =================
    static const char* tabNames[4] = { "SOUND", "PITCH & MOD", "ARP & STEPS", "DRUMS" };
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

    for (int page = 0; page < 4; ++page)
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
    g.drawText ("ChipSynth  v0.4", 14, 8, 400, 30, juce::Justification::centredLeft);
}

void ChipSynthEditor::resized()
{
    layoutAll (true);
    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i].setBounds (10 + (int) i * 134, 42, 128, 28);
    presetBox.setBounds (getWidth() - 274, 8, 260, 28);
    keyboard.setBounds (10, getHeight() - 78, getWidth() - 20, 70);
}
