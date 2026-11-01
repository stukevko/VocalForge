#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using NR  = juce::NormalisableRange<float>;

static const juce::StringArray kDelayDivs { "1/16", "1/8", "1/8 punktiert", "1/4", "1/4 punktiert", "1/2" };
static constexpr float kDelayBeats[] { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

// -----------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout VocalForgeProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, NR (0.f, 100.f, 0.1f), def,
                                      juce::AudioParameterFloatAttributes().withLabel ("%"));
    };
    auto db = [] (const char* id, const char* name, float lo, float hi, float def)
    {
        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, NR (lo, hi, 0.1f), def,
                                      juce::AudioParameterFloatAttributes().withLabel ("dB"));
    };
    auto sw = [] (const char* id, const char* name, bool def)
    {
        return std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def);
    };

    // Input
    v.push_back (db ("inGain", "Input Gain", -24.f, 24.f, 0.f));
    v.push_back (std::make_unique<APF> (juce::ParameterID { "lowCut", 1 }, "Low Cut", NR (20.f, 300.f, 1.f, 0.5f), 80.f,
                                        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    // Auto Rider
    v.push_back (sw ("riderOn", "Auto Level On", true));
    v.push_back (db ("riderTarget", "Auto Level Target", -36.f, -10.f, -20.f));
    v.push_back (db ("riderRange", "Auto Level Range", 0.f, 18.f, 9.f));
    // Auto EQ
    v.push_back (sw ("eqOn", "Auto EQ On", true));
    v.push_back (pct ("eqAmount", "Auto EQ Amount", 60.f));
    v.push_back (std::make_unique<APF> (juce::ParameterID { "eqTone", 1 }, "Tone (Dark/Bright)", NR (-1.f, 1.f, 0.01f), 0.f));
    v.push_back (db ("eqBody", "Body", -6.f, 6.f, 0.f));
    v.push_back (db ("eqPresence", "Presence", -6.f, 6.f, 0.f));
    v.push_back (db ("eqAir", "Air", -6.f, 8.f, 1.5f));
    // De-Esser
    v.push_back (sw ("dsOn", "De-Esser On", true));
    v.push_back (pct ("dsAmount", "De-Esser Amount", 50.f));
    v.push_back (std::make_unique<APF> (juce::ParameterID { "dsFreq", 1 }, "De-Esser Freq", NR (3000.f, 10000.f, 10.f, 0.6f), 5500.f,
                                        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    // Compressor
    v.push_back (sw ("compOn", "Auto Comp On", true));
    v.push_back (pct ("compAmount", "Comp Amount", 50.f));
    v.push_back (pct ("compSpeed", "Comp Speed", 50.f));
    v.push_back (pct ("compMix", "Comp Mix", 100.f));
    // Saturation
    v.push_back (sw ("satOn", "Saturation On", true));
    v.push_back (pct ("satDrive", "Saturation Drive", 20.f));
    // Delay
    v.push_back (sw ("dlyOn", "Delay On", true));
    v.push_back (std::make_unique<APC> (juce::ParameterID { "dlyTime", 1 }, "Delay Time", kDelayDivs, 1));
    v.push_back (pct ("dlyFeedback", "Delay Feedback", 30.f));
    v.push_back (pct ("dlyMix", "Delay Mix", 12.f));
    v.push_back (pct ("dlyDuck", "Delay Ducking", 70.f));
    // Reverb
    v.push_back (sw ("revOn", "Reverb On", true));
    v.push_back (pct ("revSize", "Reverb Size", 55.f));
    v.push_back (std::make_unique<APF> (juce::ParameterID { "revPre", 1 }, "Reverb Pre-Delay", NR (0.f, 200.f, 1.f), 40.f,
                                        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    v.push_back (pct ("revMix", "Reverb Mix", 15.f));
    v.push_back (pct ("revDuck", "Reverb Ducking", 50.f));
    // Output
    v.push_back (db ("outGain", "Output Gain", -24.f, 12.f, 0.f));
    v.push_back (sw ("limOn", "Limiter On", true));
    v.push_back (db ("limCeiling", "Limiter Ceiling", -12.f, 0.f, -1.f));
    v.push_back (pct ("dryWet", "Dry/Wet", 100.f));

    return { v.begin(), v.end() };
}

// -----------------------------------------------------------------------------
//  Werks-Presets: nur die Abweichungen vom Default werden gesetzt
// -----------------------------------------------------------------------------
struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

static const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Default – Allround Lead", {} },
        { "Pop Lead (hell & präsent)", { { "eqTone", 0.35f }, { "eqAir", 3.f }, { "eqPresence", 1.5f }, { "compAmount", 60.f },
                                         { "dlyMix", 14.f }, { "revMix", 18.f }, { "revSize", 60.f } } },
        { "Rap / Hip-Hop (trocken & punchy)", { { "lowCut", 100.f }, { "compAmount", 70.f }, { "compSpeed", 75.f }, { "satDrive", 30.f },
                                                { "eqPresence", 2.f }, { "dlyTime", 0.f }, { "dlyMix", 6.f }, { "revMix", 6.f },
                                                { "revSize", 30.f }, { "riderRange", 12.f } } },
        { "R&B (warm & smooth)", { { "eqTone", -0.2f }, { "eqBody", 1.5f }, { "eqAir", 2.f }, { "compAmount", 55.f }, { "compSpeed", 35.f },
                                   { "compMix", 80.f }, { "dlyTime", 3.f }, { "dlyMix", 16.f }, { "revMix", 22.f }, { "revSize", 75.f } } },
        { "Rock (dicht & aggressiv)", { { "compAmount", 75.f }, { "compSpeed", 70.f }, { "satDrive", 45.f }, { "eqPresence", 3.f },
                                        { "dlyTime", 3.f }, { "dlyMix", 10.f }, { "revMix", 12.f }, { "revSize", 45.f } } },
        { "Ballade (groß & räumlich)", { { "eqAir", 3.f }, { "compAmount", 45.f }, { "compSpeed", 30.f }, { "dlyTime", 4.f },
                                         { "dlyFeedback", 40.f }, { "dlyMix", 18.f }, { "revMix", 28.f }, { "revSize", 85.f }, { "revPre", 70.f } } },
        { "Podcast / Sprache", { { "lowCut", 90.f }, { "eqAir", 0.5f }, { "compAmount", 65.f }, { "satDrive", 5.f },
                                 { "dlyOn", 0.f }, { "revOn", 0.f }, { "riderTarget", -18.f }, { "riderRange", 14.f } } },
        { "Backing Vocals (weich & hinten)", { { "lowCut", 150.f }, { "eqTone", -0.3f }, { "eqPresence", -2.f }, { "dsAmount", 70.f },
                                               { "compAmount", 70.f }, { "dlyMix", 8.f }, { "revMix", 30.f }, { "revSize", 70.f } } },
    };
    return list;
}

int VocalForgeProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String VocalForgeProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? juce::String::fromUTF8 (presets()[(size_t) i].name) : "";
}

void VocalForgeProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;

    for (auto* param : getParameters())   // erst alles auf Default …
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, value] : presets()[(size_t) index].values)   // … dann Preset-Werte
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

