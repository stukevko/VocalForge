#pragma once
// =============================================================================
//  VocalForge – DSP-Bausteine
//  Alle "Auto"-Module analysieren das Signal laufend und passen sich an.
//  Die Regler steuern nur, WIE STARK bzw. in welche RICHTUNG sie arbeiten.
// =============================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vf
{
constexpr int kMaxChunk = 256;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-7f)); }
inline float powToDb (float p) noexcept   { return 10.0f * std::log10 (std::max (p, 1.0e-14f)); }
inline float coef (float ms, double fs) noexcept
{
    return (float) std::exp (-1.0 / (std::max (0.001, (double) ms) * 0.001 * fs));
}
inline float mixf (float a, float b, float t) noexcept { return a + (b - a) * t; }

// -----------------------------------------------------------------------------
//  Biquad (RBJ-Cookbook), Zustand für bis zu 2 Kanäle, keine Allokationen
// -----------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1[2] {}, s2[2] {};

    void reset() noexcept { s1[0] = s1[1] = s2[0] = s2[1] = 0.0; }

    inline float process (float x, int ch) noexcept
    {
        const double y = b0 * x + s1[ch];
        s1[ch] = b1 * x - a1 * y + s2[ch];
        s2[ch] = b2 * x - a2 * y;
        return (float) y;
    }

    void setPeak (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLowShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2.0 * q), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void setHighShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2.0 * q), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
    void setHighPass (double fs, double f, double q) noexcept
    {
        const double w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setLowPass (double fs, double f, double q) noexcept
    {
        const double w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void setBandPass (double fs, double f, double q) noexcept
    {
        const double w = w0 (fs, f), c = std::cos (w), al = std::sin (w) / (2.0 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }

private:
    static double w0 (double fs, double f) noexcept
    {
        return 2.0 * juce::MathConstants<double>::pi * std::clamp (f, 10.0, fs * 0.45) / fs;
    }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept
    {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
};

// -----------------------------------------------------------------------------
//  VoiceGate: erkennt, ob gerade "echte" Stimme anliegt (vs. Pausen/Rauschen),
//  damit die Auto-Analysen nicht von Stille verfälscht werden.
// -----------------------------------------------------------------------------
struct VoiceGate
{
    float ref = -60.0f, decay = 0.0f;
    void prepare (double rate) { ref = -60.0f; decay = (float) (6.0 / rate); } // 6 dB/s
    bool update (float lvlDb, float window) noexcept
    {
        ref = std::max ({ lvlDb, ref - decay, -70.0f });
        return lvlDb > -60.0f && lvlDb > ref - window;
    }
};

// -----------------------------------------------------------------------------
//  Vocal Rider – fährt die Lautstärke automatisch auf einen Ziel-Pegel
// -----------------------------------------------------------------------------
class Rider
{
public:
    void prepare (double sr)
    {
        fs = sr; ms = 0; gainDb = desired = 0; counter = 0;
        gate.prepare (sr / 16.0);
        aLvl = coef (300.0f, fs); aGain = coef (600.0f, fs);
    }

    void process (float* L, float* R, int n, float targetDb, float rangeDb, bool on) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            ms = aLvl * ms + (1.0f - aLvl) * m * m;

            if (++counter >= 16)
            {
                counter = 0;
                const float lvl = powToDb (ms);
                if (! on)                       desired = 0.0f;
                else if (gate.update (lvl, 15)) desired = std::clamp (targetDb - lvl, -rangeDb, rangeDb);
                // sonst: in Pausen Gain halten (kein Hochziehen von Atmern/Rauschen)
            }
            gainDb = aGain * gainDb + (1.0f - aGain) * desired;
            const float g = dbToGain (gainDb);
            L[i] *= g; R[i] *= g;
        }
    }
    float getGainDb() const noexcept { return gainDb; }

private:
    double fs = 44100.0;
    float ms = 0, gainDb = 0, desired = 0, aLvl = 0, aGain = 0;
    int counter = 0;
    VoiceGate gate;
};

// -----------------------------------------------------------------------------
//  Auto EQ – misst die Klangbalance in 6 Bändern und gleicht sie an eine
//  Ziel-Kurve für "fertig gemischte" Vocals an (+ Tone-Tilt + manuelle Trims)
// -----------------------------------------------------------------------------
class AutoEQ
{
public:
    static constexpr int N = 6;
    static constexpr float freqs[N] { 150.f, 350.f, 800.f, 2500.f, 5000.f, 11000.f };

    void prepare (double sr)
    {
        fs = sr; counter = 0; inMs = 0; voicedTime = 0; voiced = false;
        gate.prepare (sr / 64.0);
        for (int b = 0; b < N; ++b)
        {
            ana[b].setBandPass (fs, freqs[b], 1.2); ana[b].reset();
            eq[b].reset(); bandMs[b] = 1.0e-6f; gains[b] = 0.0f;
        }
        aBand = coef (2500.0f, fs); aIn = coef (50.0f, fs); aGain = coef (900.0f, fs / 64.0);
        updateFilters();
    }

    void process (float* L, float* R, int n, float amount, float tone, float body, float presence, float air, bool on) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            inMs = aIn * inMs + (1.0f - aIn) * m * m;

            for (int b = 0; b < N; ++b)
            {
                const float y = ana[b].process (m, 0);
                if (voiced) bandMs[b] = aBand * bandMs[b] + (1.0f - aBand) * y * y;
            }
            for (int b = 0; b < N; ++b)
            {
                L[i] = eq[b].process (L[i], 0);
                R[i] = eq[b].process (R[i], 1);
            }
            if (++counter >= 64) { counter = 0; updateBlock (amount, tone, body, presence, air, on); }
        }
    }
    float getGain (int b) const noexcept { return gains[b]; }

