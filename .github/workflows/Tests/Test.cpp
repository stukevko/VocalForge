// Offline-Test: synthetisches "Gesangs"-Signal durch VocalForge schicken und prüfen
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <random>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

// Stimm-ähnlich: Sägezahn mit Vibrato, tiefpassgefiltert; Abschnitte laut/leise/Pause; S-Laute als HP-Rauschen
static juce::AudioBuffer<float> makeVocal (double fs, double seconds, std::vector<std::pair<int,int>>& sibil)
{
    const int n = (int) (fs * seconds);
    juce::AudioBuffer<float> b (2, n);
    std::mt19937 rng (1);
    std::normal_distribution<float> nd (0.f, 1.f);
    vf::Biquad lp, hp, f1, f2;
    lp.setLowPass (fs, 3000, 0.7); hp.setHighPass (fs, 5500, 0.9);
    f1.setPeak (fs, 700, 3, 9); f2.setPeak (fs, 2600, 4, 7);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / fs;
        const double sec = std::fmod (t, 5.0);
        float level = sec < 2.0 ? 0.35f : (sec < 4.0 ? 0.035f : 0.0f);            // laut / leise (-20 dB) / Pause
        const double f0 = 210 + 6 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t);
        ph += f0 / fs; if (ph >= 1) ph -= 1;
        float v = (float) (2 * ph - 1);
        v = f2.process (f1.process (lp.process (v, 0), 0), 0) * level;
        const bool isS = sec < 4.0 && std::fmod (sec, 0.5) < 0.08;
        if (isS) v = v * 0.3f + hp.process (nd (rng), 0) * level * 1.2f;
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    // Sibilanz-Fenster (Samples) merken
    for (double s = 0; s < seconds; s += 0.5)
        if (std::fmod (s, 5.0) < 4.0) sibil.push_back ({ (int) (s * fs), (int) ((s + 0.08) * fs) });
    return b;
}

static float rmsDb (const juce::AudioBuffer<float>& b, int s, int e)
{
    double acc = 0; for (int i = s; i < e; ++i) acc += (double) b.getSample (0, i) * b.getSample (0, i);
    return vf::powToDb ((float) (acc / std::max (1, e - s)));
}

