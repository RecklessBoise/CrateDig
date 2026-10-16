#include "PluginProcessor.h"

#include "Core/Exporter.h"
#include "PluginEditor.h"

namespace
{
    constexpr int requestFromSelection = -2;
    constexpr int noRequest = -1;
} // namespace

CrateDigProcessor::CrateDigProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "CrateDig", createLayout())
{
    gainParam = parameters.getRawParameterValue ("gain");
    loopParam = parameters.getRawParameterValue ("loop");

    digger.onStatus = [this] (const juce::String& text) { setStatus (text); };
    digger.onError = [this] (const juce::String& text) { setStatus (text, true); };
    digger.onLoaded = [this] (const cratedig::LoadedSample& sample) { handleLoaded (sample); };
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
    session.setProperty ("genre", filters.genre, nullptr);
    session.setProperty ("decade", filters.decade, nullptr);
    session.setProperty ("publicDomainOnly", filters.publicDomainOnly, nullptr);
    session.setProperty ("selectionStart", selectionStart.load(), nullptr);
    session.setProperty ("selectionEnd", selectionEnd.load(), nullptr);

    if (current.info.isValid())
        session.appendChild (current.info.toValueTree().setProperty ("current", true, nullptr), nullptr);

    juce::ValueTree historyTree ("History");

    for (auto& item : history)
        historyTree.appendChild (item.toValueTree(), nullptr);

    session.appendChild (historyTree, nullptr);

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

    const juce::ScopedLock sl (stateLock);

    editorSize = { juce::jlimit (CrateDigEditor::minWidth, CrateDigEditor::maxWidth, (int) session.getProperty ("width", editorSize.x)),
                   juce::jlimit (CrateDigEditor::minHeight, CrateDigEditor::maxHeight, (int) session.getProperty ("height", editorSize.y)) };
    filters.genre = session.getProperty ("genre").toString();
    filters.decade = (int) session.getProperty ("decade", 0);
    filters.publicDomainOnly = (bool) session.getProperty ("publicDomainOnly", false);

    history.clear();

    for (auto item : session.getChildWithName ("History"))
        if (auto info = cratedig::SampleInfo::fromValueTree (item); info.isValid())
            history.push_back (info);

    auto savedSelection = juce::Range<int> ((int) session.getProperty ("selectionStart", 0),
                                            (int) session.getProperty ("selectionEnd", 0));

    for (auto child : session)
    {
        if (child.hasType ("Sample") && (bool) child.getProperty ("current", false))
        {
            auto info = cratedig::SampleInfo::fromValueTree (child);

            if (info.identifier != current.info.identifier)
            {
                setSelection (savedSelection);
                digger.load (info);
            }
        }
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

void CrateDigProcessor::dig()
{
    if (digger.isBusy())
        return;

    setSelection ({});
    digger.dig (getFilters());
    sendChangeMessage();
}

void CrateDigProcessor::loadFromHistory (int index)
{
    if (digger.isBusy() || ! juce::isPositiveAndBelow (index, (int) history.size()))
        return;

    setSelection ({});
    digger.load (history[(size_t) index]);
    sendChangeMessage();
}

void CrateDigProcessor::handleLoaded (const cratedig::LoadedSample& sample)
{
    auto previous = preview; // released after the lock, on this (message) thread

    {
        const juce::SpinLock::ScopedLockType lock (previewLock);
        preview = sample.audio;
        previewSampleRate = sample.sampleRate;
        playing.store (false);
    }

    previous.reset();

    {
        const juce::ScopedLock sl (stateLock);
        current = sample;
        addToHistory (sample.info);
    }

    // Drop a restored selection that no longer fits.
    if (getSelection().getEnd() > sample.audio->getNumSamples())
        setSelection ({});

    juce::String details;

    if (sample.bpm > 0.0)
        details << " - " << juce::roundToInt (sample.bpm) << " BPM";

    if (sample.key.isNotEmpty())
        details << " - " << sample.key;

    setStatus ("Found it" + details + ". Drag it into your DAW or select a chop.");
    play();
}

void CrateDigProcessor::addToHistory (const cratedig::SampleInfo& info)
{
    history.erase (std::remove_if (history.begin(), history.end(),
                                   [&] (const auto& h) { return h.identifier == info.identifier; }),
                   history.end());

    history.insert (history.begin(), info);

    if ((int) history.size() > maxHistory)
        history.resize ((size_t) maxHistory);
}

cratedig::archive::DigFilters CrateDigProcessor::getFilters() const
{
    const juce::ScopedLock sl (stateLock);
    return filters;
}

void CrateDigProcessor::setFilters (const cratedig::archive::DigFilters& newFilters)
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
    auto total = current.audio != nullptr ? current.audio->getNumSamples() : 0;
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
    if (current.audio == nullptr)
    {
        error = "Dig a record first";
        return {};
    }

    auto baseName = cratedig::exporter::makeBaseName (current.info, current.bpm, current.key);
    return cratedig::exporter::exportWav (*current.audio, current.sampleRate, getEffectiveSelection(),
                                          cratedig::exporter::defaultFolder(), baseName, error);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrateDigProcessor();
}
