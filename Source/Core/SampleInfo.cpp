#include "SampleInfo.h"

namespace cratedig
{

juce::String SampleInfo::genre() const
{
    for (auto& s : subjects)
        if (! s.equalsIgnoreCase ("78rpm") && ! s.equalsIgnoreCase ("LP"))
            return s;

    return {};
}

juce::String SampleInfo::pageUrl() const
{
    return "https://archive.org/details/" + percentEncode (identifier);
}

juce::String SampleInfo::downloadUrl() const
{
    return "https://archive.org/download/" + percentEncode (identifier) + "/" + percentEncode (audioFile);
}

juce::ValueTree SampleInfo::toValueTree() const
{
    juce::ValueTree tree ("Sample");
    tree.setProperty ("identifier", identifier, nullptr);
    tree.setProperty ("title", title, nullptr);
    tree.setProperty ("creator", creator, nullptr);
    tree.setProperty ("audioFile", audioFile, nullptr);
    tree.setProperty ("subjects", subjects.joinIntoString ("|"), nullptr);
    tree.setProperty ("year", year, nullptr);
    tree.setProperty ("duration", durationSeconds, nullptr);
    return tree;
}

SampleInfo SampleInfo::fromValueTree (const juce::ValueTree& tree)
{
    SampleInfo info;
    info.identifier = tree.getProperty ("identifier").toString();
    info.title = tree.getProperty ("title").toString();
    info.creator = tree.getProperty ("creator").toString();
    info.audioFile = tree.getProperty ("audioFile").toString();
    info.subjects = juce::StringArray::fromTokens (tree.getProperty ("subjects").toString(), "|", {});
    info.subjects.removeEmptyStrings();
    info.year = (int) tree.getProperty ("year", 0);
    info.durationSeconds = (double) tree.getProperty ("duration", 0.0);
    return info;
}

juce::String percentEncode (const juce::String& text)
{
    juce::MemoryOutputStream out;
    auto utf8 = text.toRawUTF8();

    for (auto* p = utf8; *p != 0; ++p)
    {
        auto c = (unsigned char) *p;

        if (juce::CharacterFunctions::isLetterOrDigit ((char) c) && c < 128)
            out << (char) c;
        else if (c == '-' || c == '_' || c == '.' || c == '~')
            out << (char) c;
        else
            out << '%' << juce::String::toHexString ((int) c).paddedLeft ('0', 2).toUpperCase();
    }

    return out.toString();
}

} // namespace cratedig
