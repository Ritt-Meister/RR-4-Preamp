#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;

static juce::String dbText (float v, int) { return juce::String (v, 1) + " dB"; }
static juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

juce::AudioProcessorValueTreeState::ParameterLayout RR4Processor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto db  = [] { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText); };
    auto pct = [] { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText); };

    p.push_back (std::make_unique<APC> (juce::ParameterID { "mode", 1 }, "Eingang", juce::StringArray { "Mic", "Line" }, 0));
    p.push_back (std::make_unique<APF> (juce::ParameterID { "gain", 1 }, "Gain", juce::NormalisableRange<float> (0.0f, 36.0f, 0.1f), 12.0f, db()));
    p.push_back (std::make_unique<APF> (juce::ParameterID { "trim", 1 }, "Line Trim", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db()));
    p.push_back (std::make_unique<APF> (juce::ParameterID { "drive", 1 }, "Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f, pct()));
    p.push_back (std::make_unique<APF> (juce::ParameterID { "punch", 1 }, "Punch", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct()));
    p.push_back (std::make_unique<APF> (juce::ParameterID { "silk", 1 }, "Silk", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct()));
    p.push_back (std::make_unique<APF> (juce::ParameterID { "weight", 1 }, "Weight", juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, db()));
    p.push_back (std::make_unique<APF> (juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db()));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "character", 1 }, "Charakter", juce::StringArray { "Trafo", "Diskret", juce::String (juce::CharPointer_UTF8 ("R\xc3\xb6hre")), "Clean" }, 0));
    p.push_back (std::make_unique<APB> (juce::ParameterID { "micload", 1 }, "Mic Load", true));
    p.push_back (std::make_unique<APB> (juce::ParameterID { "hpf", 1 }, "Hochpass", false));
    p.push_back (std::make_unique<APB> (juce::ParameterID { "pad", 1 }, "Pad", false));
    p.push_back (std::make_unique<APB> (juce::ParameterID { "phase", 1 }, "Phase", false));

    return { p.begin(), p.end() };
}

RR4Processor::RR4Processor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RR4", createLayout())
{
    pMode   = apvts.getRawParameterValue ("mode");
    pGain   = apvts.getRawParameterValue ("gain");
    pTrim   = apvts.getRawParameterValue ("trim");
    pDrive  = apvts.getRawParameterValue ("drive");
    pPunch  = apvts.getRawParameterValue ("punch");
    pSilk   = apvts.getRawParameterValue ("silk");
    pWeight = apvts.getRawParameterValue ("weight");
    pOut    = apvts.getRawParameterValue ("output");
    pChar   = apvts.getRawParameterValue ("character");
    pLoad   = apvts.getRawParameterValue ("micload");
    pHpf    = apvts.getRawParameterValue ("hpf");
    pPad    = apvts.getRawParameterValue ("pad");
    pPhase  = apvts.getRawParameterValue ("phase");
}

bool RR4Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == l.getMainInputChannelSet();
}

void RR4Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumInputChannels()));
    setLatencySamples (dsp.getLatencySamples());
}

void RR4Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    PreampParams p;
    p.line      = pMode->load()  > 0.5f;
    p.gainDb    = pGain->load();
    p.trimDb    = pTrim->load();
    p.drive     = pDrive->load();
    p.punch     = pPunch->load();
    p.silk      = pSilk->load();
    p.weightDb  = pWeight->load();
    p.outDb     = pOut->load();
    p.character = juce::jlimit (0, 3, (int) std::lround (pChar->load()));
    p.micLoad   = pLoad->load()  > 0.5f;
    p.hpf       = pHpf->load()   > 0.5f;
    p.pad       = pPad->load()   > 0.5f;
    p.phase     = pPhase->load() > 0.5f;

    dsp.process (buffer, p);
}

juce::AudioProcessorEditor* RR4Processor::createEditor() { return new RR4Editor (*this); }

void RR4Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RR4Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RR4Processor(); }
