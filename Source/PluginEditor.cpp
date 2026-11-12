#include "PluginEditor.h"

namespace Colours_
{
    const juce::Colour background { 0xff15171c };
    const juce::Colour panel      { 0xff1d2027 };
    const juce::Colour track      { 0xff2a2e37 };
    const juce::Colour grid       { 0xff3a3f4a };
    const juce::Colour text       { 0xffd8dbe2 };
    const juce::Colour textDim    { 0xff8a909c };
    const juce::Colour good       { 0xff3ecf8e };
    const juce::Colour warn       { 0xffe8b04a };
    const juce::Colour bad        { 0xffe5484d };
    const juce::Colour lowBand    { 0xffe8904a };
}

namespace
{
    juce::Colour correlationColour (float v)
    {
        if (v < 0.0f)  return Colours_::bad;
        if (v < 0.3f)  return Colours_::warn;
        return Colours_::good;
    }

    juce::Colour monoLossColour (float db)
    {
        if (db > -2.0f) return Colours_::good;
        if (db > -4.0f) return Colours_::warn;
        return Colours_::bad;
    }

    juce::String formatCorr (float v)
    {
        return (v >= 0.0f ? "+" : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")))
               + juce::String (std::abs (v), 2);
    }

    juce::String formatDb (float db)
    {
        if (db <= -59.9f) return juce::CharPointer_UTF8 ("\xe2\x88\x92\xe2\x88\x9e dB");
        return (db < -0.005f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String())
               + juce::String (std::abs (db), 1) + " dB";
    }

    juce::FontOptions font (float size, bool bold = false)
    {
        return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
    }
}

//==============================================================================
// Goniometer
//==============================================================================
Goniometer::Goniometer()
{
    setOpaque (true);
}

void Goniometer::resized()
{
    // Deliberately a software ARGB image: macOS stores native "RGB" images as ARGB internally,
    // so direct pixel access would otherwise end up transparent (black scope).
    phosphor = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()),
                            false, juce::SoftwareImageType());
    phosphor.clear (phosphor.getBounds(), juce::Colours::black);
}

void Goniometer::addSamples (const float* left, const float* right, int num)
{
    if (! phosphor.isValid())
        return;

    const int w = phosphor.getWidth(), h = phosphor.getHeight();
    juce::Image::BitmapData bd (phosphor, juce::Image::BitmapData::readWrite);

    // Afterglow: darken all pixels
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            auto* p = reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (x, y));
            p->setARGB (255,
                        (juce::uint8) ((p->getRed()   * 218) >> 8),   // ~0.85 per frame
                        (juce::uint8) ((p->getGreen() * 218) >> 8),
                        (juce::uint8) ((p->getBlue()  * 218) >> 8));
        }
    }

    if (num <= 0)
        return;

    constexpr float invSqrt2 = 0.70710678f;

    // Auto-gain: track the peak, let it decay slowly
    float framePeak = 0.0f;
    for (int i = 0; i < num; ++i)
    {
        const float x = (right[i] - left[i]) * invSqrt2;
        const float y = (right[i] + left[i]) * invSqrt2;
        framePeak = juce::jmax (framePeak, std::abs (x), std::abs (y));
    }
    peak = juce::jmax (framePeak, peak * 0.97f);
    if (peak > 1.0e-4f)
        gain = juce::jlimit (1.0f, 100.0f, 0.9f / peak);

    const float cx = w * 0.5f, cy = h * 0.5f;
    const float radius = juce::jmin (w, h) * 0.5f * 0.94f;

    for (int i = 0; i < num; ++i)
    {
        // Convention: L only -> upper-left diagonal, R only -> upper right,
        // mono -> vertical, out of phase -> horizontal
        const float x = (right[i] - left[i]) * invSqrt2 * gain;
        const float y = (right[i] + left[i]) * invSqrt2 * gain;

        const int px = (int) (cx + x * radius);
        const int py = (int) (cy - y * radius);
        if (px < 0 || py < 0 || px >= w || py >= h)
            continue;

        auto plot = [&bd] (int ix, int iy, int dr, int dg, int db)
        {
            auto* p = reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (ix, iy));
            p->setARGB (255,
                        (juce::uint8) juce::jmin (255, p->getRed()   + dr),
                        (juce::uint8) juce::jmin (255, p->getGreen() + dg),
                        (juce::uint8) juce::jmin (255, p->getBlue()  + db));
        };

        plot (px, py, 40, 130, 95);
        if (px + 1 < w) plot (px + 1, py, 12, 40, 30);
        if (py + 1 < h) plot (px, py + 1, 12, 40, 30);
    }
}

