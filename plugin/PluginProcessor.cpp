#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::StringArray seqPatternNames() { return { "Arriba", "Abajo", "Ida y vuelta", "Rasgueo", "Al azar" }; }
juce::StringArray seqRateNames() { return { "1/4", "1/8", "1/8 T", "1/16" }; }

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

PedrochordProcessor::PedrochordProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "Pedrochord", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PedrochordProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    const pedrochord::Params d;
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

    // Auto-strum.
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "seqOn", 1 }, "Rasgueo auto", d.seqOn));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "seqSync", 1 }, "Seguir tempo del DAW", true));
    layout.add(std::make_unique<P>(juce::ParameterID { "seqBpm", 1 }, "Tempo", juce::NormalisableRange<float>(40.0f, 240.0f, 1.0f), d.seqBpm,
                                   juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                                       [](float v, int) { return juce::String(juce::roundToInt(v)) + " BPM"; })));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "seqPattern", 1 }, juce::String::fromUTF8("Patr\xc3\xb3n"),
                                                            seqPatternNames(), d.seqPattern));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "seqRate", 1 }, "Figura", seqRateNames(), d.seqRate));
    layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { "seqRange", 1 }, "Cuerdas en uso", 2, pedrochord::kStrings, d.seqRange));
    return layout;
}

void PedrochordProcessor::prepareToPlay(double sampleRate, int)
{
    dsp.prepare(sampleRate);
}

bool PedrochordProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PedrochordProcessor::readParams()
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

    p.seqOn = get("seqOn") > 0.5f;
    p.seqPattern = juce::roundToInt(get("seqPattern"));
    p.seqRate = juce::roundToInt(get("seqRate"));
    p.seqRange = juce::roundToInt(get("seqRange"));
    p.seqBpm = get("seqBpm");
    if (get("seqSync") > 0.5f)
        if (auto* host = getPlayHead())
            if (const auto position = host->getPosition())
                if (const auto bpm = position->getBpm())
                    p.seqBpm = (float) *bpm;
    currentBpm.store(p.seqBpm, std::memory_order_relaxed);
}

void PedrochordProcessor::handle(const juce::MidiMessage& m)
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

void PedrochordProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    // Whatever the editor played since the last block goes in first.
    uiFifo.read(uiFifo.getNumReady()).forEach([&](int i) { midi.addEvent(uiEvents[(size_t) i].bytes, 3, 0); });
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

void PedrochordProcessor::playFromUi(const juce::MidiMessage& message)
{
    if (message.getRawDataSize() != 3)
        return;
    UiEvent event;
    std::copy_n(message.getRawData(), 3, event.bytes);
    uiFifo.write(1).forEach([&](int i) { uiEvents[(size_t) i] = event; }); // dropped only if 1024 are already waiting
}

juce::AudioProcessorEditor* PedrochordProcessor::createEditor() { return new PedrochordEditor(*this); }

void PedrochordProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PedrochordProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(state.state.getType()))
            state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PedrochordProcessor(); }