private:
    void updateBlock (float amount, float tone, float body, float presence, float air, bool on) noexcept
    {
        // Ziel-Balance (relativ, in dB) einer typischen, produzierten Lead-Vocal
        static constexpr float target[N] { -3.f, 0.f, -4.f, -9.f, -13.f, -17.f };
        static constexpr float tilt[N]   { -1.f, -0.6f, -0.2f, 0.3f, 0.6f, 1.f };
        constexpr float tMean = (-3.f + 0.f - 4.f - 9.f - 13.f - 17.f) / N;

        voiced = gate.update (powToDb (inMs), 20.0f);
        if (voiced) voicedTime += 64.0f / (float) fs;

        float rel[N], mean = 0.0f;
        for (int b = 0; b < N; ++b) { rel[b] = powToDb (bandMs[b]); mean += rel[b]; }
        mean /= N;

        const float confidence = std::clamp (voicedTime / 1.5f, 0.0f, 1.0f);

        for (int b = 0; b < N; ++b)
        {
            float t = 0.0f;
            if (on)
            {
                const float want = target[b] - tMean + tone * tilt[b] * 6.0f;
                t = std::clamp (want - (rel[b] - mean), -9.0f, 6.0f) * amount * confidence;
                if (b == 0) t += body;
                if (b == 3) t += presence;
                if (b == 5) t += air;
            }
            gains[b] = aGain * gains[b] + (1.0f - aGain) * t;
        }
        updateFilters();
    }

    void updateFilters() noexcept
    {
        for (int b = 0; b < N - 1; ++b)
            eq[b].setPeak (fs, freqs[b], b == 0 ? 0.7 : 1.0, gains[b]);
        eq[N - 1].setHighShelf (fs, 10000.0, 0.7, gains[N - 1]);
    }

    double fs = 44100.0;
    Biquad ana[N], eq[N];
    float bandMs[N] {}, gains[N] {};
    float inMs = 0, aBand = 0, aIn = 0, aGain = 0, voicedTime = 0;
    bool voiced = false;
    int counter = 0;
    VoiceGate gate;
};

// -----------------------------------------------------------------------------
//  Auto De-Esser – vergleicht Zischanteil mit Gesamtpegel (pegelunabhängig)
//  und senkt nur das Band oberhalb der Frequenz ab (Split-Band)
// -----------------------------------------------------------------------------
class DeEsser
{
public:
    void prepare (double sr)
    {
        fs = sr; det.reset(); split.reset(); envS = envF = gr = 0; lastFreq = -1;
        aAtt = coef (0.5f, fs); aRel = coef (60.0f, fs); aGrA = coef (1.0f, fs); aGrR = coef (50.0f, fs);
    }

    float process (float* L, float* R, int n, float amount, float freq, bool on) noexcept
    {
        if (! on) { gr = 0; return 0.0f; }
        if (! juce::exactlyEqual (freq, lastFreq))
        {
            det.setHighPass (fs, freq, 0.707); split.setHighPass (fs, freq, 0.707); lastFreq = freq;
        }
        const float thr = mixf (-4.0f, -22.0f, amount);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            const float s = std::abs (det.process (m, 0)), f = std::abs (m);
            envS = s > envS ? aAtt * envS + (1 - aAtt) * s : aRel * envS + (1 - aRel) * s;
            envF = f > envF ? aAtt * envF + (1 - aAtt) * f : aRel * envF + (1 - aRel) * f;

            float target = 0.0f;
            if (envF > 1.0e-3f)
            {
                const float over = gainToDb (envS) - gainToDb (envF) - thr;
                if (over > 0.0f) target = std::min (over * 0.75f, 14.0f);
            }
            gr = target > gr ? aGrA * gr + (1 - aGrA) * target : aGrR * gr + (1 - aGrR) * target;

            const float k = 1.0f - dbToGain (-gr);
            L[i] -= split.process (L[i], 0) * k;
            R[i] -= split.process (R[i], 1) * k;
            maxGr = std::max (maxGr, gr);
        }
        return maxGr;
    }

