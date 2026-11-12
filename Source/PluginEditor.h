#pragma once

#include "PluginProcessor.h"
#include <vector>

/** Goniometer (Lissajous / vectorscope) with phosphor afterglow and auto-gain. */
class Goniometer : public juce::Component
{
public:
    Goniometer();
    void resized() override;
    void paint (juce::Graphics&) override;

    /** Draws new samples and lets the image fade out. */
    void addSamples (const float* left, const float* right, int num);

private:
    juce::Image phosphor;
    float gain = 1.0f;
    float peak = 0.0f;
};

/** Correlation history over the last ~10 seconds. */
class CorrelationHistory : public juce::Component
{
public:
    static constexpr int numPoints = 300;   // 10 s at 30 Hz
    void push (float full, float low, bool fullValid, bool lowValid);
    void paint (juce::Graphics&) override;

private:
    std::array<float, numPoints> full {}, low {};
    std::array<bool,  numPoints> fullOk {}, lowOk {};
    int writePos = 0;
};

class PhaseScopeEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit PhaseScopeEditor (PhaseScopeProcessor&);
    ~PhaseScopeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    friend struct PhaseScopeScreenshotRenderer;   // tools/Screenshots.cpp

    void timerCallback() override;
    void drawCorrelationMeter (juce::Graphics&, juce::Rectangle<int> area, const juce::String& label,
                               float value, float minHold, bool present, bool large) const;
    void drawMonoLoss (juce::Graphics&, juce::Rectangle<int> area) const;

    PhaseScopeProcessor& proc;

    Goniometer         gonio;
    CorrelationHistory history;

    juce::Slider       integrationSlider;
    juce::Label        integrationLabel;
    juce::ToggleButton monoButton  { "Mono Monitor" };
    juce::TextButton   resetButton { "Reset Holds" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> integrationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAttachment;

    // Displayed values (slightly smoothed for the visuals)
    std::array<float, PhaseScopeProcessor::NumBands> shownCorr {};
    std::array<float, PhaseScopeProcessor::NumBands> shownMin {};
    std::array<bool,  PhaseScopeProcessor::NumBands> shownPresent {};
    float shownMonoLoss = 0.0f, shownMaxMonoLoss = 0.0f;

    std::vector<float> scopeL, scopeR;

    juce::Rectangle<int> meterArea, bandArea, monoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseScopeEditor)
};
