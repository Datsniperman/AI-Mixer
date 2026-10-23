#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Common/Types.h"
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>

namespace aimixer {

// ==============================================================================
// Single Hosted Plugin Slot
// ==============================================================================
class HostedPluginSlot {
public:
    HostedPluginSlot();
    ~HostedPluginSlot();

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void release();
    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    bool loadPlugin(const juce::PluginDescription& desc, juce::AudioPluginFormatManager& formatManager);
    void unloadPlugin();

        bool isLoaded() const noexcept { return instance != nullptr; }
    juce::AudioPluginInstance* getInstance() noexcept { return instance.get(); }
    juce::String getPluginName() const;
    int getReportedLatency() const noexcept;

    void setBypassed(bool b) noexcept { bypassed.store(b, std::memory_order_relaxed); }
    bool isBypassed() const noexcept { return bypassed.load(std::memory_order_relaxed); }

    void showEditor();
    void closeEditor();
    bool isEditorOpen() const;

    juce::PluginDescription getDescription() const;

    // State persistence
    void getState(juce::MemoryBlock& destData) const;
    void setState(const void* data, int sizeInBytes);

private:
    class EditorWindow : public juce::DocumentWindow {
    public:
        EditorWindow(const juce::String& title, juce::AudioProcessorEditor* editor, HostedPluginSlot& owner);
        ~EditorWindow() override;
        void closeButtonPressed() override;
    private:
        HostedPluginSlot& ownerSlot;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorWindow)
    };

    friend class EditorWindow;

    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::unique_ptr<EditorWindow> activeWindow;
    juce::PluginDescription currentDesc;

    std::atomic<bool> bypassed{false};
    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;
    int currentNumChannels = 2;

    std::mutex loadMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HostedPluginSlot)
};

// ==============================================================================
// Hosted Plugin Chain (Serial Multi-Slot Rack: 1 -> 2 -> 3)
// ==============================================================================
class HostedPluginChain {
public:
    static constexpr size_t MAX_CHAIN_SLOTS = 4;

    HostedPluginChain();
    ~HostedPluginChain();

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void release();
    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    bool loadSlot(size_t slotIndex, const juce::PluginDescription& desc);
    void clearSlot(size_t slotIndex);
    void clearAllSlots();

    size_t getNumSlots() const noexcept { return MAX_CHAIN_SLOTS; }
    HostedPluginSlot* getSlot(size_t slotIndex);
    const HostedPluginSlot* getSlot(size_t slotIndex) const;

    int getTotalLatencySamples() const noexcept;
    bool hasAnyLoaded() const noexcept;

    void setMasterBypass(bool b) noexcept { masterBypass.store(b, std::memory_order_relaxed); }
    bool isMasterBypassed() const noexcept { return masterBypass.load(std::memory_order_relaxed); }

    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }

    // Serialization for DAW project save/load
    void getState(juce::MemoryBlock& destData) const;
    void setState(const void* data, int sizeInBytes);

private:
    juce::AudioPluginFormatManager formatManager;
    std::vector<std::unique_ptr<HostedPluginSlot>> slots;
    std::atomic<bool> masterBypass{false};

    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;
    int currentNumChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HostedPluginChain)
};

} // namespace aimixer

