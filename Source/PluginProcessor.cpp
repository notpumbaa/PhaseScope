#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double silenceThreshold = 1.0e-8;   // approx. -80 dBFS power
    constexpr float  monoLossFloorDb  = -60.0f;
}

PhaseScopeProcessor::PhaseScopeProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PhaseScope", createLayout())
{
    for (size_t i = 0; i < NumBands; ++i)
    {
        correlation[i].store (1.0f);
        minCorrelation[i].store (1.0f);
        signalPresent[i].store (false);
    }

    integrationParam = apvts.getRawParameterValue ("integration");
    monoParam        = apvts.getRawParameterValue ("mono");

    lp1.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    hp1.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lp2.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    hp2.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
}

juce::AudioProcessorValueTreeState::ParameterLayout PhaseScopeProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "integration", 1 }, "Integration Time",
        juce::NormalisableRange<float> (50.0f, 3000.0f, 1.0f, 0.4f), 300.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "mono", 1 }, "Mono Monitor", false));

    return layout;
}

void PhaseScopeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    lp1.prepare (spec); hp1.prepare (spec);
    lp2.prepare (spec); hp2.prepare (spec);
    lp1.setCutoffFrequency (crossoverLowHz);
    hp1.setCutoffFrequency (crossoverLowHz);
    lp2.setCutoffFrequency (crossoverHighHz);
    hp2.setCutoffFrequency (crossoverHighHz);

    for (auto& s : corrState) s.reset();
    monoEnergy = stereoEnergy = 0.0;

    lastIntegrationMs = -1.0f;
    updateAlpha();
    resetRequested.store (true);
    fifo.reset();
}

void PhaseScopeProcessor::updateAlpha()
{
    const float ms = integrationParam->load();
    if (juce::exactlyEqual (ms, lastIntegrationMs))
        return;

    lastIntegrationMs = ms;
    const double tauSamples = juce::jmax (1.0, (double) ms * 0.001 * currentSampleRate);
    alpha = std::exp (-1.0 / tauSamples);
    holdArmLength  = (int) (tauSamples * 3.0);   // ~95 % settled
    holdArmSamples = holdArmLength;
}

bool PhaseScopeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PhaseScopeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    updateAlpha();

    if (resetRequested.exchange (false))
    {
        for (size_t i = 0; i < NumBands; ++i)
            minCorrelation[i].store (1.0f);
        maxMonoLossDb.store (0.0f);
        holdArmSamples = holdArmLength;
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    const double a = alpha, b = 1.0 - alpha;

    for (int n = 0; n < numSamples; ++n)
    {
        const float l = L[n], r = R[n];

        corrState[Full].push (a, l, r);

        // Bands: identical filters on L and R -> their phase response cancels out in the correlation
        const float lLow  = lp1.processSample (0, l);
        const float rLow  = lp1.processSample (1, r);
        const float lRest = hp1.processSample (0, l);
        const float rRest = hp1.processSample (1, r);
        const float lMid  = lp2.processSample (0, lRest);
        const float rMid  = lp2.processSample (1, rRest);
        const float lHigh = hp2.processSample (0, lRest);
        const float rHigh = hp2.processSample (1, rRest);

        corrState[Low] .push (a, lLow,  rLow);
        corrState[Mid] .push (a, lMid,  rMid);
        corrState[High].push (a, lHigh, rHigh);

        const double m = 0.5 * ((double) l + (double) r);
        monoEnergy   = a * monoEnergy   + b * m * m;
        stereoEnergy = a * stereoEnergy + b * 0.5 * ((double) l * l + (double) r * r);
    }

    lp1.snapToZero(); hp1.snapToZero(); lp2.snapToZero(); hp2.snapToZero();

    holdArmSamples = juce::jmax (0, holdArmSamples - numSamples);
    const bool holdArmed = holdArmSamples == 0;

    for (size_t i = 0; i < NumBands; ++i)
    {
        const auto& s = corrState[i];
        const double energy = s.ll + s.rr;
        const bool present = energy > silenceThreshold;
        signalPresent[i].store (present, std::memory_order_relaxed);

        float c = 1.0f;
        if (present)
        {
            const double denom = std::sqrt (s.ll * s.rr);
            c = denom > 1.0e-20 ? (float) juce::jlimit (-1.0, 1.0, s.lr / denom)
                                : 0.0f;   // one channel silent -> no correlation

            if (holdArmed && c < minCorrelation[i].load (std::memory_order_relaxed))
                minCorrelation[i].store (c, std::memory_order_relaxed);
        }
        correlation[i].store (c, std::memory_order_relaxed);
    }

    float lossDb = 0.0f;
    if (stereoEnergy > silenceThreshold)
        lossDb = juce::jmax (monoLossFloorDb, (float) (10.0 * std::log10 (juce::jmax (1.0e-12, monoEnergy / stereoEnergy))));
    monoLossDb.store (lossDb, std::memory_order_relaxed);
    if (holdArmed && stereoEnergy > silenceThreshold && lossDb < maxMonoLossDb.load (std::memory_order_relaxed))
        maxMonoLossDb.store (lossDb, std::memory_order_relaxed);

    // Raw samples for the goniometer (before any mono summing)
    {
        const auto scope = fifo.write (numSamples);
        if (scope.blockSize1 > 0)
        {
            fifoBuffer.copyFrom (0, scope.startIndex1, L, scope.blockSize1);
            fifoBuffer.copyFrom (1, scope.startIndex1, R, scope.blockSize1);
        }
        if (scope.blockSize2 > 0)
        {
            fifoBuffer.copyFrom (0, scope.startIndex2, L + scope.blockSize1, scope.blockSize2);
            fifoBuffer.copyFrom (1, scope.startIndex2, R + scope.blockSize1, scope.blockSize2);
        }
    }

    // Monitor switch: mono sum for checking
    if (monoParam->load() > 0.5f)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float m = 0.5f * (L[n] + R[n]);
            L[n] = R[n] = m;
        }
    }
}

int PhaseScopeProcessor::readScopeSamples (float* left, float* right, int maxNum)
{
    const auto scope = fifo.read (juce::jmin (maxNum, fifo.getNumReady()));

    if (scope.blockSize1 > 0)
    {
        std::copy_n (fifoBuffer.getReadPointer (0, scope.startIndex1), scope.blockSize1, left);
        std::copy_n (fifoBuffer.getReadPointer (1, scope.startIndex1), scope.blockSize1, right);
    }
    if (scope.blockSize2 > 0)
    {
        std::copy_n (fifoBuffer.getReadPointer (0, scope.startIndex2), scope.blockSize2, left  + scope.blockSize1);
        std::copy_n (fifoBuffer.getReadPointer (1, scope.startIndex2), scope.blockSize2, right + scope.blockSize1);
    }
    return scope.blockSize1 + scope.blockSize2;
}

void PhaseScopeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PhaseScopeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PhaseScopeProcessor::createEditor()
{
    return new PhaseScopeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhaseScopeProcessor();
}
