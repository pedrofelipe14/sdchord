#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> range(float lo, float hi, float centre = -1.0f)
{
    juce::NormalisableRange<float> r(lo, hi);
    if (centre > lo)
        r.setSkewForCentre(centre);
    return r;
}
} // namespace

RasguidoProcessor::RasguidoProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "Rasguido", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout RasguidoProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    const rasguido::Params d;
    auto pct = [](const juce::String& id, const juce::String& name, float def)
    {
        return std::make_unique<P>(juce::ParameterID { id, 1 }, name, range(0.0f, 1.0f), def,
                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                                       [](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; }));
    };

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(pct("chordLevel", "Acordes", d.chordLevel));
    layout.add(pct("chordTone", "Tono acordes", d.chordTone));
    layout.add(pct("bassLevel", "Bajo", d.bassLevel));
    layout.add(pct("stringLevel", "Cuerdas", d.stringLevel));
    layout.add(std::make_unique<P>(juce::ParameterID { "stringDecay", 1 }, "Sustain cuerdas", range(0.2f, 8.0f, 1.8f), d.stringDecay,
                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                                       [](float v, int) { return juce::String(v, 1) + " s"; })));
    layout.add(pct("stringTone", "Brillo cuerdas", d.stringTone));
    layout.add(pct("chorus", "Chorus", d.chorus));
    layout.add(pct("reverb", "Reverb", d.reverb));
    layout.add(pct("wobble", "Wobble", d.wobble));
    layout.add(pct("master", "Volumen", d.master));
    return layout;
}

void RasguidoProcessor::prepareToPlay(double sampleRate, int)
{
    dsp.prepare(sampleRate);
    uiMidi.reset(sampleRate);
}

bool RasguidoProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RasguidoProcessor::readParams()
{
    auto get = [this](const char* id) { return state.getRawParameterValue(id)->load(); };
    auto& p = dsp.params;
    p.chordLevel = get("chordLevel");
    p.chordTone = get("chordTone");
    p.bassLevel = get("bassLevel");
    p.stringLevel = get("stringLevel");
    p.stringDecay = get("stringDecay");
    p.stringTone = get("stringTone");
    p.chorus = get("chorus");
    p.reverb = get("reverb");
    p.wobble = get("wobble");
    p.master = get("master");
}

void RasguidoProcessor::handle(const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        dsp.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        dsp.noteOff(m.getNoteNumber());
    else if (m.isController())
        dsp.controlChange(m.getControllerNumber(), m.getControllerValue());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        dsp.allNotesOff();
}

void RasguidoProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    uiMidi.removeNextBlockOfMessages(midi, numSamples);
    readParams();

    // Render up to each MIDI event, then apply it: notes land on the exact sample.
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);

    int done = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit(0, numSamples, meta.samplePosition);
        if (at > done)
        {
            dsp.process(left + done, right + done, at - done);
            done = at;
        }
        handle(meta.getMessage());
    }
    if (done < numSamples)
        dsp.process(left + done, right + done, numSamples - done);
}

void RasguidoProcessor::playFromUi(const juce::MidiMessage& message)
{
    auto m = message;
    m.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    uiMidi.addMessageToQueue(m);
}

juce::AudioProcessorEditor* RasguidoProcessor::createEditor() { return new RasguidoEditor(*this); }

void RasguidoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void RasguidoProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RasguidoProcessor(); }
