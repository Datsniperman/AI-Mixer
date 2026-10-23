#include "Analyzer.h"
#include <cmath>
#include <algorithm>

namespace aimixer {

AudioAnalyzer::AudioAnalyzer()
    : forwardFFT(static_cast<int>(FFT_ORDER)),
      window(FFT_SIZE, juce::dsp::WindowingFunction<float>::hann) {
    fifoReadBuffer.resize(FFT_SIZE, 0.0f);
    fftBuffer.resize(FFT_SIZE * 2, 0.0f);
    fifoScratchBuffer.resize(FIFO_CAPACITY, 0.0f);
    std::fill(std::begin(smoothedBands), std::end(smoothedBands), 0.0f);
}

AudioAnalyzer::~AudioAnalyzer() = default;

void AudioAnalyzer::prepare(double sampleRate, int /*samplesPerBlock*/) {
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 48000.0;
    reset();
}

void AudioAnalyzer::reset() {
    sampleFifo.reset();
    currentPeakDb.store(-100.0f, std::memory_order_relaxed);
    currentRmsDb.store(-100.0f, std::memory_order_relaxed);
    std::fill(fifoReadBuffer.begin(), fifoReadBuffer.end(), 0.0f);
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
    std::fill(std::begin(smoothedBands), std::end(smoothedBands), 0.0f);
    smoothedRms = -100.0f;
    smoothedPeak = -100.0f;
    calculatedCentroid = 1000.0f;
    calculatedFundamental = 0.0f;
}

void AudioAnalyzer::processAudioBlock(const juce::AudioBuffer<float>& buffer) noexcept {
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    alignas(16) float monoBlock[2048];
    const int samplesToProcess = std::min(numSamples, 2048);

    float maxVal = 0.0f;
    float sumSquares = 0.0f;
    const float channelScale = 1.0f / static_cast<float>(numChannels);

    for (int i = 0; i < samplesToProcess; ++i) {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            sum += buffer.getSample(ch, i);
        }
        float monoSample = sum * channelScale;
        monoBlock[i] = monoSample;

        float absVal = std::abs(monoSample);
        if (absVal > maxVal) maxVal = absVal;
        sumSquares += monoSample * monoSample;
    }

    float rms = std::sqrt(sumSquares / static_cast<float>(samplesToProcess));
    float peakDb = (maxVal > 1e-5f) ? juce::Decibels::gainToDecibels(maxVal) : -100.0f;
    float rmsDb = (rms > 1e-5f) ? juce::Decibels::gainToDecibels(rms) : -100.0f;

    currentPeakDb.store(peakDb, std::memory_order_relaxed);
    currentRmsDb.store(rmsDb, std::memory_order_relaxed);

    sampleFifo.write(monoBlock, static_cast<size_t>(samplesToProcess));
}

void AudioAnalyzer::calculate32Bands(const float* fftData) {
    const float binWidth = static_cast<float>(currentSampleRate) / static_cast<float>(FFT_SIZE);
    const size_t numBins = FFT_SIZE / 2;

    float weightedFreqSum = 0.0f;
    float totalMagnitude = 0.0f;
    float maxLowEnergy = 0.0f;
    float peakFundFreq = 0.0f;

    for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) {
        float centerFreq = SPECTRUM_BAND_FREQUENCIES[b];
        float lowerFreq = (b == 0) ? 10.0f : centerFreq * 0.89f;
        float upperFreq = (b == NUM_SPECTRUM_BANDS - 1) ? 22000.0f : centerFreq * 1.12f;

        size_t startBin = std::clamp(static_cast<size_t>(lowerFreq / binWidth), size_t(1), numBins - 1);
        size_t endBin = std::clamp(static_cast<size_t>(upperFreq / binWidth), startBin, numBins - 1);

        float maxMag = 0.0f;
        for (size_t k = startBin; k <= endBin; ++k) {
            if (fftData[k] > maxMag) maxMag = fftData[k];
        }

        float magDb = juce::Decibels::gainToDecibels(maxMag / static_cast<float>(FFT_SIZE));
        float normalized = std::clamp((magDb + 70.0f) / 70.0f, 0.0f, 1.0f);

        if (normalized > smoothedBands[b]) {
            smoothedBands[b] = 0.4f * smoothedBands[b] + 0.6f * normalized;
        } else {
            smoothedBands[b] = 0.8f * smoothedBands[b] + 0.2f * normalized;
        }

        weightedFreqSum += centerFreq * smoothedBands[b];
        totalMagnitude += smoothedBands[b];

        // Track fundamental peak candidate in low/mid frequencies (40 Hz - 600 Hz)
        if (centerFreq >= 40.0f && centerFreq <= 600.0f && smoothedBands[b] > maxLowEnergy) {
            maxLowEnergy = smoothedBands[b];
            peakFundFreq = centerFreq;
        }
    }

    if (totalMagnitude > 0.001f) {
        calculatedCentroid = weightedFreqSum / totalMagnitude;
    }
    calculatedFundamental = peakFundFreq;
}

