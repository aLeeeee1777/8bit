#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "ChipEngine.h"

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
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    chip::Params readParams() const;
    void noteOn (int note, float vel, const chip::Params& p);
    void handleMidi (const juce::MidiMessage& m, const chip::Params& p);

    std::array<chip::Voice, 8> voices;
    uint64_t orderCounter = 0;
    float pitchBendSemis = 0.0f;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChipSynthProcessor)
};
