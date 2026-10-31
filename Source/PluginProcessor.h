#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PreampDSP.h"

class RR4Processor : public juce::AudioProcessor
{
public:
    RR4Processor();
    ~RR4Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RR-4 Preamp"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    float getVuIn()  const { return dsp.getVuIn();  }
    float getVuOut() const { return dsp.getVuOut(); }

private:
    PreampDSP dsp;
    std::atomic<float> *pMode, *pGain, *pTrim, *pDrive, *pPunch, *pSilk, *pWeight, *pOut,
                       *pChar, *pLoad, *pHpf, *pPad, *pPhase;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RR4Processor)
};
