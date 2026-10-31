#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>
#include <vector>

struct PreampParams
{
    bool  line      = false;
    float gainDb    = 12.0f;   // Mic-Gain 0..36 dB
    float trimDb    = 0.0f;    // Line-Trim -12..+12 dB
    float drive     = 0.35f;   // 0..1
    float punch     = 0.0f;    // 0..1
    float silk      = 0.0f;    // 0..1
    float weightDb  = 0.0f;    // -6..+6 dB
    float outDb     = 0.0f;    // -24..+12 dB
    int   character = 0;       // 0 Trafo, 1 Diskret, 2 Roehre, 3 Clean
    bool  micLoad   = true;
    bool  hpf       = false;
    bool  pad       = false;
    bool  phase     = false;
};

class PreampDSP
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        sr       = sampleRate;
        osSr     = sampleRate * 4.0;
        maxBlock = juce::jmax (1, maxBlockSize);
        nCh      = juce::jlimit (1, 2, numChannels);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) nCh, (size_t) 2,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing ((size_t) maxBlock);

        chans = std::vector<Chan> ((size_t) nCh);

        const float fs = (float) sr;
        hpfCoefs  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 80.0f, 0.7071f);
        loadCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, juce::jmin (12000.0f, fs * 0.45f), 0.8f, juce::Decibels::decibelsToGain (2.5f));
        weightCoefs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 110.0f, 0.7f, 1.0f);
        silkCoefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, juce::jmin (9000.0f, fs * 0.4f), 0.6f, 1.0f);

        for (auto& c : chans)
        {
            c.hpf.coefficients    = hpfCoefs;
            c.load.coefficients   = loadCoefs;
            c.weight.coefficients = weightCoefs;
            c.silk.coefficients   = silkCoefs;
        }

        envFastA = coefFor (0.0005, sr);
        envFastR = coefFor (0.020, sr);
        envSlowA = coefFor (0.015, sr);
        envSlowR = coefFor (0.150, sr);
        punchSmooth = coefFor (0.005, sr);
        dcR = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / sr);
        vuCoef = coefFor (0.065, sr);

        trafoLowCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 140.0f / (float) osSr);
        silkLpCoef   = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4000.0f / (float) osSr);

        inGainSm.reset (sr, 0.015);
        hpfSm.reset (sr, 0.020);
        loadSm.reset (sr, 0.020);
        punchSm.reset (sr, 0.030);
        outSm.reset (sr, 0.020);
        driveSm.reset (osSr, 0.030);
        silkSm.reset (osSr, 0.030);
        fadeSm.reset (osSr, 0.025);

        first = true;
        reset();
    }

    void reset()
    {
        for (auto& c : chans)
        {
            c.hpf.reset(); c.load.reset(); c.weight.reset(); c.silk.reset();
            c.envFast = c.envSlow = c.dcX = c.dcY = c.trafoLow = c.silkLp = 0.0f;
            c.punchGain = 1.0f;
        }
        vuIn = vuOut = 0.0f;
        if (oversampling) oversampling->reset();
        first = true;
    }

    int getLatencySamples() const
    {
        return oversampling ? (int) std::lround (oversampling->getLatencyInSamples()) : 0;
    }

    float getVuIn()  const { return meterIn.load();  }
    float getVuOut() const { return meterOut.load(); }

    void process (juce::AudioBuffer<float>& buffer, const PreampParams& p)
    {
        const int total = buffer.getNumSamples();
        const int chs   = juce::jmin (buffer.getNumChannels(), nCh);
        if (chs <= 0 || oversampling == nullptr) return;

        updateTargets (p);

        int pos = 0;
        while (pos < total)
        {
            const int n = juce::jmin (maxBlock, total - pos);
            float* ptrs[2] = { buffer.getWritePointer (0, pos),
                               chs > 1 ? buffer.getWritePointer (1, pos) : nullptr };
            processChunk (ptrs, chs, n, p);
            pos += n;
        }

        meterIn.store  (vuToDb (vuIn));
        meterOut.store (vuToDb (vuOut));
    }

