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

    // یک ردیف ماتریکس: منبع، مقصد، مقدار
    struct SlotRow : public juce::Component
    {
        SlotRow (APVTS& s, const juce::String& srcId, const juce::String& dstId, const juce::String& amtId)
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (srcId))) src.addItemList (c->choices, 1);
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (dstId))) dst.addItemList (c->choices, 1);
            amt.setSliderStyle (juce::Slider::LinearHorizontal);
            amt.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
            amt.setTextBoxIsEditable (false);
            amt.setDoubleClickReturnValue (true, 0.0);
            addAndMakeVisible (src); addAndMakeVisible (dst); addAndMakeVisible (amt);
            srcA = std::make_unique<APVTS::ComboBoxAttachment> (s, srcId, src);
            dstA = std::make_unique<APVTS::ComboBoxAttachment> (s, dstId, dst);
            amtA = std::make_unique<APVTS::SliderAttachment> (s, amtId, amt);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            src.setBounds (r.removeFromLeft (112).reduced (1));
            r.removeFromLeft (4);
            dst.setBounds (r.removeFromLeft (118).reduced (1));
            r.removeFromLeft (4);
            amt.setBounds (r);
        }
        juce::ComboBox src, dst;
        juce::Slider amt;
        std::unique_ptr<APVTS::ComboBoxAttachment> srcA, dstA;
        std::unique_ptr<APVTS::SliderAttachment> amtA;
    };

    // دکمه‌ی بارگذاری سمپل
    struct SamplerLoader : public juce::Component
    {
        explicit SamplerLoader (ChipSynthProcessor& p) : proc (p)
        {
            button.setButtonText ("Load sample (wav / aiff / flac / ogg)...");
            button.onClick = [this] { chooseFile(); };
            label.setText (proc.getSampleName().isEmpty() ? juce::String ("No sample loaded") : proc.getSampleName(),
                           juce::dontSendNotification);
            addAndMakeVisible (button);
            addAndMakeVisible (label);
        }
        void chooseFile()
        {
            chooser = std::make_unique<juce::FileChooser> ("Select a sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
            const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                if (proc.loadSample (f)) label.setText (f.getFileName(), juce::dontSendNotification);
                else                     label.setText ("Could not load this file", juce::dontSendNotification);
            });
        }
        void resized() override
        {
            auto r = getLocalBounds();
            button.setBounds (r.removeFromTop (30));
            label.setBounds (r.removeFromTop (24));
        }
        ChipSynthProcessor& proc;
        juce::TextButton button;
        juce::Label label;
        std::unique_ptr<juce::FileChooser> chooser;
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

    // دکمه‌های مولد افکت آرکید
    struct GenButtons : public juce::Component
    {
        explicit GenButtons (ChipSynthProcessor& p) : proc (p)
        {
            static const char* names[8] = { "Shoot", "Explosion", "Power-up", "Hit / Hurt", "Jump", "Blip", "Random", "Mutate" };
            for (size_t i = 0; i < 8; ++i)
            {
                btn[i].setButtonText (names[i]);
                const int kind = (int) i;
                btn[i].onClick = [this, kind] { proc.generateSfx (kind); };
                addAndMakeVisible (btn[i]);
            }
        }
        void resized() override
        {
            const int w = getWidth() / 4;
            for (size_t i = 0; i < 8; ++i)
                btn[i].setBounds ((int) (i % 4) * w + 2, (int) (i / 4) * 38, w - 4, 34);
        }
        ChipSynthProcessor& proc;
        std::array<juce::TextButton, 8> btn;
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

    void knob   (Panel& p, const juce::String& id, const juce::String& name) { p.add (std::make_unique<KnobCell>   (proc.apvts, id, name), 84, 96); }
    void combo  (Panel& p, const juce::String& id, const juce::String& name, int w = 200) { p.add (std::make_unique<ComboCell> (proc.apvts, id, name), w, 50); }
    void toggle (Panel& p, const juce::String& id, const juce::String& name) { p.add (std::make_unique<ToggleCell> (proc.apvts, id, name), 200, 28); }
    void slot   (Panel& p, const juce::String& srcId, const juce::String& dstId, const juce::String& amtId)
    { p.add (std::make_unique<SlotRow> (proc.apvts, srcId, dstId, amtId), 440, 28); }

    int  layoutAll (bool apply);
    void showPage (int index);

    static constexpr int numPages = 9;

    ChipSynthProcessor& proc;
    std::vector<std::unique_ptr<Panel>> panels;
    std::array<juce::TextButton, numPages> tabs;
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
