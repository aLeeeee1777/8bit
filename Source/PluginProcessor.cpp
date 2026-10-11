#include "PluginProcessor.h"

ChipSynthProcessor::ChipSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ChipSynthProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addChoice = [&] (const char* id, const char* name, StringArray items, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    auto addFloat = [&] (const char* id, const char* name, float lo, float hi, float def, float skew = 1.0f)
    { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                         NormalisableRange<float> (lo, hi, 0.0f, skew), def)); };

    addChoice ("chip", "Chip", { "NES (2A03)", "Game Boy (DMG)", "Master System (SN76489)", "C64 (SID 6581)" }, 0);
    addChoice ("wave", "Waveform", { "Pulse", "Triangle", "Saw", "Noise", "Wavetable" }, 0);
    addChoice ("gbwave", "GB Wave Table", { "Triangle", "Saw", "Sine", "Pulse 25%" }, 0);
    addFloat  ("pw", "Pulse Width", 0.05f, 0.95f, 0.5f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "noiseshort", 1 }, "Noise Short Mode", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "smooth", 1 }, "Anti-alias (4x)", true));

    addFloat ("attack",  "Attack (s)",  0.001f, 2.0f, 0.001f, 0.4f);
    addFloat ("decay",   "Decay (s)",   0.001f, 2.0f, 0.15f, 0.4f);
    addFloat ("sustain", "Sustain",     0.0f,   1.0f, 0.7f);
    addFloat ("release", "Release (s)", 0.001f, 3.0f, 0.06f, 0.4f);

    addFloat ("vibrate",  "Vibrato Rate (Hz)",   0.1f, 15.0f, 5.0f);
    addFloat ("vibdepth", "Vibrato Depth (cents)", 0.0f, 200.0f, 0.0f);
    addFloat ("slide",    "Slide Start (semitones)", -12.0f, 12.0f, 0.0f);
    addFloat ("slidetime","Slide Time (s)", 0.005f, 1.0f, 0.1f, 0.5f);

    addChoice ("arp", "Arpeggio", { "Off", "Major", "Minor", "Octave", "Power (5th)", "Diminished" }, 0);
    addFloat  ("arprate", "Arp Rate (Hz)", 4.0f, 60.0f, 30.0f);

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "poly", 1 }, "Polyphony", 1, 8, 4));
    addFloat ("velsens", "Velocity Sensitivity", 0.0f, 1.0f, 0.5f);
    addFloat ("master",  "Master", 0.0f, 1.0f, 0.6f);

    return layout;
}

chip::Params ChipSynthProcessor::readParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    chip::Params p;
    p.chip       = (int) get ("chip");
    p.wave       = (int) get ("wave");
    p.gbWave     = (int) get ("gbwave");
    p.pulseWidth = get ("pw");
    p.noiseShort = get ("noiseshort") > 0.5f;
    p.smooth     = get ("smooth") > 0.5f;
    p.attack     = get ("attack");
    p.decay      = get ("decay");
    p.sustain    = get ("sustain");
    p.release    = get ("release");
    p.vibRate    = get ("vibrate");
    p.vibDepth   = get ("vibdepth");
    p.slideSemis = get ("slide");
    p.slideTime  = get ("slidetime");
    p.arpPattern = (int) get ("arp");
    p.arpRate    = get ("arprate");
    p.polyphony  = (int) get ("poly");
    p.velSens    = get ("velsens");
    p.master     = get ("master");
    return p;
}

void ChipSynthProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    for (auto& v : voices) v.stage = chip::Idle;
    pitchBendSemis = 0.0f;
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
}

void ChipSynthProcessor::handleMidi (const juce::MidiMessage& m, const chip::Params& p)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity(), p);
    else if (m.isNoteOff())
    {
        for (auto& v : voices)
            if (v.stage != chip::Idle && v.note == m.getNoteNumber()) v.noteOff (p);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) v.stage = chip::Idle;
    }
    else if (m.isPitchWheel())
        pitchBendSemis = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f;
}

void ChipSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto p = readParams();
    const int numSamples = buffer.getNumSamples();
    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto renderRange = [&] (int from, int to)
    {
        for (int i = from; i < to; ++i)
        {
            float s = 0.0f;
            for (auto& v : voices) s += v.render (p, sampleRate, pitchBendSemis);
            s = std::tanh (s * p.master * 0.5f);
            left[i] = s;
            if (right != nullptr) right[i] = s;
        }
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        renderRange (pos, at);
        pos = at;
        handleMidi (meta.getMessage(), p);
    }
    renderRange (pos, numSamples);
}

juce::AudioProcessorEditor* ChipSynthProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);   // رابط ساده؛ بعداً GUI اختصاصی می‌سازیم
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
