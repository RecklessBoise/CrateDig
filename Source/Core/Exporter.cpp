#include "Exporter.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace cratedig::exporter
{

juce::String sanitiseFileName (const juce::String& name, int maxLength)
{
    auto cleaned = name.removeCharacters ("\\/:*?\"<>|")
                       .replaceCharacters ("\t\r\n", "   ");

    // Strip control characters.
    juce::String result;

    for (auto c : cleaned)
        if (c >= 32)
            result += c;

    while (result.contains ("  "))
        result = result.replace ("  ", " ");

    result = result.trim().trimCharactersAtStart (".").trim();

    if (result.length() > maxLength)
        result = result.substring (0, maxLength).trimEnd();

    return result.isEmpty() ? juce::String ("sample") : result;
}

juce::String makeBaseName (const SampleInfo& info, double bpm, const juce::String& key)
{
    // YouTube auto-generated channels are called "Artist - Topic".
    auto artist = info.channel.trim();

    if (artist.endsWithIgnoreCase (" - Topic"))
        artist = artist.dropLastCharacters (8).trim();

    auto title = info.title.isNotEmpty() ? info.title : info.id;
    juce::String name;

    // Uploads usually carry "Artist - Song" in the title already; only add the channel when they don't.
    if (artist.isNotEmpty() && ! title.contains (" - ") && ! title.containsIgnoreCase (artist))
        name << artist << " - ";

    name << title;

    if (info.year > 0 && ! title.contains (juce::String (info.year)))
        name << " (" << info.year << ")";

    juce::StringArray tags;

    if (bpm > 0.0)
        tags.add (juce::String (juce::roundToInt (bpm)) + " BPM");

    if (key.isNotEmpty())
        tags.add (key);

    if (! tags.isEmpty())
        name << " [" << tags.joinIntoString (", ") << "]";

    return sanitiseFileName (name);
}

juce::String regionSuffix (juce::Range<int> region, int totalSamples, double sampleRate)
{
    if (sampleRate <= 0.0 || region.isEmpty() || (region.getStart() <= 0 && region.getEnd() >= totalSamples))
        return {};

    auto fmt = [sampleRate] (int samples) { return juce::String (samples / sampleRate, 1) + "s"; };
    return " (chop " + fmt (region.getStart()) + "-" + fmt (region.getEnd()) + ")";
}

juce::File defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("CrateDig");
}

juce::File exportWav (const juce::AudioBuffer<float>& audio,
                      double sampleRate,
                      juce::Range<int> region,
                      const juce::File& folder,
                      const juce::String& baseName,
                      juce::String& error)
{
    error.clear();
    auto total = audio.getNumSamples();
    region = region.getIntersectionWith ({ 0, total });

    if (region.isEmpty())
        region = { 0, total };

    if (total == 0 || audio.getNumChannels() == 0 || sampleRate <= 0.0)
    {
        error = "Nothing to export";
        return {};
    }

    if (auto result = folder.createDirectory(); result.failed())
    {
        error = "Cannot create folder: " + result.getErrorMessage();
        return {};
    }

    auto name = sanitiseFileName (baseName + regionSuffix (region, total, sampleRate), 200);
    auto file = folder.getChildFile (name + ".wav");

    if (file.existsAsFile() && file.getSize() > 44)
        return file;

    auto temp = file.getSiblingFile (file.getFileNameWithoutExtension() + ".tmp");
    temp.deleteFile();

    {
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (temp);

        if (static_cast<juce::FileOutputStream*> (stream.get())->failedToOpen())
        {
            error = "Cannot write " + temp.getFullPathName();
            return {};
        }

        juce::WavAudioFormat wav;
        auto options = juce::AudioFormatWriterOptions{}
                           .withSampleRate (sampleRate)
                           .withNumChannels (audio.getNumChannels())
                           .withBitsPerSample (24);

        auto writer = wav.createWriterFor (stream, options);

        if (writer == nullptr)
        {
            error = "Cannot create WAV writer";
            temp.deleteFile();
            return {};
        }

        if (! writer->writeFromAudioSampleBuffer (audio, region.getStart(), region.getLength()))
        {
            error = "Failed while writing audio";
            writer.reset();
            temp.deleteFile();
            return {};
        }
    }

    if (! temp.moveFileTo (file))
    {
        error = "Cannot move file into place";
        temp.deleteFile();
        return {};
    }

    return file;
}

} // namespace cratedig::exporter