// -----------------------------------------------------------------------------
VocalForgeProcessor::VocalForgeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool VocalForgeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void VocalForgeProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    rider.prepare (fs); autoEq.prepare (fs); deEsser.prepare (fs); comp.prepare (fs);
    ducker.prepare (fs); delay.prepare (fs); reverb.prepare (fs); limiter.prepare (fs);
    lowCut.reset(); lastLowCut = -1.f;

    for (auto* b : { &sL, &sR, &dL, &dR, &rL, &rR, &act })
        b->assign (vf::kMaxChunk, 0.0f);

    inGain.reset (fs, 0.05);  inGain.setCurrentAndTargetValue (vf::dbToGain (p ("inGain")));
    outGain.reset (fs, 0.05); outGain.setCurrentAndTargetValue (vf::dbToGain (p ("outGain")));
}

void VocalForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (numOut == 0 || n == 0) return;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = std::clamp (*b, 30.0, 300.0);

    // Parameter einmal pro Block lesen
    inGain.setTargetValue (vf::dbToGain (p ("inGain")));
    outGain.setTargetValue (vf::dbToGain (p ("outGain")));
    const float lc = p ("lowCut");
    if (! juce::exactlyEqual (lc, lastLowCut)) { lowCut.setHighPass (fs, lc, 0.707); lastLowCut = lc; }

    const bool riderOn = on ("riderOn"), eqOn = on ("eqOn"), dsOn = on ("dsOn"), compOn = on ("compOn"),
               satOn = on ("satOn"), dlyOn = on ("dlyOn"), revOn = on ("revOn"), limOn = on ("limOn");
    const float rTarget = p ("riderTarget"), rRange = p ("riderRange");
    const float eqAmt = p ("eqAmount") * 0.01f, eqTone = p ("eqTone"), eqBody = p ("eqBody"),
                eqPres = p ("eqPresence"), eqAir = p ("eqAir");
    const float dsAmt = p ("dsAmount") * 0.01f, dsFreq = p ("dsFreq");
    const float cAmt = p ("compAmount") * 0.01f, cSpd = p ("compSpeed") * 0.01f, cMix = p ("compMix") * 0.01f;
    const float drive = p ("satDrive") * 0.01f;
    const int   div = std::clamp ((int) p ("dlyTime"), 0, 5);
    const float dlySamples = (float) (kDelayBeats[div] * 60.0 / bpm * fs);
    const float dFb = p ("dlyFeedback") * 0.0085f, dMix = p ("dlyMix") * 0.01f, dDuck = p ("dlyDuck") * 0.01f;
    const float rSize = p ("revSize") * 0.01f, rPre = p ("revPre"), rMix = p ("revMix") * 0.01f, rDuck = p ("revDuck") * 0.01f;
    const float ceil = p ("limCeiling"), dryWet = p ("dryWet") * 0.01f;

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;

    float peakIn = 0.f, peakOut = 0.f, maxDs = 0.f, maxComp = 0.f;

    for (int start = 0; start < n; start += vf::kMaxChunk)
    {
        const int len = std::min (vf::kMaxChunk, n - start);
        float* L = sL.data(); float* R = sR.data();

        for (int i = 0; i < len; ++i)
        {
            const float g = inGain.getNextValue();
            L[i] = inL[start + i] * g;
            R[i] = inR[start + i] * g;
            peakIn = std::max ({ peakIn, std::abs (L[i]), std::abs (R[i]) });
            L[i] = lowCut.process (L[i], 0);
            R[i] = lowCut.process (R[i], 1);
        }

        // ---- Kette: Rider → Auto-EQ → De-Esser → Comp → Sättigung → De-Esser-Nachlauf → FX ----
        rider.process (L, R, len, rTarget, rRange, riderOn);
        autoEq.process (L, R, len, eqAmt, eqTone, eqBody, eqPres, eqAir, eqOn);
        maxDs = std::max (maxDs, deEsser.process (L, R, len, dsAmt, dsFreq, dsOn));
        maxComp = std::max (maxComp, comp.process (L, R, len, cAmt, cSpd, cMix, compOn));
        vf::Saturator::process (L, R, len, drive, satOn);

        ducker.process (L, R, len, act.data());
        delay.process  (L, R, dL.data(), dR.data(), act.data(), len, dlySamples, dFb, dMix, dDuck, dlyOn);
        reverb.process (L, R, rL.data(), rR.data(), act.data(), len, rSize, rPre, rMix, rDuck, revOn);

        for (int i = 0; i < len; ++i)
        {
            const float g = outGain.getNextValue();
            L[i] = (L[i] + dL[(size_t) i] + rL[(size_t) i]) * g;
            R[i] = (R[i] + dR[(size_t) i] + rR[(size_t) i]) * g;
        }
        limiter.process (L, R, len, ceil, limOn);

        for (int i = 0; i < len; ++i)
        {
            const float dryL = inL[start + i], dryR = inR[start + i];
            const float oL = dryL + (L[i] - dryL) * dryWet;
            const float oR = dryR + (R[i] - dryR) * dryWet;
            if (outR) { outL[start + i] = oL; outR[start + i] = oR; }
            else      { outL[start + i] = 0.5f * (oL + oR); }
            peakOut = std::max ({ peakOut, std::abs (oL), std::abs (oR) });
        }
    }

    meterIn.store (vf::gainToDb (peakIn));
    meterOut.store (vf::gainToDb (peakOut));
    riderGain.store (rider.getGainDb());
    deEssGR.store (maxDs);
    compGR.store (maxComp);
    for (int b = 0; b < vf::AutoEQ::N; ++b) eqGains[(size_t) b].store (autoEq.getGain (b));
}

// -----------------------------------------------------------------------------
void VocalForgeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void VocalForgeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* VocalForgeProcessor::createEditor() { return new VocalForgeEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalForgeProcessor(); }
