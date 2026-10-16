#include "YtDlp.h"

namespace cratedig
{

YtDlp::YtDlp (const juce::File& customPath)
{
    auto folders = searchFolders();

    if (customPath.existsAsFile())
    {
        executable = customPath;
        folders.insert (0, customPath.getParentDirectory());
    }
    else
    {
        executable = findTool ("yt-dlp", folders);
    }

    ffmpeg = findTool ("ffmpeg", folders);
    deno = findTool ("deno", folders);
}

juce::Array<juce::File> YtDlp::searchFolders()
{
    juce::Array<juce::File> folders;
    auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    auto add = [&folders] (const juce::File& f)
    {
        if (f.isDirectory() && ! folders.contains (f))
            folders.add (f);
    };

#if JUCE_WINDOWS
    auto localAppData = juce::File::getSpecialLocation (juce::File::windowsLocalAppData);
    add (localAppData.getChildFile ("Microsoft/WinGet/Links"));
    add (home.getChildFile ("scoop/shims"));
    add (juce::File ("C:/ProgramData/chocolatey/bin"));
#else
    add (juce::File ("/opt/homebrew/bin"));
    add (juce::File ("/usr/local/bin"));
    add (home.getChildFile (".local/bin"));
    add (home.getChildFile ("bin"));
    add (home.getChildFile (".deno/bin"));
    add (juce::File ("/opt/local/bin"));
    add (juce::File ("/usr/bin"));
#endif

   #if JUCE_WINDOWS
    const auto* pathSeparator = ";";
   #else
    const auto* pathSeparator = ":";
   #endif

    for (auto& entry : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("PATH", {}), pathSeparator, {}))
        if (entry.isNotEmpty() && juce::File::isAbsolutePath (entry))
            add (juce::File (entry));

    return folders;
}

juce::File YtDlp::findTool (const juce::String& name, const juce::Array<juce::File>& folders)
{
#if JUCE_WINDOWS
    auto fileName = name + ".exe";
#else
    auto fileName = name;
#endif

    for (auto& folder : folders)
        if (auto candidate = folder.getChildFile (fileName); candidate.existsAsFile())
            return candidate;

    return {};
}

juce::String YtDlp::installHint()
{
#if JUCE_WINDOWS
    return "yt-dlp not found - install it (winget install yt-dlp.yt-dlp) or click \"yt-dlp...\" to locate it";
#else
    return "yt-dlp not found - install it (brew install yt-dlp) or click \"yt-dlp...\" to locate it";
#endif
}

YtDlp::Result YtDlp::run (const juce::StringArray& args, int timeoutMs, const std::function<bool()>& shouldCancel) const
{
    Result result;

    if (! isAvailable())
    {
        result.output = installHint();
        return result;
    }

    juce::StringArray command { executable.getFullPathName() };

    // yt-dlp looks for these on PATH, which plugins usually don't have.
    if (deno.existsAsFile())
        command.addArray ({ "--js-runtimes", "deno:" + deno.getFullPathName() });

    if (ffmpeg.existsAsFile())
        command.addArray ({ "--ffmpeg-location", ffmpeg.getFullPathName() });

    command.addArray (args);

    juce::ChildProcess process;

    if (! process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        result.output = "Could not start " + executable.getFullPathName();
        return result;
    }

    // Output is kept small (see youtube::buildSearchArgs), so it is read once the process exits;
    // polling lets a cancel or timeout kill a stuck process instead of blocking on the pipe.
    auto started = juce::Time::getMillisecondCounter();

    while (process.isRunning())
    {
        if (shouldCancel && shouldCancel())
        {
            result.cancelled = true;
            process.kill();
            return result;
        }

        if ((int) (juce::Time::getMillisecondCounter() - started) > timeoutMs)
        {
            result.timedOut = true;
            process.kill();
            return result;
        }

        juce::Thread::sleep (50);
    }

    result.output = process.readAllProcessOutput();
    result.exitCode = (int) process.getExitCode();
    return result;
}

} // namespace cratedig
