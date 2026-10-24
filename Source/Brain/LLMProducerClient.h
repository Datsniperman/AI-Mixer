#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Common/Types.h"
#include "../Analysis/InstrumentClassifier.h"
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>

namespace aimixer {

// ==============================================================================
// Diagnostic data measured for each track in the session
// ==============================================================================
struct SessionTrackDiagnosis {
    uint32_t instanceId = 0;
    juce::String trackName;
    InstrumentType identifiedInstrument = InstrumentType::Unknown;
    SideHint sideHint = SideHint::Centre;
    bool isStereo = true;
    
    // Measured acoustic hearing features
    float rmsDb = -18.0f;
    float peakDb = -6.0f;
    float crestFactorDb = 12.0f;
    float fundamentalHz = 0.0f;
    float spectralCentroidHz = 1000.0f;
    float spectralTiltDbPerOct = -4.5f;
    float detectedResonanceHz = 0.0f;
    float resonanceProminenceDb = 0.0f;
    float resonanceQ = 2.0f;
    float sibilanceRatio = 0.0f;
    float sibilanceCentroidHz = 7000.0f;
    float boxinessRatio = 0.0f;
    float subEnergyRatio = 0.0f;
    float harmonicRichness = 0.0f;
    float attackSharpness = 0.0f;
};

// ==============================================================================
// Discrete instruction for a single hosted plugin slot in the track rack
// ==============================================================================
struct ProducedSlotDirective {
    juce::String wavesPluginName; // E.g. "SSLChannel Stereo", "CLA-76 Stereo", "F6-RTA Stereo", "RVox Stereo", "RBass Stereo"
    bool bypassed = false;

    // Derived acoustic math settings
    float targetThresholdDb = -18.0f;
    float targetRatio = 4.0f;
    float targetAttackMs = 20.0f;
    float targetReleaseMs = 200.0f;
    float eqLowFreqHz = 60.0f;
    float eqLowGainDb = 0.0f;
    float eqMidFreqHz = 1000.0f;
    float eqMidGainDb = 0.0f;
    float eqMidQ = 1.0f;
    float eqHighFreqHz = 8000.0f;
    float eqHighGainDb = 0.0f;
    float deEsserFreqHz = 7000.0f;
    float harmonicDrive = 0.0f;
};

// ==============================================================================
// Complete production instructions for one track
// ==============================================================================
struct ProducedTrackPlan {
    uint32_t instanceId = 0;
    juce::String trackName;
    InstrumentType refinedInstrument = InstrumentType::Unknown;
    float balanceTrimDb = 0.0f;
    float panPosition = 0.0f; // -1.0 (Left) to +1.0 (Right)
    float reverbSendDb = -60.0f;
    float delaySendDb = -60.0f;

    std::vector<ProducedSlotDirective> slots;
    juce::String engineerNotes;
};

// ==============================================================================
// Holistic Session Mix Plan produced by the Local LLM
// ==============================================================================
struct ProducedMixPlan {
    bool success = false;
    juce::String errorReason;
    juce::String mixSummary;
    std::vector<ProducedTrackPlan> tracks;
};

// ==============================================================================
// Local LLM Producer Engine Client (Asynchronous Background Thread Worker)
// ==============================================================================
class LLMProducerClient : public juce::Thread {
public:
    using CompletionCallback = std::function<void(const ProducedMixPlan&)>;

    static LLMProducerClient& getInstance();

    // Check connectivity to Ollama on http://127.0.0.1:11434
    bool isOllamaOnline() const noexcept { return serverOnline.load(); }
    juce::String getActiveModelName() const;
    void refreshModelStatusAsync();

    // Trigger full session mix analysis
    bool produceSessionMixAsync(const std::vector<SessionTrackDiagnosis>& sessionTracks,
                                const juce::String& genreAesthetic,
                                CompletionCallback callback);

    // Static Prompt & Parsing Helpers (Public for testing & deterministic verification)
    static juce::String buildSystemPrompt();
    static juce::String buildUserPrompt(const std::vector<SessionTrackDiagnosis>& sessionTracks,
                                        const juce::String& genreAesthetic);
    static ProducedMixPlan parseModelResponse(const juce::String& responseText,
                                              const std::vector<SessionTrackDiagnosis>& sessionTracks = {});

    bool isBusy() const noexcept { return isGenerating.load(); }

private:
    LLMProducerClient();
    ~LLMProducerClient() override;

    void run() override;

    // HTTP Helpers
    juce::String sendHttpPost(const juce::String& urlString, const juce::String& jsonBody, int timeoutMs = 60000);
    juce::String sendHttpGet(const juce::String& urlString, int timeoutMs = 4000);

    std::atomic<bool> serverOnline{false};
    std::atomic<bool> isGenerating{false};
    juce::String currentModelName{"llama3.1:latest"};
    mutable std::mutex modelMutex;

    // Job Queue
    struct MixJob {
        std::vector<SessionTrackDiagnosis> tracks;
        juce::String genre;
        CompletionCallback callback;
    };
    std::unique_ptr<MixJob> pendingJob;
    std::mutex jobMutex;
    juce::WaitableEvent jobEvent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LLMProducerClient)
};

} // namespace aimixer
