#pragma once

#include "PluginProcessor.h"

namespace rr
{
    const juce::Colour panel    (0xff3a4b3f);
    const juce::Colour panelDk  (0xff1f2a22);
    const juce::Colour rim      (0xff6f8574);
    const juce::Colour cream    (0xffefe6cf);
    const juce::Colour knobOut  (0xffe8dcc0);
    const juce::Colour knobIn   (0xffd9ccA8);
    const juce::Colour amber    (0xffe0a030);
    const juce::Colour tickCol  (0xffc9be9f);
    const juce::Colour meterBg  (0xffeadfbe);
    const juce::Colour ink      (0xff2b2b2b);
    const juce::Colour redZone  (0xffb5452f);
    const juce::Colour btnOff   (0xff2b382f);

    inline juce::Font mono (float h, float kerning = 0.12f)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain).withKerningFactor (kerning));
    }
    inline juce::Font serif (float h)
    {
        return juce::Font (juce::FontOptions ("Georgia", h, juce::Font::plain));
    }
}

//==============================================================================
class RRLook : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
        const float r  = (float) juce::jmin (w, h) * 0.5f * (26.0f / 30.0f);
        const float k  = r / 26.0f;
        g.setColour (rr::knobOut);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (rr::panelDk);
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 2.0f * k);
        g.setColour (rr::knobIn);
        g.fillEllipse (cx - 18.0f * k, cy - 18.0f * k, 36.0f * k, 36.0f * k);

        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path p;
        p.startNewSubPath (cx, cy - 4.0f * k);
        p.lineTo (cx, cy - 22.0f * k);
        p.applyTransform (juce::AffineTransform::rotation (angle, cx, cy));
        g.setColour (rr::panel);
        g.strokePath (p, juce::PathStrokeType (3.5f * k, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

//==============================================================================
class VuMeter : public juce::Component, private juce::Timer
{
public:
    explicit VuMeter (RR4Processor& p) : proc (p)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 190.0f));
        const juce::Rectangle<float> box (0.0f, 0.0f, 190.0f, 124.0f);
        g.setColour (rr::meterBg);  g.fillRoundedRectangle (box.reduced (1.0f), 6.0f);
        g.setColour (rr::panelDk);  g.drawRoundedRectangle (box.reduced (1.0f), 6.0f, 2.0f);

        const juce::Point<float> pv (95.0f, 109.0f);
        auto at = [&] (float deg, float r) {
            const float a = juce::degreesToRadians (deg);
            return juce::Point<float> (pv.x + r * std::sin (a), pv.y - r * std::cos (a));
        };

        juce::Path arc, red;
        arc.addCentredArc (pv.x, pv.y, 80.0f, 80.0f, 0.0f, juce::degreesToRadians (-50.0f), juce::degreesToRadians (50.0f), true);
        red.addCentredArc (pv.x, pv.y, 80.0f, 80.0f, 0.0f, juce::degreesToRadians (angFor (0.0f)), juce::degreesToRadians (50.0f), true);
        g.setColour (rr::ink);      g.strokePath (arc, juce::PathStrokeType (1.5f));
        g.setColour (rr::redZone);  g.strokePath (red, juce::PathStrokeType (4.0f));

        g.setFont (rr::mono (8.5f, 0.0f));
        const int marks[] = { -20, -10, -7, -5, -3, 0, 3 };
        for (int m : marks)
        {
            const float a = angFor ((float) m);
            g.setColour (rr::ink);
            g.drawLine (juce::Line<float> (at (a, 80.0f), at (a, 72.0f)), 1.5f);
            if (m == -20 || m == -10 || m == -5 || m == 0 || m == 3)
            {
                auto q = at (a, 61.0f);
                g.drawText (m > 0 ? "+" + juce::String (m) : juce::String (m),
                            juce::Rectangle<float> (q.x - 12.0f, q.y - 6.0f, 24.0f, 12.0f), juce::Justification::centred);
            }
        }

        g.setColour (rr::ink);
        g.setFont (rr::serif (13.0f));
        g.drawText ("VU", juce::Rectangle<float> (pv.x - 20.0f, 70.0f, 40.0f, 16.0f), juce::Justification::centred);
        g.setFont (rr::mono (9.0f, 0.1f));
        g.drawText (showOut ? "OUT" : "IN", juce::Rectangle<float> (8.0f, 104.0f, 40.0f, 12.0f), juce::Justification::centredLeft);

        g.saveState();
        g.reduceClipRegion (box.reduced (2.0f).toNearestInt());
        g.setColour (rr::ink);
        g.drawLine (juce::Line<float> (pv, at (needle, 90.0f)), 1.5f);
        g.fillEllipse (pv.x - 5.0f, pv.y - 5.0f, 10.0f, 10.0f);
        g.restoreState();
    }

    void mouseDown (const juce::MouseEvent&) override { showOut = ! showOut; repaint(); }

