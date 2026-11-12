#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

/**
    PhaseScope – Analyse-Plugin für Phasen- und Monokompatibilität im Mastering.

    Gemessen wird:
      - Korrelationsgrad L/R (Vollband), Integrationszeit einstellbar
      - Korrelationsgrad in drei Bändern (Bass / Mitten / Höhen, Linkwitz-Riley-Weiche)
      - Mono-Verlust: Pegel der Mono-Summe relativ zum Stereo-Pegel in dB
      - Minimale Korrelation seit dem letzten Reset (Hold)
      - Rohsamples für das Goniometer (lock-free FIFO zum Editor)

    Das Audiosignal wird unverändert durchgereicht – außer der Abhörschalter
    "Mono" ist aktiv, dann wird (L+R)/2 auf beide Kanäle gelegt.
*/
class PhaseScopeProcessor : public juce::AudioProcessor
{
public:
    enum Band { Full = 0, Low, Mid, High, NumBands };

    static constexpr float crossoverLowHz  = 150.0f;
    static constexpr float crossoverHighHz = 2500.0f;

    PhaseScopeProcessor();
    ~PhaseScopeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi() const override                   { return false; }
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- Schnittstelle für den Editor (Message-Thread) ----
    float getCorrelation (int band) const   { return correlation[(size_t) band].load (std::memory_order_relaxed); }
    float getMinCorrelation (int band) const{ return minCorrelation[(size_t) band].load (std::memory_order_relaxed); }
    bool  hasSignal (int band) const        { return signalPresent[(size_t) band].load (std::memory_order_relaxed); }
    float getMonoLossDb() const             { return monoLossDb.load (std::memory_order_relaxed); }
    float getMaxMonoLossDb() const          { return maxMonoLossDb.load (std::memory_order_relaxed); }
    void  resetHolds()                      { resetRequested.store (true); }

    /** Holt bis zu maxNum Stereo-Samples für das Goniometer. Gibt die Anzahl zurück. */
    int readScopeSamples (float* left, float* right, int maxNum);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct CorrState
    {
        double lr = 0.0, ll = 0.0, rr = 0.0;
        void reset() { lr = ll = rr = 0.0; }
        void push (double a, float l, float r) noexcept
        {
            const double b = 1.0 - a;
            lr = a * lr + b * (double) l * (double) r;
            ll = a * ll + b * (double) l * (double) l;
            rr = a * rr + b * (double) r * (double) r;
        }
    };

    void updateAlpha();

    std::array<CorrState, NumBands> corrState;
    std::array<std::atomic<float>, NumBands> correlation;
    std::array<std::atomic<float>, NumBands> minCorrelation;
    std::array<std::atomic<bool>,  NumBands> signalPresent;

    double monoEnergy = 0.0, stereoEnergy = 0.0;
    std::atomic<float> monoLossDb    { 0.0f };
    std::atomic<float> maxMonoLossDb { 0.0f };
    std::atomic<bool>  resetRequested { false };

    double currentSampleRate = 44100.0;
    double alpha = 0.0;           // Glättungskoeffizient (Integrationszeit)
    float  lastIntegrationMs = -1.0f;
    int    holdArmSamples = 0;    // Hold erst nach Einschwingen scharf schalten
    int    holdArmLength  = 0;

    std::atomic<float>* integrationParam = nullptr;
    std::atomic<float>* monoParam        = nullptr;

    // Bandaufteilung: lp1 -> Bass, hp1 -> Rest; Rest -> lp2 (Mitten) / hp2 (Höhen)
    juce::dsp::LinkwitzRileyFilter<float> lp1, hp1, lp2, hp2;

    static constexpr int fifoSize = 1 << 15;
    juce::AbstractFifo fifo { fifoSize };
    juce::AudioBuffer<float> fifoBuffer { 2, fifoSize };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseScopeProcessor)
};
