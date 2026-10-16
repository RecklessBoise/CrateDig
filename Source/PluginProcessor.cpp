#include "PluginProcessor.h"

#include "Core/Exporter.h"
#include "PluginEditor.h"

using namespace cratedig;

namespace
{
    constexpr int requestFromSelection = -2;
    constexpr int noRequest = -1;

    /** Machine-wide settings shared by every CrateDig instance (currently the yt-dlp location). */
    std::unique_ptr<juce::PropertiesFile> openGlobalSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "CrateDig";
        options.filenameSuffix = ".settings";
        options.folderName = "CrateDig";
        options.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (options);
    }

    void saveList (juce::ValueTree& parent, const juce::String& name, const std::vector<SampleInfo>& list)
    {
        juce::ValueTree tree (name);

        for (auto& item : list)
            tree.appendChild (item.toValueTree(), nullptr);

        parent.appendChild (tree, nullptr);
    }

    std::vector<SampleInfo> loadList (const juce::ValueTree& parent, const juce::String& name)
    {
        std::vector<SampleInfo> list;

        for (auto item : parent.getChildWithName (name))
            if (auto info = SampleInfo::fromValueTree (item); youtube::isValidVideoId (info.id))
                list.push_back (info);

        return list;
    }
} // namespace

CrateDigProcessor::CrateDigProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "CrateDig", createLayout())
{
    gainParam = parameters.getRawParameterValue ("gain");
    loopParam = parameters.getRawParameterValue ("loop");

    if (auto settings = openGlobalSettings())
        digger.setCustomYtDlp (juce::File (settings->getValue ("ytdlpPath")));

    digger.onStatus = [this] (const juce::String& text) { setStatus (text); };
    digger.onError = [this] (const juce::String& text)
    {
        downloadWhenLoaded = false;
        setStatus (text, true);
    };
    digger.onResults = [this] (const juce::String& query, const std::vector<SampleInfo>& found) { handleResults (query, found); };
    digger.onLoaded = [this] (const LoadedSample& sample) { handleLoaded (sample); };

    status = digger.isYtDlpAvailable() ? "Hit DIG for a random record, or type a search"
                                       : YtDlp::installHint();
    statusIsError = ! digger.isYtDlpAvailable();
}

CrateDigProcessor::~CrateDigProcessor()
{
    digger.cancel();
}

juce::AudioProcessorValueTreeState::ParameterLayout CrateDigProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gain", 1 }, "Preview Gain",
                                                             juce::NormalisableRange<float> (-48.0f, 6.0f, 0.1f), -6.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "loop", 1 }, "Loop", true));
    return layout;
}

//==============================================================================
void CrateDigProcessor::prepareToPlay (double sampleRate, int)
{
    hostSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    gainSmoothed.reset (hostSampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));
}

bool CrateDigProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void CrateDigProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto metadata : midi)
        if (metadata.getMessage().isNoteOn())
            startRequest.store (requestFromSelection);

    midi.clear();

    const juce::SpinLock::ScopedTryLockType lock (previewLock);

    if (! lock.isLocked() || preview == nullptr || preview->getNumSamples() < 2 || preview->getNumChannels() == 0)
        return;

    auto& source = *preview;
    auto sourceLength = source.getNumSamples();
    auto selStart = juce::jlimit (0, sourceLength, selectionStart.load());
    auto selEnd = juce::jlimit (0, sourceLength, selectionEnd.load());

    if (selEnd - selStart < 2)
    {
        selStart = 0;
        selEnd = sourceLength;
    }

    if (stopRequest.exchange (false))
        playing.store (false);

    if (auto request = startRequest.exchange (noRequest); request != noRequest)
    {
        readPosition = request == requestFromSelection ? (double) selStart : (double) juce::jlimit (0, sourceLength - 1, request);
        playing.store (true);
    }

    if (! playing.load())
        return;

    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load(), -48.0f));
    auto looping = loopParam->load() > 0.5f;
    auto ratio = previewSampleRate / hostSampleRate;
    auto numOut = buffer.getNumChannels();
    auto numSrc = source.getNumChannels();

    // A playhead placed after the selection (by clicking) plays to the end of the record.
    auto end = readPosition >= selEnd ? sourceLength : selEnd;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (readPosition >= end - 1)
        {
            if (looping)
            {
                readPosition = selStart + (readPosition - (end - 1));
                end = selEnd;
            }
            else
            {
                playing.store (false);
                break;
            }
        }

        auto index = (int) readPosition;
        auto frac = (float) (readPosition - index);
        auto gain = gainSmoothed.getNextValue();

        for (int ch = 0; ch < numOut; ++ch)
        {
            auto* src = source.getReadPointer (juce::jmin (ch, numSrc - 1));
            auto sample = src[index] + frac * (src[index + 1] - src[index]);

            if (numOut == 1 && numSrc > 1)
                sample = 0.5f * (sample + source.getSample (1, index));

            buffer.setSample (ch, i, sample * gain);
        }

        readPosition += ratio;
    }

    playhead.store (readPosition);
}