private:
    struct Chan
    {
        juce::dsp::IIR::Filter<float> hpf, load, weight, silk;
        float envFast = 0, envSlow = 0, punchGain = 1, dcX = 0, dcY = 0, trafoLow = 0, silkLp = 0;
    };

    static float coefFor (double seconds, double rate)
    {
        return 1.0f - (float) std::exp (-1.0 / (seconds * rate));
    }

    static float vuToDb (float ms)
    {
        return 10.0f * std::log10 (ms + 1.0e-12f) + 18.0f;   // 0 VU = -18 dBFS RMS
    }

    void updateTargets (const PreampParams& p)
    {
        const float gDb = (p.line ? p.trimDb : p.gainDb) - (p.pad ? 20.0f : 0.0f);
        const float g   = juce::Decibels::decibelsToGain (gDb, -100.0f) * (p.phase ? -1.0f : 1.0f);
        const float load = (p.micLoad && ! p.line) ? 1.0f : 0.0f;
        const float a = 1.0f + 5.0f * p.drive;

        if (first)
        {
            inGainSm.setCurrentAndTargetValue (g);
            hpfSm.setCurrentAndTargetValue (p.hpf ? 1.0f : 0.0f);
            loadSm.setCurrentAndTargetValue (load);
            punchSm.setCurrentAndTargetValue (p.punch);
            outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.outDb, -100.0f));
            driveSm.setCurrentAndTargetValue (a);
            silkSm.setCurrentAndTargetValue (p.silk);
            fadeSm.setCurrentAndTargetValue (1.0f);
            curChar = prevChar = p.character;
            lastWeight = 1000.0f; lastSilk = -1.0f;
            first = false;
        }
        else
        {
            inGainSm.setTargetValue (g);
            hpfSm.setTargetValue (p.hpf ? 1.0f : 0.0f);
            loadSm.setTargetValue (load);
            punchSm.setTargetValue (p.punch);
            outSm.setTargetValue (juce::Decibels::decibelsToGain (p.outDb, -100.0f));
            driveSm.setTargetValue (a);
            silkSm.setTargetValue (p.silk);
            if (p.character != curChar)
            {
                prevChar = curChar;
                curChar  = p.character;
                fadeSm.setCurrentAndTargetValue (0.0f);
                fadeSm.setTargetValue (1.0f);
            }
        }

        if (std::abs (p.weightDb - lastWeight) > 0.001f)
        {
            lastWeight = p.weightDb;
            weightCoefs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 110.0f, 0.7f, juce::Decibels::decibelsToGain (p.weightDb));
            for (auto& c : chans) c.weight.coefficients = weightCoefs;
        }
        if (std::abs (p.silk - lastSilk) > 0.001f)
        {
            lastSilk = p.silk;
            silkCoefs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, juce::jmin (9000.0f, (float) sr * 0.4f), 0.6f,
                                                                           juce::Decibels::decibelsToGain (3.0f * p.silk));
            for (auto& c : chans) c.silk.coefficients = silkCoefs;
        }
    }

    // ---- Saettigungskennlinien (Eingang u, Ausgang ca. +-1) ----
    static float satTrafo (float u) noexcept   // leicht asymmetrisch: 2. Harmonische
    {
        return (std::tanh (u + 0.2f) - 0.197375f) * 1.0406f;
    }

    static float satDiskret (float u) noexcept // harter Knick, symmetrisch + wenig Bias
    {
        auto f = [] (float v) { return v / std::cbrt (1.0f + std::abs (v * v * v)); };
        return f (u + 0.05f) - f (0.05f);
    }

    static float satTube (float u) noexcept    // Triode: positive Haelfte weicher begrenzt als negative
    {
        if (u >= 0.0f) return std::tanh (u);
        return -(1.0f - std::exp (0.8f * u)) / 0.8f;
    }

    float shape (int c, float x, float a, float comp, Chan& ch) noexcept
    {
        switch (c)
        {
            case 0: // Trafo: Tiefen saettigen frueher (Eisenkern)
            {
                ch.trafoLow += trafoLowCoef * (x - ch.trafoLow);
                const float low = ch.trafoLow;
                const float hi  = x - low;
                const float a2  = a * 2.2f;
                return (satTrafo (a * hi) / a + satTrafo (a2 * low) / a2) * comp;
            }
            case 1: return satDiskret (a * x) / a * comp;
            case 2: return satTube (a * x) / a * comp;
            default:
            {
                const float ae = a * 0.35f;
                return std::tanh (ae * x) / ae;
            }
        }
    }

    void processChunk (float** ptrs, int chs, int n, const PreampParams& p)
    {
        // ---------- Stufe A: 1x (Gain, Filter, Punch) ----------
        for (int i = 0; i < n; ++i)
        {
            const float g       = inGainSm.getNextValue();
            const float hpfMix  = hpfSm.getNextValue();
            const float loadMix = loadSm.getNextValue();
            const float punchAm = punchSm.getNextValue();
            float msIn = 0.0f;

            for (int c = 0; c < chs; ++c)
            {
                auto& st = chans[(size_t) c];
                float x = ptrs[c][i];
                msIn += x * x;
                x *= g;

                const float hp = st.hpf.processSample (x);
                x += hpfMix * (hp - x);

                const float pk = st.load.processSample (x);
                x += loadMix * (pk - x);

                x = st.weight.processSample (x);

                // Transienten-Punch
                const float ax = std::abs (x);
                st.envFast += (ax > st.envFast ? envFastA : envFastR) * (ax - st.envFast);
                st.envSlow += (ax > st.envSlow ? envSlowA : envSlowR) * (ax - st.envSlow);
                float targetGain = 1.0f;
                if (st.envSlow > 1.0e-4f && punchAm > 0.0001f)
                {
                    const float rDb = 20.0f * std::log10 (juce::jmax (1.0f, st.envFast / st.envSlow));
                    const float boostDb = punchAm * 6.0f * juce::jlimit (0.0f, 1.0f, rDb / 6.0f);
                    targetGain = juce::Decibels::decibelsToGain (boostDb);
                }
                st.punchGain += punchSmooth * (targetGain - st.punchGain);
                x *= st.punchGain;

                ptrs[c][i] = x;
            }
            vuIn += vuCoef * (msIn / (float) chs - vuIn);
        }

        // ---------- Stufe B: 4x Oversampling (Saettigung, Silk-Exciter) ----------
        juce::dsp::AudioBlock<float> block (ptrs, (size_t) chs, (size_t) n);
        auto osBlock = oversampling->processSamplesUp (block);
        const int nOs = (int) osBlock.getNumSamples();

        for (int j = 0; j < nOs; ++j)
        {
            const float a    = driveSm.getNextValue();
            const float silk = silkSm.getNextValue();
            const float fade = fadeSm.getNextValue();
            const float comp = std::pow (a, 0.3f);

            for (int c = 0; c < chs; ++c)
            {
                auto& st = chans[(size_t) c];
                float* d = osBlock.getChannelPointer ((size_t) c);
                const float x = d[j];

                float y = shape (curChar, x, a, comp, st);
                if (fade < 1.0f && prevChar != curChar)
                {
                    const float yp = shape (prevChar, x, a, comp, st);
                    y = yp + fade * (y - yp);
                }

                // Silk: asymmetrisch gesaettigte Hoehen -> 2./3. Harmonische ("Luft")
                st.silkLp += silkLpCoef * (y - st.silkLp);
                const float hf = y - st.silkLp;
                const float u  = hf * 6.0f;
                const float ex = (std::tanh (u + 0.3f) - 0.29131f - 0.9151f * u) / 6.0f;
                y += silk * 2.5f * ex;

                d[j] = y;
            }
        }

        oversampling->processSamplesDown (block);

        // ---------- Stufe C: 1x (DC, Silk-Shelf, Output, Meter) ----------
        for (int i = 0; i < n; ++i)
        {
            const float og = outSm.getNextValue();
            float msOut = 0.0f;
            for (int c = 0; c < chs; ++c)
            {
                auto& st = chans[(size_t) c];
                float x = ptrs[c][i];

                const float y = x - st.dcX + dcR * st.dcY;
                st.dcX = x; st.dcY = y;
                x = st.silk.processSample (y);

                x *= og;
                msOut += x * x;
                ptrs[c][i] = x;
            }
            vuOut += vuCoef * (msOut / (float) chs - vuOut);
        }

        juce::ignoreUnused (p);
    }

    double sr = 44100.0, osSr = 176400.0;
    int maxBlock = 512, nCh = 2;
    bool first = true;
    int curChar = 0, prevChar = 0;
    float lastWeight = 1000.0f, lastSilk = -1.0f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    std::vector<Chan> chans;
    juce::dsp::IIR::Coefficients<float>::Ptr hpfCoefs, loadCoefs, weightCoefs, silkCoefs;

    juce::SmoothedValue<float> inGainSm, hpfSm, loadSm, punchSm, outSm, driveSm, silkSm, fadeSm;

    float envFastA = 0, envFastR = 0, envSlowA = 0, envSlowR = 0, punchSmooth = 0;
    float dcR = 0.999f, vuCoef = 0, trafoLowCoef = 0, silkLpCoef = 0;
    float vuIn = 0, vuOut = 0;
    std::atomic<float> meterIn { -60.0f }, meterOut { -60.0f };
};
