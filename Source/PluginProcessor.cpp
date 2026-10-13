#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

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
    for (int i = 0; i < 8; ++i)
    {
        const juce::String n (i + 1);
        mSrcP[(size_t) i] = apvts.getRawParameterValue ("m" + n + "src");
        mDstP[(size_t) i] = apvts.getRawParameterValue ("m" + n + "dst");
        mAmtP[(size_t) i] = apvts.getRawParameterValue ("m" + n + "amt");
    }
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n (i + 1);
        gSrcP[(size_t) i] = apvts.getRawParameterValue ("g" + n + "src");
        gDstP[(size_t) i] = apvts.getRawParameterValue ("g" + n + "dst");
        gAmtP[(size_t) i] = apvts.getRawParameterValue ("g" + n + "amt");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ChipSynthProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addChoice = [&] (const String& id, const String& name, StringArray items, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    auto addFloat = [&] (const String& id, const String& name, float lo, float hi, float def, float skew = 1.0f)
    { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                         NormalisableRange<float> (lo, hi, 0.0f, skew), def)); };

    auto addBool = [&] (const String& id, const String& name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    auto addInt = [&] (const String& id, const String& name, int lo, int hi, int def)
    { layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };

    const StringArray chipNames { "NES (2A03)", "Game Boy (DMG)", "Master System (SN76489)", "C64 (SID 6581)",
                                  "Atari POKEY", "Atari 2600 (TIA)", "AY-3-8910 / YM2149", "PC Engine (HuC6280)" };
    const StringArray waveNames { "Pulse", "Triangle", "Saw", "Noise", "Wavetable" };
    const StringArray tableNames { "Triangle", "Saw", "Sine", "Pulse 25%" };
    const StringArray lfoShapes { "Sine", "Triangle", "Square", "Saw", "Random (S&H)" };

    // --- صدا ---
    addChoice ("mode", "Mode", { "Synth", "Drums", "Sampler", "SFX" }, 0);
    addChoice ("chip", "Chip", chipNames, 0);
    addChoice ("wave", "Waveform", waveNames, 0);
    addChoice ("gbwave", "Wave Table", tableNames, 0);
    addFloat  ("pw", "Pulse Width", 0.05f, 0.95f, 0.5f);
    addFloat  ("pwmrate", "PWM Rate", 0.1f, 20.0f, 4.0f, 0.5f);
    addFloat  ("pwmdepth", "PWM Depth", 0.0f, 1.0f, 0.0f);
    addBool   ("noiseshort", "Noise Short Mode", false);
    addBool   ("smooth", "Anti-alias (4x)", true);
    addBool   ("authentic", "Authentic Tuning", true);

    // --- لایه‌ها و Unison ---
    addFloat ("o1level", "Osc 1 Level", 0.0f, 1.5f, 1.0f);
    addInt   ("unison", "Unison Voices", 1, 5, 1);
    addFloat ("unidetune", "Unison Detune", 0.0f, 100.0f, 12.0f);
    addFloat ("unispread", "Unison Spread", 0.0f, 1.0f, 0.6f);
    for (int k = 2; k <= 3; ++k)
    {
        const String pre ("l" + String (k)), nm ("Osc " + String (k) + " ");
        addBool   (pre + "on", nm + "On", false);
        addChoice (pre + "chip", nm + "Chip", chipNames, k == 2 ? 3 : 4);
        addChoice (pre + "wave", nm + "Waveform", waveNames, k == 2 ? 2 : 0);
        addChoice (pre + "gb", nm + "Wave Table", tableNames, 0);
        addFloat  (pre + "pw", nm + "Pulse Width", 0.05f, 0.95f, 0.5f);
        addFloat  (pre + "level", nm + "Level", 0.0f, 1.5f, 0.7f);
        addInt    (pre + "semi", nm + "Semitones", -24, 24, k == 2 ? -12 : 0);
        addFloat  (pre + "fine", nm + "Fine", -100.0f, 100.0f, 0.0f);
        addBool   (pre + "short", nm + "Noise Short", false);
    }

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

    // --- LFO ها و ModEnv (هر نت) ---
    for (int k = 1; k <= 2; ++k)
    {
        const String pre ("lfo" + String (k)), nm ("LFO " + String (k) + " ");
        addChoice (pre + "shape", nm + "Shape", lfoShapes, 0);
        addFloat  (pre + "rate", nm + "Rate", 0.05f, 40.0f, 2.0f, 0.4f);
        addFloat  (pre + "delay", nm + "Fade-in", 0.0f, 3.0f, 0.0f, 0.5f);
    }
    addFloat ("menvatk", "ModEnv Attack", 0.001f, 2.0f, 0.005f, 0.4f);
    addFloat ("menvdec", "ModEnv Decay", 0.01f, 4.0f, 0.5f, 0.4f);

    // --- ماتریکس هر نت (۸ اسلات) ---
    const StringArray srcNames { "None", "LFO 1", "LFO 2", "Mod Env", "Noise S&H", "Velocity", "Note Random", "Key Track" };
    const StringArray dstNames { "None", "Pitch", "Cutoff", "Resonance", "Pulse Width", "Level (use -)", "Hard Sync",
                                 "Ring Mod", "Noise Mix", "Osc 2 Level", "Osc 3 Level" };
    for (int i = 1; i <= 8; ++i)
    {
        const String n (i);
        addChoice ("m" + n + "src", "Mod " + n + " Source", srcNames, 0);
        addChoice ("m" + n + "dst", "Mod " + n + " Dest", dstNames, 0);
        addFloat  ("m" + n + "amt", "Mod " + n + " Amount", -1.0f, 1.0f, 0.0f);
    }

    // --- LFO های گلوبال + ماتریکس گلوبال (۴ اسلات) ---
    addChoice ("glfoashape", "Global LFO A Shape", lfoShapes, 0);
    addFloat  ("glfoarate", "Global LFO A Rate", 0.05f, 30.0f, 0.5f, 0.4f);
    addChoice ("glfobshape", "Global LFO B Shape", lfoShapes, 3);
    addFloat  ("glfobrate", "Global LFO B Rate", 0.05f, 30.0f, 0.2f, 0.4f);
    const StringArray gsrcNames { "None", "G-LFO A", "G-LFO B" };
    const StringArray gdstNames { "None", "Bit Depth", "Sample Rate", "Drive", "Chorus", "Delay Mix", "Reverb Mix", "Master" };
    for (int i = 1; i <= 4; ++i)
    {
        const String n (i);
        addChoice ("g" + n + "src", "Global " + n + " Source", gsrcNames, 0);
        addChoice ("g" + n + "dst", "Global " + n + " Dest", gdstNames, 0);
        addFloat  ("g" + n + "amt", "Global " + n + " Amount", -1.0f, 1.0f, 0.0f);
    }

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

    // --- درام ---
    addFloat ("kicktune", "Kick Tune", -12.0f, 12.0f, 0.0f);
    addFloat ("kickdecay", "Kick Decay", 0.05f, 0.8f, 0.28f);
    addFloat ("kicksweep", "Kick Punch", 0.0f, 48.0f, 24.0f);
    addFloat ("kickclick", "Kick Click", 0.0f, 1.0f, 0.3f);
    addFloat ("kickdrive", "Kick Drive", 0.0f, 1.0f, 0.0f);
    addFloat ("kicksub", "Kick Sub", 0.0f, 1.0f, 0.0f);
    addFloat ("snaretune", "Snare Tune", -12.0f, 12.0f, 0.0f);
    addFloat ("snaretone", "Snare Tone", 0.0f, 1.0f, 0.5f);
    addFloat ("snaredecay", "Snare Decay", 0.05f, 0.6f, 0.18f);
    addFloat ("hatdecay", "Hat Decay", 0.01f, 0.2f, 0.035f);
    addFloat ("ohdecay", "Open Hat Decay", 0.1f, 1.2f, 0.35f);
    addFloat ("tomtune", "Tom Tune", -12.0f, 12.0f, 0.0f);
    addFloat ("drumvol", "Drum Volume", 0.0f, 1.0f, 0.9f);

    // --- سمپلر ---
    addInt    ("smproot", "Sample Root Note", 0, 127, 60);
    addChoice ("smploop", "Sample Loop", { "One-shot", "Loop" }, 0);
    addFloat  ("smpstart", "Sample Start", 0.0f, 1.0f, 0.0f);
    addFloat  ("smpend", "Sample End", 0.0f, 1.0f, 1.0f);
    addFloat  ("smpgain", "Sample Gain", 0.0f, 2.0f, 1.0f);
    addFloat  ("smpbits", "Sample Bits", 1.0f, 16.0f, 16.0f);
    addFloat  ("smprate", "Sample Rate", 500.0f, 44100.0f, 44100.0f, 0.4f);

    // --- SFX (افکت‌های آرکید) ---
    addChoice ("sfxwave", "SFX Waveform", { "Pulse", "Triangle", "Saw", "Noise" }, 0);
    addFloat  ("sfxpitch", "SFX Pitch", chip::SfxR::pitchLo, chip::SfxR::pitchHi, 18.0f);
    addFloat  ("sfxkey", "SFX Key Track", 0.0f, 1.0f, 1.0f);
    addFloat  ("sfxslide", "SFX Slide", -chip::SfxR::slideLim, chip::SfxR::slideLim, -170.0f);
    addFloat  ("sfxaccel", "SFX Slide Accel", -chip::SfxR::accelLim, chip::SfxR::accelLim, 0.0f);
    addFloat  ("sfxstep", "SFX Step Rate", 0.0f, chip::SfxR::stepHi, 60.0f);
    addFloat  ("sfxattack", "SFX Attack", chip::SfxR::attackLo, chip::SfxR::attackHi, 0.001f, 0.4f);
    addFloat  ("sfxhold", "SFX Hold", 0.0f, chip::SfxR::holdHi, 0.01f, 0.5f);
    addFloat  ("sfxdecay", "SFX Decay", chip::SfxR::decayLo, chip::SfxR::decayHi, 0.16f, 0.5f);
    addFloat  ("sfxduty", "SFX Duty", chip::SfxR::dutyLo, chip::SfxR::dutyHi, 0.5f);
    addFloat  ("sfxdutysweep", "SFX Duty Sweep", -chip::SfxR::dutySweepLim, chip::SfxR::dutySweepLim, 0.0f);
    addFloat  ("sfxvibdepth", "SFX Vib Depth", 0.0f, chip::SfxR::vibDepthHi, 0.0f);
    addFloat  ("sfxvibrate", "SFX Vib Rate", chip::SfxR::vibRateLo, chip::SfxR::vibRateHi, 8.0f, 0.5f);
    addFloat  ("sfxjump", "SFX Pitch Jump", -chip::SfxR::jumpLim, chip::SfxR::jumpLim, 0.0f);
    addFloat  ("sfxjumptime", "SFX Jump Time", 0.0f, chip::SfxR::jumpTimeHi, 0.05f);
    addFloat  ("sfxnoise", "SFX Noise", 0.0f, 1.0f, 0.0f);
    addFloat  ("sfxretrig", "SFX Retrigger Hz", 0.0f, chip::SfxR::retrigHi, 0.0f);
    addFloat  ("sfxvol", "SFX Volume", 0.0f, chip::SfxR::volHi, 1.0f);

    // --- افکت ---
    addFloat ("chorusmix", "Chorus", 0.0f, 1.0f, 0.0f);
    addFloat ("chorusrate", "Ch Rate", 0.1f, 5.0f, 0.8f);
    addFloat ("chorusdepth", "Ch Depth", 0.0f, 8.0f, 3.0f);
    addFloat ("delaytime", "Dly Time", 20.0f, 1000.0f, 300.0f, 0.5f);
    addFloat ("delayfb", "Dly Fdbk", 0.0f, 0.9f, 0.35f);
    addFloat ("delaymix", "Delay", 0.0f, 1.0f, 0.0f);
    addFloat ("revmix", "Reverb", 0.0f, 1.0f, 0.0f);
    addFloat ("revsize", "Rev Size", 0.0f, 1.0f, 0.5f);
    addFloat ("revdecay", "Rev Decay", 0.0f, 1.0f, 0.6f);
    addFloat ("revdamp", "Rev Damping", 0.0f, 1.0f, 0.5f);
    addFloat ("revpre", "Rev Pre-delay", 0.0f, 250.0f, 0.0f);
    addFloat ("revlofi", "Rev Lo-Fi", 0.0f, 1.0f, 0.0f);
    addFloat ("revwidth", "Rev Width", 0.0f, 1.0f, 1.0f);

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

    p.level1     = getParam ("o1level");
    p.unison     = (int) getParam ("unison");
    p.uniDetune  = getParam ("unidetune");
    p.uniSpread  = getParam ("unispread");
    for (int k = 0; k < 2; ++k)
    {
        const juce::String pre ("l" + juce::String (k + 2));
        auto& ly = p.layer[k];
        ly.on         = getParamS (pre + "on") > 0.5f;
        ly.chip       = (int) getParamS (pre + "chip");
        ly.wave       = (int) getParamS (pre + "wave");
        ly.gbWave     = (int) getParamS (pre + "gb");
        ly.pw         = getParamS (pre + "pw");
        ly.level      = getParamS (pre + "level");
        ly.semis      = (int) std::round (getParamS (pre + "semi"));
        ly.fine       = getParamS (pre + "fine");
        ly.noiseShort = getParamS (pre + "short") > 0.5f;
    }

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
    p.transpose  = (int) std::round (getParam ("transpose"));
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

    p.lfo[0].shape = (int) getParam ("lfo1shape");
    p.lfo[0].rate  = getParam ("lfo1rate");
    p.lfo[0].delay = getParam ("lfo1delay");
    p.lfo[1].shape = (int) getParam ("lfo2shape");
    p.lfo[1].rate  = getParam ("lfo2rate");
    p.lfo[1].delay = getParam ("lfo2delay");
    p.menvAtk = getParam ("menvatk");
    p.menvDec = getParam ("menvdec");
    for (size_t i = 0; i < 8; ++i)
    {
        p.slot[i].src = (int) mSrcP[i]->load();
        p.slot[i].dst = (int) mDstP[i]->load();
        p.slot[i].amt = mAmtP[i]->load();
    }
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

chip::ReverbParams ChipSynthProcessor::readReverb() const
{
    chip::ReverbParams r;
    r.mix      = getParam ("revmix");
    r.size     = getParam ("revsize");
    r.decay    = getParam ("revdecay");
    r.damp     = getParam ("revdamp");
    r.predelay = getParam ("revpre");
    r.lofi     = getParam ("revlofi");
    r.width    = getParam ("revwidth");
    return r;
}

chip::DrumParams ChipSynthProcessor::readDrum (const chip::Params& p) const
{
    chip::DrumParams d;
    d.kickTune   = getParam ("kicktune");
    d.kickDecay  = getParam ("kickdecay");
    d.kickSweep  = getParam ("kicksweep");
    d.kickClick  = getParam ("kickclick");
    d.kickDrive  = getParam ("kickdrive");
    d.kickSub    = getParam ("kicksub");
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

chip::SampleParams ChipSynthProcessor::readSampler (const chip::Params& p) const
{
    chip::SampleParams s;
    s.rootNote = (int) std::round (getParam ("smproot"));
    s.loop     = (int) getParam ("smploop");
    s.start    = getParam ("smpstart");
    s.end      = getParam ("smpend");
    s.gain     = getParam ("smpgain");
    s.bits     = getParam ("smpbits");
    s.rate     = getParam ("smprate");
    s.attack   = p.attack;
    s.decay    = p.decay;
    s.sustain  = p.sustain;
    s.release  = p.release;
    return s;
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

void ChipSynthProcessor::triggerSample (int note, float vel, const chip::SampleParams& sp)
{
    chip::SampleVoice* target = nullptr;
    for (auto& v : smpVoices) if (v.stage == chip::SampleVoice::Idle) { target = &v; break; }
    if (target == nullptr)
    {
        target = &smpVoices[0];
        for (auto& v : smpVoices) if (v.age < target->age) target = &v;
    }
    target->trigger (note, vel, audioSample.get(), sp, ++smpOrder);
}

bool ChipSynthProcessor::loadSample (const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0) return false;

    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));   // حداکثر ۳۰ ثانیه
    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);

    auto sd = std::make_shared<chip::SampleData>();
    sd->sampleRate = reader->sampleRate;
    sd->data.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < buf.getNumChannels(); ++c) s += buf.getSample (c, i);
        sd->data[(size_t) i] = s / (float) buf.getNumChannels();
    }

    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        sample = sd;
    }
    samplePath = file.getFullPathName();
    sampleName = file.getFileName();
    return true;
}