//==============================================================================
juce::AudioProcessorEditor* CrateDigProcessor::createEditor()
{
    return new CrateDigEditor (*this);
}

void CrateDigProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    const juce::ScopedLock sl (stateLock);

    juce::ValueTree session ("Session");
    session.setProperty ("width", editorSize.x, nullptr);
    session.setProperty ("height", editorSize.y, nullptr);
    session.setProperty ("text", filters.text, nullptr);
    session.setProperty ("genre", filters.genre, nullptr);
    session.setProperty ("decade", filters.decade, nullptr);
    session.setProperty ("query", lastQuery, nullptr);
    session.setProperty ("selectionStart", selectionStart.load(), nullptr);
    session.setProperty ("selectionEnd", selectionEnd.load(), nullptr);

    if (selected.isValid())
        session.appendChild (selected.toValueTree().setProperty ("selected", true, nullptr), nullptr);

    saveList (session, "Results", results);
    saveList (session, "History", history);

    state.removeChild (state.getChildWithName ("Session"), nullptr);
    state.appendChild (session, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CrateDigProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    auto session = state.getChildWithName ("Session");
    state.removeChild (session, nullptr);
    parameters.replaceState (state);

    if (! session.isValid())
        return;

    SampleInfo restoredSelection;

    {
        const juce::ScopedLock sl (stateLock);

        editorSize = { juce::jlimit (CrateDigEditor::minWidth, CrateDigEditor::maxWidth, (int) session.getProperty ("width", editorSize.x)),
                       juce::jlimit (CrateDigEditor::minHeight, CrateDigEditor::maxHeight, (int) session.getProperty ("height", editorSize.y)) };
        filters.text = session.getProperty ("text").toString();
        filters.genre = session.getProperty ("genre").toString();
        filters.decade = (int) session.getProperty ("decade", 0);
        lastQuery = session.getProperty ("query").toString();
        results = loadList (session, "Results");
        history = loadList (session, "History");

        for (auto child : session)
            if (child.hasType ("Sample") && (bool) child.getProperty ("selected", false))
                restoredSelection = SampleInfo::fromValueTree (child);

        selected = restoredSelection;
    }

    // Reload the preview only if it is still in the local cache: restoring a project never hits the network.
    if (restoredSelection.isValid() && restoredSelection.id != loaded.info.id && Digger::isCached (restoredSelection))
    {
        setSelection ({ (int) session.getProperty ("selectionStart", 0), (int) session.getProperty ("selectionEnd", 0) });
        digger.fetch (restoredSelection);
    }

    sendChangeMessage();
}

//==============================================================================
void CrateDigProcessor::setStatus (const juce::String& text, bool isError)
{
    status = text;
    statusIsError = isError;
    sendChangeMessage();
}

void CrateDigProcessor::setCustomYtDlp (const juce::File& file)
{
    digger.setCustomYtDlp (file);
    saveGlobalSettings();

    if (digger.isYtDlpAvailable())
        setStatus ("yt-dlp ready: " + YtDlp (file).getExecutable().getFullPathName());
    else
        setStatus (YtDlp::installHint(), true);
}

void CrateDigProcessor::saveGlobalSettings()
{
    if (auto settings = openGlobalSettings())
    {
        settings->setValue ("ytdlpPath", digger.getCustomYtDlp().getFullPathName());
        settings->saveIfNeeded();
    }
}

void CrateDigProcessor::search()
{
    if (digger.isBusy())
        return;

    digger.search (getFilters());
    sendChangeMessage();
}

void CrateDigProcessor::handleResults (const juce::String& query, const std::vector<SampleInfo>& found)
{
    {
        const juce::ScopedLock sl (stateLock);
        results = found;
        lastQuery = query;
    }

    select (found.front());
    setStatus (juce::String ((int) found.size()) + " results for \"" + query + "\" - press Listen to hear one");
}

void CrateDigProcessor::selectResult (int index)
{
    if (juce::isPositiveAndBelow (index, (int) results.size()))
        select (results[(size_t) index]);
}

void CrateDigProcessor::selectHistory (int index)
{
    if (juce::isPositiveAndBelow (index, (int) history.size()))
        select (history[(size_t) index]);
}

void CrateDigProcessor::select (const SampleInfo& info)
{
    if (info.id == selected.id)
        return;

    {
        const juce::ScopedLock sl (stateLock);
        selected = info;
    }

    downloadWhenLoaded = false;
    stop();
    setSelection ({});
    sendChangeMessage();
}

