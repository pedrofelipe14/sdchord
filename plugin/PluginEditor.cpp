#include "PluginEditor.h"

// The editor is drawn as a piece of 1980s hardware: a cream plastic body with silkscreened labels,
// raised chord keys, a red LED readout, a ribbed metal strum plate and black knurled knobs.

namespace
{
namespace Colours
{
const juce::Colour bodyTop { 0xffeee4cf }, bodyBottom { 0xffd9c9aa }, bodyEdge { 0xffa8946f };
const juce::Colour print { 0xff4a3a2b }, printSoft { 0xff7d6a54 }; // silkscreen ink
const juce::Colour stripes[] = { juce::Colour { 0xff6b4630 }, juce::Colour { 0xffc5662f }, juce::Colour { 0xffd9a441 } };
const juce::Colour keyTop { 0xfff6f1e6 }, keyBottom { 0xffd8cfbf }, keySide { 0xff9c8f7b };
const juce::Colour recess { 0xffcbb995 }; // panels set into the body
const juce::Colour ledOn { 0xffff3b22 }, ledOff { 0xff5a1810 }, ledWindow { 0xff1d0c09 };
const juce::Colour plateTop { 0xff34302b }, plateBottom { 0xff1f1c19 }, ridge { 0xff4a443d };
const juce::Colour ringing { 0xffd9d3c7 }; // the ridge of a string that is sounding
} // namespace Colours

const char* const rootNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
const char* const suffixes[] = { "", "m", "7", "m7" };
const char* const rowNames[] = { "MAYOR", "MENOR", "7", "m7" };

juce::String chordName(int code)
{
    if (code < 0)
        return "-";
    return juce::String(rootNames[code % 12]) + suffixes[code / 12];
}

juce::String noteName(int midi) { return juce::String(rootNames[midi % 12]) + juce::String(midi / 12 - 1); }

juce::Font font(float height, const char* style = "Regular")
{
    return juce::Font(juce::FontOptions {}.withHeight(height).withStyle(style));
}

juce::MidiMessage noteOn(int note, float velocity) { return juce::MidiMessage::noteOn(1, note, juce::jlimit(0.05f, 1.0f, velocity)); }
juce::MidiMessage noteOff(int note) { return juce::MidiMessage::noteOff(1, note); }

/** A panel moulded into the body: darker, with a shadow on the top edge and light on the bottom. */
void drawRecess(juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour(Colours::recess);
    g.fillRoundedRectangle(r, corner);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.22f), 0, r.getY(),
                                           juce::Colours::transparentBlack, 0, r.getY() + 10, false));
    g.fillRoundedRectangle(r, corner);
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawLine(r.getX() + corner, r.getBottom() - 0.5f, r.getRight() - corner, r.getBottom() - 0.5f, 1.0f);
    g.setColour(Colours::bodyEdge.withAlpha(0.6f));
    g.drawRoundedRectangle(r, corner, 1.0f);
}

/** A small round LED with its glow. */
void drawLed(juce::Graphics& g, juce::Point<float> c, float radius, float level)
{
    if (level > 0.01f)
    {
        g.setColour(Colours::ledOn.withAlpha(0.25f * level));
        g.fillEllipse(juce::Rectangle<float>(radius * 5, radius * 5).withCentre(c));
    }
    g.setColour(Colours::ledOff.interpolatedWith(Colours::ledOn, level));
    g.fillEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(c));
    g.setColour(juce::Colours::white.withAlpha(0.35f + 0.3f * level));
    g.fillEllipse(juce::Rectangle<float>(radius * 0.7f, radius * 0.7f).withCentre(c.translated(-radius * 0.3f, -radius * 0.3f)));
}
} // namespace

// ---- look and feel -----------------------------------------------------------------------------

PedrochordLook::PedrochordLook()
{
    setColour(juce::BubbleComponent::backgroundColourId, Colours::ledWindow);
    setColour(juce::BubbleComponent::outlineColourId, Colours::print);
    setColour(juce::TooltipWindow::textColourId, Colours::ledOn);
}

void PedrochordLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    const auto area = juce::Rectangle<int>(x, y, w, h).toFloat();
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const float angle = start + pos * (end - start);

    // Scale printed on the body around the knob.
    g.setColour(Colours::print);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const float inner = radius - (i % 5 == 0 ? 6.0f : 4.0f);
        g.drawLine(juce::Line<float>(centre.getPointOnCircumference(inner, a), centre.getPointOnCircumference(radius - 1.0f, a)),
                   i % 5 == 0 ? 1.4f : 0.9f);
    }

    // Knurled black knob: shadow, ribbed skirt, domed cap.
    const float skirt = radius - 9.0f, cap = skirt - 4.0f;
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillEllipse(juce::Rectangle<float>(skirt * 2, skirt * 2).withCentre(centre.translated(0, 2.5f)));
    g.setColour(juce::Colour(0xff151312));
    g.fillEllipse(juce::Rectangle<float>(skirt * 2, skirt * 2).withCentre(centre));
    g.setColour(juce::Colour(0xff3a3632));
    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i * juce::MathConstants<float>::twoPi / 28.0f + angle;
        g.drawLine(juce::Line<float>(centre.getPointOnCircumference(cap, a), centre.getPointOnCircumference(skirt - 0.5f, a)), 1.0f);
    }
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4540), centre.x - cap * 0.4f, centre.y - cap * 0.6f,
                                           juce::Colour(0xff121110), centre.x + cap * 0.5f, centre.y + cap, true));
    g.fillEllipse(juce::Rectangle<float>(cap * 2, cap * 2).withCentre(centre));

    // White pointer line.
    g.setColour(juce::Colour(0xfff2ede2));
    g.drawLine(juce::Line<float>(centre.getPointOnCircumference(cap * 0.25f, angle), centre.getPointOnCircumference(cap - 1.5f, angle)), 2.2f);
}

// ---- chord grid --------------------------------------------------------------------------------

juce::Rectangle<float> ChordGrid::cellBounds(int root, int type) const
{
    const float w = ((float) getWidth() - labelWidth) / 12.0f;
    const float h = (float) getHeight() / (float) pedrochord::kChordTypes;
    return { labelWidth + w * (float) root, h * (float) type, w, h };
}

int ChordGrid::codeAt(juce::Point<float> p) const
{
    if (p.x < labelWidth || !getLocalBounds().toFloat().contains(p))
        return -1;
    const int root = juce::jlimit(0, 11, (int) ((p.x - labelWidth) / (((float) getWidth() - labelWidth) / 12.0f)));
    const int type = juce::jlimit(0, pedrochord::kChordTypes - 1, (int) (p.y / ((float) getHeight() / pedrochord::kChordTypes)));
    return root + 12 * type;
}

void ChordGrid::paint(juce::Graphics& g)
{
    drawRecess(g, getLocalBounds().toFloat().withTrimmedLeft(labelWidth - 8.0f), 10.0f);

    for (int type = 0; type < pedrochord::kChordTypes; ++type)
    {
        const auto row = cellBounds(0, type);
        g.setColour(Colours::print);
        g.setFont(font(12.5f, "Bold"));
        g.drawText(rowNames[type], juce::Rectangle<float>(0, row.getY(), labelWidth - 16, row.getHeight()), juce::Justification::centredRight);

        for (int root = 0; root < 12; ++root)
        {
            const int code = root + 12 * type;
            const bool down = code == held;
            const bool remembered = held < 0 && code == memory;
            auto key = cellBounds(root, type).reduced(5.0f, 7.0f);
            const float depth = down ? 1.5f : 5.0f; // how far the key stands out of the body

            // Shadow and side of the key, then its face.
            g.setColour(juce::Colours::black.withAlpha(down ? 0.12f : 0.22f));
            g.fillRoundedRectangle(key.translated(0, depth + 1.5f), 5.0f);
            g.setColour(Colours::keySide);
            g.fillRoundedRectangle(key.translated(0, depth), 5.0f);
            key.translate(0, down ? 3.5f : 0.0f);
            g.setGradientFill(juce::ColourGradient(down ? Colours::keyBottom : Colours::keyTop, 0, key.getY(),
                                                   down ? Colours::keyBottom.darker(0.08f) : Colours::keyBottom, 0, key.getBottom(), false));
            g.fillRoundedRectangle(key, 5.0f);
            g.setColour(juce::Colours::white.withAlpha(down ? 0.2f : 0.7f));
            g.drawLine(key.getX() + 4, key.getY() + 1, key.getRight() - 4, key.getY() + 1, 1.0f);
            g.setColour(Colours::keySide.withAlpha(0.8f));
            g.drawRoundedRectangle(key, 5.0f, 0.8f);

            g.setColour(Colours::print);
            g.setFont(font(key.getWidth() < 40 ? 11.5f : 13.0f, "Bold"));
            g.drawText(chordName(code), key.withTrimmedTop(6.0f), juce::Justification::centred);

            // A small LED on each key: lit while held, dim for the chord the strings remember.
            drawLed(g, { key.getCentreX(), key.getY() + 7.0f }, 2.6f, down ? 1.0f : remembered ? 0.35f : 0.0f);
        }
    }
}

