#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "pedrochord/engine.h"

/** Display names of the auto-strum's patterns and step lengths, in engine order. */
juce::StringArray seqPatternNames();
juce::StringArray seqRateNames();

class PedrochordProcessor : public juce::AudioProcessor
{
public:
    PedrochordProcessor();

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

    /** The editor plays the instrument by sending the same MIDI a DAW would. Called from the message
        thread only; the audio thread applies it at the start of its next block. */
    void playFromUi(const juce::MidiMessage& message);

    const pedrochord::Engine& engine() const { return dsp; }
    /** The tempo the auto-strum is using: the host's when synced, otherwise the Tempo knob. */
    float autoStrumBpm() const { return currentBpm.load(std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState state;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handle(const juce::MidiMessage& message);
    void readParams();

    pedrochord::Engine dsp;
    std::atomic<float> currentBpm { 100.0f };
    // Lock-free queue from the editor to the audio thread. (A MidiMessageCollector would keep the
    // events' relative timing, but always at the cost of one extra block of latency.)
    struct UiEvent
    {
        juce::uint8 bytes[3];
    };
    static constexpr int uiQueueSize = 1024;
    juce::AbstractFifo uiFifo { uiQueueSize };
    std::array<UiEvent, uiQueueSize> uiEvents {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PedrochordProcessor)
};