chip::SfxParams ChipSynthProcessor::readSfx() const
{
    chip::SfxParams s;
    s.wave = (int) getParam ("sfxwave");
    s.pitch = getParam ("sfxpitch");       s.key = getParam ("sfxkey");
    s.slide = getParam ("sfxslide");       s.accel = getParam ("sfxaccel");     s.step = getParam ("sfxstep");
    s.attack = getParam ("sfxattack");     s.hold = getParam ("sfxhold");       s.decay = getParam ("sfxdecay");
    s.duty = getParam ("sfxduty");         s.dutySweep = getParam ("sfxdutysweep");
    s.vibDepth = getParam ("sfxvibdepth"); s.vibRate = getParam ("sfxvibrate");
    s.jump = getParam ("sfxjump");         s.jumpTime = getParam ("sfxjumptime");
    s.noise = getParam ("sfxnoise");       s.retrig = getParam ("sfxretrig");   s.vol = getParam ("sfxvol");
    return s;
}

void ChipSynthProcessor::writeSfx (const chip::SfxParams& s)
{
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("sfxwave", (float) s.wave);
    set ("sfxpitch", s.pitch);         set ("sfxkey", s.key);
    set ("sfxslide", s.slide);         set ("sfxaccel", s.accel);       set ("sfxstep", s.step);
    set ("sfxattack", s.attack);       set ("sfxhold", s.hold);         set ("sfxdecay", s.decay);
    set ("sfxduty", s.duty);           set ("sfxdutysweep", s.dutySweep);
    set ("sfxvibdepth", s.vibDepth);   set ("sfxvibrate", s.vibRate);
    set ("sfxjump", s.jump);           set ("sfxjumptime", s.jumpTime);
    set ("sfxnoise", s.noise);         set ("sfxretrig", s.retrig);     set ("sfxvol", s.vol);
}