void Goniometer::paint (juce::Graphics& g)
{
    g.drawImageAt (phosphor, 0, 0);

    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f * 0.94f;

    g.setColour (Colours_::grid.withAlpha (0.7f));
    g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c), 1.0f);
    g.drawEllipse (juce::Rectangle<float> (r, r).withCentre (c), 0.6f);

    const float d = r * 0.70710678f;
    g.drawLine (c.x - d, c.y - d, c.x + d, c.y + d, 0.8f);   // L axis
    g.drawLine (c.x + d, c.y - d, c.x - d, c.y + d, 0.8f);   // R axis
    g.setColour (Colours_::grid);
    g.drawLine (c.x, c.y - r, c.x, c.y + r, 1.0f);           // M
    g.drawLine (c.x - r, c.y, c.x + r, c.y, 1.0f);           // S

    g.setColour (Colours_::textDim);
    g.setFont (font (12.0f, true));
    g.drawText ("M",   juce::Rectangle<float> (20, 16).withCentre ({ c.x + 10, c.y - r + 10 }), juce::Justification::centred);
    g.drawText ("L",   juce::Rectangle<float> (20, 16).withCentre ({ c.x - d - 8, c.y - d - 6 }), juce::Justification::centred);
    g.drawText ("R",   juce::Rectangle<float> (20, 16).withCentre ({ c.x + d + 8, c.y - d - 6 }), juce::Justification::centred);
    g.drawText ("+S",  juce::Rectangle<float> (24, 16).withCentre ({ c.x + r - 14, c.y - 10 }), juce::Justification::centred);
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x88\x92S"),
                juce::Rectangle<float> (24, 16).withCentre ({ c.x - r + 14, c.y - 10 }), juce::Justification::centred);

    g.setFont (font (11.0f));
    g.drawText ("Auto-Gain +" + juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (gain))) + " dB",
                getLocalBounds().reduced (8).removeFromBottom (14), juce::Justification::bottomRight);
}

//==============================================================================
// Correlation history
//==============================================================================
void CorrelationHistory::push (float f, float l, bool fv, bool lv)
{
    full[(size_t) writePos] = f;  fullOk[(size_t) writePos] = fv;
    low [(size_t) writePos] = l;  lowOk [(size_t) writePos] = lv;
    writePos = (writePos + 1) % numPoints;
}

void CorrelationHistory::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (Colours_::panel);
    g.fillRoundedRectangle (area, 6.0f);

    // Leave a row at the top for the legend so the curves never cover it
    auto plot = area.reduced (36, 10).withTrimmedTop (18).withTrimmedRight (-24);
    auto yFor = [&] (float v) { return plot.getY() + (1.0f - v) * 0.5f * plot.getHeight(); };

    // shade the negative range red
    g.setColour (Colours_::bad.withAlpha (0.08f));
    g.fillRect (plot.withTop (yFor (0.0f)));

    g.setFont (font (10.0f));
    for (float v : { 1.0f, 0.5f, 0.0f, -0.5f, -1.0f })
    {
        g.setColour (juce::exactlyEqual (v, 0.0f) ? Colours_::grid.brighter (0.3f) : Colours_::grid.withAlpha (0.6f));
        g.drawHorizontalLine ((int) yFor (v), plot.getX(), plot.getRight());
        g.setColour (Colours_::textDim);
        g.drawText (formatCorr (v).dropLastCharacters (1),
                    juce::Rectangle<float> (30, 12).withCentre ({ area.getX() + 18, yFor (v) }),
                    juce::Justification::centred);
    }

    auto drawSeries = [&] (const std::array<float, numPoints>& data, const std::array<bool, numPoints>& ok,
                           juce::Colour col, float thickness)
    {
        juce::Path p;
        bool started = false;
        for (int i = 0; i < numPoints; ++i)
        {
            const auto idx = (size_t) ((writePos + i) % numPoints);
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (numPoints - 1);
            if (! ok[idx]) { started = false; continue; }
            const float y = yFor (data[idx]);
            if (! started) { p.startNewSubPath (x, y); started = true; }
            else           p.lineTo (x, y);
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved));
    };

    drawSeries (low,  lowOk,  Colours_::lowBand.withAlpha (0.85f), 1.2f);
    drawSeries (full, fullOk, Colours_::text, 1.6f);

    // Legend
    auto legend = area.reduced (10, 6).removeFromTop (14).withTrimmedLeft (36);
    g.setFont (font (11.0f));
    g.setColour (Colours_::text);
    g.drawText ("Full Band", legend.removeFromLeft (70), juce::Justification::centredLeft);
    g.setColour (Colours_::lowBand);
    g.drawText (juce::CharPointer_UTF8 ("Bass < 150 Hz"), legend.removeFromLeft (100), juce::Justification::centredLeft);
    g.setColour (Colours_::textDim);
    g.drawText ("History, last 10 s", legend, juce::Justification::centredRight);
}

