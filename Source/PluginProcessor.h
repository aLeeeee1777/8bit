#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "ChipEngine.h"
#include "Arp.h"
#include "Effects.h"
#include "Drums.h"

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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void applyPreset (int index);   // همیشه اعمال می‌شه (برای منوی پریست رابط)

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // کیبورد روی صفحه‌ی پلاگین

private:
    int currentProgram = 0;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    chip::Params readParams() const;
    chip::FxParams readFx() const;
    chip::CrushParams readCrush() const;
    chip::DrumParams readDrum (const chip::Params& p) const;
    void triggerDrum (int note, float vel, const chip::Params& p);

    void noteOn (int note, float vel, const chip::Params& p);
    void noteOffNote (int note, const chip::Params& p);
    void handleMidi (const juce::MidiMessage& m, const chip::Params& p, int arpMode, bool latch, bool drumMode);

    std::array<chip::Voice, 8> voices;
    uint64_t orderCounter = 0;
    float pitchBendNorm = 0.0f;
    double sampleRate = 44100.0;
    int lastNote = -1;

    chip::ArpRunner arpRunner;
    chip::StereoFx fx;
    chip::Crusher crusher;
    int lastArpMode = 0;
    bool lastDrumMode = false;
    std::array<chip::DrumVoice, 16> drums;
    uint64_t drumOrder = 0;

    std::array<std::atomic<float>*, 16> stepOnP {}, stepAccP {}, stepTrP {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChipSynthProcessor)
};
