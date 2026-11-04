#include "PluginEditor.h"

namespace
{
namespace Colours
{
const juce::Colour bgTop { 0xff1a1630 }, bgBottom { 0xff0d0b18 };
const juce::Colour panel { 0xff221d3a }, panelEdge { 0xff352d55 };
const juce::Colour text { 0xfff3eefc }, muted { 0xffa49cc0 }, faint { 0xff6d6590 };
const juce::Colour glow { 0xffffcf7a }; // the strings
// One per chord type: major, minor, seventh, minor seventh.
const juce::Colour rows[] = { juce::Colour { 0xfff6e3bf }, juce::Colour { 0xffc6bdf7 }, juce::Colour { 0xfff7b9a0 }, juce::Colour { 0xffa9e6cf } };
} // namespace Colours

const char* const rootNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* const suffixes[] = { "", "m", "7", "m7" };
const char* const rowNames[] = { "Mayor", "menor", "7", "m7" };

juce::String chordName(int code)
{
    if (code < 0)
        return "-";
    return juce::String(rootNames[code % 12]) + suffixes[code / 12];
}

juce::String noteName(int midi) { return juce::String(rootNames[midi % 12]) + juce::String(midi / 12 - 1); }

juce::Font font(float height, bool bold = false)
{
    return juce::Font(juce::FontOptions {}.withHeight(height).withStyle(bold ? "Bold" : "Regular"));
}

juce::MidiMessage noteOn(int note, float velocity) { return juce::MidiMessage::noteOn(1, note, juce::jlimit(0.05f, 1.0f, velocity)); }
juce::MidiMessage noteOff(int note) { return juce::MidiMessage::noteOff(1, note); }
} // namespace

// ---- look and feel -----------------------------------------------------------------------------

RasguidoLook::RasguidoLook()
{
    setColour(juce::BubbleComponent::backgroundColourId, Colours::panel);
    setColour(juce::BubbleComponent::outlineColourId, Colours::panelEdge);
    setColour(juce::TooltipWindow::textColourId, Colours::text);
}

void RasguidoLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto accent = s.findColour(juce::Slider::rotarySliderFillColourId);
    const auto area = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(4.0f);
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const float angle = start + pos * (end - start);

    // Track and value arc.
    juce::Path track, value;
    track.addCentredArc(centre.x, centre.y, radius - 2, radius - 2, 0.0f, start, end, true);
    value.addCentredArc(centre.x, centre.y, radius - 2, radius - 2, 0.0f, start, angle, true);
    g.setColour(Colours::panelEdge);
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent);
    g.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Body.
    const float body = radius - 8.0f;
    g.setGradientFill(juce::ColourGradient(Colours::panelEdge.brighter(0.25f), centre.x, centre.y - body,
                                           Colours::panel.darker(0.3f), centre.x, centre.y + body, false));
    g.fillEllipse(centre.x - body, centre.y - body, body * 2, body * 2);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawEllipse(centre.x - body, centre.y - body, body * 2, body * 2, 1.0f);

    // Pointer.
    const auto tip = centre.getPointOnCircumference(body - 5.0f, angle);
    g.setColour(accent);
    g.fillEllipse(tip.x - 2.5f, tip.y - 2.5f, 5.0f, 5.0f);
}

// ---- chord grid --------------------------------------------------------------------------------

juce::Rectangle<float> ChordGrid::cellBounds(int root, int type) const
{
    const float w = ((float) getWidth() - labelWidth) / 12.0f;
    const float h = (float) getHeight() / (float) rasguido::kChordTypes;
    return { labelWidth + w * (float) root, h * (float) type, w, h };
}

int ChordGrid::codeAt(juce::Point<float> p) const
{
    if (p.x < labelWidth || !getLocalBounds().toFloat().contains(p))
        return -1;
    const int root = juce::jlimit(0, 11, (int) ((p.x - labelWidth) / (((float) getWidth() - labelWidth) / 12.0f)));
    const int type = juce::jlimit(0, rasguido::kChordTypes - 1, (int) (p.y / ((float) getHeight() / rasguido::kChordTypes)));
    return root + 12 * type;
}