private:
    double fs = 44100.0;
    Biquad det, split;
    float envS = 0, envF = 0, gr = 0, lastFreq = -1, aAtt = 0, aRel = 0, aGrA = 0, aGrR = 0;
};

// -----------------------------------------------------------------------------
//  Auto Compressor – Threshold folgt automatisch dem Durchschnittspegel der
//  Stimme, Ratio/Threshold über "Amount", automatisches Make-up-Gain
// -----------------------------------------------------------------------------
class Compressor
{
public:
    void prepare (double sr)
    {
        fs = sr; p = 0; gr = 0; longAvg = -24; avgGr = 0;
        gate.prepare (sr);
        aDet = coef (5.0f, fs); aLong = coef (3000.0f, fs); aMk = coef (2500.0f, fs);
    }

    float process (float* L, float* R, int n, float amount, float speed, float mix, bool on) noexcept
    {
        if (! on) { gr = 0; return 0.0f; }
        const float aAtt = coef (mixf (25.0f, 2.0f, speed), fs);
        const float aRel = coef (mixf (320.0f, 60.0f, speed), fs);
        const float offset = mixf (2.0f, -14.0f, amount);
        const float slope = 1.0f - 1.0f / mixf (1.5f, 6.0f, amount);
        constexpr float knee = 6.0f;
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (L[i]), std::abs (R[i]));
            p = aDet * p + (1 - aDet) * x * x;
            const float lvl = powToDb (p);
            const bool voiced = gate.update (lvl, 20.0f);
            if (voiced) longAvg = aLong * longAvg + (1 - aLong) * lvl;

            const float over = lvl - (longAvg + offset);
            float target;
            if (over <= -knee * 0.5f)     target = 0.0f;
            else if (over >= knee * 0.5f) target = slope * over;
            else                          target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

            gr = target > gr ? aAtt * gr + (1 - aAtt) * target : aRel * gr + (1 - aRel) * target;
            if (voiced) avgGr = aMk * avgGr + (1 - aMk) * gr;

            const float g = mixf (1.0f, dbToGain (avgGr * 0.85f - gr), mix); // Parallel-Mix
            L[i] *= g; R[i] *= g;
            maxGr = std::max (maxGr, gr);
        }
        return maxGr;
    }

private:
    double fs = 44100.0;
    float p = 0, gr = 0, longAvg = -24, avgGr = 0, aDet = 0, aLong = 0, aMk = 0;
    VoiceGate gate;
};

// -----------------------------------------------------------------------------
//  Sättigung (weiche tanh-Kennlinie, Wärme/Dichte)
// -----------------------------------------------------------------------------
struct Saturator
{
    static void process (float* L, float* R, int n, float drive, bool on) noexcept
    {
        if (! on || drive <= 0.001f) return;
        const float g = 1.0f + 9.0f * drive, inv = 1.0f / g, wet = std::min (1.0f, drive * 1.5f);
        for (int i = 0; i < n; ++i)
        {
            L[i] += wet * (std::tanh (g * L[i]) * inv - L[i]);
            R[i] += wet * (std::tanh (g * R[i]) * inv - R[i]);
        }
    }
};

// -----------------------------------------------------------------------------
//  Ducker – liefert 0..1 "Stimme aktiv", damit Delay/Hall in Pausen aufblühen
// -----------------------------------------------------------------------------
class Ducker
{
public:
    void prepare (double sr) { env = 0; aA = coef (5.0f, sr); aR = coef (250.0f, sr); gate.prepare (sr); }
    void process (const float* L, const float* R, int n, float* act) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = std::max (std::abs (L[i]), std::abs (R[i]));
            env = x > env ? aA * env + (1 - aA) * x : aR * env + (1 - aR) * x;
            const float lvl = gainToDb (env);
            gate.update (lvl, 0.0f);
            act[i] = std::clamp ((lvl - (gate.ref - 30.0f)) / 24.0f, 0.0f, 1.0f);
        }
    }

private:
    float env = 0, aA = 0, aR = 0;
    VoiceGate gate;
};

// -----------------------------------------------------------------------------
//  Stereo Ping-Pong Delay (Tempo-Sync, gefiltertes Feedback, Ducking)
// -----------------------------------------------------------------------------
class StereoDelay
{
public:
    void prepare (double sr)
    {
        fs = sr; size = (int) (sr * 3.0) + 4;
        bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
        w = 0; cur = -1.0f; cleared = true;
        hp.setHighPass (fs, 250.0, 0.7); lp.setLowPass (fs, 5500.0, 0.7); hp.reset(); lp.reset();
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, const float* act, int n,
                  float delaySamples, float fb, float mix, float duck, bool on) noexcept
    {
        if (! on)
        {
            if (! cleared) { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); hp.reset(); lp.reset(); cleared = true; }
            std::fill (outL, outL + n, 0.0f); std::fill (outR, outR + n, 0.0f);
            return;
        }
        cleared = false;
        delaySamples = std::clamp (delaySamples, 1.0f, (float) (size - 3));
        if (cur < 0.0f) cur = delaySamples;

