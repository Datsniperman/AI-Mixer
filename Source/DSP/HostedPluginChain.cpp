#include "HostedPluginChain.h"
#include "../Common/LoadTrace.h"

namespace aimixer {

// ==============================================================================
// EditorWindow Implementation
// ==============================================================================
HostedPluginSlot::EditorWindow::EditorWindow(const juce::String& title,
                                             juce::AudioProcessorEditor* editor,
                                             HostedPluginSlot& owner)
    : juce::DocumentWindow(title,
                           juce::Desktop::getInstance().getDefaultLookAndFeel()
                               .findColour(juce::ResizableWindow::backgroundColourId),
                           juce::DocumentWindow::closeButton),
      ownerSlot(owner) {
    setUsingNativeTitleBar(true);
    if (editor != nullptr) {
        setContentOwned(editor, true);
    }
    setResizable(false, false);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
}

HostedPluginSlot::EditorWindow::~EditorWindow() {
    clearContentComponent();
}

void HostedPluginSlot::EditorWindow::closeButtonPressed() {
    ownerSlot.closeEditor();
}

// ==============================================================================
// HostedPluginSlot Implementation
// ==============================================================================
HostedPluginSlot::HostedPluginSlot() = default;

HostedPluginSlot::~HostedPluginSlot() {
    closeEditor();
    unloadPlugin();
}

void HostedPluginSlot::prepare(double sampleRate, int samplesPerBlock, int numChannels) {
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    currentNumChannels = numChannels;

    std::lock_guard<std::mutex> lock(loadMutex);
    if (instance != nullptr) {
        auto channelSet = (numChannels == 1) ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        instance->setBusesLayout({ channelSet, channelSet });
        instance->prepareToPlay(sampleRate, samplesPerBlock);
    }
}

void HostedPluginSlot::release() {
    std::lock_guard<std::mutex> lock(loadMutex);
    if (instance != nullptr) {
        instance->releaseResources();
    }
}

void HostedPluginSlot::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    if (bypassed.load(std::memory_order_relaxed)) return;

    // Use try_lock or lock to avoid race conditions with load/unload
    std::unique_lock<std::mutex> lock(loadMutex, std::try_to_lock);
    if (!lock.owns_lock() || instance == nullptr) return;

    const int totalChannels = buffer.getNumChannels();
    const int instIn = instance->getTotalNumInputChannels();
    const int instOut = instance->getTotalNumOutputChannels();

    if (instIn <= 0 || instOut <= 0) return;

    if (totalChannels >= 2 && instIn == 1) {
        // Plugin is mono-in; process in place or through mono slice
        juce::AudioBuffer<float> monoSlice(buffer.getArrayOfWritePointers(), 1, buffer.getNumSamples());
        instance->processBlock(monoSlice, midi);
        // Duplicate mono output to both channels
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
    } else {
        instance->processBlock(buffer, midi);
    }
}

bool HostedPluginSlot::loadPlugin(const juce::PluginDescription& desc, juce::AudioPluginFormatManager& formatManager) {
    closeEditor();

    juce::String errorMessage;
    auto newInst = formatManager.createPluginInstance(desc, currentSampleRate, currentBlockSize, errorMessage);

    if (newInst == nullptr) {
        traceLog("HostedPluginSlot: Failed to create instance of %s: %s",
                 desc.name.toRawUTF8(), errorMessage.toRawUTF8());
        return false;
    }

    auto channelSet = (currentNumChannels == 1) ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    newInst->setBusesLayout({ channelSet, channelSet });
    newInst->prepareToPlay(currentSampleRate, currentBlockSize);

    {
        std::lock_guard<std::mutex> lock(loadMutex);
        instance = std::move(newInst);
        currentDesc = desc;
        bypassed.store(false, std::memory_order_relaxed);
    }

    traceLog("HostedPluginSlot: Successfully loaded '%s' (Latency=%d)",
             desc.name.toRawUTF8(), instance->getLatencySamples());
    return true;
}

void HostedPluginSlot::unloadPlugin() {
    closeEditor();
    std::lock_guard<std::mutex> lock(loadMutex);
    if (instance != nullptr) {
        instance->releaseResources();
        instance.reset();
        currentDesc = juce::PluginDescription();
    }
}

juce::String HostedPluginSlot::getPluginName() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(loadMutex));
    if (instance != nullptr) {
        return instance->getName();
    }
    return {};
}

int HostedPluginSlot::getReportedLatency() const noexcept {
    if (instance != nullptr && !bypassed.load(std::memory_order_relaxed)) {
        return instance->getLatencySamples();
    }
    return 0;
}

void HostedPluginSlot::showEditor() {
    if (activeWindow != nullptr) {
        activeWindow->toFront(true);
        return;
    }

    juce::AudioProcessorEditor* ed = nullptr;
    {
        std::lock_guard<std::mutex> lock(loadMutex);
        if (instance != nullptr && instance->hasEditor()) {
            ed = instance->createEditorIfNeeded();
        }
    }

    if (ed != nullptr) {
        activeWindow = std::make_unique<EditorWindow>(
            "AI Mixer Hosted: " + instance->getName(), ed, *this);
    }
}

void HostedPluginSlot::closeEditor() {
    if (activeWindow != nullptr) {
        activeWindow.reset();
    }
}

bool HostedPluginSlot::isEditorOpen() const {
    return activeWindow != nullptr;
}

