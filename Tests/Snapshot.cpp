#include "PluginProcessor.h"
#include "PluginEditor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    RR4Processor proc;
    proc.prepareToPlay (48000.0, 512);
    // Pegel anlegen, damit die Nadel ausschlaegt
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 200; ++b)
    {
        for (int i = 0; i < 512; ++i) { float v = 0.18f * std::sin (0.13f * (float) (b * 512 + i)); buf.setSample (0, i, v); buf.setSample (1, i, v); }
        juce::MidiBuffer m; proc.processBlock (buf, m);
    }
    for (int mode = 0; mode < 2; ++mode)
    {
        proc.apvts.getParameter ("mode")->setValueNotifyingHost ((float) mode);
        proc.apvts.getParameter ("hpf")->setValueNotifyingHost (mode ? 1.0f : 0.0f);
        proc.apvts.getParameter ("character")->setValueNotifyingHost (mode ? 0.667f : 0.0f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (380, 600);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        for (int k = 0; k < 40; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        juce::File f ("/tmp/snap" + juce::String (mode) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
    return 0;
}
