#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "rasguido/engine.h"

class RasguidoProcessor : public juce::AudioProcessor
{
public:
    RasguidoProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    /** The editor plays the instrument by sending the same MIDI a DAW would (thread-safe). */
    void playFromUi(const juce::MidiMessage& message);

    const rasguido::Engine& engine() const { return dsp; }

    juce::AudioProcessorValueTreeState state;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handle(const juce::MidiMessage& message);
    void readParams();

    rasguido::Engine dsp;
    juce::MidiMessageCollector uiMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RasguidoProcessor)
};