private:
    // Zeigerwinkel: proportional zur Spannung, 0 VU = -18 dBFS RMS, +3 VU = Vollausschlag rechts
    static float angFor (float vu)
    {
        const float a = std::pow (10.0f, vu / 20.0f);
        return juce::jlimit (-50.0f, 52.0f, -50.0f + (a / 1.4125f) * 100.0f);
    }

    void timerCallback() override
    {
        const float target = angFor (showOut ? proc.getVuOut() : proc.getVuIn());
        const float next = needle + 0.4f * (target - needle);
        if (std::abs (next - needle) > 0.02f) { needle = next; repaint(); }
    }

    RR4Processor& proc;
    bool showOut = true;
    float needle = -50.0f;
};

//==============================================================================
class Knob : public juce::Component
{
public:
    explicit Knob (juce::String labelText) : label (std::move (labelText))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setMouseDragSensitivity (250);
        slider.setLookAndFeel (&look);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }
    ~Knob() override { slider.setLookAndFeel (nullptr); }

    void bind (juce::RangedAudioParameter& param)
    {
        attachment.reset();
        attachment = std::make_unique<juce::SliderParameterAttachment> (param, slider);
        slider.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
        repaint();
    }

    void setLabel (const juce::String& t) { label = t; repaint(); }

    void resized() override
    {
        const float s = (float) getWidth() / 90.0f;
        slider.setBounds (juce::Rectangle<float> (15.0f * s, 10.0f * s, 60.0f * s, 60.0f * s).toNearestInt());
    }

    void paint (juce::Graphics& g) override
    {
        g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 90.0f));
        g.setColour (rr::tickCol);
        for (int j = 0; j < 9; ++j)
        {
            const float a = juce::degreesToRadians (-135.0f + 33.75f * (float) j);
            g.fillEllipse (45.0f + 35.0f * std::sin (a) - 1.6f, 40.0f - 35.0f * std::cos (a) - 1.6f, 3.2f, 3.2f);
        }
        g.setColour (rr::cream);
        g.setFont (rr::mono (11.0f, 0.12f));
        g.drawText (label, juce::Rectangle<float> (0.0f, 78.0f, 90.0f, 14.0f), juce::Justification::centred);
        g.setColour (rr::amber);
        g.setFont (rr::mono (10.0f, 0.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()), juce::Rectangle<float> (0.0f, 92.0f, 90.0f, 13.0f), juce::Justification::centred);
    }

private:
    RRLook look;
    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    juce::String label;
};

//==============================================================================
class InputSelector : public juce::Component, private juce::Timer
{
public:
    explicit InputSelector (juce::RangedAudioParameter& param)
        : attach (param, [this] (float v) { setLine (v > 0.5f); })
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void start() { attach.sendInitialUpdate(); }
    void setMicLoad (bool on) { micLoad = on; repaint(); }
    std::function<void (bool)> onModeChange;

    void paint (juce::Graphics& g) override
    {
        g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 120.0f));
        const juce::Point<float> c (60.0f, 54.0f);
        g.setColour (rr::cream);
        g.setFont (rr::mono (10.0f, 0.15f));
        g.drawText ("MIC",  juce::Rectangle<float> (14.0f, 4.0f, 32.0f, 12.0f), juce::Justification::centred);
        g.drawText ("LINE", juce::Rectangle<float> (72.0f, 4.0f, 36.0f, 12.0f), juce::Justification::centred);

        g.setColour (rr::knobOut);  g.fillEllipse (c.x - 34.0f, c.y - 34.0f, 68.0f, 68.0f);
        g.setColour (rr::panelDk);  g.drawEllipse (c.x - 34.0f, c.y - 34.0f, 68.0f, 68.0f, 2.0f);
        g.setColour (rr::knobIn);   g.fillEllipse (c.x - 24.0f, c.y - 24.0f, 48.0f, 48.0f);

        juce::Path p;
        p.startNewSubPath (c.x, c.y);
        p.lineTo (c.x, c.y - 28.0f);
        p.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (pointer), c.x, c.y));
        g.setColour (rr::panel);
        g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (rr::cream);
        g.setFont (rr::mono (11.0f, 0.15f));
        g.drawText ("INPUT", juce::Rectangle<float> (0.0f, 102.0f, 120.0f, 13.0f), juce::Justification::centred);
        g.setColour (rr::amber);
        g.setFont (rr::mono (10.0f, 0.0f));
        const juce::String txt = line ? juce::String (juce::CharPointer_UTF8 ("LINE \xc2\xb7 10 k\xce\xa9")) : (micLoad ? juce::String (juce::CharPointer_UTF8 ("MIC \xc2\xb7 1200 \xce\xa9")) : juce::String (juce::CharPointer_UTF8 ("MIC \xc2\xb7 LOAD AUS")));
        g.drawText (txt, juce::Rectangle<float> (0.0f, 116.0f, 120.0f, 13.0f), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { attach.setValueAsCompleteGesture (line ? 0.0f : 1.0f); }

private:
    void setLine (bool l)
    {
        const bool changed = (l != line);
        line = l;
        startTimerHz (60);
        if (changed || ! initialised) { initialised = true; if (onModeChange) onModeChange (line); }
        repaint();
    }

    void timerCallback() override
    {
        const float target = line ? 45.0f : -45.0f;
        pointer += 0.35f * (target - pointer);
        if (std::abs (target - pointer) < 0.2f) { pointer = target; stopTimer(); }
        repaint();
    }

    juce::ParameterAttachment attach;
    bool line = false, micLoad = true, initialised = false;
    float pointer = -45.0f;
};

