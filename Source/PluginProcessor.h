#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

/**
    PhaseScope - analysis plugin for phase and mono compatibility in mastering.

    Measures:
      - L/R correlation (full band), adjustable integration time
      - Correlation in three bands (bass / mids / highs, Linkwitz-Riley crossover)
      - Mono loss: level of the mono sum relative to the stereo level in dB
      - Minimum correlation since the last reset (hold)
      - Raw samples for the goniometer (lock-free FIFO to the editor)

    Audio is passed through unchanged - unless the "Mono Monitor" switch is
    on, in which case (L+R)/2 is sent to both channels.
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

    // ---- Interface for the editor (message thread) ----
    float getCorrelation (int band) const   { return correlation[(size_t) band].load (std::memory_order_relaxed); }
    float getMinCorrelation (int band) const{ return minCorrelation[(size_t) band].load (std::memory_order_relaxed); }
    bool  hasSignal (int band) const        { return signalPresent[(size_t) band].load (std::memory_order_relaxed); }
    float getMonoLossDb() const             { return monoLossDb.load (std::memory_order_relaxed); }
    float getMaxMonoLossDb() const          { return maxMonoLossDb.load (std::memory_order_relaxed); }
    void  resetHolds()                      { resetRequested.store (true); }

    /** Fetches up to maxNum stereo samples for the goniometer. Returns the count. */
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
    double alpha = 0.0;           // smoothing coefficient (integration time)
    float  lastIntegrationMs = -1.0f;
    int    holdArmSamples = 0;    // arm the hold only after settling
    int    holdArmLength  = 0;

    std::atomic<float>* integrationParam = nullptr;
    std::atomic<float>* monoParam        = nullptr;

    // Band split: lp1 -> bass, hp1 -> rest; rest -> lp2 (mids) / hp2 (highs)
    juce::dsp::LinkwitzRileyFilter<float> lp1, hp1, lp2, hp2;

    static constexpr int fifoSize = 1 << 15;
    juce::AbstractFifo fifo { fifoSize };
    juce::AudioBuffer<float> fifoBuffer { 2, fifoSize };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseScopeProcessor)
};
