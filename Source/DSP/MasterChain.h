#pragma once

#include "../Common/Types.h"
#include "../Common/SharedProtocol.h"
#include "../Analysis/LoudnessMeter.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

namespace aimixer {

struct DestinationSettings {
    float targetIntegratedLufs = -14.0f;
    float ceilingDb = -1.0f;
    bool isPresenceForward = false;
};

class MasterChain {
public:
    MasterChain();
    ~MasterChain();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // AUDIO THREAD: Real-time safe processing
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    // Destination setting
    void setDestination(DestinationTag dest);
    DestinationTag getDestination() const noexcept { return currentDestination; }

    // Latency reporting for host
    int getReportedLatencySamples() const noexcept { return totalReportedLatency; }

    // Worker thread feeds analysis and loudness updates (~2-15 Hz)
    void updateWorkerLoudness(float integratedLufs, float shortTermLufs, float instantRmsDb, const float* spectrum32Bands);

    // Loudness Auto Gain
    void setAutoGainEnabled(bool enabled) noexcept { autoGainEnabled.store(enabled, std::memory_order_relaxed); }
    bool isAutoGainEnabled() const noexcept { return autoGainEnabled.load(std::memory_order_relaxed); }
    void setManualGainOffsetDb(float db) noexcept { manualGainOffsetDb.store(db, std::memory_order_relaxed); }
    float getManualGainOffsetDb() const noexcept { return manualGainOffsetDb.load(std::memory_order_relaxed); }
    float getActiveAutoGainDb() const noexcept { return currentAutoGainDb.load(std::memory_order_relaxed); }

    // Tonal Balance
    void setTonalBalanceEnabled(bool enabled) noexcept { tonalBalanceEnabled.store(enabled, std::memory_order_relaxed); }
    bool isTonalBalanceEnabled() const noexcept { return tonalBalanceEnabled.load(std::memory_order_relaxed); }
    float getTonalCorrectionDb() const noexcept { return currentTonalCorrectionDb.load(std::memory_order_relaxed); }
    juce::String getTonalStatusDescription() const;
    void setTasteTonalDelta(float lowDb, float presDb, float airDb) noexcept;

    // Safety Warnings
    bool isNarrowbandWarningActive() const noexcept { return narrowbandWarning.load(std::memory_order_relaxed); }
    float getLimiterGainReductionDb() const noexcept { return currentLimiterGrDb.load(std::memory_order_relaxed); }
    float getGlueGainReductionDb() const noexcept { return currentGlueGrDb.load(std::memory_order_relaxed); }

    static DestinationSettings getSettingsForDestination(DestinationTag dest) noexcept;

private:
    void updateTonalFilters();

    double currentSampleRate = 48000.0;
    DestinationTag currentDestination = DestinationTag::PALive;
    DestinationSettings destSettings;

    int lookaheadSamples = 48; // 1.0 ms
    int totalReportedLatency = 48;

    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    // 3d. Tonal Balance EQ (up to 3 broad biquads)
    std::atomic<bool> tonalBalanceEnabled{true};
    std::atomic<float> currentTonalCorrectionDb{0.0f};
    Filter tonalLowShelfLeft, tonalLowShelfRight;
    Filter tonalPresenceLeft, tonalPresenceRight;
    Filter tonalAirShelfLeft, tonalAirShelfRight;

    float targetLowShelfDb = 0.0f;
    float targetPresenceDb = 0.0f;
    float targetAirShelfDb = 0.0f;
    std::atomic<float> tasteLowShelfDb{0.0f};
    std::atomic<float> tastePresenceDb{0.0f};
    std::atomic<float> tasteAirShelfDb{0.0f};
    float smoothLowShelfDb = 0.0f;
    float smoothPresenceDb = 0.0f;
    float smoothAirShelfDb = 0.0f;

    // 3e. Master Glue Comp (1.5:1, 40ms att, 300ms rel, soft knee, 2dB cap)
    float glueCompEnvelope = 1.0f;
    float glueAttackCoeff = 0.0f;
    float glueReleaseCoeff = 0.0f;
    std::atomic<float> currentGlueGrDb{0.0f};

    // 3c. Loudness Auto Gain
    std::atomic<bool> autoGainEnabled{true};
    std::atomic<float> currentAutoGainDb{0.0f};
    std::atomic<float> manualGainOffsetDb{0.0f};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMasterGain;
    float targetAutoGainDb = 0.0f;
    float timeActiveSeconds = 0.0f;

    // 3f. 4x Oversampling & Lookahead Limiter
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    int lastMaxBlockSize = 512;
    size_t currentNumChannels = 2;

    float limiterEnvelope = 1.0f;
    float limiterReleaseCoeff = 0.0f;
    std::atomic<float> currentLimiterGrDb{0.0f};

    // Safety Warnings
    std::atomic<bool> narrowbandWarning{false};
    int narrowbandWarningCounter = 0;
};

} // namespace aimixer
