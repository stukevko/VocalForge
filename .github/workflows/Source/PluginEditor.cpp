#include "PluginEditor.h"

using namespace vfui;

// =============================================================================
//  LookAndFeel
// =============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, col::dim);
    setColour (juce::ComboBox::backgroundColourId, col::panel);
    setColour (juce::ComboBox::outlineColourId, col::edge);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panel);
    setColour (juce::TextButton::textColourOffId, col::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const bool enabled = s.isEnabled();
    const auto acc = enabled ? col::accent : col::dim;

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
    g.setColour (col::edge);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolare Regler (z. B. -6..+6 dB) füllen ab der Mitte
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bipolar ? a0 + (float) ((0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (a1 - a0) : a0;
    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 2, r - 2, 0, std::min (from, ang), std::max (from, ang), true);
    g.setColour (acc);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (col::bg);
    g.fillEllipse (juce::Rectangle<float> (r * 1.3f, r * 1.3f).withCentre (c));
    g.setColour (col::text.withAlpha (enabled ? 1.0f : 0.4f));
    const juce::Point<float> tip (c.x + std::sin (ang) * r * 0.55f, c.y - std::cos (ang) * r * 0.55f);
    g.drawLine ({ c.getPointOnCircumference (r * 0.2f, ang), tip }, 2.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2);
    const bool onState = b.getToggleState();
    g.setColour (onState ? col::accent : col::edge);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    const float d = r.getHeight() - 4;
    g.setColour (col::text);
    g.fillEllipse (onState ? r.getRight() - d - 2 : r.getX() + 2, r.getY() + 2, d, d);
}

// =============================================================================
//  Panel
// =============================================================================
Panel::Panel (juce::AudioProcessorValueTreeState& s, const char* t, const juce::String& onId,
              const std::vector<std::pair<const char*, const char*>>& knobs, const juce::String& comboId)
    : title (juce::String::fromUTF8 (t))
{
    if (onId.isNotEmpty())
    {
        addAndMakeVisible (power);
        powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, onId, power);
        power.onStateChange = [this] { for (auto* sl : sliders) sl->setEnabled (power.getToggleState()); if (combo) combo->setEnabled (power.getToggleState()); };
    }
    if (comboId.isNotEmpty())
    {
        combo = std::make_unique<juce::ComboBox>();
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (comboId)))
            combo->addItemList (cp->choices, 1);
        addAndMakeVisible (*combo);
        comboAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, comboId, *combo);
    }
    for (auto& [id, name] : knobs)
    {
        auto* sl = sliders.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
        sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        sl->setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (sl);
        sliderAtts.add (new juce::AudioProcessorValueTreeState::SliderAttachment (s, id, *sl));
        sl->setNumDecimalPlacesToDisplay (1);
        sl->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        sl->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        sl->setColour (juce::Slider::textBoxTextColourId, col::text);

        auto* lb = labels.add (new juce::Label ({}, juce::String::fromUTF8 (name)));
        lb->setJustificationType (juce::Justification::centred);
        lb->setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (lb);
    }
    if (onId.isNotEmpty()) power.onStateChange();
}

void Panel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3);
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 8, 1);
    g.setColour (col::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), getLocalBounds().reduced (12, 8).removeFromTop (18), juce::Justification::centredLeft);
}

void Panel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto head = r.removeFromTop (20);
    power.setBounds (head.removeFromRight (36).withSizeKeepingCentre (34, 18));
    if (combo) { r.removeFromTop (6); combo->setBounds (r.removeFromTop (22)); }
    r.removeFromTop (4);

    const int n = sliders.size();
    if (n == 0) return;
    const int w = r.getWidth() / n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = r.removeFromLeft (w);
        auto knob = cell.withSizeKeepingCentre (std::min (cell.getWidth(), 86), std::min (cell.getHeight() - 16, 104));
        sliders[i]->setBounds (knob);
        labels[i]->setBounds (cell.getX(), knob.getBottom(), cell.getWidth(), 16);
    }
}

// =============================================================================
//  Analyse-Anzeige
// =============================================================================
void AnalysisView::update()
{
    in = proc.meterIn.load(); out = proc.meterOut.load(); rider = proc.riderGain.load();
    ds = proc.deEssGR.load(); comp = proc.compGR.load();
    for (size_t b = 0; b < eq.size(); ++b) eq[b] = proc.eqGains[b].load();
    repaint();
}

