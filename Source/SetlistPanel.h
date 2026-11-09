#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

// V0.9.8: Setlist editor. Deliberately does not touch audio thread or WAV engines.
class SetlistPanel final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit SetlistPanel (std::function<void (const juce::File&)> openSong)
        : onOpenSong (std::move (openSong))
    {
        addAndMakeVisible (title);
        title.setText ("SETLIST MODE – V0.9.8", juce::dontSendNotification);
        title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        addAndMakeVisible (list);
        list.setModel (this);
        list.setRowHeight (36);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff17171c));
        list.setColour (juce::ListBox::outlineColourId, juce::Colour (0xff44444c));
        list.setOutlineThickness (1);
        setup (add, "ADD SONG", [this] { addSong(); });
        setup (remove, "REMOVE", [this] { changeSelection (-2); });
        setup (up, "MOVE UP", [this] { changeSelection (-1); });
        setup (down, "MOVE DOWN", [this] { changeSelection (1); });
        setup (save, "SAVE SETLIST", [this] { saveSetlist(); });
        setup (load, "LOAD SETLIST", [this] { loadSetlist(); });
        setup (open, "OPEN SELECTED", [this]
        {
            const int i = list.getSelectedRow();
            if (juce::isPositiveAndBelow (i, songs.size()) && songs.getReference(i).existsAsFile())
                onOpenSong (songs.getReference(i));
            else
                showMessage ("Select an existing .sklive song first.");
        });
        addAndMakeVisible (info);
        info.setText ("Edit the setlist here. OPEN SELECTED stops the current song before loading another.", juce::dontSendNotification);
        info.setJustificationType (juce::Justification::centredLeft);
        setSize (800, 520);
    }

    int getNumRows() override { return songs.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, songs.size())) return;
        g.fillAll (selected ? juce::Colour (0xff315a66) : (row % 2 == 0 ? juce::Colour (0xff222229) : juce::Colour (0xff292931)));
        g.setColour (songs.getReference (row).existsAsFile() ? juce::Colours::white : juce::Colours::orange);
        g.setFont (15.0f);
        g.drawText (juce::String (row + 1).paddedLeft ('0', 2) + "   " + songs.getReference (row).getFileNameWithoutExtension(),
                    10, 0, width - 20, height, juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (40));
        r.removeFromTop (8);
        auto footer = r.removeFromBottom (72);
        info.setBounds (footer.removeFromBottom (30));
        auto actions = footer.removeFromTop (38);
        const int w = actions.getWidth() / 3;
        save.setBounds (actions.removeFromLeft (w).reduced (3));
        load.setBounds (actions.removeFromLeft (w).reduced (3));
        open.setBounds (actions.reduced (3));
        auto controls = r.removeFromRight (155);
        controls.removeFromLeft (8);
        for (auto* b : { &add, &remove, &up, &down })
        {
            b->setBounds (controls.removeFromTop (46).reduced (2));
            controls.removeFromTop (5);
        }
        list.setBounds (r);
    }

private:
    juce::Label title, info;
    juce::ListBox list;
    juce::Array<juce::File> songs;
    juce::TextButton add, remove, up, down, save, load, open;
    std::unique_ptr<juce::FileChooser> chooser;
    std::function<void (const juce::File&)> onOpenSong;

    void setup (juce::TextButton& b, const juce::String& text, std::function<void()> f)
    {
        b.setButtonText (text);
        b.onClick = std::move (f);
        addAndMakeVisible (b);
    }
    void redraw (int selected)
    {
        list.updateContent();
        if (selected >= 0 && selected < songs.size()) list.selectRow (selected);
        list.repaint();
    }
    void showMessage (const juce::String& text)
    {
        info.setText (text, juce::dontSendNotification);
    }
    void addSong()
    {
        chooser = std::make_unique<juce::FileChooser> ("Add SkizoDroid song", juce::File(), "*.sklive");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<SetlistPanel> (this)] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                const auto f = fc.getResult();
                if (f.existsAsFile() && f.hasFileExtension ("sklive"))
                {
                    safe->songs.add (f);
                    safe->redraw (safe->songs.size() - 1);
                    safe->showMessage ("Song added: " + f.getFileName());
                }
            });
    }
    void changeSelection (int direction)
    {
        const int i = list.getSelectedRow();
        if (! juce::isPositiveAndBelow (i, songs.size())) return;
        if (direction == -2)
        {
            songs.remove (i);
            redraw (juce::jmin (i, songs.size() - 1));
        }
        else
        {
            int j = i + direction;
            if (! juce::isPositiveAndBelow (j, songs.size())) return;
            songs.swap (i, j);
            redraw (j);
        }
    }
    void saveSetlist()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save setlist", juce::File(), "*.skset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<SetlistPanel> (this)] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                auto f = fc.getResult();
                if (f == juce::File()) return;
                f = f.withFileExtension ("skset");
                juce::XmlElement xml ("SKIZODROID_SETLIST");
                xml.setAttribute ("version", 1);
                for (auto& song : safe->songs)
                {
                    auto* x = xml.createNewChildElement ("SONG");
                    x->setAttribute ("path", song.getFullPathName());
                    x->setAttribute ("relative", song.getRelativePathFrom (f.getParentDirectory()));
                }
                if (! xml.writeTo (f)) safe->showMessage ("Could not save setlist.");
                else safe->showMessage ("Saved: " + f.getFileName());
            });
    }
    void loadSetlist()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open setlist", juce::File(), "*.skset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<SetlistPanel> (this)] (const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                auto xml = juce::XmlDocument::parse (f);
                if (xml == nullptr || ! xml->hasTagName ("SKIZODROID_SETLIST"))
                { safe->showMessage ("Invalid .skset file."); return; }
                juce::Array<juce::File> items;
                for (auto* song : xml->getChildIterator())
                {
                    if (! song->hasTagName ("SONG")) continue;
                    auto relative = f.getParentDirectory().getChildFile (song->getStringAttribute ("relative"));
                    auto absolute = juce::File (song->getStringAttribute ("path"));
                    items.add (relative.existsAsFile() ? relative : absolute);
                }
                safe->songs = std::move (items);
                safe->redraw (safe->songs.isEmpty() ? -1 : 0);
                safe->showMessage ("Loaded " + juce::String (safe->songs.size()) + " songs. Orange entries indicate missing files.");
            });
    }
};
