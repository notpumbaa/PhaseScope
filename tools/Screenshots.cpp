/*
    PhaseScopeScreenshots - renders the plugin UI offscreen with synthetic
    test signals and saves PNGs (for the README / releases).

    Build:  cmake -B build-tools -DPHASESCOPE_BUILD_SCREENSHOTS=ON
            cmake --build build-tools --target PhaseScopeScreenshots --config Release
    Usage:  PhaseScopeScreenshots <output folder>
*/

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <functional>
#include <random>

struct PhaseScopeScreenshotRenderer
{
    static void tick (PhaseScopeEditor& e) { e.timerCallback(); }
};

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int    blockSize  = 512;
    constexpr double twoPi      = 6.283185307179586;

    /** Simple one-pole low-/high-pass for the test signals. */
    struct OnePole
    {
        float a = 0.0f, z = 0.0f;
        void setCutoff (float hz) { a = std::exp (-(float) twoPi * hz / (float) sampleRate); }
        float lp (float x) { z = (1.0f - a) * x + a * z; return z; }
        float hp (float x) { return x - lp (x); }
    };

    /** Music-like test signal: bass, chord pad and hi-hats, available separately. */
    struct Music
    {
        std::mt19937 rng { 1234 };
        std::normal_distribution<float> noise { 0.0f, 1.0f };
        OnePole padL, padR, hatL, hatR, hatL2, hatR2;
        double t = 0.0;

        Music()
        {
            padL.setCutoff (1800.0f); padR.setCutoff (1800.0f);
            hatL.setCutoff (6000.0f); hatR.setCutoff (6000.0f);
            hatL2.setCutoff (6000.0f); hatR2.setCutoff (6000.0f);
        }

        struct Frame { float bass, padCommon, padSide, hatCommon, hatL, hatR; };

        Frame next()
        {
            const double beat = std::fmod (t * 2.0, 1.0);             // 120 BPM
            const float  bassEnv = (float) std::exp (-beat * 2.5);
            const double bassHz  = (std::fmod (t, 4.0) < 2.0) ? 55.0 : 41.2;
            const float  bass = bassEnv * (float) (0.55 * std::sin (twoPi * bassHz * t)
                                                 + 0.20 * std::sin (twoPi * 2.0 * bassHz * t));

            float pad = 0.0f;
            for (double f : { 220.0, 277.18, 329.63, 440.0 })
                pad += (float) std::sin (twoPi * f * t + f);
            pad *= 0.06f;
            const float padSide = 0.05f * (float) std::sin (twoPi * 0.3 * t)
                                  * (float) std::sin (twoPi * 277.18 * 1.003 * t);

            const double eighth = std::fmod (t * 4.0, 1.0);
            const float  hatEnv = (float) std::exp (-eighth * 18.0) * 0.35f;
            const float  n = noise (rng), nl = noise (rng), nr = noise (rng);

            t += 1.0 / sampleRate;
            return { bass, pad, padSide,
                     hatEnv * hatL.hp (n),
                     hatEnv * hatL2.hp (nl),
                     hatEnv * hatR2.hp (nr) };
        }
    };

    using Mixer = std::function<void (const Music::Frame&, double time, float& l, float& r)>;

    void render (const juce::File& outFile, double seconds, const Mixer& mix, float integrationMs = 300.0f)
    {
        PhaseScopeProcessor proc;
        proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        proc.prepareToPlay (sampleRate, blockSize);
        if (auto* p = proc.apvts.getParameter ("integration"))
            p->setValueNotifyingHost (p->convertTo0to1 (integrationMs));

        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto& editor = *static_cast<PhaseScopeEditor*> (ed.get());
        editor.setSize (920, 640);

        Music music;
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        const int totalSamples = (int) (seconds * sampleRate);
        const int samplesPerTick = (int) (sampleRate / 30.0);
        int sinceTick = 0;
        double time = 0.0;

        for (int pos = 0; pos < totalSamples; pos += blockSize)
        {
            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            for (int i = 0; i < blockSize; ++i)
            {
                mix (music.next(), time, l[i], r[i]);
                time += 1.0 / sampleRate;
            }
            proc.processBlock (buffer, midi);

            sinceTick += blockSize;
            if (sinceTick >= samplesPerTick)
            {
                sinceTick -= samplesPerTick;
                PhaseScopeScreenshotRenderer::tick (editor);
            }
        }

        auto image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f);
        outFile.deleteFile();
        juce::FileOutputStream out (outFile);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << "written: " << outFile.getFullPathName() << std::endl;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    const juce::File dir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                    : juce::File::getCurrentWorkingDirectory();
    dir.createDirectory();

    // 1) Healthy mix: mono bass, slightly wide pad, partly decorrelated hi-hats
    render (dir.getChildFile ("mix-ok.png"), 12.0, [] (const Music::Frame& f, double, float& l, float& r)
    {
        const float hatC = 0.6f * f.hatCommon;
        l = f.bass + f.padCommon + f.padSide + hatC + 0.4f * f.hatL;
        r = f.bass + f.padCommon - f.padSide + hatC + 0.4f * f.hatR;
    });

    // 2) Very wide stereo: pad with a large side component, fully uncorrelated hi-hats
    render (dir.getChildFile ("mix-wide.png"), 12.0, [] (const Music::Frame& f, double, float& l, float& r)
    {
        l = f.bass + 0.8f * f.padCommon + 2.5f * f.padSide + f.hatL;
        r = f.bass + 0.8f * f.padCommon - 2.5f * f.padSide + f.hatR;
    });

    // 3) Problem case: after 7 s the bass flips polarity on one channel
    render (dir.getChildFile ("bass-out-of-phase.png"), 12.0, [] (const Music::Frame& f, double t, float& l, float& r)
    {
        const float bassR = t < 7.0 ? f.bass : -f.bass;
        const float hatC = 0.6f * f.hatCommon;
        l = f.bass + f.padCommon + f.padSide + hatC + 0.4f * f.hatL;
        r = bassR  + f.padCommon - f.padSide + hatC + 0.4f * f.hatR;
    });

    return 0;
}