void AnalysisView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3);
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 8);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 8, 1);

    auto area = getLocalBounds().reduced (14, 10);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.setColour (col::text);
    g.drawText ("WAS DIE AUTOMATIK GERADE MACHT", area.removeFromTop (18), juce::Justification::centredLeft);
    area.removeFromTop (6);

    // --- Auto-EQ Balken (Korrektur je Band) ---
    auto eqArea = area.removeFromLeft (area.getWidth() / 2).reduced (0, 2);
    static const char* names[] { "150", "350", "800", "2.5k", "5k", "11k" };
    const int bw = eqArea.getWidth() / (int) eq.size();
    const float mid = (float) eqArea.getY() + (eqArea.getHeight() - 16) * 0.5f;
    const float scale = (eqArea.getHeight() - 16) * 0.5f / 9.0f;
    g.setColour (col::edge);
    g.drawHorizontalLine ((int) mid, (float) eqArea.getX(), (float) eqArea.getRight() - 10);
    for (size_t b = 0; b < eq.size(); ++b)
    {
        auto cell = eqArea.withX (eqArea.getX() + (int) b * bw).withWidth (bw);
        const float v = std::clamp (eq[b], -9.0f, 9.0f) * scale;
        g.setColour (v >= 0 ? col::accent : col::accent2);
        g.fillRoundedRectangle ((float) cell.getX() + bw * 0.25f, v >= 0 ? mid - v : mid, bw * 0.5f - 10, std::abs (v), 2);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (names[b], cell.removeFromBottom (14).withTrimmedRight (10), juce::Justification::centred);
    }

    // --- Pegel & Gain-Reduction ---
    area.removeFromLeft (16);
    auto row = [&] (const juce::String& name, float value, float lo, float hi, juce::Colour c, const juce::String& txt)
    {
        auto line = area.removeFromTop (std::max (14, area.getHeight() / 5 + 1));
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (name, line.removeFromLeft (92), juce::Justification::centredLeft);
        g.drawText (txt, line.removeFromRight (62), juce::Justification::centredRight);
        auto bar = line.reduced (4, line.getHeight() / 2 - 3).toFloat();
        g.setColour (col::edge);
        g.fillRoundedRectangle (bar, 3);
        const float t = juce::jlimit (0.0f, 1.0f, (value - lo) / (hi - lo));
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * t), 3);
    };
    auto dbTxt = [] (float v) { return v <= -99 ? juce::String ("-inf") : juce::String (v, 1) + " dB"; };
    row ("Input",        in,    -60, 0,  col::accent2, dbTxt (in));
    row ("Auto Level",   rider + 18, 0, 36, col::accent, (rider >= 0 ? "+" : "") + juce::String (rider, 1) + " dB");
    row ("De-Esser GR",  ds,    0, 12,   col::accent, "-" + juce::String (ds, 1) + " dB");
    row ("Comp GR",      comp,  0, 18,   col::accent, "-" + juce::String (comp, 1) + " dB");
    row ("Output",       out,   -60, 0,  out > -0.5f ? juce::Colours::red : col::accent2, dbTxt (out));
}