static void run (VocalForgeProcessor& p, juce::AudioBuffer<float>& buf, int block, bool& finite, float& peak)
{
    juce::MidiBuffer midi;
    finite = true; peak = 0;
    for (int s = 0; s < buf.getNumSamples(); s += block)
    {
        const int len = std::min (block, buf.getNumSamples() - s);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), buf.getNumChannels(), s, len);
        p.processBlock (view, midi);
    }
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const float v = buf.getSample (ch, i);
            if (! std::isfinite (v)) finite = false;
            peak = std::max (peak, std::abs (v));
        }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    for (double fs : { 44100.0, 48000.0, 96000.0 })
    for (int block : { 64, 512, 1024 })
    {
        std::printf ("== %.0f Hz, Block %d ==\n", fs, block);
        VocalForgeProcessor p;
        p.setPlayConfigDetails (2, 2, fs, block);
        p.prepareToPlay (fs, block);
        std::vector<std::pair<int,int>> sib;
        auto in = makeVocal (fs, 15.0, sib);
        auto out = in;
        bool finite; float peak;
        run (p, out, block, finite, peak);
        CHECK (finite, "NaN/Inf im Ausgang");
        CHECK (peak <= vf::dbToGain (-1.0f) + 1e-3f, "Limiter-Ceiling überschritten: %.2f dB", vf::gainToDb (peak));

        // Letzter Zyklus (10..15 s): laut vs. leise Abschnitt
        auto seg = [&] (const juce::AudioBuffer<float>& b, double a, double e) { return rmsDb (b, (int) (a * fs), (int) (e * fs)); };
        const float inDiff  = seg (in, 10.6, 11.9) - seg (in, 12.6, 13.9);
        const float outDiff = seg (out, 10.6, 11.9) - seg (out, 12.6, 13.9);
        std::printf ("  Lautheits-Unterschied laut/leise: In %.1f dB -> Out %.1f dB\n", inDiff, outDiff);
        CHECK (outDiff < inDiff * 0.5f, "Auto Level/Comp gleicht zu wenig aus");

        // Sibilanz: Verhältnis S-Laut zu Vokal sollte sinken
        auto sRatio = [&] (const juce::AudioBuffer<float>& b)
        {
            vf::Biquad h; h.setHighPass (fs, 5000, 0.7);
            double sAcc = 0, vAcc = 0; int sN = 0, vN = 0;
            const int st = (int) (10 * fs), en = (int) (12 * fs);
            for (int i = st; i < en; ++i)
            {
                const float y = h.process (b.getSample (0, i), 0);
                bool isS = false;
                for (auto& w : sib) if (i >= w.first && i < w.second) { isS = true; break; }
                (isS ? sAcc : vAcc) += (double) y * y; (isS ? sN : vN)++;
            }
            return vf::powToDb ((float) (sAcc / sN)) - rmsDb (b, st, en);
        };
        const float sIn = sRatio (in), sOut = sRatio (out);
        std::printf ("  Sibilanz (relativ): In %.1f dB -> Out %.1f dB, De-Ess GR zuletzt %.1f dB\n", sIn, sOut, p.deEssGR.load());
        CHECK (sOut < sIn - 2.0f, "De-Esser reduziert Zischlaute nicht genug");

        std::printf ("  Auto-EQ Bänder:");
        for (auto& g : p.eqGains) std::printf (" %+.1f", g.load());
        std::printf ("\n");
    }

    // Presets, Mono, State-Speicherung, Bypass-Extreme
    {
        std::printf ("== Presets / Mono / State ==\n");
        for (int i = 0; i < 8; ++i)
        {
            VocalForgeProcessor p;
            p.setPlayConfigDetails (2, 2, 48000, 256);
            p.setCurrentProgram (i);
            p.prepareToPlay (48000, 256);
            std::vector<std::pair<int,int>> sib;
            auto b = makeVocal (48000, 6, sib);
            bool finite; float peak; run (p, b, 256, finite, peak);
            std::printf ("  Preset %d '%s': ok=%d peak=%.2f dB\n", i, p.getProgramName (i).toRawUTF8(), finite, vf::gainToDb (peak));
            CHECK (finite && peak < 1.0f, "Preset %d defekt", i);
        }
        // Extremwerte
        VocalForgeProcessor p;
        p.setPlayConfigDetails (1, 1, 48000, 128);
        for (auto* prm : p.getParameters()) prm->setValueNotifyingHost (1.0f);
        p.prepareToPlay (48000, 128);
        std::vector<std::pair<int,int>> sib;
        auto full = makeVocal (48000, 6, sib);
        juce::AudioBuffer<float> mono (1, full.getNumSamples());
        mono.copyFrom (0, 0, full, 0, 0, full.getNumSamples());
        bool finite; float peak; run (p, mono, 128, finite, peak);
        std::printf ("  Mono + alle Regler auf Maximum: ok=%d peak=%.2f dB\n", finite, vf::gainToDb (peak));
        CHECK (finite && peak <= 1.001f, "Extremwerte instabil");

        // State speichern / laden
        VocalForgeProcessor a, c;
        a.apvts.getParameter ("revMix")->setValueNotifyingHost (0.77f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        c.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (c.apvts.getParameter ("revMix")->getValue() - 0.77f) < 1e-4f, "State-Restore fehlerhaft");

        // Editor erzeugen (GUI-Smoke-Test)
        std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor());
        CHECK (ed != nullptr && ed->getWidth() > 0, "Editor fehlerhaft");
        std::printf ("  Editor %dx%d\n", ed->getWidth(), ed->getHeight());
        if (auto* env = std::getenv ("VF_SNAPSHOT"))
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (env); f.deleteFile();
            juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        }
    }

    std::printf (failures ? "\n%d FEHLER\n" : "\nALLE TESTS OK\n", failures);
    return failures ? 1 : 0;
}
