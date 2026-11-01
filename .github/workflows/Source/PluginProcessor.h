#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"

class VocalForgeProcessor : public juce::AudioProcessor
{
public:
    VocalForgeProcessor();
    ~VocalForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VocalForge"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    // Werks-Presets (Vocal-Stile)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Meter-Werte für die Oberfläche (thread-sicher)
    std::atomic<float> meterIn { -100.f }, meterOut { -100.f }, riderGain { 0.f },
                       deEssGR { 0.f }, compGR { 0.f };
    std::array<std::atomic<float>, vf::AutoEQ::N> eqGains {};

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  on (const char* id) const { return p (id) > 0.5f; }

    vf::Rider rider;
    vf::AutoEQ autoEq;
    vf::DeEsser deEsser;
    vf::Compressor comp;
    vf::Ducker ducker;
    vf::StereoDelay delay;
    vf::VocalReverb reverb;
    vf::Limiter limiter;
    vf::Biquad lowCut;
    float lastLowCut = -1.f;

    juce::SmoothedValue<float> inGain, outGain;
    std::vector<float> sL, sR, dL, dR, rL, rR, act;
    double fs = 44100.0;
    double bpm = 120.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalForgeProcessor)
};