// =============================================================================
//  Editor
// =============================================================================
VocalForgeEditor::VocalForgeEditor (VocalForgeProcessor& p)
    : AudioProcessorEditor (&p), proc (p), analysis (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    panels.add (new vfui::Panel (s, "Input", {},          { { "inGain", "Gain" }, { "lowCut", "Low Cut" } }));
    panels.add (new vfui::Panel (s, "Auto Level", "riderOn", { { "riderTarget", "Ziel" }, { "riderRange", "Bereich" } }));
    panels.add (new vfui::Panel (s, "Auto EQ", "eqOn",    { { "eqAmount", "Stärke" }, { "eqTone", "Dunkel/Hell" }, { "eqBody", "Body" },
                                                            { "eqPresence", "Präsenz" }, { "eqAir", "Air" } }));
    panels.add (new vfui::Panel (s, "Auto De-Esser", "dsOn", { { "dsAmount", "Stärke" }, { "dsFreq", "Frequenz" } }));
    panels.add (new vfui::Panel (s, "Auto Comp", "compOn", { { "compAmount", "Stärke" }, { "compSpeed", "Tempo" }, { "compMix", "Mix" } }));
    panels.add (new vfui::Panel (s, "SÄTTIGUNG", "satOn", { { "satDrive", "Drive" } }));
    panels.add (new vfui::Panel (s, "Auto Delay", "dlyOn", { { "dlyFeedback", "Feedback" }, { "dlyMix", "Mix" }, { "dlyDuck", "Ducking" } }, "dlyTime"));
    panels.add (new vfui::Panel (s, "Auto Reverb", "revOn", { { "revSize", "Größe" }, { "revPre", "Pre-Delay" }, { "revMix", "Mix" }, { "revDuck", "Ducking" } }));
    panels.add (new vfui::Panel (s, "Output", "limOn",    { { "outGain", "Gain" }, { "limCeiling", "Ceiling" }, { "dryWet", "Dry/Wet" } }));
    for (auto* pn : panels) addAndMakeVisible (pn);
    addAndMakeVisible (analysis);

    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id <= proc.getNumPrograms()) proc.setCurrentProgram (id - 1);
    };
    addAndMakeVisible (presetBox);
    saveBtn.onClick = [this] { saveUserPreset(); };
    loadBtn.onClick = [this] { loadUserPreset(); };
    addAndMakeVisible (saveBtn);
    addAndMakeVisible (loadBtn);

    setResizable (true, true);
    setResizeLimits (900, 560, 1800, 1100);
    setSize (1080, 640);
    startTimerHz (30);
}

VocalForgeEditor::~VocalForgeEditor() { setLookAndFeel (nullptr); }

void VocalForgeEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

juce::File VocalForgeEditor::presetFolder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VocalForge Presets");
    f.createDirectory();
    return f;
}

void VocalForgeEditor::saveUserPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Preset speichern", presetFolder().getChildFile ("Mein Vocal.vfpreset"), "*.vfpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            if (auto xml = proc.apvts.copyState().createXml())
                xml->writeTo (file.withFileExtension ("vfpreset"));
        });
}

void VocalForgeEditor::loadUserPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Preset laden", presetFolder(), "*.vfpreset");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            if (auto xml = juce::XmlDocument::parse (fc.getResult()))
                if (xml->hasTagName (proc.apvts.state.getType()))
                {
                    proc.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    presetBox.setText (fc.getResult().getFileNameWithoutExtension(), juce::dontSendNotification);
                }
        });
}

void VocalForgeEditor::timerCallback() { analysis.update(); }

void VocalForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (vfui::col::bg);
    auto head = getLocalBounds().reduced (16, 10).removeFromTop (36);
    g.setColour (vfui::col::accent);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("VOCALFORGE", head, juce::Justification::centredLeft);
    g.setColour (vfui::col::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("All-in-One Auto Vocal Chain", head.withTrimmedLeft (170), juce::Justification::centredLeft);
}

void VocalForgeEditor::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto head = r.removeFromTop (36);
    loadBtn.setBounds (head.removeFromRight (80).reduced (2, 4));
    saveBtn.setBounds (head.removeFromRight (90).reduced (2, 4));
    presetBox.setBounds (head.removeFromRight (280).reduced (6, 4));
    r.removeFromTop (6);

    const int rowH = (r.getHeight() - 150) / 2;
    auto row1 = r.removeFromTop (rowH);
    auto row2 = r.removeFromTop (rowH);
    auto layout = [] (juce::Rectangle<int> row, std::initializer_list<std::pair<vfui::Panel*, int>> items)
    {
        int total = 0;
        for (auto& it : items) total += it.second;
        const int unit = row.getWidth() / total;
        for (auto& it : items) it.first->setBounds (row.removeFromLeft (unit * it.second));
    };
    layout (row1, { { panels[0], 2 }, { panels[1], 2 }, { panels[2], 5 }, { panels[3], 2 } });
    layout (row2, { { panels[4], 3 }, { panels[5], 2 }, { panels[6], 3 }, { panels[7], 4 }, { panels[8], 3 } });
    analysis.setBounds (r);
}
