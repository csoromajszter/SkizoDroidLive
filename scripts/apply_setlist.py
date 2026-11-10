#!/usr/bin/env python3
"""Apply V0.9.8 Setlist UI integration during CI build.
Fails closed if source layout changed; leaves the audio engine untouched.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
hp = root / 'Source' / 'MainComponent.h'
cp = root / 'Source' / 'MainComponent.cpp'
h = hp.read_text(encoding='utf-8')
c = cp.read_text(encoding='utf-8')


def replace_once(text, anchor, repl, name):
    occurrences = text.count(anchor)
    if occurrences != 1:
        raise RuntimeError(f'{name}: expected anchor once, found {occurrences}: {anchor[:80]}')
    return text.replace(anchor, repl, 1)


if 'setlistModeButton' in h or 'setlistModeButton' in c:
    print('Setlist integration already present; nothing changed.')
    raise SystemExit(0)

h = replace_once(h,
    '    juce::TextButton songModeButton { "SONG" };',
    '    juce::TextButton songModeButton { "SONG" };\n    juce::TextButton setlistModeButton { "SETLIST" };',
    'MainComponent.h controls')

c = replace_once(c,
    '#include "MainComponent.h"',
    '#include "MainComponent.h"\n#include "SetlistPanel.h"',
    'MainComponent.cpp includes')

c = replace_once(c,
    '    songModeButton.onClick = [this] { setPerformanceMode (true); };',
    '''    songModeButton.onClick = [this] { setPerformanceMode (true); };
    // Make the SafePointer before entering the callback, then copy it into
    // the nested onOpenSong callback. This avoids capturing the wrong `this`.
    setlistModeButton.onClick = [safeMain = juce::Component::SafePointer<MainComponent> (this)]
    {
        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "SkizoDroid Live - SETLIST MODE";
        options.dialogBackgroundColour = juce::Colour::fromRGB (22, 22, 26);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.content.setOwned (new SetlistPanel (
            [safeMain] (const juce::File& file)
            {
                if (safeMain == nullptr || ! file.existsAsFile()) return;
                // V0.9.8 switches songs only after stopping playback.
                safeMain->stopAll();
                safeMain->preloadPool.removeAllJobs (true, 10000);
                safeMain->clearAudioBanks();
                if (! safeMain->unpackProjectToWorkingFolder (file))
                {
                    safeMain->statusLabel.setText ("COULD NOT OPEN SETLIST SONG", juce::dontSendNotification);
                    return;
                }
                safeMain->projectFile = file;
                safeMain->rememberDirectory (file);
                safeMain->updateProjectLabel();
                safeMain->updateStatus();
            }));
        options.launchAsync();
    };''',
    'MainComponent.cpp button callback')

c = replace_once(c,
    '                           &playButton, &stopButton, &patternModeButton, &songModeButton })',
    '                           &playButton, &stopButton, &patternModeButton, &songModeButton, &setlistModeButton })',
    'MainComponent.cpp visible buttons')

c = replace_once(c,
    '    songModeButton.setBounds (menuRow.removeFromLeft (70).reduced (2));',
    '    songModeButton.setBounds (menuRow.removeFromLeft (70).reduced (2));\n    setlistModeButton.setBounds (menuRow.removeFromLeft (88).reduced (2));',
    'MainComponent.cpp layout')

hp.write_text(h, encoding='utf-8')
cp.write_text(c, encoding='utf-8')
print('V0.9.8 Setlist UI integrated: Source/MainComponent.h, Source/MainComponent.cpp')
