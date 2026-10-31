#include "PluginEditor.h"

RR4Editor::RR4Editor (RR4Processor& p)
    : AudioProcessorEditor (&p), proc (p),
      meter (p),
      input (*p.apvts.getParameter ("mode")),
      chars (*p.apvts.getParameter ("character")),
      gainKnob ("GAIN"), driveKnob ("DRIVE"), punchKnob ("PUNCH"),
      silkKnob ("SILK"), weightKnob ("WEIGHT"), outKnob ("OUTPUT"),
      micLoadSw ("MIC LOAD", *p.apvts.getParameter ("micload")),
      hpfSw     ("HPF",      *p.apvts.getParameter ("hpf")),
      padSw     ("PAD",      *p.apvts.getParameter ("pad")),
      phaseSw   ("PHASE",    *p.apvts.getParameter ("phase"))
{
    driveKnob.bind  (*p.apvts.getParameter ("drive"));
    punchKnob.bind  (*p.apvts.getParameter ("punch"));
    silkKnob.bind   (*p.apvts.getParameter ("silk"));
    weightKnob.bind (*p.apvts.getParameter ("weight"));
    outKnob.bind    (*p.apvts.getParameter ("output"));

    for (juce::Component* c : std::initializer_list<juce::Component*> {
            &meter, &input, &chars, &gainKnob, &driveKnob, &punchKnob, &silkKnob, &weightKnob, &outKnob,
            &micLoadSw, &hpfSw, &padSw, &phaseSw })
        addAndMakeVisible (c);

    micLoadSw.onChange = [this] (bool on) { input.setMicLoad (on); };
    input.onModeChange = [this] (bool line)
    {
        gainKnob.setLabel (line ? "LINE TRIM" : "GAIN");
        gainKnob.bind (*proc.apvts.getParameter (line ? "trim" : "gain"));
        micLoadSw.setDimmed (line);
    };

    input.start();
    micLoadSw.start(); hpfSw.start(); padSw.start(); phaseSw.start();

    constrainer.setFixedAspectRatio (380.0 / 600.0);
    constrainer.setSizeLimits (285, 450, 760, 1200);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (380, 600);
}

void RR4Editor::paint (juce::Graphics& g)
{
    g.fillAll (rr::panelDk);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 380.0f));

    g.setColour (rr::panel);
    g.fillRoundedRectangle (4.0f, 4.0f, 372.0f, 592.0f, 10.0f);
    g.setColour (rr::panelDk);
    g.drawRoundedRectangle (4.0f, 4.0f, 372.0f, 592.0f, 10.0f, 2.0f);
    g.setColour (rr::rim);
    g.drawRoundedRectangle (14.0f, 14.0f, 352.0f, 572.0f, 6.0f, 0.5f);

    const float sx[] = { 26.0f, 354.0f }, sy[] = { 26.0f, 574.0f };
    for (float x : sx)
        for (float y : sy)
        {
            g.setColour (juce::Colour (0xff8e9a90));  g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
            g.setColour (rr::panelDk);                g.drawEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.0f);
            g.drawLine (x - 4.0f, y, x + 4.0f, y, 1.2f);
        }

    g.setColour (rr::cream);
    g.setFont (rr::serif (24.0f));
    g.drawText ("RR-4", juce::Rectangle<float> (40.0f, 28.0f, 120.0f, 30.0f), juce::Justification::centredLeft);
    g.setColour (rr::tickCol);
    g.setFont (rr::mono (10.0f, 0.2f));
    g.drawText ("MIC / LINE PREAMP", juce::Rectangle<float> (170.0f, 38.0f, 170.0f, 14.0f), juce::Justification::centredRight);
    g.setColour (rr::rim);
    g.drawLine (30.0f, 64.0f, 350.0f, 64.0f, 0.5f);

    g.setColour (rr::tickCol);
    g.setFont (rr::mono (10.0f, 0.2f));
    g.drawText ("CHARACTER", juce::Rectangle<float> (30.0f, 216.0f, 150.0f, 14.0f), juce::Justification::centredLeft);
}

void RR4Editor::resized()
{
    const float s = (float) getWidth() / 380.0f;
    auto B = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt();
    };

    meter.setBounds   (B (30.0f, 76.0f, 190.0f, 124.0f));
    input.setBounds   (B (232.0f, 76.0f, 120.0f, 130.0f));
    chars.setBounds   (B (30.0f, 236.0f, 326.0f, 34.0f));

    gainKnob.setBounds   (B (30.0f,  292.0f, 90.0f, 108.0f));
    driveKnob.setBounds  (B (145.0f, 292.0f, 90.0f, 108.0f));
    punchKnob.setBounds  (B (260.0f, 292.0f, 90.0f, 108.0f));
    silkKnob.setBounds   (B (30.0f,  402.0f, 90.0f, 108.0f));
    weightKnob.setBounds (B (145.0f, 402.0f, 90.0f, 108.0f));
    outKnob.setBounds    (B (260.0f, 402.0f, 90.0f, 108.0f));

    micLoadSw.setBounds (B (25.0f,  518.0f, 90.0f, 62.0f));
    hpfSw.setBounds     (B (110.0f, 518.0f, 90.0f, 62.0f));
    padSw.setBounds     (B (195.0f, 518.0f, 90.0f, 62.0f));
    phaseSw.setBounds   (B (280.0f, 518.0f, 90.0f, 62.0f));
}