//==============================================================================
// Editor
//==============================================================================
PhaseScopeEditor::PhaseScopeEditor (PhaseScopeProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    scopeL.resize (1 << 15);
    scopeR.resize (1 << 15);
    shownMin.fill (1.0f);
    shownCorr.fill (1.0f);

    addAndMakeVisible (gonio);
    addAndMakeVisible (history);

    integrationSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    integrationSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    integrationSlider.setTextValueSuffix (" ms");
    integrationSlider.setColour (juce::Slider::thumbColourId, Colours_::good);
    integrationSlider.setColour (juce::Slider::trackColourId, Colours_::grid);
    integrationSlider.setColour (juce::Slider::backgroundColourId, Colours_::track);
    integrationSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    integrationSlider.setTooltip ("Integration time of the correlation measurement");
    addAndMakeVisible (integrationSlider);

    integrationLabel.setText ("Integration", juce::dontSendNotification);
    integrationLabel.setColour (juce::Label::textColourId, Colours_::textDim);
    integrationLabel.setFont (font (12.0f));
    addAndMakeVisible (integrationLabel);

    monoButton.setColour (juce::ToggleButton::textColourId, Colours_::text);
    monoButton.setColour (juce::ToggleButton::tickColourId, Colours_::warn);
    monoButton.setTooltip ("Sum the output to mono for checking");
    addAndMakeVisible (monoButton);

    resetButton.setColour (juce::TextButton::buttonColourId, Colours_::track);
    resetButton.setColour (juce::TextButton::textColourOffId, Colours_::text);
    resetButton.onClick = [this] { proc.resetHolds(); };
    addAndMakeVisible (resetButton);

    integrationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "integration", integrationSlider);
    monoAttachment        = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "mono", monoButton);

    setResizable (true, true);
    setResizeLimits (820, 600, 1600, 1200);
    setSize (920, 640);

    startTimerHz (30);
}

PhaseScopeEditor::~PhaseScopeEditor()
{
    stopTimer();
}

void PhaseScopeEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (34);   // title

    auto historyArea = area.removeFromBottom (juce::jmax (120, getHeight() / 5));
    area.removeFromBottom (12);
    history.setBounds (historyArea);

    const int gonioSize = juce::jmin (area.getHeight(), area.getWidth() / 2);
    gonio.setBounds (area.removeFromLeft (gonioSize).withSizeKeepingCentre (gonioSize, gonioSize));
    area.removeFromLeft (16);

    auto controls = area.removeFromBottom (64);
    auto row1 = controls.removeFromTop (28);
    integrationLabel.setBounds (row1.removeFromLeft (80));
    integrationSlider.setBounds (row1);
    controls.removeFromTop (8);
    auto row2 = controls.removeFromTop (28);
    monoButton.setBounds (row2.removeFromLeft (140));
    resetButton.setBounds (row2.removeFromRight (150));

    area.removeFromBottom (12);
    meterArea = area.removeFromTop (juce::jmax (90, area.getHeight() * 28 / 100));
    area.removeFromTop (10);
    monoArea  = area.removeFromBottom (juce::jmax (78, area.getHeight() * 28 / 100));
    area.removeFromBottom (10);
    bandArea  = area;
}