void ChordGrid::paint(juce::Graphics& g)
{
    for (int type = 0; type < rasguido::kChordTypes; ++type)
    {
        const auto colour = Colours::rows[type];
        const auto row = cellBounds(0, type);
        g.setColour(colour.withAlpha(0.8f));
        g.setFont(font(13.0f, true));
        g.drawText(rowNames[type], juce::Rectangle<float>(0, row.getY(), labelWidth - 10, row.getHeight()), juce::Justification::centredRight);

        for (int root = 0; root < 12; ++root)
        {
            const int code = root + 12 * type;
            const auto cell = cellBounds(root, type).reduced(3.0f);
            const bool isHeld = code == held;
            const bool isMemory = !isHeld && held < 0 && code == memory;

            if (isHeld)
            {
                // Soft halo, then a solid cap.
                for (int i = 3; i > 0; --i)
                {
                    g.setColour(colour.withAlpha(0.09f * (float) i));
                    g.fillRoundedRectangle(cell.expanded(2.5f * (float) (4 - i)), 10.0f);
                }
                g.setColour(colour);
                g.fillRoundedRectangle(cell, 8.0f);
                g.setColour(Colours::bgBottom);
            }
            else
            {
                g.setGradientFill(juce::ColourGradient(colour.withAlpha(0.16f), cell.getX(), cell.getY(),
                                                       colour.withAlpha(0.07f), cell.getX(), cell.getBottom(), false));
                g.fillRoundedRectangle(cell, 8.0f);
                g.setColour(colour.withAlpha(isMemory ? 0.9f : 0.28f));
                g.drawRoundedRectangle(cell, 8.0f, isMemory ? 1.6f : 1.0f);
                g.setColour(colour.withAlpha(0.95f));
            }
            g.setFont(font(cell.getWidth() < 44 ? 12.5f : 14.0f, true));
            g.drawText(chordName(code), cell, juce::Justification::centred);
        }
    }
}

void ChordGrid::press(int source, int code)
{
    if (auto it = pressed.find(source); it != pressed.end())
    {
        if (it->second == code)
            return;
        processor.playFromUi(noteOff(rasguido::kFirstChordNote + it->second));
        pressed.erase(it);
    }
    if (code >= 0)
    {
        pressed[source] = code;
        processor.playFromUi(noteOn(rasguido::kFirstChordNote + code, 1.0f));
    }
}

void ChordGrid::release(int source)
{
    if (auto it = pressed.find(source); it != pressed.end())
    {
        processor.playFromUi(noteOff(rasguido::kFirstChordNote + it->second));
        pressed.erase(it);
    }
}

void ChordGrid::mouseDown(const juce::MouseEvent& e) { press(e.source.getIndex(), codeAt(e.position)); }
void ChordGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (const int code = codeAt(e.position); code >= 0)
        press(e.source.getIndex(), code); // sliding onto another chord switches to it
}
void ChordGrid::mouseUp(const juce::MouseEvent& e) { release(e.source.getIndex()); }

void ChordGrid::show(int heldCode, int stringCode)
{
    if (heldCode != held || stringCode != memory)
    {
        held = heldCode;
        memory = stringCode;
        repaint();
    }
}

// ---- strum plate -------------------------------------------------------------------------------

juce::Rectangle<float> StrumPlate::stringArea() const { return getLocalBounds().toFloat().reduced(18.0f, 26.0f).withTrimmedLeft(30.0f); }

float StrumPlate::positionAt(float y) const
{
    const auto area = stringArea();
    return juce::jlimit(0.0f, 1.0f, 1.0f - (y - area.getY()) / area.getHeight());
}

void StrumPlate::pluck(int string, float velocity)
{
    processor.playFromUi(noteOn(rasguido::noteForString(string), velocity));
}

void StrumPlate::mouseDown(const juce::MouseEvent& e)
{
    const float p = positionAt(e.position.y);
    fingers[e.source.getIndex()] = { p, juce::Time::getMillisecondCounterHiRes() };
    pluck(juce::jlimit(0, rasguido::kStrings - 1, (int) (p * rasguido::kStrings)), 0.75f);
}