void ChipSynthProcessor::generateSfx (int kind)
{
    sfxRng ^= (uint32_t) juce::Time::getMillisecondCounter() * 2654435761u;
    if (sfxRng == 0) sfxRng = 1u;
    chip::SfxParams s = readSfx();
    if (kind >= 7) chip::sfxMutate (s, sfxRng, 0.12f);
    else           chip::sfxGenerate (kind, sfxRng, s);
    writeSfx (s);
    if (auto* m = apvts.getParameter ("mode")) m->setValueNotifyingHost (m->convertTo0to1 (3.0f));   // مود رو روی SFX بذار
}

void ChipSynthProcessor::triggerSfx (int note, float vel, const chip::Params& p)
{
    chip::SfxVoice* target = nullptr;
    for (auto& v : sfxVoices) if (! v.active) { target = &v; break; }
    if (target == nullptr)
    {
        target = &sfxVoices[0];
        for (auto& v : sfxVoices) if (v.age < target->age) target = &v;
    }
    target->trigger (note, vel, p, ++sfxOrder);
}

namespace
{
using PV = std::vector<std::pair<const char*, float>>;
struct PresetDef { const char* name; PV values; };

// مقدارها: ایندکس برای منوها، مقدار واقعی برای نوب‌ها، 0/1 برای تیک‌ها
// chip: 0 NES, 1 GB, 2 SMS, 3 SID, 4 POKEY, 5 TIA, 6 AY, 7 PCE
// wave: 0 Pulse, 1 Triangle, 2 Saw, 3 Noise, 4 Wavetable
// ماتریکس: src 1 LFO1,2 LFO2,3 ModEnv,4 Noise,5 Vel,6 Rand,7 Key | dst 1 Pitch,2 Cutoff,3 Reso,4 PW,5 Level,6 Sync,7 Ring,8 NoiseMix,9 L2,10 L3
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

