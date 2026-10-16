#pragma once

#include <juce_core/juce_core.h>

#include <functional>

namespace cratedig
{

/**
    Finds and runs the user's own yt-dlp install. CrateDig does not ship yt-dlp.

    Plugins do not inherit the shell PATH, so the usual install folders
    (Homebrew, pipx, ~/.local/bin, WinGet...) are searched explicitly.
*/
class YtDlp
{
public:
    struct Result
    {
        int exitCode = -1;
        juce::String output;   // stdout and stderr, merged
        bool timedOut = false;
        bool cancelled = false;

        bool succeeded() const { return exitCode == 0 && ! timedOut && ! cancelled; }
    };

    /** `customPath` (chosen by the user) wins when it points at an existing file. */
    explicit YtDlp (const juce::File& customPath = {});

    bool isAvailable() const { return executable.existsAsFile(); }
    juce::File getExecutable() const { return executable; }
    juce::File getFfmpeg() const { return ffmpeg; }

    /** Runs yt-dlp with `args` (one argv entry each, no shell). */
    Result run (const juce::StringArray& args, int timeoutMs, const std::function<bool()>& shouldCancel = {}) const;

    /** Folders searched for yt-dlp, ffmpeg and deno, in priority order. */
    static juce::Array<juce::File> searchFolders();

    /** First executable called `name` (".exe" added on Windows) in `folders`. */
    static juce::File findTool (const juce::String& name, const juce::Array<juce::File>& folders);

    static juce::String installHint();

private:
    juce::File executable, ffmpeg, deno;
};

} // namespace cratedig