void StrumPlate::mouseDrag(const juce::MouseEvent& e)
{
    auto it = fingers.find(e.source.getIndex());
    if (it == fingers.end())
        return;
    const float p = positionAt(e.position.y);
    const double now = juce::Time::getMillisecondCounterHiRes();
    auto& finger = it->second;

    // Faster strums play louder.
    const double seconds = juce::jmax(0.001, (now - finger.time) * 0.001);
    const float speed = (float) (std::abs(p - finger.position) / seconds); // plates per second
    const float velocity = juce::jlimit(0.3f, 1.0f, 0.35f + speed * 0.12f);

    const float lo = juce::jmin(finger.position, p), hi = juce::jmax(finger.position, p);
    for (int s = 0; s < rasguido::kStrings; ++s)
    {
        const float centre = ((float) s + 0.5f) / (float) rasguido::kStrings;
        if (centre > lo && centre <= hi)
            pluck(s, velocity);
    }
    finger = { p, now };
}

void StrumPlate::mouseUp(const juce::MouseEvent& e) { fingers.erase(e.source.getIndex()); }

void StrumPlate::refresh()
{
    const auto& engine = processor.engine();
    bool changed = engine.stringChordCode() != chordCode;
    chordCode = engine.stringChordCode();
    for (int s = 0; s < rasguido::kStrings; ++s)
    {
        const float level = engine.stringGlow(s);
        changed = changed || std::abs(level - glow[(size_t) s]) > 0.003f;
        glow[(size_t) s] = level;
    }
    if (changed)
        repaint();
}

void StrumPlate::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(Colours::panel, 0, 0, Colours::bgBottom, 0, bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 18.0f);
    g.setColour(Colours::panelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 18.0f, 1.0f);

    const auto area = stringArea();
    const float gap = area.getHeight() / (float) rasguido::kStrings;
    const auto notes = rasguido::stringNotes({ juce::jmax(0, chordCode) % 12, static_cast<rasguido::ChordType>(juce::jmax(0, chordCode) / 12) });

    for (int s = 0; s < rasguido::kStrings; ++s)
    {
        const float y = area.getBottom() - gap * ((float) s + 0.5f);
        const float level = juce::jlimit(0.0f, 1.0f, glow[(size_t) s] * 1.6f);

        g.setColour(Colours::faint);
        g.setFont(font(11.0f));
        g.drawText(noteName(notes[(size_t) s]), juce::Rectangle<float>(area.getX() - 34, y - 8, 30, 16), juce::Justification::centredRight);

        if (level > 0.01f)
        {
            // Glow: wide and faint underneath, narrow and bright on top. A ringing string shimmers a little.
            const float shimmer = 1.5f * level * std::sin((float) juce::Time::getMillisecondCounter() * 0.05f + (float) s);
            for (int i = 4; i > 0; --i)
            {
                g.setColour(Colours::glow.withAlpha(level * 0.12f * (float) (5 - i)));
                g.fillRoundedRectangle(area.getX(), y - (float) i * 2.2f + shimmer * 0.3f, area.getWidth(), (float) i * 4.4f, (float) i * 2.2f);
            }
        }
        g.setColour(Colours::panelEdge.brighter(0.3f).interpolatedWith(Colours::glow, level));
        g.fillRoundedRectangle(area.getX(), y - 1.0f - level, area.getWidth(), 2.0f + 2.0f * level, 1.5f);
    }
}

// ---- editor ------------------------------------------------------------------------------------

RasguidoEditor::RasguidoEditor(RasguidoProcessor& p) : AudioProcessorEditor(&p), processor(p), grid(p), plate(p)
{
    setLookAndFeel(&look);

    const std::pair<const char*, const char*> params[] = {
        { "chordLevel", "Nivel" }, { "chordTone", "Tono" },    { "bassLevel", "Nivel" }, { "stringLevel", "Nivel" },
        { "stringDecay", "Sustain" }, { "stringTone", "Brillo" }, { "chorus", "Chorus" },  { "reverb", "Reverb" },
        { "wobble", "Wobble" },     { "master", "Volumen" },
    };
    sections = {
        { "ACORDES", Colours::rows[1], 0, 2 },
        { "BAJO", Colours::rows[2], 2, 1 },
        { "CUERDAS", Colours::glow, 3, 3 },
        { "EFECTOS", Colours::rows[3], 6, 3 },
        { "SALIDA", Colours::rows[0], 9, 1 },
    };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.label = params[i].second;
        k.slider.setPopupDisplayEnabled(true, true, this);
        k.slider.setMouseDragSensitivity(180);
        addAndMakeVisible(k.slider);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, params[i].first, k.slider);
    }
    for (const auto& s : sections)
        for (int i = s.first; i < s.first + s.count; ++i)
            knobs[(size_t) i].slider.setColour(juce::Slider::rotarySliderFillColourId, s.colour);

    addAndMakeVisible(grid);
    addAndMakeVisible(plate);
    setSize(1040, 600);
    startTimerHz(40);
}

