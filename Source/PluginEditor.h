#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include "PluginProcessor.h"

class ChipSynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChipSynthEditor (ChipSynthProcessor&);
    ~ChipSynthEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct KnobCell : public juce::Component
    {
        KnobCell (APVTS& s, const juce::String& id, const juce::String& name)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 16);
            slider.setTextBoxIsEditable (false);   // تا فوکوس کیبورد رو از FL نگیره
            label.setText (name, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (slider);
            addAndMakeVisible (label);
            attach = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (16));
            slider.setBounds (r);
        }
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attach;
    };

    struct ComboCell : public juce::Component
    {
        ComboCell (APVTS& s, const juce::String& id, const juce::String& name)
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
                combo.addItemList (c->choices, 1);
            label.setText (name, juce::dontSendNotification);
            addAndMakeVisible (combo);
            addAndMakeVisible (label);
            attach = std::make_unique<APVTS::ComboBoxAttachment> (s, id, combo);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (18));
            combo.setBounds (r.reduced (0, 2));
        }
        juce::ComboBox combo;
        juce::Label label;
        std::unique_ptr<APVTS::ComboBoxAttachment> attach;
    };

    struct ToggleCell : public juce::Component
    {
        ToggleCell (APVTS& s, const juce::String& id, const juce::String& name)
        {
            button.setButtonText (name);
            addAndMakeVisible (button);
            attach = std::make_unique<APVTS::ButtonAttachment> (s, id, button);
        }
        void resized() override { button.setBounds (getLocalBounds()); }
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attach;
    };

    // گرید ۱۶ استپی: روشن/خاموش، اکسنت، ترنسپوز
    struct StepGrid : public juce::Component
    {
        explicit StepGrid (APVTS& s)
        {
            for (size_t i = 0; i < 16; ++i)
            {
                const juce::String n ((int) i + 1);
                addAndMakeVisible (onBtn[i]);
                addAndMakeVisible (accBtn[i]);
                addAndMakeVisible (trSlider[i]);

                trSlider[i].setSliderStyle (juce::Slider::LinearVertical);
                trSlider[i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                trSlider[i].setPopupDisplayEnabled (true, true, this);
                trSlider[i].setDoubleClickReturnValue (true, 0.0);

                onA[i]  = std::make_unique<APVTS::ButtonAttachment> (s, "s" + n + "on", onBtn[i]);
                accA[i] = std::make_unique<APVTS::ButtonAttachment> (s, "s" + n + "acc", accBtn[i]);
                trA[i]  = std::make_unique<APVTS::SliderAttachment> (s, "s" + n + "tr", trSlider[i]);
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::lightgrey);
            g.setFont (12.0f);
            g.drawText ("Step on/off (rest)", 0, 0, getWidth(), 14, juce::Justification::centredLeft);
            g.drawText ("Accent", 0, 42, getWidth(), 14, juce::Justification::centredLeft);
            g.drawText ("Transpose per step (-12..+12, double-click = 0)", 0, 84, getWidth(), 14, juce::Justification::centredLeft);
        }

        void resized() override
        {
            const float cw = (float) getWidth() / 16.0f;
            for (size_t i = 0; i < 16; ++i)
            {
                const int x = (int) ((float) i * cw);
                const int w = (int) cw - 2;
                onBtn[i].setBounds (x, 14, w, 24);
                accBtn[i].setBounds (x, 56, w, 24);
                trSlider[i].setBounds (x, 98, w, 80);
            }
        }

        std::array<juce::ToggleButton, 16> onBtn, accBtn;
        std::array<juce::Slider, 16> trSlider;
        std::array<std::unique_ptr<APVTS::ButtonAttachment>, 16> onA, accA;
        std::array<std::unique_ptr<APVTS::SliderAttachment>, 16> trA;
    };

    struct InfoBox : public juce::Component
    {
        explicit InfoBox (juce::StringArray l) : lines (std::move (l)) {}
        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::lightgrey);
            g.setFont (13.0f);
            int y = 0;
            for (auto& line : lines) { g.drawText (line, 0, y, getWidth(), 18, juce::Justification::centredLeft); y += 20; }
        }
        juce::StringArray lines;
    };

    struct Panel : public juce::Component
    {
        explicit Panel (const juce::String& t) : title (t) {}

        template <typename T>
        void add (std::unique_ptr<T> c, int w, int h)
        {
            addAndMakeVisible (*c);
            items.push_back ({ c.get(), w, h });
            owned.push_back (std::move (c));
        }

        int doLayout (int width, bool apply)
        {
            const int pad = 10;
            int x = pad, y = 28, rowH = 0;
            for (auto& it : items)
            {
                if (x + it.w > width - pad && x > pad) { x = pad; y += rowH + 4; rowH = 0; }
                if (apply) it.comp->setBounds (x, y, it.w, it.h);
                x += it.w + 4;
                rowH = std::max (rowH, it.h);
            }
            return y + rowH + pad;
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colour (0xff2a2a35));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
            g.setColour (juce::Colour (0xff7fd1ff));
            g.setFont (15.0f);
            g.drawText (title, 12, 4, getWidth() - 24, 20, juce::Justification::centredLeft);
        }

        struct Item { juce::Component* comp; int w; int h; };
        juce::String title;
        int page = 0, col = 0;
        std::vector<Item> items;
        std::vector<std::unique_ptr<juce::Component>> owned;
    };

    void knob   (Panel& p, const char* id, const char* name) { p.add (std::make_unique<KnobCell>   (proc.apvts, id, name), 84, 96); }
    void combo  (Panel& p, const char* id, const char* name, int w = 200) { p.add (std::make_unique<ComboCell> (proc.apvts, id, name), w, 50); }
    void toggle (Panel& p, const char* id, const char* name) { p.add (std::make_unique<ToggleCell> (proc.apvts, id, name), 200, 28); }

    int  layoutAll (bool apply);
    void showPage (int index);

    ChipSynthProcessor& proc;
    std::vector<std::unique_ptr<Panel>> panels;
    std::array<juce::TextButton, 4> tabs;
    int currentPage = 0;
    juce::ComboBox presetBox;
    juce::MidiKeyboardComponent keyboard;

    static void disableFocus (juce::Component& c)
    {
        c.setWantsKeyboardFocus (false);
        c.setMouseClickGrabsKeyboardFocus (false);
        for (auto* child : c.getChildren())
            disableFocus (*child);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChipSynthEditor)
};
