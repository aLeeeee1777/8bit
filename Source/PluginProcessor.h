#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <memory>
#include "ChipEngine.h"
#include "Arp.h"
#include "Effects.h"
#include "Drums.h"
#include "Sampler.h"
#include "Sfx.h"

class ChipSynthProcessor : public juce::AudioProcessor
{
public:
    ChipSynthProcessor();
    ~ChipSynthProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ChipSynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void applyPreset (int index);   // همیشه اعمال می‌شه (برای منوی پریست رابط)

    // سمپلر
    bool loadSample (const juce::File& file);
    juce::String getSampleName() const { return sampleName; }

    // مولد افکت آرکید: 0 Shoot، 1 Explosion، 2 Power-up، 3 Hit، 4 Jump، 5 Blip، 6 Random، 7 Mutate
    void generateSfx (int kind);

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // کیبورد روی صفحه‌ی پلاگین

private:
    int currentProgram = 0;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    float getParamS (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    chip::Params readParams() const;
    chip::FxParams readFx() const;
    chip::CrushParams readCrush() const;
    chip::ReverbParams readReverb() const;
    chip::DrumParams readDrum (const chip::Params& p) const;
    chip::SampleParams readSampler (const chip::Params& p) const;
    chip::SfxParams readSfx() const;
    void writeSfx (const chip::SfxParams& s);

    void noteOn (int note, float vel, const chip::Params& p);
    void noteOffNote (int note, const chip::Params& p);
    void triggerDrum (int note, float vel, const chip::Params& p);
    void triggerSample (int note, float vel, const chip::SampleParams& sp);
    void triggerSfx (int note, float vel, const chip::Params& p);
    void handleMidi (const juce::MidiMessage& m, const chip::Params& p, const chip::SampleParams& sp,
                     int arpMode, bool latch, int mode);
    float nextRand01()
    {
        gRng = gRng * 1664525u + 1013904223u;
        return (float) (gRng >> 8) * (1.0f / 16777216.0f);
    }

    std::array<chip::Voice, 8> voices;
    std::array<chip::DrumVoice, 16> drums;
    std::array<chip::SampleVoice, 8> smpVoices;
    std::array<chip::SfxVoice, 8> sfxVoices;
    uint64_t orderCounter = 0, drumOrder = 0, smpOrder = 0, sfxOrder = 0;
    uint32_t sfxRng = 424242u;
    float pitchBendNorm = 0.0f;
    double sampleRate = 44100.0;
    int lastNote = -1;

    chip::ArpRunner arpRunner;
    chip::StereoFx fx;
    chip::Reverb reverb;
    chip::Crusher crusherL, crusherR;
    int lastArpMode = 0, lastMode = 0;

    // سمپل: sample توسط thread اصلی نوشته می‌شه، audioSample توسط thread صدا خونده می‌شه
    juce::SpinLock sampleLock;
    std::shared_ptr<const chip::SampleData> sample, audioSample;
    juce::String samplePath, sampleName;

    // LFO های گلوبال
    double gPh[2] = { 0.0, 0.0 };
    float  gSH[2] = { 0.0f, 0.0f };
    uint32_t gRng = 987654321u;

    std::array<std::atomic<float>*, 16> stepOnP {}, stepAccP {}, stepTrP {};
    std::array<std::atomic<float>*, 8>  mSrcP {}, mDstP {}, mAmtP {};
    std::array<std::atomic<float>*, 4>  gSrcP {}, gDstP {}, gAmtP {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChipSynthProcessor)
};