void PhaseScopeEditor::timerCallback()
{
    // Goniometer: drain the FIFO, draw the newest samples
    int total = 0;
    for (int got; (got = proc.readScopeSamples (scopeL.data() + total, scopeR.data() + total,
                                                (int) scopeL.size() - total)) > 0;)
        total += got;

    const int toDraw = juce::jmin (total, 8192);
    gonio.addSamples (scopeL.data() + (total - toDraw), scopeR.data() + (total - toDraw), toDraw);

    for (int b = 0; b < PhaseScopeProcessor::NumBands; ++b)
    {
        const auto i = (size_t) b;
        shownPresent[i] = proc.hasSignal (b);
        shownCorr[i] += 0.5f * (proc.getCorrelation (b) - shownCorr[i]);
        shownMin[i]  = proc.getMinCorrelation (b);
    }
    shownMonoLoss   += 0.5f * (proc.getMonoLossDb() - shownMonoLoss);
    shownMaxMonoLoss = proc.getMaxMonoLossDb();

    history.push (shownCorr[PhaseScopeProcessor::Full], shownCorr[PhaseScopeProcessor::Low],
                  shownPresent[PhaseScopeProcessor::Full], shownPresent[PhaseScopeProcessor::Low]);

    gonio.repaint();
    history.repaint();
    repaint (meterArea.getUnion (bandArea).getUnion (monoArea));
}

void PhaseScopeEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours_::background);

    auto title = getLocalBounds().reduced (16).removeFromTop (26);
    g.setColour (Colours_::text);
    g.setFont (font (18.0f, true));
    g.drawText ("PhaseScope", title, juce::Justification::centredLeft);
    g.setColour (Colours_::textDim);
    g.setFont (font (12.0f));
    g.drawText ("Phase & Mono Compatibility", title, juce::Justification::centredRight);

    using P = PhaseScopeProcessor;
    drawCorrelationMeter (g, meterArea, "Correlation (Full Band)",
                          shownCorr[P::Full], shownMin[P::Full], shownPresent[P::Full], true);

    const int bandH = bandArea.getHeight() / 3;
    auto bands = bandArea;
    drawCorrelationMeter (g, bands.removeFromTop (bandH).reduced (0, 3),
                          juce::CharPointer_UTF8 ("Bass  < 150 Hz"),
                          shownCorr[P::Low], shownMin[P::Low], shownPresent[P::Low], false);
    drawCorrelationMeter (g, bands.removeFromTop (bandH).reduced (0, 3),
                          juce::CharPointer_UTF8 ("Mids  150 Hz \xe2\x80\x93 2.5 kHz"),
                          shownCorr[P::Mid], shownMin[P::Mid], shownPresent[P::Mid], false);
    drawCorrelationMeter (g, bands.reduced (0, 3),
                          juce::CharPointer_UTF8 ("Highs  > 2.5 kHz"),
                          shownCorr[P::High], shownMin[P::High], shownPresent[P::High], false);

    drawMonoLoss (g, monoArea);
}

void PhaseScopeEditor::drawCorrelationMeter (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                                             float value, float minHold, bool present, bool large) const
{
    auto r = area.toFloat();
    g.setColour (Colours_::panel);
    g.fillRoundedRectangle (r, 6.0f);
    r = r.reduced (12.0f, large ? 10.0f : 6.0f);

    auto header = r.removeFromTop (large ? 18.0f : 15.0f);
    g.setColour (Colours_::textDim);
    g.setFont (font (large ? 13.0f : 11.5f));
    g.drawText (label, header, juce::Justification::centredLeft);

    // Min-hold text
    if (minHold < 0.999f)
    {
        g.setColour (minHold < 0.0f ? Colours_::bad : Colours_::textDim);
        g.drawText ("min " + formatCorr (minHold), header, juce::Justification::centredRight);
    }

    auto valueBox = r.removeFromRight (large ? 78.0f : 58.0f);
    g.setFont (font (large ? 26.0f : 16.0f, true));
    g.setColour (present ? correlationColour (value) : Colours_::textDim);
    g.drawText (present ? formatCorr (value) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")),
                valueBox, juce::Justification::centredRight);

    // Scale
    auto scaleRow = large ? r.removeFromBottom (14.0f) : juce::Rectangle<float>();
    auto bar = r.withSizeKeepingCentre (r.getWidth() - 8.0f, large ? 18.0f : 10.0f);
    auto xFor = [&] (float v) { return bar.getX() + (v + 1.0f) * 0.5f * bar.getWidth(); };

    g.setColour (Colours_::track);
    g.fillRoundedRectangle (bar, 3.0f);
    g.setColour (Colours_::bad.withAlpha (0.12f));
    g.fillRect (bar.withRight (xFor (0.0f)));

    if (present)
    {
        const float x0 = xFor (0.0f), x1 = xFor (value);
        g.setColour (correlationColour (value));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (x0, x1), bar.getY(),
                                                                juce::jmax (x0, x1), bar.getBottom()));
        g.setColour (juce::Colours::white);
        g.fillRect (juce::Rectangle<float> (2.0f, bar.getHeight() + 4.0f).withCentre ({ x1, bar.getCentreY() }));
    }

    if (minHold < 0.999f)
    {
        g.setColour (Colours_::bad);
        g.fillRect (juce::Rectangle<float> (1.5f, bar.getHeight() + 6.0f).withCentre ({ xFor (minHold), bar.getCentreY() }));
    }

    g.setColour (Colours_::grid.brighter (0.4f));
    for (float v : { -0.5f, 0.0f, 0.5f })
        g.drawVerticalLine ((int) xFor (v), bar.getY(), bar.getBottom());

    if (large)
    {
        g.setFont (font (10.0f));
        g.setColour (Colours_::textDim);
        const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
        g.drawText (minus + "1 out of phase", scaleRow.withWidth (110).withX (bar.getX()), juce::Justification::centredLeft);
        g.drawText ("0 uncorrelated", juce::Rectangle<float> (100, scaleRow.getHeight()).withCentre ({ xFor (0.0f), scaleRow.getCentreY() }), juce::Justification::centred);
        g.drawText ("+1 mono", scaleRow.withLeft (bar.getRight() - 80).withRight (bar.getRight()), juce::Justification::centredRight);
    }
}

