// Renders the editor off-screen to a PNG while the instrument plays a chord and a strum, so the UI
// can be checked (and screenshotted for the README) without clicking anything.
//   SDChordSnapshot <out.png>

#include "../plugin/PluginEditor.h"
#include "../plugin/PluginProcessor.h"

int main(int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "snapshot.png"));

    SDChordProcessor processor;
    processor.setPlayConfigDetails(0, 2, 48000.0, 512);
    processor.prepareToPlay(48000.0, 512);

    // Show the auto-strum switched on, playing up and down in eighths.
    processor.state.getParameter("seqOn")->setValueNotifyingHost(1.0f);
    processor.state.getParameter("seqPattern")->setValueNotifyingHost(processor.state.getParameter("seqPattern")->convertTo0to1(2.0f));
    // And a gentle tremolo on the LFO.
    processor.state.getParameter("lfoTarget")->setValueNotifyingHost(processor.state.getParameter("lfoTarget")->convertTo0to1(1.0f));
    processor.state.getParameter("lfoDepth")->setValueNotifyingHost(0.4f);

    juce::AudioBuffer<float> buffer(2, 512);
    auto play = [&](juce::MidiBuffer midi, int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            processor.processBlock(buffer, midi);
            midi.clear();
        }
    };

    // Hold A minor 7 and strum up across the plate, quickly, then let it ring a moment.
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, sdchord::noteForChord({ 9, sdchord::ChordType::MinorSeventh }), 1.0f), 0);
    play(midi, 4);
    for (int s = 0; s < sdchord::kStrings; ++s)
    {
        juce::MidiBuffer pluck;
        pluck.addEvent(juce::MidiMessage::noteOn(1, sdchord::noteForString(s), 0.5f + 0.04f * (float) s), 0);
        play(pluck, 2);
    }
    play({}, 6);

    std::unique_ptr<SDChordEditor> editor(static_cast<SDChordEditor*>(processor.createEditor()));
    editor->refreshFromEngine();

    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
    out.deleteFile();
    juce::FileOutputStream stream(out);
    if (!stream.openedOk() || !juce::PNGImageFormat().writeImageToStream(image, stream))
        return 1;
    std::printf("%s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