void ChordGrid::press(int source, int code)
{
    if (auto it = pressed.find(source); it != pressed.end())
    {
        if (it->second == code)
            return;
        processor.playFromUi(noteOff(pedrochord::kFirstChordNote + it->second));
        pressed.erase(it);
    }
    if (code >= 0)
    {
        pressed[source] = code;
        processor.playFromUi(noteOn(pedrochord::kFirstChordNote + code, 1.0f));
    }
}

void ChordGrid::release(int source)
{
    if (auto it = pressed.find(source); it != pressed.end())
    {
        processor.playFromUi(noteOff(pedrochord::kFirstChordNote + it->second));
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

juce::Rectangle<float> StrumPlate::stringArea() const { return getLocalBounds().toFloat().reduced(26.0f, 40.0f).withTrimmedLeft(26.0f); }

float StrumPlate::positionAt(float y) const
{
    const auto area = stringArea();
    return juce::jlimit(0.0f, 1.0f, 1.0f - (y - area.getY()) / area.getHeight());
}

void StrumPlate::pluck(int string, float velocity)
{
    processor.playFromUi(noteOn(pedrochord::noteForString(string), velocity));
}

void StrumPlate::mouseDown(const juce::MouseEvent& e)
{
    const float p = positionAt(e.position.y);
    fingers[e.source.getIndex()] = { p, juce::Time::getMillisecondCounterHiRes() };
    pluck(juce::jlimit(0, pedrochord::kStrings - 1, (int) (p * pedrochord::kStrings)), 0.75f);
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
    for (int s = 0; s < pedrochord::kStrings; ++s)
    {
        const float centre = ((float) s + 0.5f) / (float) pedrochord::kStrings;
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
    for (int s = 0; s < pedrochord::kStrings; ++s)
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

    // Moulded frame around the plate.
    drawRecess(g, bounds, 16.0f);
    g.setColour(Colours::print);
    g.setFont(font(11.0f, "Bold"));
    g.drawText("RASGUEO", bounds.withHeight(30).translated(0, 6), juce::Justification::centred);
    g.setColour(Colours::printSoft);
    g.setFont(font(10.0f));
    g.drawText(juce::String::fromUTF8("\xe2\x96\xb2 agudo arriba  \xc2\xb7  grave abajo \xe2\x96\xbc"), bounds.withHeight(20).withBottomY(bounds.getBottom() - 8),
               juce::Justification::centred);

    // The dark metal plate with a bevelled edge.
    const auto plate = bounds.reduced(16.0f, 32.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff8c857b), plate.getX(), plate.getY(), juce::Colour(0xff3a3530), plate.getRight(), plate.getBottom(), false));
    g.fillRoundedRectangle(plate.expanded(3.0f), 9.0f);
    g.setGradientFill(juce::ColourGradient(Colours::plateTop, 0, plate.getY(), Colours::plateBottom, 0, plate.getBottom(), false));
    g.fillRoundedRectangle(plate, 7.0f);

    const auto area = stringArea();
    const float gap = area.getHeight() / (float) pedrochord::kStrings;
    const int code = juce::jmax(0, chordCode);
    const auto notes = pedrochord::stringNotes({ code % 12, static_cast<pedrochord::ChordType>(code / 12) });

    for (int s = 0; s < pedrochord::kStrings; ++s)
    {
        const float y = area.getBottom() - gap * ((float) s + 0.5f);
        const float level = juce::jlimit(0.0f, 1.0f, glow[(size_t) s] * 1.6f);

        g.setColour(juce::Colour(0xffa79f92));
        g.setFont(font(10.0f));
        g.drawText(noteName(notes[(size_t) s]), juce::Rectangle<float>(area.getX() - 30, y - 8, 26, 16), juce::Justification::centredRight);

        // Each string is a ridge pressed into the metal: dark groove above, highlight below.
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(area.getX(), y - 2.0f, area.getWidth(), 2.0f);
        // A ringing string only catches a little more light on its ridge.
        g.setColour(Colours::ridge.brighter(0.4f).interpolatedWith(Colours::ringing, level * 0.8f));
        g.fillRect(area.getX(), y, area.getWidth(), 1.0f);
    }
}

// ---- editor ------------------------------------------------------------------------------------

PedrochordEditor::PedrochordEditor(PedrochordProcessor& p) : AudioProcessorEditor(&p), processor(p), grid(p), plate(p)
{
    setLookAndFeel(&look);

    const std::pair<const char*, const char*> params[] = {
        { "chordLevel", "NIVEL" }, { "chordTone", "TONO" },     { "bassLevel", "NIVEL" }, { "stringLevel", "NIVEL" },
        { "stringDecay", "SUSTAIN" }, { "stringTone", "BRILLO" }, { "chorus", "CHORUS" },  { "reverb", "REVERB" },
        { "wobble", "WOBBLE" },     { "master", "VOLUMEN" },
    };
    sections = {
        { "ACORDES", Colours::print, 0, 2 },
        { "BAJO", Colours::print, 2, 1 },
        { "CUERDAS", Colours::print, 3, 3 },
        { "EFECTOS", Colours::print, 6, 3 },
        { "SALIDA", Colours::print, 9, 1 },
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

    addAndMakeVisible(grid);
    addAndMakeVisible(plate);
    setWantsKeyboardFocus(true);
    setSize(1060, 620);
    startTimerHz(40);
    keyPoller.startTimer(4);
}

PedrochordEditor::~PedrochordEditor()
{
    keyPoller.stopTimer();
    for (size_t code = 0; code < keysDown.size(); ++code)
        if (keysDown[code])
            processor.playFromUi(noteOff(pedrochord::kFirstChordNote + (int) code));
    setLookAndFeel(nullptr);
}

// ---- computer keyboard -------------------------------------------------------------------------

namespace
{
using KeyRow = std::array<juce::juce_wchar, 12>;

/** One row per chord type, twelve keys from C to B, laid out for a Latin American keyboard (letters and
    digits work on any layout). The bottom row only has ten keys, so it stops at A minor 7 (0 = no key). */
const std::array<KeyRow, pedrochord::kChordTypes>& keyRows()
{
    static const std::array<KeyRow, pedrochord::kChordTypes> rows {
        KeyRow { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\'', 0xbf }, // ... 0 ' ¿
        KeyRow { 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 0xb4, '+' }, //  ... p ´ +
        KeyRow { 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xf1, '{', '}' }, //  ... l ñ { }
        KeyRow { 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-', 0, 0 },
    };
    return rows;
}

bool isMappedKey(juce::juce_wchar c)
{
    c = juce::CharacterFunctions::toLowerCase(c);
    for (const auto& row : keyRows())
        if (c != 0 && std::find(row.begin(), row.end(), c) != row.end())
            return true;
    return false;
}
} // namespace

bool PedrochordEditor::keyPressed(const juce::KeyPress& key)
{
    // Swallow mapped keys (auto-repeat included) so the host doesn't act on them too.
    return !key.getModifiers().isCommandDown() && isMappedKey(key.getTextCharacter());
}

bool PedrochordEditor::keyStateChanged(bool)
{
    pollKeys(); // react to the event right away; the poller covers anything the events miss
    return std::any_of(keysDown.begin(), keysDown.end(), [](bool down) { return down; });
}

void PedrochordEditor::pollKeys()
{
    // Only while this editor has the keyboard: typing elsewhere must not play, and losing focus with
    // keys held lets them go. Ctrl/Alt shortcuts (Ctrl+S in a DAW...) never play chords.
    const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
    const bool active = isShowing() && hasKeyboardFocus(true) && !mods.isCommandDown() && !mods.isAltDown();

    const auto& rows = keyRows();
    for (int type = 0; type < pedrochord::kChordTypes; ++type)
    {
        for (int root = 0; root < 12; ++root)
        {
            const auto key = rows[(size_t) type][(size_t) root];
            if (key == 0)
                continue;
            const int code = root + 12 * type;
            const bool down = active && juce::KeyPress::isKeyCurrentlyDown((int) key);
            if (down == keysDown[(size_t) code])
                continue;
            keysDown[(size_t) code] = down;
            processor.playFromUi(down ? noteOn(pedrochord::kFirstChordNote + code, 1.0f) : noteOff(pedrochord::kFirstChordNote + code));
        }
    }
}

void PedrochordEditor::refreshFromEngine()
{
    // Take keyboard focus once the window is up, so typing plays chords straight away.
    if (!focusGrabbed && isShowing())
    {
        grabKeyboardFocus();
        focusGrabbed = true;
    }

    const auto& engine = processor.engine();
    const int held = engine.heldChordCode(), memory = engine.stringChordCode();
    grid.show(held, memory);
    plate.refresh();
    if (held != shownHeld || memory != shownMemory)
    {
        shownHeld = held;
        shownMemory = memory;
        repaint(getLocalBounds().removeFromTop(100));
    }
}

void PedrochordEditor::paint(juce::Graphics& g)
{
    // Behind the instrument, then the body itself with a soft highlight along the top.
    g.fillAll(juce::Colour(0xff2b2420));
    const auto body = getLocalBounds().toFloat().reduced(6.0f);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(body.translated(0, 3), 30.0f);
    g.setGradientFill(juce::ColourGradient(Colours::bodyTop, 0, body.getY(), Colours::bodyBottom, 0, body.getBottom(), false));
    g.fillRoundedRectangle(body, 30.0f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawRoundedRectangle(body.reduced(1.5f), 29.0f, 1.2f);
    g.setColour(Colours::bodyEdge);
    g.drawRoundedRectangle(body, 30.0f, 1.5f);

    // Faint plastic grain, the same every time.
    juce::Random grain(7);
    for (int i = 0; i < 2600; ++i)
    {
        g.setColour((i % 2 ? juce::Colours::white : Colours::bodyEdge).withAlpha(0.06f + grain.nextFloat() * 0.06f));
        g.fillRect(body.getX() + grain.nextFloat() * body.getWidth(), body.getY() + grain.nextFloat() * body.getHeight(), 1.0f, 1.0f);
    }

    // Brand, silkscreened, with the three-colour stripe under it.
    const float left = 44.0f;
    g.setColour(Colours::print);
    g.setFont(font(34.0f, "Bold Italic"));
    g.drawText("Pedrochord", juce::Rectangle<float>(left, 22, 240, 44), juce::Justification::centredLeft);
    g.setFont(font(11.5f, "Bold"));
    g.setColour(Colours::printSoft);
    g.drawText(juce::String::fromUTF8("ARPA ELECTR\xc3\x93NICA DE ACORDES"), juce::Rectangle<float>(left + 2, 64, 300, 16), juce::Justification::centredLeft);
    const float stripeRight = (float) plate.getX() - 30.0f;
    for (int i = 0; i < 3; ++i)
    {
        g.setColour(Colours::stripes[i]);
        g.fillRect(left, 88.0f + (float) i * 5.0f, stripeRight - left, 3.0f);
    }

    // Red LED readout with the current chord (dimmer when it is only the strings' memory).
    const bool holding = shownHeld >= 0;
    const auto window = juce::Rectangle<float>(stripeRight - 214, 20, 150, 46);
    g.setColour(Colours::bodyEdge);
    g.fillRoundedRectangle(window.expanded(4.0f), 8.0f);
    g.setColour(Colours::ledWindow);
    g.fillRoundedRectangle(window, 5.0f);
    g.setFont(font(30.0f, "Bold"));
    const auto lit = Colours::ledOn.withAlpha(holding ? 1.0f : 0.55f);
    g.setColour(lit.withAlpha(holding ? 0.3f : 0.15f));
    g.drawText(chordName(holding ? shownHeld : shownMemory), window.translated(0, 1).expanded(1.0f), juce::Justification::centred);
    g.setColour(lit);
    g.drawText(chordName(holding ? shownHeld : shownMemory), window, juce::Justification::centred);
    g.setColour(Colours::print);
    g.setFont(font(10.0f, "Bold"));
    g.drawText("ACORDE", window.withY(window.getBottom() + 5).withHeight(13), juce::Justification::centred);

    // Power LED.
    drawLed(g, { stripeRight - 30, 42 }, 4.0f, 1.0f);
    g.setColour(Colours::print);
    g.setFont(font(9.5f, "Bold"));
    g.drawText("ON", juce::Rectangle<float>(stripeRight - 48, 50, 36, 14), juce::Justification::centred);

    // Control panel set into the body, with bracketed section titles like a hardware front panel.
    const auto firstKnob = knobs.front().slider.getBounds().toFloat();
    drawRecess(g, juce::Rectangle<float>(left, firstKnob.getY() - 40, stripeRight - left, firstKnob.getHeight() + 70), 12.0f);
    for (const auto& s : sections)
    {
        const auto first = knobs[(size_t) s.first].slider.getBounds().toFloat();
        const auto last = knobs[(size_t) (s.first + s.count - 1)].slider.getBounds().toFloat();
        const float x0 = first.getX() + 4, x1 = last.getRight() - 4, y = first.getY() - 18;
        g.setColour(Colours::print);
        g.setFont(font(10.5f, "Bold"));
        const float textWidth = (float) s.title.length() * 7.5f + 12.0f;
        const float mid = (x0 + x1) * 0.5f;
        g.drawText(s.title, juce::Rectangle<float>(mid - textWidth / 2, y - 7, textWidth, 14), juce::Justification::centred);
        if (x1 - x0 > textWidth + 8)
        {
            g.drawLine(x0, y, mid - textWidth / 2, y, 1.0f);
            g.drawLine(mid + textWidth / 2, y, x1, y, 1.0f);
            g.drawLine(x0, y, x0, y + 5, 1.0f);
            g.drawLine(x1, y, x1, y + 5, 1.0f);
        }

        for (int i = s.first; i < s.first + s.count; ++i)
        {
            const auto& k = knobs[(size_t) i];
            g.setFont(font(10.0f, "Bold"));
            g.drawText(k.label, k.slider.getBounds().withY(k.slider.getBottom() + 1).withHeight(16), juce::Justification::centred);
        }
    }

    // How to play it from a DAW, printed small along the bottom edge.
    g.setColour(Colours::printSoft);
    g.setFont(font(10.5f));
    g.drawText(juce::String::fromUTF8("TECLADO: 1-0 mayor \xc2\xb7 Q-P menor \xc2\xb7 A-\xc3\x91 7 \xc2\xb7 Z-M m7"),
               juce::Rectangle<float>(left, (float) getHeight() - 52, 640, 16), juce::Justification::centredLeft);
    g.drawText(juce::String::fromUTF8("MIDI: C1 mayor \xc2\xb7 C2 menor \xc2\xb7 C3 7 \xc2\xb7 C4 m7 \xc2\xb7 C5-C6 cuerdas \xc2\xb7 rueda de modulaci\xc3\xb3n = rasgueo"),
               juce::Rectangle<float>(left, (float) getHeight() - 36, 640, 16), juce::Justification::centredLeft);
}

void PedrochordEditor::resized()
{
    plate.setBounds(getWidth() - 318, 28, 284, getHeight() - 56);
    const int left = 44, right = plate.getX() - 30;
    grid.setBounds(left - 6, 118, right - left + 6, 268);

    // Knobs in one row, grouped with a gap between sections.
    const int knob = 58, gap = 4, sectionGap = 18, y = 462;
    int total = 0;
    for (const auto& s : sections)
        total += s.count * (knob + gap) - gap + sectionGap;
    int x = left + (right - left - (total - sectionGap)) / 2;
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