        // --- درام کیت‌ها (مود Drums) ---
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
        { "Drums - Dirty Reverb Kit",{ {"mode",1},{"chip",3},{"bits",7},{"srate",16000},{"drive",0.25f},{"dtype",1},{"revmix",0.3f},{"revlofi",0.7f},{"revdecay",0.7f},{"kickdecay",0.34f} } },

        // --- کیک‌ها (مود Drums: C3 = کیک. هر پریست یه کیک جدا) ---
        { "Kick - NES Punch",       { {"mode",1},{"chip",0},{"kickdecay",0.25f},{"kicksweep",30},{"kickclick",0.4f} } },
        { "Kick - NES Deep",        { {"mode",1},{"chip",0},{"kicktune",-6},{"kickdecay",0.45f},{"kicksweep",20},{"kickclick",0.2f} } },
        { "Kick - GB Thump",        { {"mode",1},{"chip",1},{"kicktune",-2},{"kickdecay",0.3f},{"kicksweep",24},{"kicksub",0.3f},{"kickclick",0.3f} } },
        { "Kick - 808 Sub (bit)",   { {"mode",1},{"chip",1},{"kicktune",-8},{"kickdecay",0.7f},{"kicksweep",14},{"kicksub",0.7f},{"kickclick",0.15f},{"kickdrive",0.1f} } },
        { "Kick - SID Boom",        { {"mode",1},{"chip",3},{"kicktune",-4},{"kickdecay",0.5f},{"kicksweep",32},{"kicksub",0.5f},{"kickclick",0.35f},{"kickdrive",0.15f} } },
        { "Kick - Hard Distorted",  { {"mode",1},{"chip",0},{"kicktune",-2},{"kickdecay",0.35f},{"kicksweep",36},{"kickclick",0.5f},{"kickdrive",0.8f},{"drive",0.2f},{"dtype",1} } },
        { "Kick - Gabber Crush",    { {"mode",1},{"chip",3},{"kickdecay",0.5f},{"kicksweep",40},{"kicksub",0.4f},{"kickdrive",1},{"bits",6},{"srate",14000},{"drive",0.4f},{"dtype",1} } },
        { "Kick - Short Click",     { {"mode",1},{"chip",2},{"kickdecay",0.12f},{"kicksweep",18},{"kickclick",0.8f} } },
        { "Kick - Laser Dive",      { {"mode",1},{"chip",0},{"kicktune",6},{"kickdecay",0.4f},{"kicksweep",48},{"kickclick",0.1f} } },
        { "Kick - PCE Tight",       { {"mode",1},{"chip",7},{"kickdecay",0.22f},{"kicksweep",26},{"kicksub",0.2f},{"kickclick",0.3f} } },
        { "Kick - Atari Buzz",      { {"mode",1},{"chip",5},{"kickdecay",0.3f},{"kicksweep",28},{"kickclick",0.5f},{"kickdrive",0.3f} } },
        { "Kick - Noisy Room",      { {"mode",1},{"chip",3},{"kickdecay",0.4f},{"kicksweep",28},{"kicksub",0.4f},{"kickclick",0.3f},{"bits",8},{"srate",16000},
                                      {"revmix",0.25f},{"revlofi",0.6f},{"revdecay",0.6f} } },

