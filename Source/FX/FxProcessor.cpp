#include "FxProcessor.h"

ChipFxProcessor::ChipFxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ChipFxProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addChoice = [&] (const String& id, const String& name, StringArray items, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto addFloat = [&] (const String& id, const String& name, float lo, float hi, float def, float skew = 1.0f)
    { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                         NormalisableRange<float> (lo, hi, 0.0f, skew), def)); };
    auto addInt = [&] (const String& id, const String& name, int lo, int hi, int def)
    { layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };

    addFloat ("ingain",  "Input Gain (dB)",  -24.0f, 24.0f, 0.0f);
    addFloat ("outgain", "Output Gain (dB)", -24.0f, 12.0f, 0.0f);
    addFloat ("mix",     "Dry/Wet", 0.0f, 1.0f, 1.0f);

    // پیچ‌شیفتر
    addInt   ("psemis", "Pitch (semitones)", -24, 24, 0);
    addFloat ("pfine",  "Pitch Fine (cents)", -100.0f, 100.0f, 0.0f);
    addFloat ("pmix",   "Pitch Mix", 0.0f, 1.0f, 0.0f);
    addFloat ("pwin",   "Pitch Window (ms)", 10.0f, 120.0f, 40.0f);

    // کراشر / دیستورشن
    addFloat  ("bits",  "Bit Depth", 1.0f, 16.0f, 16.0f);
    addFloat  ("srate", "Sample Rate", 500.0f, 44100.0f, 44100.0f, 0.4f);
    addFloat  ("drive", "Drive", 0.0f, 1.0f, 0.0f);
    addChoice ("dtype", "Drive Type", { "Soft", "Hard", "Fold" }, 0);

    // فیلتر
    addChoice ("fmode",  "Filter", { "Off", "Low-pass", "Band-pass", "High-pass" }, 0);
    addFloat  ("cutoff", "Cutoff", 40.0f, 16000.0f, 8000.0f, 0.35f);
    addFloat  ("reso",   "Resonance", 0.0f, 1.0f, 0.2f);

    // گیت ریتمیک و استاتر
    addChoice ("gpat", "Gate Pattern", { "Off", "Alternate", "Syncopated", "3 of 4", "Dotted", "Half bar", "Skip beat", "One in 4" }, 0);
    addChoice ("gdiv", "Gate Step", { "1/8", "1/16", "1/32", "1/64" }, 1);
    addFloat  ("gdepth",  "Gate Depth", 0.0f, 1.0f, 1.0f);
    addFloat  ("gsmooth", "Gate Smooth", 0.0f, 1.0f, 0.2f);
    addChoice ("stutter", "Stutter (automate me)", { "Off", "1/4", "1/8", "1/16", "1/32" }, 0);

    // دیلی
    addChoice ("dsync", "Delay Clock", { "Free (ms)", "1/4", "1/8", "1/8 dotted", "1/16" }, 0);
    addFloat  ("dtime", "Delay Time (ms)", 20.0f, 1000.0f, 300.0f, 0.5f);
    addFloat  ("dfb",   "Delay Feedback", 0.0f, 0.9f, 0.35f);
    addFloat  ("dmix",  "Delay Mix", 0.0f, 1.0f, 0.0f);

    // ریورب
    addFloat ("rmix",   "Reverb Mix", 0.0f, 1.0f, 0.0f);
    addFloat ("rsize",  "Reverb Size", 0.0f, 1.0f, 0.5f);
    addFloat ("rdecay", "Reverb Decay", 0.0f, 1.0f, 0.6f);
    addFloat ("rdamp",  "Reverb Damping", 0.0f, 1.0f, 0.5f);
    addFloat ("rpre",   "Reverb Pre-delay (ms)", 0.0f, 250.0f, 0.0f);
    addFloat ("rlofi",  "Reverb Lo-Fi", 0.0f, 1.0f, 0.0f);
    addFloat ("rwidth", "Reverb Width", 0.0f, 1.0f, 1.0f);

    return layout;
}

chip::FxEngineParams ChipFxProcessor::readParams (double bpm) const
{
    chip::FxEngineParams p;
    p.inGain  = std::pow (10.0f, getParam ("ingain") / 20.0f);
    p.outGain = std::pow (10.0f, getParam ("outgain") / 20.0f);
    p.mix     = getParam ("mix");

    p.pitchSemis = std::round (getParam ("psemis"));
    p.pitchFine  = getParam ("pfine");
    p.pitchMix   = getParam ("pmix");
    p.pitchWin   = getParam ("pwin");

    p.crush.bits  = getParam ("bits");
    p.crush.rate  = getParam ("srate");
    p.crush.drive = getParam ("drive");
    p.crush.type  = (int) getParam ("dtype");

    p.fmode  = (int) getParam ("fmode");
    p.cutoff = getParam ("cutoff");
    p.reso   = getParam ("reso");

    p.gatePattern = (int) getParam ("gpat");
    p.gateDiv     = (int) getParam ("gdiv");
    p.gateDepth   = getParam ("gdepth");
    p.gateSmooth  = getParam ("gsmooth");
    p.stutterDiv  = (int) getParam ("stutter");

    static const double dBeats[5] = { 0.0, 1.0, 0.5, 0.75, 0.25 };
    const int ds = juce::jlimit (0, 4, (int) getParam ("dsync"));
    p.dTime = ds == 0 ? getParam ("dtime") : (float) (dBeats[ds] * 60.0 / std::max (20.0, bpm) * 1000.0);
    p.dFb   = getParam ("dfb");
    p.dMix  = getParam ("dmix");

    p.rev.mix      = getParam ("rmix");
    p.rev.size     = getParam ("rsize");
    p.rev.decay    = getParam ("rdecay");
    p.rev.damp     = getParam ("rdamp");
    p.rev.predelay = getParam ("rpre");
    p.rev.lofi     = getParam ("rlofi");
    p.rev.width    = getParam ("rwidth");
    return p;
}

void ChipFxProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    engine.prepare (sr);
    freeBeat = 0.0;
}

bool ChipFxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && layouts.getMainInputChannelSet() == out;
}

void ChipFxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;
    for (int c = 2; c < numCh; ++c) buffer.clear (c, 0, n);

    // تمپو و موقعیت (ppq) از هاست؛ وقتی هاست پلی نیست از ساعت داخلی استفاده می‌کنیم تا گیت قفل نکنه
    double bpm = 120.0, ppq = freeBeat;
    bool usePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) { ppq = *q; usePpq = true; }
        }
    (void) usePpq;

    const auto p = readParams (bpm);
    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    const double beatPerSample = bpm / 60.0 / sampleRate;

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i];
        const float inR = R != nullptr ? R[i] : inL;
        float oL, oR;
        engine.process (inL, inR, oL, oR, p, ppq + (double) i * beatPerSample, bpm);
        if (R != nullptr) { L[i] = oL; R[i] = oR; }
        else              { L[i] = 0.5f * (oL + oR); }
    }
    freeBeat = ppq + (double) n * beatPerSample;
}

void ChipFxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChipFxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipFxProcessor();
}
