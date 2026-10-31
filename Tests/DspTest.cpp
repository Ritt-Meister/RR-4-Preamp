#include <juce_dsp/juce_dsp.h>
#include "PreampDSP.h"
#include <cstdio>
#include <random>

static const double SR = 48000.0;

static double goertzelMag (const std::vector<float>& x, int start, int n, double f)
{
    double w = 2.0 * juce::MathConstants<double>::pi * f / SR, c = 2.0 * std::cos (w), s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { double s = x[(size_t) (start + i)] + c * s1 - s2; s2 = s1; s1 = s; }
    double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / n;
}

static std::vector<float> run (PreampParams p, const std::vector<float>& in, int block = 512, int ch = 1)
{
    PreampDSP d; d.prepare (SR, block, ch);
    std::vector<float> out (in.size());
    juce::AudioBuffer<float> buf (ch, block);
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        int n = (int) juce::jmin ((size_t) block, in.size() - pos);
        buf.setSize (ch, n, false, false, true);
        for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) buf.setSample (c, i, in[pos + (size_t) i]);
        d.process (buf, p);
        for (int i = 0; i < n; ++i) out[pos + (size_t) i] = buf.getSample (0, i);
    }
    std::printf ("");
    return out;
}

static std::vector<float> sine (double f, double dbfs, int n)
{
    std::vector<float> v ((size_t) n);
    double a = std::pow (10.0, dbfs / 20.0);
    for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (a * std::sin (2.0 * juce::MathConstants<double>::pi * f * i / SR));
    return v;
}

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* msg) { std::printf ("%s  %s\n", ok ? "OK  " : "FAIL", msg); if (! ok) ++fails; };

    { PreampDSP d; d.prepare (SR, 512, 2); std::printf ("Latenz: %d Samples\n", d.getLatencySamples()); }

    const int N = 48000 * 2, S0 = 48000, M = 48000;
    const char* names[] = { "Trafo", "Diskret", "Roehre", "Clean" };

    std::printf ("\n-- Klirr bei 1 kHz, -18 dBFS Eingang, Gain 0 dB (Zahlen in dB relativ zur Grundwelle) --\n");
    for (int c = 0; c < 4; ++c)
        for (float drive : { 0.0f, 0.5f, 1.0f })
        {
            PreampParams p; p.line = true; p.trimDb = 0; p.drive = drive; p.character = c;
            auto y = run (p, sine (1000.0, -18.0, N));
            double f1 = goertzelMag (y, S0, M, 1000), f2 = goertzelMag (y, S0, M, 2000), f3 = goertzelMag (y, S0, M, 3000);
            std::printf ("%-8s drive %.1f: Grundwelle %6.2f dBFS  H2 %6.1f dB  H3 %6.1f dB\n", names[c], drive,
                         20 * std::log10 (f1 + 1e-12), 20 * std::log10 (f2 / f1 + 1e-12), 20 * std::log10 (f3 / f1 + 1e-12));
        }

    std::printf ("\n-- Klirr bei -3 dBFS, Drive 0.5 --\n");
    for (int c = 0; c < 4; ++c)
    {
        PreampParams p; p.line = true; p.drive = 0.5f; p.character = c;
        auto y = run (p, sine (1000.0, -3.0, N));
        double f1 = goertzelMag (y, S0, M, 1000), f2 = goertzelMag (y, S0, M, 2000), f3 = goertzelMag (y, S0, M, 3000);
        std::printf ("%-8s: Grundwelle %6.2f dBFS  H2 %6.1f dB  H3 %6.1f dB\n", names[c], 20 * std::log10 (f1), 20 * std::log10 (f2 / f1), 20 * std::log10 (f3 / f1));
    }

    std::printf ("\n-- Gain-Genauigkeit (Clean, Drive 0, -50 dBFS) --\n");
    {
        PreampParams p; p.line = false; p.gainDb = 12.0f; p.drive = 0; p.character = 3; p.micLoad = false;
        auto y = run (p, sine (1000.0, -50.0, N));
        double g = 20 * std::log10 (goertzelMag (y, S0, M, 1000)) + 50.0;
        std::printf ("Mic 12 dB -> %.2f dB\n", g);
        check (std::abs (g - 12.0) < 0.3, "Mic-Gain 12 dB stimmt");
        p.pad = true;
        y = run (p, sine (1000.0, -50.0, N));
        g = 20 * std::log10 (goertzelMag (y, S0, M, 1000)) + 50.0;
        check (std::abs (g - (-8.0)) < 0.3, "Pad -20 dB stimmt");
        p.pad = false; p.line = true; p.trimDb = -6.0f;
        y = run (p, sine (1000.0, -50.0, N));
        g = 20 * std::log10 (goertzelMag (y, S0, M, 1000)) + 50.0;
        check (std::abs (g + 6.0) < 0.3, "Line-Trim -6 dB stimmt");
        p.trimDb = 0; p.phase = true;
        auto in = sine (1000.0, -30.0, N); y = run (p, in);
        double dot = 0; for (int i = S0; i < S0 + M; ++i) dot += in[(size_t) i] * y[(size_t) i];
        check (dot < 0, "Phase invertiert");
        p.phase = false; p.outDb = 6.0f;
        y = run (p, sine (1000.0, -50.0, N));
        g = 20 * std::log10 (goertzelMag (y, S0, M, 1000)) + 50.0;
        check (std::abs (g - 6.0) < 0.3, "Output +6 dB stimmt");
    }

    std::printf ("\n-- Filter --\n");
    {
        PreampParams p; p.line = true; p.drive = 0; p.character = 3;
        double a40 = 0, b40 = 0, a12 = 0, b12 = 0, w1 = 0, w2 = 0;
        auto y = run (p, sine (40.0, -40.0, N)); a40 = goertzelMag (y, S0, M, 40);
        p.hpf = true; y = run (p, sine (40.0, -40.0, N)); b40 = goertzelMag (y, S0, M, 40);
        std::printf ("HPF bei 40 Hz: %.1f dB\n", 20 * std::log10 (b40 / a40));
        check (20 * std::log10 (b40 / a40) < -6.0, "HPF daempft 40 Hz");
        p.hpf = false; p.line = false; p.gainDb = 0; p.micLoad = false;
        y = run (p, sine (12000.0, -40.0, N)); a12 = goertzelMag (y, S0, M, 12000);
        p.micLoad = true; y = run (p, sine (12000.0, -40.0, N)); b12 = goertzelMag (y, S0, M, 12000);
        std::printf ("Mic Load bei 12 kHz: %+.1f dB\n", 20 * std::log10 (b12 / a12));
        check (20 * std::log10 (b12 / a12) > 1.5, "Mic Load hebt 12 kHz an");
        p.micLoad = false; p.weightDb = 6.0f;
        y = run (p, sine (60.0, -40.0, N)); w1 = goertzelMag (y, S0, M, 60);
        p.weightDb = 0; y = run (p, sine (60.0, -40.0, N)); w2 = goertzelMag (y, S0, M, 60);
        std::printf ("Weight +6 bei 60 Hz: %+.1f dB\n", 20 * std::log10 (w1 / w2));
        check (20 * std::log10 (w1 / w2) > 3.5, "Weight hebt Bass an");
    }

    std::printf ("\n-- Silk (Clean, 8 kHz + 12 kHz Mix, -20 dBFS) --\n");
    {
        auto a = sine (3000.0, -20.0, N), b = sine (5000.0, -20.0, N);
        for (size_t i = 0; i < a.size(); ++i) a[i] += b[i];
        PreampParams p; p.line = true; p.drive = 0.5f; p.character = 0;
        auto y0 = run (p, a); p.silk = 1.0f; auto y1 = run (p, a);
        double h0 = goertzelMag (y0, S0, M, 8000), h1 = goertzelMag (y1, S0, M, 8000);
        double t0 = goertzelMag (y0, S0, M, 10000), t1 = goertzelMag (y1, S0, M, 10000);
        std::printf ("Summenton 8 kHz: %+.1f dB, 10 kHz: %+.1f dB\n", 20 * std::log10 (h1 / (h0 + 1e-9)), 20 * std::log10 (t1 / (t0 + 1e-9)));
    }

    std::printf ("\n-- Silk absolut: 5 kHz Sinus -20 dBFS, Clean --\n");
    for (float sk : { 0.0f, 0.5f, 1.0f })
    {
        PreampParams p; p.line = true; p.drive = 0.0f; p.character = 3; p.silk = sk;
        auto y = run (p, sine (5000.0, -20.0, N));
        double f1 = goertzelMag (y, S0, M, 5000), f2 = goertzelMag (y, S0, M, 10000), f3 = goertzelMag (y, S0, M, 15000);
        std::printf ("Silk %.1f: Grundwelle %.2f dBFS, H2 %.1f dB, H3 %.1f dB\n", sk, 20 * std::log10 (f1), 20 * std::log10 (f2 / f1 + 1e-12), 20 * std::log10 (f3 / f1 + 1e-12));
    }

    std::printf ("\n-- Punch: Transienten --\n");
    {
        std::vector<float> in ((size_t) N, 0.0f);
        for (int k = 0; k < 8; ++k)
            for (int i = 0; i < 4800; ++i)
                in[(size_t) (k * 12000 + 6000 + i)] = (float) (0.5 * std::exp (-i / 600.0) * std::sin (2 * juce::MathConstants<double>::pi * 100.0 * i / SR));
        PreampParams p; p.line = true; p.drive = 0; p.character = 3;
        auto y0 = run (p, in); p.punch = 1.0f; auto y1 = run (p, in);
        float pk0 = 0, pk1 = 0, rms0 = 0, rms1 = 0;
        for (int i = 48000; i < N; ++i) { pk0 = std::max (pk0, std::abs (y0[(size_t) i])); pk1 = std::max (pk1, std::abs (y1[(size_t) i])); }
        for (int k = 4; k < 8; ++k) for (int i = 3000; i < 4800; ++i) { rms0 += y0[(size_t) (k * 12000 + 6000 + i)] * y0[(size_t) (k * 12000 + 6000 + i)]; rms1 += y1[(size_t) (k * 12000 + 6000 + i)] * y1[(size_t) (k * 12000 + 6000 + i)]; }
        std::printf ("Peak: %.3f -> %.3f (%+.1f dB), Ausklang: %+.1f dB\n", pk0, pk1, 20 * std::log10 (pk1 / pk0), 10 * std::log10 (rms1 / rms0));
        check (pk1 > pk0 * 1.2f && pk1 < pk0 * 2.2f, "Punch hebt Transienten moderat an");
    }

    std::printf ("\n-- VU-Meter --\n");
    {
        PreampDSP d; d.prepare (SR, 512, 1);
        PreampParams p; p.line = true; p.drive = 0; p.character = 3;
        auto in = sine (1000.0, -15.0, N);   // Spitze -15 dBFS => RMS -18 dBFS => 0 VU
        juce::AudioBuffer<float> buf (1, 512);
        for (size_t pos = 0; pos + 512 <= in.size(); pos += 512)
        { for (int i = 0; i < 512; ++i) buf.setSample (0, i, in[pos + (size_t) i]); d.process (buf, p); }
        std::printf ("VU Eingang %.2f, Ausgang %.2f\n", d.getVuIn(), d.getVuOut());
        check (std::abs (d.getVuOut()) < 0.7 && std::abs (d.getVuIn()) < 0.7, "0 VU bei -18 dBFS RMS");
    }

    std::printf ("\n-- Stresstest: zufaellige Parameter, Blockgroessen, Mono/Stereo --\n");
    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> u (0, 1);
        bool bad = false; float maxAbs = 0;
        for (int ch = 1; ch <= 2; ++ch)
        {
            PreampDSP d; d.prepare (SR, 1024, ch);
            for (int iter = 0; iter < 400; ++iter)
            {
                PreampParams p;
                p.line = u (rng) > 0.5f; p.gainDb = u (rng) * 36; p.trimDb = -12 + u (rng) * 24; p.drive = u (rng); p.punch = u (rng);
                p.silk = u (rng); p.weightDb = -6 + u (rng) * 12; p.outDb = -24 + u (rng) * 36; p.character = (int) (u (rng) * 3.99f);
                p.micLoad = u (rng) > 0.5f; p.hpf = u (rng) > 0.5f; p.pad = u (rng) > 0.5f; p.phase = u (rng) > 0.5f;
                int n = 1 + (int) (u (rng) * 1500);
                juce::AudioBuffer<float> buf (ch, n);
                for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) buf.setSample (c, i, (u (rng) * 2 - 1) * (iter % 7 == 0 ? 1.5f : 0.3f));
                d.process (buf, p);
                for (int c = 0; c < ch; ++c) for (int i = 0; i < n; ++i) { float v = buf.getSample (c, i); if (! std::isfinite (v)) bad = true; maxAbs = std::max (maxAbs, std::abs (v)); }
            }
        }
        std::printf ("max |Ausgang| = %.2f\n", maxAbs);
        check (! bad, "keine NaN/Inf");
    }

    std::printf ("\n%s (%d Fehler)\n", fails == 0 ? "ALLE TESTS BESTANDEN" : "TESTS FEHLGESCHLAGEN", fails);
    return fails;
}
