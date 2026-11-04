// Renders the editor off-screen to a PNG while the instrument plays a chord and a strum, so the UI
// can be checked (and screenshotted for the README) without clicking anything.
//   PedrochordSnapshot <out.png>

#include "../plugin/PluginEditor.h"
#include "../plugin/PluginProcessor.h"

int main(int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out(juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "snapshot.png"));

    PedrochordProcessor processor;
    processor.setPlayConfigDetails(0, 2, 48000.0, 512);
    processor.prepareToPlay(48000.0, 512);

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
    midi.addEvent(juce::MidiMessage::noteOn(1, pedrochord::noteForChord({ 9, pedrochord::ChordType::MinorSeventh }), 1.0f), 0);
    play(midi, 4);
    for (int s = 0; s < pedrochord::kStrings; ++s)
    {
        juce::MidiBuffer pluck;
        pluck.addEvent(juce::MidiMessage::noteOn(1, pedrochord::noteForString(s), 0.5f + 0.04f * (float) s), 0);
        play(pluck, 2);
    }
    play({}, 6);

    std::unique_ptr<PedrochordEditor> editor(static_cast<PedrochordEditor*>(processor.createEditor()));
    editor->refreshFromEngine();

    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
    out.deleteFile();
    juce::FileOutputStream stream(out);
    if (!stream.openedOk() || !juce::PNGImageFormat().writeImageToStream(image, stream))
        return 1;
    std::printf("%s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