RasguidoEditor::~RasguidoEditor() { setLookAndFeel(nullptr); }

void RasguidoEditor::refreshFromEngine()
{
    const auto& engine = processor.engine();
    const int held = engine.heldChordCode(), memory = engine.stringChordCode();
    grid.show(held, memory);
    plate.refresh();
    if (held != shownHeld || memory != shownMemory)
    {
        shownHeld = held;
        shownMemory = memory;
        repaint(getLocalBounds().removeFromTop(80));
    }
}

void RasguidoEditor::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(Colours::bgTop, 0, 0, Colours::bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    // Title.
    g.setColour(Colours::text);
    g.setFont(font(30.0f, true));
    g.drawText("Rasguido", 32, 20, 220, 40, juce::Justification::centredLeft);
    g.setColour(Colours::muted);
    g.setFont(font(13.0f));
    g.drawText("acordes + arpa", 182, 32, 160, 20, juce::Justification::centredLeft);

    // Current chord: what you hold, or (dimmer) what the strings will play.
    const bool holding = shownHeld >= 0;
    const auto pill = juce::Rectangle<float>(470, 18, 150, 44);
    const auto colour = Colours::rows[juce::jmax(0, holding ? shownHeld : shownMemory) / 12];
    g.setColour(colour.withAlpha(holding ? 0.22f : 0.08f));
    g.fillRoundedRectangle(pill, 22.0f);
    g.setColour(colour.withAlpha(holding ? 1.0f : 0.45f));
    g.drawRoundedRectangle(pill, 22.0f, 1.2f);
    g.setFont(font(24.0f, true));
    g.drawText(chordName(holding ? shownHeld : shownMemory), pill, juce::Justification::centred);
    g.setColour(Colours::faint);
    g.setFont(font(12.0f));
    g.drawText(holding ? "sonando" : "en las cuerdas", pill.withX(pill.getRight() + 12).withWidth(110), juce::Justification::centredLeft);

    // Knob sections.
    for (const auto& s : sections)
    {
        const auto first = knobs[(size_t) s.first].slider.getBounds();
        const auto last = knobs[(size_t) (s.first + s.count - 1)].slider.getBounds();
        const auto box = first.getUnion(last).toFloat().expanded(6.0f, 0.0f).withTop((float) first.getY() - 30).withBottom((float) first.getBottom() + 26);
        g.setColour(Colours::panel.withAlpha(0.55f));
        g.fillRoundedRectangle(box, 14.0f);
        g.setColour(s.colour.withAlpha(0.85f));
        g.setFont(font(11.5f, true));
        g.drawText(s.title, box.withHeight(26).translated(0, 4), juce::Justification::centred);

        for (int i = s.first; i < s.first + s.count; ++i)
        {
            const auto& k = knobs[(size_t) i];
            g.setColour(Colours::muted);
            g.setFont(font(12.0f));
            g.drawText(k.label, k.slider.getBounds().withY(k.slider.getBottom() - 2).withHeight(20), juce::Justification::centred);
        }
    }

    // How to play it from a DAW.
    g.setColour(Colours::faint);
    g.setFont(font(11.5f));
    g.drawText(juce::String::fromUTF8("MIDI: C1 mayor \xc2\xb7 C2 menor \xc2\xb7 C3 7 \xc2\xb7 C4 m7 \xc2\xb7 C5-C6 cuerdas \xc2\xb7 rueda de modulaci\xc3\xb3n = rasgueo"),
               32, getHeight() - 30, 700, 18, juce::Justification::centredLeft);
}

void RasguidoEditor::resized()
{
    plate.setBounds(getWidth() - 300, 24, 276, getHeight() - 48);
    grid.setBounds(32, 90, getWidth() - 300 - 32 - 28, 280);

    // Knobs in one row, grouped with a gap between sections.
    const int knob = 54, gap = 6, sectionGap = 22, y = 450;
    int x = 38;
    for (const auto& s : sections)
    {
        for (int i = s.first; i < s.first + s.count; ++i)
        {
            knobs[(size_t) i].slider.setBounds(x, y, knob, knob);
            x += knob + gap;
        }
        x += sectionGap - gap;
    }
}