        // --- کیک با مود Synth (پیچ‌انولوپ؛ روی نت پایین مثل C1/C2 بزن) ---
        { "Synth Kick - Triangle Dive",{ {"chip",0},{"wave",1},{"penvamt",36},{"penvtime",0.09f},{"decay",0.3f},{"sustain",0},{"release",0.05f},{"poly",1} } },
        { "Synth Kick - Stepped Zap",  { {"chip",0},{"wave",1},{"penvamt",30},{"penvtime",0.12f},{"penvshape",2},{"decay",0.25f},{"sustain",0},{"release",0.05f},{"poly",1} } },
        { "Synth Kick - Pulse Thump",  { {"chip",2},{"wave",0},{"penvamt",24},{"penvtime",0.06f},{"decay",0.2f},{"sustain",0},{"release",0.04f},{"poly",1} } },
        { "Synth Kick - Sub Layer",    { {"chip",0},{"wave",1},{"l2on",1},{"l2chip",3},{"l2wave",1},{"l2semi",-12},{"l2level",0.9f},{"penvamt",30},{"penvtime",0.1f},
                                         {"decay",0.45f},{"sustain",0},{"release",0.06f},{"poly",1} } },

        // --- سبک نویزی/لو-فای (با ماتریکس، Unison و ریورب) ---
        { "CC - Noisy Matrix Lead", { {"chip",4},{"wave",2},{"smooth",0},{"unison",3},{"unidetune",18},{"unispread",0.8f},{"sync",2.5f},{"ring",1.2f},
                                      {"fmode",2},{"cutoff",1800},{"reso",0.7f},{"poly",1},{"glide",0.03f},
                                      {"lfo1shape",3},{"lfo1rate",6},{"m1src",1},{"m1dst",2},{"m1amt",0.5f},
                                      {"m2src",4},{"m2dst",1},{"m2amt",0.05f},{"nmrate",45},{"nmmix",0.12f},
                                      {"bits",6},{"srate",11025},{"drive",0.35f},{"dtype",1},
                                      {"delaymix",0.25f},{"delaytime",280},{"revmix",0.35f},{"revlofi",0.6f},{"revdecay",0.8f},{"revdamp",0.6f} } },
        { "CC - Detuned Saw Wall",  { {"chip",3},{"wave",2},{"unison",5},{"unidetune",30},{"unispread",1},{"fmode",1},{"cutoff",3200},{"reso",0.3f},{"fenv",1},
                                      {"decay",0.4f},{"sustain",0.7f},{"revmix",0.4f},{"revdecay",0.85f},{"chorusmix",0.4f} } },
        { "CC - Layered Chip Pad",  { {"chip",0},{"wave",0},{"pw",0.25f},{"l2on",1},{"l2chip",3},{"l2wave",2},{"l2semi",-12},{"l3on",1},{"l3chip",1},{"l3wave",4},{"l3level",0.5f},
                                      {"attack",0.25f},{"release",0.8f},{"sustain",0.8f},{"revmix",0.5f},{"revdecay",0.9f},{"revlofi",0.3f},{"poly",6} } },
        { "CC - Gritty Reverb Pad", { {"chip",4},{"wave",1},{"unison",3},{"unidetune",20},{"attack",0.3f},{"release",1.0f},{"nmmix",0.1f},{"bits",8},{"srate",12000},
                                      {"revmix",0.6f},{"revlofi",0.8f},{"revdecay",0.95f},{"revdamp",0.7f},{"revpre",40},{"poly",6},
                                      {"glfoashape",0},{"glfoarate",0.2f},{"g1src",1},{"g1dst",2},{"g1amt",0.3f} } },
        { "CC - Wobble Bit Bass",   { {"chip",1},{"wave",2},{"poly",1},{"fmode",1},{"cutoff",600},{"reso",0.6f},{"lfo1shape",1},{"lfo1rate",3},{"m1src",1},{"m1dst",2},{"m1amt",0.8f},
                                      {"bits",8},{"drive",0.3f},{"dtype",0} } },
        { "CC - Glitch Gate FX",    { {"chip",4},{"wave",4},{"poly",4},{"bits",5},{"srate",9000},{"g1src",1},{"g1dst",1},{"g1amt",0.5f},{"glfoarate",2.5f},{"glfoashape",2},
                                      {"g2src",2},{"g2dst",2},{"g2amt",-0.8f},{"glfobrate",5},{"glfobshape",4},{"delaymix",0.3f},{"revmix",0.3f},{"revlofi",0.7f} } },

