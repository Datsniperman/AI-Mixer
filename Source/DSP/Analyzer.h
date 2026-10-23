#pragma once

#include "../Common/SharedProtocol.h"
#include "../Common/LockFreeFifo.h"
#include "../Analysis/InstrumentClassifier.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace aimixer {

class AudioAnalyzer {
public:
    static constexpr size_t FFT_ORDER = 11; // 2048 points
    static constexpr size_t FFT_SIZE = 1 << FFT_ORDER;
    static constexpr size_t FIFO_CAPACITY = 8192;

    AudioAnalyzer();
    ~AudioAnalyzer();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // AUDIO THREAD: Zero-allocation, lock-free, zero-latency
    void processAudioBlock(const juce::AudioBuffer<float>& buffer) noexcept;

    // BACKGROUND / WORKER THREAD: Computes FFT, 32-band spectrum, crest factor, features
    void updateAnalysis(InstanceTelemetry& outTelemetry);

    // Extracts acoustic features for the classifier
    AcousticFeatures getAcousticFeatures() const;

    float getInstantPeakDb() const noexcept { return currentPeakDb.load(std::memory_order_relaxed); }
    float getInstantRmsDb() const noexcept { return currentRmsDb.load(std::memory_order_relaxed); }

private:
    void calculate32Bands(const float* fftData);

    double currentSampleRate = 48000.0;
    LockFreeFifo<float, FIFO_CAPACITY> sampleFifo;

    std::atomic<float> currentPeakDb{-100.0f};
    std::atomic<float> currentRmsDb{-100.0f};

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    
    std::vector<float> fifoReadBuffer;
    std::vector<float> fftBuffer;
    std::vector<float> fifoScratchBuffer;
    float smoothedBands[NUM_SPECTRUM_BANDS] = {0.0f};

    float smoothedRms = -100.0f;
    float smoothedPeak = -100.0f;
    float calculatedCentroid = 1000.0f;
    float calculatedFundamental = 0.0f;
};

} // namespace aimixer