bool CrateDigProcessor::isSelectedLoaded() const
{
    return loaded.audio != nullptr && selected.isValid() && loaded.info.id == selected.id;
}

void CrateDigProcessor::listen()
{
    if (isSelectedLoaded())
    {
        play();
        return;
    }

    if (selected.isValid() && ! digger.isBusy())
    {
        digger.fetch (selected);
        sendChangeMessage();
    }
}

void CrateDigProcessor::download()
{
    if (isSelectedLoaded())
    {
        juce::String error;
        auto file = exportSelection (error);

        if (file.existsAsFile())
        {
            setStatus ("Downloaded " + file.getFileName());
            file.revealToUser();
        }
        else
        {
            setStatus (error.isNotEmpty() ? error : juce::String ("Export failed"), true);
        }

        return;
    }

    if (selected.isValid() && ! digger.isBusy())
    {
        downloadWhenLoaded = true;
        digger.fetch (selected);
        sendChangeMessage();
    }
}

void CrateDigProcessor::handleLoaded (const LoadedSample& sample)
{
    auto previous = preview; // released after the lock, on this (message) thread

    {
        const juce::SpinLock::ScopedLockType lock (previewLock);
        preview = sample.audio;
        previewSampleRate = sample.sampleRate;
        playing.store (false);
    }

    previous.reset();
    loaded = sample;

    {
        const juce::ScopedLock sl (stateLock);
        addToHistory (sample.info);
    }

    // Drop a restored selection that no longer fits.
    if (getSelection().getEnd() > sample.audio->getNumSamples())
        setSelection ({});

    // The user may have picked another card while this one was loading.
    if (sample.info.id != selected.id)
    {
        downloadWhenLoaded = false;
        setStatus ("Preview ready for \"" + sample.info.title + "\"");
        return;
    }

    if (std::exchange (downloadWhenLoaded, false))
    {
        download();
        return;
    }

    juce::String details;

    if (sample.bpm > 0.0)
        details << " - " << juce::roundToInt (sample.bpm) << " BPM";

    if (sample.key.isNotEmpty())
        details << " - " << sample.key;

    setStatus ("Previewing" + details + ". Download or drag it to keep it.");
    play();
}

void CrateDigProcessor::addToHistory (const SampleInfo& info)
{
    history.erase (std::remove_if (history.begin(), history.end(),
                                   [&] (const auto& h) { return h.id == info.id; }),
                   history.end());

    history.insert (history.begin(), info);

    if ((int) history.size() > maxHistory)
        history.resize ((size_t) maxHistory);
}

youtube::DigFilters CrateDigProcessor::getFilters() const
{
    const juce::ScopedLock sl (stateLock);
    return filters;
}

void CrateDigProcessor::setFilters (const youtube::DigFilters& newFilters)
{
    const juce::ScopedLock sl (stateLock);
    filters = newFilters;
}

juce::Point<int> CrateDigProcessor::getEditorSize() const
{
    const juce::ScopedLock sl (stateLock);
    return editorSize;
}

void CrateDigProcessor::setEditorSize (juce::Point<int> size)
{
    const juce::ScopedLock sl (stateLock);
    editorSize = size;
}

juce::Range<int> CrateDigProcessor::getSelection() const
{
    return { selectionStart.load(), juce::jmax (selectionStart.load(), selectionEnd.load()) };
}

void CrateDigProcessor::setSelection (juce::Range<int> range)
{
    selectionStart.store (range.getStart());
    selectionEnd.store (range.getEnd());
}

juce::Range<int> CrateDigProcessor::getEffectiveSelection() const
{
    auto total = loaded.audio != nullptr ? loaded.audio->getNumSamples() : 0;
    auto sel = getSelection().getIntersectionWith ({ 0, total });
    return sel.getLength() < 2 ? juce::Range<int> (0, total) : sel;
}

void CrateDigProcessor::play()
{
    startRequest.store (requestFromSelection);
}

void CrateDigProcessor::playFrom (int sample)
{
    startRequest.store (juce::jmax (0, sample));
}

void CrateDigProcessor::stop()
{
    startRequest.store (noRequest);
    stopRequest.store (true);
}

juce::File CrateDigProcessor::exportSelection (juce::String& error)
{
    if (! isSelectedLoaded())
    {
        error = "Listen to the track first";
        return {};
    }

    auto baseName = exporter::makeBaseName (loaded.info, loaded.bpm, loaded.key);
    return exporter::exportWav (*loaded.audio, loaded.sampleRate, getEffectiveSelection(),
                                exporter::defaultFolder(), baseName, error);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrateDigProcessor();
}
