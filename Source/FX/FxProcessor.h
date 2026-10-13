#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FxEngine.h"

// ChipFX: افکت لو-فای/نویزی برای وکال و کل میکس
class ChipFxProcessor : public juce::AudioProcessor
{
public:
    ChipFxProcessor();
    ~ChipFxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ChipFX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

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
    float getParam (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    chip::FxEngineParams readParams (double bpm) const;

    chip::ChipFxEngine engine;
    double sampleRate = 44100.0;
    double freeBeat = 0.0;   // ساعت داخلی وقتی هاست پلی نیست

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChipFxProcessor)
};