juce::PluginDescription HostedPluginSlot::getDescription() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(loadMutex));
    return currentDesc;
}

void HostedPluginSlot::getState(juce::MemoryBlock& destData) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(loadMutex));
    if (instance != nullptr) {
        instance->getStateInformation(destData);
    } else {
        destData.reset();
    }
}

void HostedPluginSlot::setState(const void* data, int sizeInBytes) {
    std::lock_guard<std::mutex> lock(loadMutex);
    if (instance != nullptr && data != nullptr && sizeInBytes > 0) {
        instance->setStateInformation(data, sizeInBytes);
    }
}

// ==============================================================================
// HostedPluginChain Implementation
// ==============================================================================
HostedPluginChain::HostedPluginChain() {
    formatManager.addFormat(new juce::VST3PluginFormat());
    for (size_t i = 0; i < MAX_CHAIN_SLOTS; ++i) {
        slots.push_back(std::make_unique<HostedPluginSlot>());
    }
}

HostedPluginChain::~HostedPluginChain() {
    clearAllSlots();
}

void HostedPluginChain::prepare(double sampleRate, int samplesPerBlock, int numChannels) {
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    currentNumChannels = numChannels;

    for (auto& s : slots) {
        if (s) s->prepare(sampleRate, samplesPerBlock, numChannels);
    }
}

void HostedPluginChain::release() {
    for (auto& s : slots) {
        if (s) s->release();
    }
}

void HostedPluginChain::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    if (masterBypass.load(std::memory_order_relaxed)) return;

    for (auto& slot : slots) {
        if (slot && slot->isLoaded() && !slot->isBypassed()) {
            slot->process(buffer, midi);
        }
    }
}

bool HostedPluginChain::loadSlot(size_t slotIndex, const juce::PluginDescription& desc) {
    if (slotIndex >= slots.size()) return false;
    return slots[slotIndex]->loadPlugin(desc, formatManager);
}

void HostedPluginChain::clearSlot(size_t slotIndex) {
    if (slotIndex < slots.size()) {
        slots[slotIndex]->unloadPlugin();
    }
}

void HostedPluginChain::clearAllSlots() {
    for (auto& s : slots) {
        if (s) s->unloadPlugin();
    }
}

HostedPluginSlot* HostedPluginChain::getSlot(size_t slotIndex) {
    if (slotIndex < slots.size()) return slots[slotIndex].get();
    return nullptr;
}

const HostedPluginSlot* HostedPluginChain::getSlot(size_t slotIndex) const {
    if (slotIndex < slots.size()) return slots[slotIndex].get();
    return nullptr;
}

int HostedPluginChain::getTotalLatencySamples() const noexcept {
    if (masterBypass.load(std::memory_order_relaxed)) return 0;
    int total = 0;
    for (const auto& s : slots) {
        if (s) total += s->getReportedLatency();
    }
    return total;
}

bool HostedPluginChain::hasAnyLoaded() const noexcept {
    for (const auto& s : slots) {
        if (s && s->isLoaded()) return true;
    }
    return false;
}

void HostedPluginChain::getState(juce::MemoryBlock& destData) const {
    juce::MemoryOutputStream stream(destData, false);
    stream.writeString("AIMX_CHAIN_V1");
    stream.writeInt(static_cast<int>(slots.size()));

    for (size_t i = 0; i < slots.size(); ++i) {
        const auto* slot = slots[i].get();
        if (slot && slot->isLoaded()) {
            stream.writeBool(true);
            stream.writeBool(slot->isBypassed());
            
            auto desc = slot->getDescription();
            std::unique_ptr<juce::XmlElement> xml(desc.createXml());
            if (xml != nullptr) {
                stream.writeString(xml->toString());
            } else {
                stream.writeString({});
            }

            juce::MemoryBlock slotState;
            slot->getState(slotState);
            stream.writeInt(static_cast<int>(slotState.getSize()));
            if (slotState.getSize() > 0) {
                stream.write(slotState.getData(), slotState.getSize());
            }
        } else {
            stream.writeBool(false);
        }
    }
}

void HostedPluginChain::setState(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) return;

    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    juce::String header = stream.readString();
    if (header != "AIMX_CHAIN_V1") return;

    int numSlotsInState = stream.readInt();
    for (int i = 0; i < numSlotsInState; ++i) {
        bool isLoaded = stream.readBool();
        if (isLoaded) {
            bool bypassed = stream.readBool();
            juce::String descXmlString = stream.readString();
            int stateSize = stream.readInt();
            juce::MemoryBlock pluginState;
            if (stateSize > 0) {
                stream.readIntoMemoryBlock(pluginState, stateSize);
            }

            if (static_cast<size_t>(i) < slots.size() && descXmlString.isNotEmpty()) {
                auto xml = juce::parseXML(descXmlString);
                if (xml != nullptr) {
                    juce::PluginDescription desc;
                    if (desc.loadFromXml(*xml)) {
                        if (loadSlot(static_cast<size_t>(i), desc)) {
                            slots[static_cast<size_t>(i)]->setBypassed(bypassed);
                            if (pluginState.getSize() > 0) {
                                slots[static_cast<size_t>(i)]->setState(
                                    pluginState.getData(), static_cast<int>(pluginState.getSize()));
                            }
                        }
                    }
                }
            }
        } else {
            if (static_cast<size_t>(i) < slots.size()) {
                clearSlot(static_cast<size_t>(i));
            }
        }
    }
}

} // namespace aimixer