void PhaseScopeEditor::drawMonoLoss (juce::Graphics& g, juce::Rectangle<int> area) const
{
    auto r = area.toFloat();
    g.setColour (Colours_::panel);
    g.fillRoundedRectangle (r, 6.0f);
    r = r.reduced (12.0f, 10.0f);

    const bool present = proc.hasSignal (PhaseScopeProcessor::Full);

    auto header = r.removeFromTop (18.0f);
    g.setColour (Colours_::textDim);
    g.setFont (font (13.0f));
    g.drawText ("Mono Loss (Sum vs. Stereo)", header, juce::Justification::centredLeft);
    if (shownMaxMonoLoss < -0.05f)
    {
        g.setColour (monoLossColour (shownMaxMonoLoss));
        g.drawText ("max " + formatDb (shownMaxMonoLoss), header, juce::Justification::centredRight);
    }

    auto valueBox = r.removeFromRight (110.0f);
    g.setFont (font (26.0f, true));
    g.setColour (present ? monoLossColour (shownMonoLoss) : Colours_::textDim);
    g.drawText (present ? formatDb (shownMonoLoss) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")),
                valueBox, juce::Justification::centredRight);

    // Bar 0 … -12 dB
    auto scaleRow = r.removeFromBottom (14.0f);
    auto bar = r.withSizeKeepingCentre (r.getWidth() - 8.0f, 12.0f);
    auto xFor = [&] (float db) { return bar.getX() + juce::jlimit (0.0f, 1.0f, -db / 12.0f) * bar.getWidth(); };

    g.setColour (Colours_::track);
    g.fillRoundedRectangle (bar, 3.0f);
    if (present)
    {
        g.setColour (monoLossColour (shownMonoLoss));
        g.fillRect (bar.withRight (xFor (shownMonoLoss)));
    }
    if (shownMaxMonoLoss < -0.05f)
    {
        g.setColour (Colours_::bad);
        g.fillRect (juce::Rectangle<float> (1.5f, bar.getHeight() + 6.0f).withCentre ({ xFor (shownMaxMonoLoss), bar.getCentreY() }));
    }

    g.setFont (font (10.0f));
    g.setColour (Colours_::textDim);
    const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
    for (float db : { 0.0f, -3.0f, -6.0f, -12.0f })
    {
        g.setColour (Colours_::grid.brighter (0.4f));
        g.drawVerticalLine ((int) xFor (db), bar.getY(), bar.getBottom());
        g.setColour (Colours_::textDim);
        const juce::String txt = juce::exactlyEqual (db, 0.0f) ? "0" : minus + juce::String ((int) -db);
        g.drawText (txt, juce::Rectangle<float> (30, scaleRow.getHeight()).withCentre ({ juce::jlimit (bar.getX() + 8, bar.getRight() - 12, xFor (db)), scaleRow.getCentreY() }),
                    juce::Justification::centred);
    }
}
