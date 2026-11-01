#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace vfui
{
namespace col
{
    const juce::Colour bg      { 0xff14161b };
    const juce::Colour panel   { 0xff1e2129 };
    const juce::Colour edge    { 0xff2c303b };
    const juce::Colour text    { 0xffe6e8ee };
    const juce::Colour dim     { 0xff8a90a0 };
    const juce::Colour accent  { 0xffff7a45 };
    const juce::Colour accent2 { 0xff4cc2ff };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hl, bool down) override;
};

// Ein Modul-Panel: Titel + An/Aus + Drehregler (+ optional Auswahlbox)
class Panel : public juce::Component
{
public:
    Panel (juce::AudioProcessorValueTreeState&, const char* title, const juce::String& onId,
           const std::vector<std::pair<const char*, const char*>>& knobs, const juce::String& comboId = {});
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtts;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtt;
};

// Analyse-Anzeige: was die Automatik gerade macht
class AnalysisView : public juce::Component
{
public:
    explicit AnalysisView (VocalForgeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void update();

private:
    VocalForgeProcessor& proc;
    float in = -100, out = -100, rider = 0, ds = 0, comp = 0;
    std::array<float, vf::AutoEQ::N> eq {};
};
} // namespace vfui

class VocalForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalForgeEditor (VocalForgeProcessor&);
    ~VocalForgeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetBox();
    void saveUserPreset();
    void loadUserPreset();
    static juce::File presetFolder();

    VocalForgeProcessor& proc;
    vfui::LookAndFeel lnf;
    juce::ComboBox presetBox;
    juce::TextButton saveBtn { "Speichern" }, loadBtn { "Laden" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::OwnedArray<vfui::Panel> panels;
    vfui::AnalysisView analysis;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalForgeEditor)
};