        for (int i = 0; i < n; ++i)
        {
            cur += (delaySamples - cur) * 0.0004f;
            const float dL = read (bufL, cur), dR = read (bufR, cur);
            const float in = 0.5f * (inL[i] + inR[i]);
            float wL = in * 0.65f + fb * dR;
            float wR = in * 0.35f + fb * dL;
            wL = lp.process (hp.process (wL, 0), 0);
            wR = lp.process (hp.process (wR, 1), 1);
            bufL[(size_t) w] = wL; bufR[(size_t) w] = wR;
            if (++w >= size) w = 0;

            const float g = mix * (1.0f - 0.9f * duck * act[i]);
            outL[i] = dL * g; outR[i] = dR * g;
        }
    }

private:
    float read (const std::vector<float>& b, float d) const noexcept
    {
        float rp = (float) w - d;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float fr = rp - (float) i0;
        return b[(size_t) i0] + fr * (b[(size_t) i1] - b[(size_t) i0]);
    }

    double fs = 44100.0;
    std::vector<float> bufL, bufR;
    int size = 0, w = 0;
    float cur = -1.0f;
    bool cleared = true;
    Biquad hp, lp;
};

// -----------------------------------------------------------------------------
//  Vocal Reverb (Pre-Delay, gefilterter Send, Ducking)
// -----------------------------------------------------------------------------
class VocalReverb
{
public:
    void prepare (double sr)
    {
        fs = sr; rev.setSampleRate (sr); rev.reset();
        pre.assign ((size_t) (sr * 0.3) + 2, 0.0f); w = 0; lastSize = -1.0f; cleared = true;
        hp.setHighPass (fs, 220.0, 0.7); lp.setLowPass (fs, 7500.0, 0.7); hp.reset(); lp.reset();
    }

    void process (const float* inL, const float* inR, float* outL, float* outR, const float* act, int n,
                  float size, float preMs, float mix, float duck, bool on) noexcept
    {
        if (! on)
        {
            if (! cleared) { rev.reset(); std::fill (pre.begin(), pre.end(), 0.0f); cleared = true; }
            std::fill (outL, outL + n, 0.0f); std::fill (outR, outR + n, 0.0f);
            return;
        }
        cleared = false;
        if (! juce::exactlyEqual (size, lastSize))
        {
            juce::Reverb::Parameters p;
            p.roomSize = 0.35f + 0.6f * size; p.damping = 0.45f;
            p.wetLevel = 0.33f; p.dryLevel = 0.0f; p.width = 1.0f; p.freezeMode = 0.0f;
            rev.setParameters (p); lastSize = size;
        }
        const int psize = (int) pre.size();
        const int d = std::clamp ((int) (preMs * 0.001 * fs), 0, psize - 1);

        for (int i = 0; i < n; ++i)
        {
            const float s = lp.process (hp.process (0.5f * (inL[i] + inR[i]), 0), 0);
            pre[(size_t) w] = s;
            int r = w - d; if (r < 0) r += psize;
            outL[i] = outR[i] = pre[(size_t) r];
            if (++w >= psize) w = 0;
        }
        rev.processStereo (outL, outR, n);
        for (int i = 0; i < n; ++i)
        {
            const float g = mix * (1.0f - 0.85f * duck * act[i]);
            outL[i] *= g; outR[i] *= g;
        }
    }

private:
    double fs = 44100.0;
    juce::Reverb rev;
    std::vector<float> pre;
    int w = 0;
    float lastSize = -1.0f;
    bool cleared = true;
    Biquad hp, lp;
};

// -----------------------------------------------------------------------------
//  Output Limiter (schützt vor Übersteuerung, Ceiling einstellbar)
// -----------------------------------------------------------------------------
class Limiter
{
public:
    void prepare (double sr) { g = 1.0f; aRel = coef (80.0f, sr); }
    void process (float* L, float* R, int n, float ceilingDb, bool on) noexcept
    {
        if (! on) return;
        const float c = dbToGain (ceilingDb);
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            const float t = pk > c ? c / pk : 1.0f;
            g = t < g ? t : aRel * g + (1 - aRel) * t;
            L[i] = std::clamp (L[i] * g, -c, c);
            R[i] = std::clamp (R[i] * g, -c, c);
        }
    }

private:
    float g = 1.0f, aRel = 0.0f;
};

} // namespace vf