        // --- افکت‌های آرکید / آتاری (مود SFX: هر کلید یه شلیک. کلید روی پیچ کم اثر می‌ذاره) ---
        { "SFX - Atari Shot",         { {"mode",3},{"chip",5},{"sfxwave",0},{"sfxpitch",18},{"sfxkey",0.3f},{"sfxslide",-170},{"sfxstep",60},{"sfxdecay",0.16f},{"sfxhold",0.01f} } },
        { "SFX - Pixel Blaster",      { {"mode",3},{"chip",0},{"sfxwave",0},{"sfxpitch",24},{"sfxkey",0.3f},{"sfxslide",-200},{"sfxstep",0},{"sfxduty",0.125f},{"sfxdutysweep",-0.4f},{"sfxdecay",0.22f},{"sfxnoise",0.05f} } },
        { "SFX - Rapid Fire",         { {"mode",3},{"chip",0},{"sfxwave",0},{"sfxpitch",20},{"sfxkey",0.3f},{"sfxslide",-250},{"sfxdecay",0.09f},{"sfxretrig",14},{"sfxstep",0} } },
        { "SFX - Heavy Cannon",       { {"mode",3},{"chip",3},{"sfxwave",2},{"sfxpitch",0},{"sfxkey",0.2f},{"sfxslide",-60},{"sfxhold",0.03f},{"sfxdecay",0.45f},{"sfxnoise",0.35f},{"sfxstep",30} } },
        { "SFX - Laser Pew (stepped)",{ {"mode",3},{"chip",0},{"sfxwave",0},{"sfxpitch",30},{"sfxkey",0.3f},{"sfxslide",-260},{"sfxstep",30},{"sfxduty",0.25f},{"sfxdecay",0.25f} } },
        { "SFX - Ricochet",           { {"mode",3},{"chip",0},{"sfxwave",0},{"sfxpitch",36},{"sfxkey",0.3f},{"sfxslide",-400},{"sfxaccel",600},{"sfxstep",0},{"sfxdecay",0.3f} } },
        { "SFX - POKEY Zap",          { {"mode",3},{"chip",4},{"sfxwave",2},{"sfxpitch",12},{"sfxkey",0.3f},{"sfxslide",-150},{"sfxdecay",0.3f},{"sfxstep",60} } },
        { "SFX - 8-bit Explosion",    { {"mode",3},{"chip",0},{"sfxwave",3},{"sfxpitch",-6},{"sfxkey",0.2f},{"sfxslide",-20},{"sfxhold",0.12f},{"sfxdecay",0.9f},{"sfxnoise",0.8f},{"sfxstep",30} } },
        { "SFX - Atari Boom",         { {"mode",3},{"chip",5},{"sfxwave",3},{"sfxpitch",-12},{"sfxkey",0.2f},{"sfxslide",-8},{"sfxhold",0.1f},{"sfxdecay",1.0f},{"sfxnoise",0.6f} } },
        { "SFX - Machine Gun",        { {"mode",3},{"chip",0},{"sfxwave",3},{"sfxpitch",0},{"sfxkey",0.2f},{"sfxslide",0},{"sfxdecay",0.06f},{"sfxretrig",18},{"sfxnoise",0.6f},{"noiseshort",1} } },
        { "SFX - Hit / Hurt",         { {"mode",3},{"chip",0},{"sfxwave",0},{"sfxpitch",6},{"sfxkey",0.3f},{"sfxslide",-110},{"sfxdecay",0.12f},{"sfxnoise",0.6f} } },
        { "SFX - Jump",               { {"mode",3},{"chip",0},{"sfxwave",0},{"sfxpitch",0},{"sfxkey",0.5f},{"sfxslide",90},{"sfxdecay",0.18f},{"sfxstep",0} } },
        { "SFX - Power-up",           { {"mode",3},{"chip",0},{"sfxwave",0},{"sfxpitch",4},{"sfxkey",0.5f},{"sfxslide",50},{"sfxjump",12},{"sfxjumptime",0.08f},{"sfxvibdepth",0.5f},{"sfxvibrate",7},{"sfxdecay",0.45f} } },
        { "SFX - Coin",               { {"mode",3},{"chip",0},{"sfxwave",0},{"sfxpitch",24},{"sfxkey",0.5f},{"sfxslide",0},{"sfxjump",5},{"sfxjumptime",0.07f},{"sfxduty",0.25f},{"sfxdecay",0.35f},{"sfxstep",0} } },
        { "SFX - Siren",              { {"mode",3},{"chip",2},{"sfxwave",0},{"sfxpitch",12},{"sfxkey",0.3f},{"sfxslide",0},{"sfxvibdepth",7},{"sfxvibrate",3},{"sfxhold",0.8f},{"sfxdecay",1.5f},{"sfxstep",0} } },
        { "SFX - Gritty Blast",       { {"mode",3},{"chip",4},{"sfxwave",2},{"sfxpitch",14},{"sfxkey",0.3f},{"sfxslide",-180},{"sfxdecay",0.3f},{"sfxnoise",0.2f},{"bits",6},{"srate",12000},{"drive",0.3f},{"dtype",1},{"revmix",0.25f},{"revlofi",0.6f} } },

