#pragma once

#include <map>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

/** Knobs with a value arc in the colour of their section. */
class SDChordLook : public juce::LookAndFeel_V4
{
public:
    SDChordLook();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    /** Toggle buttons are drawn as hardware keys with an LED. */
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

/** 12 roots x 4 chord types. Hold a cell to play its chord; slide to change chords. Multi-touch aware. */
class ChordGrid : public juce::Component
{
public:
    explicit ChordGrid(SDChordProcessor& p) : processor(p) {}

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    /** Called by the editor's timer with the engine's state. */
    void show(int heldCode, int stringCode);

private:
    static constexpr float labelWidth = 64.0f;
    juce::Rectangle<float> cellBounds(int column, int type) const;
    int codeAt(juce::Point<float>) const;
    void press(int source, int code);
    void release(int source);

    SDChordProcessor& processor;
    std::map<int, int> pressed; // mouse/touch source -> chord code
    int held = -1, memory = 0;
};

/** The strum plate: 13 strings, lowest at the bottom. Touch one to pluck it, drag across to strum. */
class StrumPlate : public juce::Component
{
public:
    explicit StrumPlate(SDChordProcessor& p) : processor(p) {}

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    /** Repaints only while something rings or the chord changed. */
    void refresh();

private:
    juce::Rectangle<float> stringArea() const;
    float positionAt(float y) const; // 0 = bottom string, 1 = top
    void pluck(int string, float velocity);

    SDChordProcessor& processor;
    struct Finger
    {
        float position;
        double time;
    };
    std::map<int, Finger> fingers;
    std::array<float, sdchord::kStrings> glow {};
    int chordCode = -1;
};

class SDChordEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SDChordEditor(SDChordProcessor&);
    ~SDChordEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    /** Pulls the engine's state into the UI (the timer calls it 40 times a second). */
    void refreshFromEngine();

    // Computer keyboard: each row of keys is one chord type, left to right from C.
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType) override { pollKeys(); }

private:
    void timerCallback() override { refreshFromEngine(); }

    /** Reads the physical state of every mapped key and sends the presses and releases that changed.
        Key events alone can be late or go missing (focus moving, dead keys, several keys at once),
        which left chords stuck; polling the real state means a release can never be lost. */
    void pollKeys();

    std::array<bool, 12 * sdchord::kChordTypes> keysDown {};
    juce::TimedCallback keyPoller { [this] { pollKeys(); } };
    bool focusGrabbed = false;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::String label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Colour colour;
        int first, count;
    };

    SDChordProcessor& processor;
    SDChordLook look;
    ChordGrid grid;
    StrumPlate plate;
    std::array<Knob, 10> knobs;

    // Auto-strum panel.
    juce::ToggleButton seqOnButton { "AUTO" }, seqSyncButton { "SYNC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqOnAttachment, seqSyncAttachment;
    std::array<Knob, 4> seqKnobs;
    juce::Rectangle<float> seqReadout;
    juce::String shownSeq;
    juce::String seqReadoutText() const;

    // LFO panel: a lamp that follows the LFO, a readout and four knobs.
    std::array<Knob, 4> lfoKnobs;
    juce::Rectangle<float> lfoReadout;
    juce::Point<float> lfoLamp;
    juce::String shownLfo;
    float shownLfoLevel = 0.0f;
    juce::String lfoReadoutText() const;
    std::vector<Section> sections;
    int shownHeld = -2, shownMemory = -2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SDChordEditor)
};
