#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Common/Types.h"
#include "Common/SharedProtocol.h"
#include "Common/LoadTrace.h"
#include "Interprocess/InstanceNode.h"
#include "DSP/Analyzer.h"
#include "DSP/SafeChain.h"
#include "DSP/PlaybookStage.h"
#include "DSP/BusStage.h"
#include "DSP/MasterChain.h"
#include "Analysis/LoudnessMeter.h"
#include "DSP/SolverStage.h"
#include "Brain/MixSolver.h"
#include "Analysis/TrackNameParser.h"
#include "Analysis/InstrumentClassifier.h"
#include "Brain/ProfileManager.h"
#include "Brain/TasteProfile.h"

namespace aimixer {

class AIMixerAudioProcessor : public juce::AudioProcessor,
                              private juce::Timer {
public:
    AIMixerAudioProcessor();
    ~AIMixerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AI Mixer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void updateTrackProperties(const TrackProperties& properties) override;

    // Accessors for UI
    InstanceNode& getInstanceNode() noexcept { return instanceNode; }
    AudioAnalyzer& getAnalyzer() noexcept { return analyzer; }
    SafeChain& getSafeChain() noexcept { return safeChain; }
    PlaybookStage& getPlaybookStage() noexcept { return playbookStage; }
    BusStage& getBusStage() noexcept { return busStage; }
    MasterChain& getMasterChain() noexcept { return masterChain; }
    LoudnessMeter& getLoudnessMeter() noexcept { return loudnessMeter; }
    SolverStage& getSolverStage() noexcept { return solverStage; }
    SolverState& getSolverState() noexcept { return solverState; }
    ProfileManager& getProfileManager() noexcept { return profileManager; }
    const GlobalTasteState& getCurrentTasteState() const noexcept { return currentTasteState; }

    // More/Less of Me & Local FX
    void setMoreLessDb(float db) noexcept { moreLessDb.store(std::clamp(db, -3.0f, 3.0f), std::memory_order_relaxed); }
    float getMoreLessDb() const noexcept { return moreLessDb.load(std::memory_order_relaxed); }

    void setLocalTailEnabled(bool enabled) noexcept { localTailEnabled.store(enabled, std::memory_order_relaxed); }
    bool isLocalTailEnabled() const noexcept { return localTailEnabled.load(std::memory_order_relaxed); }

    void setSendTrimDb(float db) noexcept { sendTrimDb.store(std::clamp(db, -12.0f, 6.0f), std::memory_order_relaxed); }
    float getSendTrimDb() const noexcept { return sendTrimDb.load(std::memory_order_relaxed); }

    bool isGlobalSolverEnabled() const noexcept { return globalSolverEnabled.load(std::memory_order_relaxed); }
    void setGlobalSolverEnabled(bool enabled) noexcept { globalSolverEnabled.store(enabled, std::memory_order_relaxed); }
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    PluginRole getCurrentRole() const noexcept { return currentRole; }
    void setRole(PluginRole newRole);

    InstrumentType getResolvedInstrument() const noexcept { return resolvedInstrument; }
    float getResolvedConfidence() const noexcept { return resolvedConfidence; }
    IdentitySource getIdentitySource() const noexcept { return resolvedSource; }
    const ParseResult& getTrackParseResult() const noexcept { return currentParseResult; }
    const std::string& getTrackName() const noexcept { return trackName; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void timerCallback() override;
    void applyRoleLatency();

    juce::AudioProcessorValueTreeState apvts;
    PluginRole currentRole = PluginRole::Source;
    std::string trackName = "Audio Track";

    InstanceNode instanceNode;
    AudioAnalyzer analyzer;
    SafeChain safeChain;
    PlaybookStage playbookStage;
    BusStage busStage;
    MasterChain masterChain;
    LoudnessMeter loudnessMeter;
    SolverStage solverStage;
    SolverState solverState;
    InstrumentClassifier classifier;
    ProfileManager profileManager;

    GlobalTasteState currentTasteState;

    std::atomic<float> moreLessDb{0.0f};
    std::atomic<bool> localTailEnabled{true};
    std::atomic<float> sendTrimDb{0.0f};

    uint32_t myProposalSeq = 0;
    uint64_t lastProposalSentMs = 0;
    float timeActiveSignalSeconds = 0.0f;

    ParseResult currentParseResult;
    InstrumentType resolvedInstrument = InstrumentType::Unknown;
    float resolvedConfidence = 0.0f;
    IdentitySource resolvedSource = IdentitySource::None;

    std::atomic<bool> isBypassed{false};
    std::atomic<bool> isFrozen{false};
    std::atomic<bool> globalSolverEnabled{true};
    std::atomic<int> processBlockCount{0};
    int procTimerTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AIMixerAudioProcessor)
};

} // namespace aimixer