//==============================================================================
class CharacterSelector : public juce::Component
{
public:
    explicit CharacterSelector (juce::RangedAudioParameter& param)
        : attach (param, [this] (float v) { index = juce::jlimit (0, 3, juce::roundToInt (v)); repaint(); })
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        attach.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 326.0f));
        const juce::String names[] = { "Trafo", "Diskret", juce::String (juce::CharPointer_UTF8 ("R\xc3\xb6hre")), "Clean" };
        g.setFont (rr::serif (13.0f));
        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> r ((float) i * 84.0f + 1.0f, 1.0f, 72.0f, 32.0f);
            const bool on = (i == index);
            g.setColour (on ? rr::amber : rr::btnOff);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? rr::panelDk : rr::rim);
            g.drawRoundedRectangle (r, 4.0f, 1.5f);
            g.setColour (on ? rr::ink : rr::cream);
            g.drawText (names[i], r, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float x = (float) e.x * 326.0f / (float) getWidth();
        const int i = juce::jlimit (0, 3, (int) (x / 84.0f));
        attach.setValueAsCompleteGesture ((float) i);
    }

private:
    juce::ParameterAttachment attach;
    int index = 0;
};

//==============================================================================
class LeverSwitch : public juce::Component
{
public:
    LeverSwitch (juce::String labelText, juce::RangedAudioParameter& param)
        : label (std::move (labelText)),
          attach (param, [this] (float v) { on = v > 0.5f; repaint(); if (onChange) onChange (on); })
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void start() { attach.sendInitialUpdate(); }
    void setDimmed (bool d) { dimmed = d; repaint(); }
    std::function<void (bool)> onChange;

    void paint (juce::Graphics& g) override
    {
        g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 90.0f));
        if (dimmed) g.beginTransparencyLayer (0.35f);
        g.setColour (rr::panelDk);  g.fillRoundedRectangle (35.0f, 0.0f, 20.0f, 36.0f, 5.0f);
        g.setColour (rr::rim);      g.drawRoundedRectangle (35.0f, 0.0f, 20.0f, 36.0f, 5.0f, 0.5f);
        const float cy = on ? 10.0f : 26.0f;
        g.setColour (rr::knobOut);  g.fillEllipse (38.0f, cy - 7.0f, 14.0f, 14.0f);
        g.setColour (rr::panelDk);  g.drawEllipse (38.0f, cy - 7.0f, 14.0f, 14.0f, 1.5f);
        g.setColour (rr::cream);
        g.setFont (rr::mono (10.0f, 0.12f));
        g.drawText (label, juce::Rectangle<float> (0.0f, 44.0f, 90.0f, 13.0f), juce::Justification::centred);
        if (dimmed) g.endTransparencyLayer();
    }

    void mouseDown (const juce::MouseEvent&) override { attach.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

private:
    juce::String label;
    juce::ParameterAttachment attach;
    bool on = false, dimmed = false;
};

//==============================================================================
class RR4Editor : public juce::AudioProcessorEditor
{
public:
    explicit RR4Editor (RR4Processor&);
    ~RR4Editor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RR4Processor& proc;
    juce::ComponentBoundsConstrainer constrainer;

    VuMeter meter;
    InputSelector input;
    CharacterSelector chars;
    Knob gainKnob, driveKnob, punchKnob, silkKnob, weightKnob, outKnob;
    LeverSwitch micLoadSw, hpfSw, padSw, phaseSw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RR4Editor)
};