        // --- سبک «Ayo Technology» (سینت باز و وزوزی با حس C64/SID؛ الهام‌گرفته، نه کپی دقیق) ---
        { "Ayo Style - C64 Buzz Lead", { {"chip",3},{"wave",2},{"unison",2},{"unidetune",10},{"unispread",0.5f},
                                         {"l2on",1},{"l2chip",3},{"l2wave",0},{"l2pw",0.4f},{"l2semi",0},{"l2fine",8},{"l2level",0.5f},
                                         {"fmode",1},{"cutoff",2600},{"reso",0.45f},{"fenv",1.5f},
                                         {"attack",0.002f},{"decay",0.22f},{"sustain",0.55f},{"release",0.12f},{"poly",1},
                                         {"vibdepth",15},{"vibdelay",0.2f},{"chorusmix",0.3f},{"delaymix",0.2f},{"delaytime",280},{"revmix",0.12f} } },
        { "Ayo Style - Gritty Stab",   { {"chip",3},{"wave",0},{"pw",0.35f},{"sync",2},{"fmode",2},{"cutoff",2200},{"reso",0.55f},{"fenv",2},
                                         {"decay",0.15f},{"sustain",0.3f},{"release",0.1f},{"bits",10},{"drive",0.2f},{"dtype",0},{"delaymix",0.15f},{"poly",4} } },
        { "Ayo Style - Sub Bass",      { {"chip",3},{"wave",1},{"l2on",1},{"l2chip",3},{"l2wave",2},{"l2semi",-12},{"l2level",0.3f},
                                         {"fmode",1},{"cutoff",500},{"reso",0.25f},{"fenv",1},{"sustain",0.9f},{"release",0.08f},{"drive",0.15f},{"poly",1} } },
        { "Ayo Style - Wide Pluck",    { {"chip",3},{"wave",2},{"unison",4},{"unidetune",16},{"unispread",0.9f},{"fmode",1},{"cutoff",1800},{"reso",0.35f},{"fenv",2.2f},
                                         {"decay",0.18f},{"sustain",0.25f},{"release",0.15f},{"chorusmix",0.4f},{"revmix",0.2f},{"poly",4} } },
        { "Ayo Style - PWM Lead",      { {"chip",3},{"wave",0},{"pw",0.5f},{"pwmdepth",0.7f},{"pwmrate",3.5f},{"fmode",1},{"cutoff",3800},{"reso",0.3f},
                                         {"sustain",0.8f},{"vibdepth",12},{"vibdelay",0.25f},{"chorusmix",0.35f},{"delaymix",0.2f},{"poly",1} } },