void AudioAnalyzer::updateAnalysis(InstanceTelemetry& outTelemetry) {
    size_t available = sampleFifo.getNumAvailableToRead();
    if (available >= 512) {
        available = std::min(available, FIFO_CAPACITY);
        size_t numRead = sampleFifo.read(fifoScratchBuffer.data(), available);

        if (numRead >= FFT_SIZE) {
            std::memcpy(fifoReadBuffer.data(), fifoScratchBuffer.data() + (numRead - FFT_SIZE), FFT_SIZE * sizeof(float));
        } else {
            size_t shift = FFT_SIZE - numRead;
            std::memmove(fifoReadBuffer.data(), fifoReadBuffer.data() + numRead, shift * sizeof(float));
            std::memcpy(fifoReadBuffer.data() + shift, fifoScratchBuffer.data(), numRead * sizeof(float));
        }

        std::memcpy(fftBuffer.data(), fifoReadBuffer.data(), FFT_SIZE * sizeof(float));
        std::fill(fftBuffer.begin() + FFT_SIZE, fftBuffer.end(), 0.0f);
        window.multiplyWithWindowingTable(fftBuffer.data(), FFT_SIZE);
        forwardFFT.performFrequencyOnlyForwardTransform(fftBuffer.data());

        calculate32Bands(fftBuffer.data());
    }

    float instPeak = getInstantPeakDb();
    float instRms = getInstantRmsDb();

    smoothedPeak = (instPeak > smoothedPeak) ? (0.6f * smoothedPeak + 0.4f * instPeak)
                                            : (0.9f * smoothedPeak + 0.1f * instPeak);
    smoothedRms = 0.7f * smoothedRms + 0.3f * instRms;

    outTelemetry.peakDb = smoothedPeak;
    outTelemetry.rmsDb = smoothedRms;
    outTelemetry.momentaryLufs = smoothedRms;
    outTelemetry.crestFactorDb = std::max(0.0f, smoothedPeak - smoothedRms);
    outTelemetry.signalPresent = (smoothedRms > -55.0f);
    outTelemetry.spectralCentroidHz = calculatedCentroid;
    outTelemetry.fundamentalHz = calculatedFundamental;

    // Estimate percussiveness from crest factor (crest factor > 10 dB indicates high transients)
    float crest = std::max(0.0f, smoothedPeak - smoothedRms);
    outTelemetry.isPercussive = std::clamp((crest - 8.0f) / 12.0f, 0.0f, 1.0f);

    for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) {
        outTelemetry.spectrumBands[b] = smoothedBands[b];
    }
}

AcousticFeatures AudioAnalyzer::getAcousticFeatures() const {
    AcousticFeatures f;
    f.rmsDb = smoothedRms;
    f.peakDb = smoothedPeak;
    f.crestFactorDb = std::max(0.0f, smoothedPeak - smoothedRms);
    f.fundamentalHz = calculatedFundamental;
    f.spectralCentroidHz = calculatedCentroid;

    float lowSum = 0.0f;
    float bodySum = 0.0f;
    float midSum = 0.0f;
    float highSum = 0.0f;
    float totalSum = 0.0001f;

    for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) {
        float freq = SPECTRUM_BAND_FREQUENCIES[b];
        float val = smoothedBands[b];
        totalSum += val;

        if (freq <= 125.0f) {
            lowSum += val;
        } else if (freq >= 180.0f && freq <= 280.0f) {
            bodySum += val;
        } else if (freq >= 300.0f && freq <= 3500.0f) {
            midSum += val;
        } else if (freq >= 5000.0f) {
            highSum += val;
        }
    }

    f.lowFreqRatio = lowSum / totalSum;
    f.bodyBandRatio = bodySum / totalSum;
    f.midFreqRatio = midSum / totalSum;
    f.highFreqRatio = highSum / totalSum;
    f.isPercussive = std::clamp((f.crestFactorDb - 8.0f) / 12.0f, 0.0f, 1.0f);
    f.stereoWidth = 1.0f;
    return f;
}

} // namespace aimixer
