#include "SampleInfo.h"

namespace cratedig
{

juce::String SampleInfo::watchUrl() const
{
    return "https://www.youtube.com/watch?v=" + id;
}

juce::String SampleInfo::thumbnailUrl() const
{
    return "https://i.ytimg.com/vi/" + id + "/mqdefault.jpg";
}

juce::ValueTree SampleInfo::toValueTree() const
{
    juce::ValueTree tree ("Sample");
    tree.setProperty ("id", id, nullptr);
    tree.setProperty ("title", title, nullptr);
    tree.setProperty ("channel", channel, nullptr);
    tree.setProperty ("duration", durationSeconds, nullptr);
    tree.setProperty ("views", viewCount, nullptr);
    tree.setProperty ("year", year, nullptr);
    return tree;
}

SampleInfo SampleInfo::fromValueTree (const juce::ValueTree& tree)
{
    SampleInfo info;
    info.id = tree.getProperty ("id").toString();
    info.title = tree.getProperty ("title").toString();
    info.channel = tree.getProperty ("channel").toString();
    info.durationSeconds = (double) tree.getProperty ("duration", 0.0);
    info.viewCount = (juce::int64) tree.getProperty ("views", -1);
    info.year = (int) tree.getProperty ("year", 0);
    return info;
}

} // namespace cratedig