        // --- سمپلر ---
        { "Sampler - Clean",        { {"mode",2},{"sustain",1},{"release",0.15f},{"poly",8} } },
        { "Sampler - 4-bit Chip DAC",{ {"mode",2},{"sustain",1},{"release",0.1f},{"smpbits",4},{"smprate",8000},{"smploop",0} } },
        { "Sampler - Crushed Loop", { {"mode",2},{"sustain",1},{"smploop",1},{"smpbits",6},{"smprate",11025},{"drive",0.3f},{"dtype",1},{"revmix",0.3f},{"revlofi",0.6f} } },
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
    for (auto& d : drums) d.active = false;
    for (auto& s : smpVoices) s.stage = chip::SampleVoice::Idle;
    for (auto& s : sfxVoices) s.active = false;
    pitchBendNorm = 0.0f;
    lastNote = -1;
    arpRunner.reset();
    lastArpMode = 0;
    lastMode = 0;
    fx.prepare (sr);
    reverb.prepare (sr);
    crusherL.prepare (sr);
    crusherR.prepare (sr);
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

void ChipSynthProcessor::handleMidi (const juce::MidiMessage& m, const chip::Params& p, const chip::SampleParams& sp,
                                     int arpMode, bool latch, int mode)
{
    auto off = [this, &p] (int n) { noteOffNote (n, p); };

    if (m.isNoteOn())
    {
        if (mode == 1)          triggerDrum (m.getNoteNumber(), m.getFloatVelocity(), p);
        else if (mode == 2)     triggerSample (m.getNoteNumber(), m.getFloatVelocity(), sp);
        else if (mode == 3)     triggerSfx (m.getNoteNumber(), m.getFloatVelocity(), p);
        else if (arpMode != 0)  arpRunner.press (m.getNoteNumber(), m.getFloatVelocity(), latch);
        else                    noteOn (m.getNoteNumber(), m.getFloatVelocity(), p);
    }
    else if (m.isNoteOff())
    {
        if (mode == 1)          { /* درام‌ها one-shot هستن */ }
        else if (mode == 2)
        {
            for (auto& v : smpVoices)
                if (v.stage != chip::SampleVoice::Idle && v.note == m.getNoteNumber()) v.noteOff (sp);
        }
        else if (mode == 3)
        {
            for (auto& v : sfxVoices)
                if (v.active && v.note == m.getNoteNumber()) v.noteOff();
        }
        else if (arpMode != 0)  arpRunner.release (m.getNoteNumber(), latch, off);
        else                    noteOffNote (m.getNoteNumber(), p);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) v.stage = chip::Idle;
        for (auto& d : drums) d.active = false;
        for (auto& s : smpVoices) s.stage = chip::SampleVoice::Idle;
        for (auto& s : sfxVoices) s.active = false;
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

    {
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        if (lock.isLocked()) audioSample = sample;
    }

    const auto p   = readParams();
    const auto fxp = readFx();
    const auto cp  = readCrush();
    const auto rp  = readReverb();
    const int  mode = (int) getParam ("mode");
    const auto dp  = readDrum (p);
    const auto sp  = readSampler (p);
    const auto sfxp = readSfx();
    const int  arpMode = (mode == 0) ? (int) getParam ("arpmode") : 0;
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

    if (arpMode != lastArpMode || mode != lastMode)   // موقع عوض‌کردن مود، همه‌چیز رو آزاد کن
    {
        for (auto& v : voices) v.noteOff (p);
        for (auto& s : smpVoices) s.noteOff (sp);
        for (auto& s : sfxVoices) s.noteOff();
        arpRunner.reset();
        lastArpMode = arpMode;
        lastMode = mode;
    }
    if (arpMode != 0 && ! latch && arpRunner.arp.physDown == 0 && arpRunner.arp.count > 0)
    {
        arpRunner.arp.clearHeld();          // Latch رو خاموش کردی و کلیدی پایین نیست
        arpRunner.closeAll (arpOff);
    }

    // --- ماتریکس گلوبال ---
    const float gRate[2]  = { getParam ("glfoarate"), getParam ("glfobrate") };
    const int   gShape[2] = { (int) getParam ("glfoashape"), (int) getParam ("glfobshape") };
    int gSrc[4], gDst[4]; float gAmt[4]; bool anyG = false;
    for (size_t k = 0; k < 4; ++k)
    {
        gSrc[k] = (int) gSrcP[k]->load(); gDst[k] = (int) gDstP[k]->load(); gAmt[k] = gAmtP[k]->load();
        if (gSrc[k] > 0 && gSrc[k] < 3 && gDst[k] > 0 && gDst[k] < 8 && std::abs (gAmt[k]) > 1.0e-4f) anyG = true;
    }

    const int numSamples = buffer.getNumSamples();
    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float bend = pitchBendNorm * p.bendRange;
    const chip::SampleData* smp = audioSample.get();

    auto renderRange = [&] (int from, int to)
    {
        for (int i = from; i < to; ++i)
        {
            if (arpMode != 0) arpRunner.tick (as, sampleRate, arpOn, arpOff);

            float sumL = 0.0f, sumR = 0.0f;
            for (auto& v : voices)
            {
                float vl, vr;
                v.renderStereo (p, sampleRate, bend, vl, vr);
                sumL += vl; sumR += vr;
            }
            for (auto& dv : drums)     { const float d = dv.render (dp, p.chip, sampleRate); sumL += d; sumR += d; }
            for (auto& sv : smpVoices) { const float d = sv.render (smp, sp, sampleRate);     sumL += d; sumR += d; }
            for (auto& fv : sfxVoices) { const float d = fv.render (sfxp, p, sampleRate);     sumL += d; sumR += d; }

            // ماتریکس گلوبال (هر سمپل)
            chip::CrushParams cpm = cp;
            chip::FxParams fxm = fxp;
            chip::ReverbParams rpm = rp;
            float masterMul = 1.0f;
            if (anyG)
            {
                float gsrc[3] = { 0.0f, 0.0f, 0.0f };
                for (int k = 0; k < 2; ++k)
                {
                    gPh[k] += (double) gRate[k] / sampleRate;
                    if (gPh[k] >= 1.0) { gPh[k] -= 1.0; gSH[k] = nextRand01() * 2.0f - 1.0f; }
                    gsrc[1 + k] = chip::lfoShape (gShape[k], (float) gPh[k], gSH[k]);
                }
                float gm[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                for (int k = 0; k < 4; ++k)
                    if (gSrc[k] > 0 && gSrc[k] < 3 && gDst[k] > 0 && gDst[k] < 8) gm[gDst[k]] += gsrc[gSrc[k]] * gAmt[k];
                cpm.bits  = juce::jlimit (1.0f, 16.0f, cp.bits + gm[1] * 8.0f);
                cpm.rate  = juce::jlimit (500.0f, (float) sampleRate, cp.rate * std::exp2 (gm[2] * 3.0f));
                cpm.drive = juce::jlimit (0.0f, 1.0f, cp.drive + gm[3]);
                fxm.chorusMix = juce::jlimit (0.0f, 1.0f, fxp.chorusMix + gm[4]);
                fxm.delayMix  = juce::jlimit (0.0f, 1.0f, fxp.delayMix + gm[5]);
                rpm.mix       = juce::jlimit (0.0f, 1.0f, rp.mix + gm[6]);
                masterMul     = juce::jlimit (0.0f, 2.0f, 1.0f + gm[7]);
            }

            const float g = p.master * 0.85f * masterMul;
            sumL = crusherL.process (sumL * g, cpm);
            sumR = crusherR.process (sumR * g, cpm);

            float l, r;
            fx.processStereo (sumL, sumR, l, r, fxm);
            reverb.process (l, r, l, r, rpm);
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
        handleMidi (meta.getMessage(), p, sp, arpMode, latch, mode);
    }
    renderRange (pos, numSamples);
}

juce::AudioProcessorEditor* ChipSynthProcessor::createEditor()
{
    return new ChipSynthEditor (*this);
}

void ChipSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("samplePath", samplePath, nullptr);
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChipSynthProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            const juce::String path = apvts.state.getProperty ("samplePath").toString();
            if (path.isNotEmpty())
            {
                const juce::File f (path);
                if (f.existsAsFile()) loadSample (f);
            }
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipSynthProcessor();
}
