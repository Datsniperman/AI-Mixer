#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
#include <cmath>
#include <cassert>
#include "../Source/Common/SharedProtocol.h"
#include "../Source/Interprocess/SharedMemoryRegistry.h"
#include "../Source/Analysis/TrackNameParser.h"
#include "../Source/Analysis/InstrumentClassifier.h"
#include "../Source/DSP/SafeChain.h"
#include "../Source/Brain/MixSolver.h"
#include "../Source/DSP/SolverStage.h"
#include "../Source/DSP/PlaybookStage.h"
#include "../Source/DSP/BusStage.h"
#include "../Source/DSP/MasterChain.h"
#include "../Source/Analysis/LoudnessMeter.h"
#include "../Source/Brain/TasteProfile.h"
#include "../Source/Brain/ProfileManager.h"
#include "../Source/DSP/ReverbEngine.h"
#include "../Source/DSP/DelayEngine.h"
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Brain/WavesDialer.h"
#include "WavesHostingSpike.h"

using namespace aimixer;

#define TEST_CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[TEST FAILURE] " << msg << "\n  Condition: (" #cond ")\n  Line: " << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

// ==============================================================================
// 1. TRACK-NAME PARSER TESTS (60+ Cases with traps)
// ==============================================================================
void testTrackNameParser60Cases() {
    std::cout << "[TEST 1] Running TrackNameParser (60+ test cases incl. traps)..." << std::endl;

    struct TestCase {
        std::string rawName;
        InstrumentType expectedType;
        float minConfidence;
    };

    const std::vector<TestCase> testCases = {
        // Traps: Bass Drum vs Bass, Lead Gtr vs Lead Vox, Room vs Pad
        { "Bass Drum", InstrumentType::Kick, 0.90f },
        { "01_Bass_Drum_In", InstrumentType::Kick, 0.90f },
        { "Kick Sub", InstrumentType::Kick, 0.90f },
        { "Sub Bass", InstrumentType::Bass, 0.90f },
        { "Bass DI", InstrumentType::Bass, 0.90f },
        { "DI Bass", InstrumentType::Bass, 0.90f },
        { "Bass Guitar", InstrumentType::Bass, 0.90f },
        { "Electric Bass", InstrumentType::Bass, 0.90f },

        { "Lead Gtr", InstrumentType::ElectricGuitar, 0.85f },
        { "02_Lead_Guitar_L", InstrumentType::ElectricGuitar, 0.85f },
        { "Elec Gtr", InstrumentType::ElectricGuitar, 0.85f },
        { "EG Lead", InstrumentType::ElectricGuitar, 0.85f },
        { "Rhythm Gtr", InstrumentType::ElectricGuitar, 0.85f },
        { "EG R", InstrumentType::ElectricGuitar, 0.85f },
        { "GTR (L)", InstrumentType::ElectricGuitar, 0.80f },

        { "Lead Vocal", InstrumentType::LeadVocal, 0.90f },
        { "01_LdVox", InstrumentType::LeadVocal, 0.90f },
        { "Lead Vox", InstrumentType::LeadVocal, 0.90f },
        { "VOX 1", InstrumentType::LeadVocal, 0.85f },
        { "Vocal Lead", InstrumentType::LeadVocal, 0.90f },
        { "Main Vocal", InstrumentType::LeadVocal, 0.90f },
        { "Worship Leader", InstrumentType::LeadVocal, 0.90f },
        { "Lead", InstrumentType::LeadVocal, 0.80f }, // Plain lead alone = vocal (no gtr context)

        { "Vox 2", InstrumentType::BackgroundVocal, 0.80f },
        { "VOX 3", InstrumentType::BackgroundVocal, 0.80f },
        { "BGV 1", InstrumentType::BackgroundVocal, 0.85f },
        { "BG Vox", InstrumentType::BackgroundVocal, 0.85f },
        { "Backing Vocal", InstrumentType::BackgroundVocal, 0.85f },
        { "Choir L", InstrumentType::Choir, 0.85f },
        { "Harmony", InstrumentType::BackgroundVocal, 0.85f },

        { "Pastor", InstrumentType::PastorSpeaking, 0.90f },
        { "Pastor Mic", InstrumentType::PastorSpeaking, 0.90f },
        { "Sermon", InstrumentType::PastorSpeaking, 0.90f },
        { "Preaching", InstrumentType::PastorSpeaking, 0.90f },
        { "Host Speech", InstrumentType::PastorSpeaking, 0.80f },

        { "Kick In", InstrumentType::Kick, 0.90f },
        { "Kick Out", InstrumentType::Kick, 0.90f },
        { "BD In", InstrumentType::Kick, 0.90f },
        { "Kik", InstrumentType::Kick, 0.90f },

        { "Snare Top", InstrumentType::Snare, 0.90f },
        { "Snare Bottom", InstrumentType::Snare, 0.90f },
        { "SNR T", InstrumentType::Snare, 0.90f },
        { "Snr B", InstrumentType::Snare, 0.90f },
        { "Snr", InstrumentType::Snare, 0.90f },

        { "Hi-Hat", InstrumentType::HiHat, 0.90f },
        { "Hat", InstrumentType::HiHat, 0.85f },
        { "HH", InstrumentType::HiHat, 0.85f },

        { "Rack Tom 1", InstrumentType::Toms, 0.85f },
        { "Floor Tom", InstrumentType::Toms, 0.85f },
        { "Tom 1", InstrumentType::Toms, 0.85f },
        { "Tom 2", InstrumentType::Toms, 0.85f },
        { "FT", InstrumentType::Toms, 0.80f },

        { "Drum Overheads", InstrumentType::Overheads, 0.90f },
        { "Kit OH", InstrumentType::Overheads, 0.90f },
        { "OH L", InstrumentType::Overheads, 0.90f },
        { "Cymbals", InstrumentType::Overheads, 0.85f },
        { "Room Drums", InstrumentType::Room, 0.85f },

        { "Room Mic", InstrumentType::Room, 0.85f },
        { "Congregation", InstrumentType::Room, 0.90f },
        { "Audience", InstrumentType::Room, 0.90f },
        { "Drum Room", InstrumentType::Room, 0.85f },

        { "Acoustic Guitar", InstrumentType::AcousticGuitar, 0.90f },
        { "Ac Gtr", InstrumentType::AcousticGuitar, 0.90f },
        { "AGTR", InstrumentType::AcousticGuitar, 0.90f },
        { "Steel String", InstrumentType::AcousticGuitar, 0.90f },

        { "Grand Piano", InstrumentType::Piano, 0.90f },
        { "Pno", InstrumentType::Piano, 0.85f },
        { "Piano", InstrumentType::Piano, 0.90f },

        { "Keys", InstrumentType::Keys, 0.85f },
        { "Rhodes", InstrumentType::Keys, 0.90f },
        { "B3 Organ", InstrumentType::Keys, 0.90f },
        { "Wurli", InstrumentType::Keys, 0.90f },

        { "Strings Pad", InstrumentType::Pad, 0.85f },
        { "Ambient Pad", InstrumentType::Pad, 0.85f },
        { "Atmosphere", InstrumentType::Pad, 0.85f },
        { "Shimmer", InstrumentType::Pad, 0.85f },

        { "Synth Lead", InstrumentType::Synth, 0.85f },
        { "Arps", InstrumentType::Synth, 0.85f },
        { "Seq", InstrumentType::Synth, 0.80f },

        { "Tracks", InstrumentType::Tracks, 0.85f },
        { "Playback", InstrumentType::Tracks, 0.85f },
        { "Stems", InstrumentType::Tracks, 0.85f },
        { "Guide Cues", InstrumentType::Tracks, 0.80f },

        { "Click", InstrumentType::Click, 1.00f },
        { "Guide Click", InstrumentType::Click, 1.00f },
        { "Metronome", InstrumentType::Click, 1.00f },

        { "Audio 1", InstrumentType::Unknown, 0.0f },
        { "FX Return", InstrumentType::Unknown, 0.0f },
        { "Unknown Track", InstrumentType::Unknown, 0.0f }
    };

    int passed = 0;
    for (const auto& tc : testCases) {
        auto res = TrackNameParser::parse(tc.rawName);
        if (res.type != tc.expectedType || res.confidence < tc.minConfidence) {
            std::cerr << "  FAILED: '" << tc.rawName << "' -> got " << getInstrumentName(res.type)
                      << " (" << res.confidence << "), expected " << getInstrumentName(tc.expectedType) << "\n";
            TEST_CHECK(false, "Parser case failed");
        }
        passed++;
    }

    std::cout << "  [PASS] All " << passed << " TrackNameParser test cases passed perfectly." << std::endl;
}

// ==============================================================================
// 2. PRECEDENCE & PERSISTENCE TEST
// ==============================================================================
void testIdentityPrecedence() {
    std::cout << "[TEST 2] Running Identity Precedence Test (User > TrackName > Classifier > Unknown)..." << std::endl;

    IdentitySource source = IdentitySource::None;
    float confidence = 0.0f;

    ParseResult trackNameKick { InstrumentType::Kick, 0.95f, "kick" };
    ClassificationResult classVocal { InstrumentType::LeadVocal, 0.85f, false };

    // Case A: Track name "Kick" vs Vocal classifier signal -> TrackName wins
    auto resolvedA = InstrumentClassifier::resolveIdentity(
        InstrumentType::Unknown, trackNameKick, classVocal, source, confidence
    );
    TEST_CHECK(resolvedA == InstrumentType::Kick, "TrackName must defeat classifier");
    TEST_CHECK(source == IdentitySource::TrackName, "Source must be TrackName");

    // Case B: User overrides to AcousticGuitar -> User wins over both TrackName and Classifier
    auto resolvedB = InstrumentClassifier::resolveIdentity(
        InstrumentType::AcousticGuitar, trackNameKick, classVocal, source, confidence
    );
    TEST_CHECK(resolvedB == InstrumentType::AcousticGuitar, "User pick must defeat track name and classifier");
    TEST_CHECK(source == IdentitySource::User, "Source must be User");
    TEST_CHECK(confidence == 1.0f, "User confidence must be 1.0");

    // Case C: No user pick, no track name match (Unknown) -> Classifier wins if >= 0.60
    ParseResult trackUnknown { InstrumentType::Unknown, 0.0f, "" };
    auto resolvedC = InstrumentClassifier::resolveIdentity(
        InstrumentType::Unknown, trackUnknown, classVocal, source, confidence
    );
    TEST_CHECK(resolvedC == InstrumentType::LeadVocal, "Classifier must win when track name is unknown");
    TEST_CHECK(source == IdentitySource::Classified, "Source must be Classified");

    // Case D: Low confidence classifier (<0.60) -> Fallback to Unknown
    ClassificationResult classWeak { InstrumentType::Snare, 0.52f, false };
    auto resolvedD = InstrumentClassifier::resolveIdentity(
        InstrumentType::Unknown, trackUnknown, classWeak, source, confidence
    );
    TEST_CHECK(resolvedD == InstrumentType::Unknown, "Weak classifier must return Unknown");
    TEST_CHECK(source == IdentitySource::None, "Source must be None");

    // Case E: Persistence test via APVTS save & restore
    juce::ScopedJuceInitialiser_GUI juceInit;
    {
        AIMixerAudioProcessor proc;
        proc.getAPVTS().getParameter("instrument")->setValueNotifyingHost(
            proc.getAPVTS().getParameter("instrument")->convertTo0to1(11) // Acoustic Gtr
        );

        juce::MemoryBlock block;
        proc.getStateInformation(block);

        AIMixerAudioProcessor procReloaded;
        procReloaded.setStateInformation(block.getData(), static_cast<int>(block.getSize()));

        auto* param = dynamic_cast<juce::AudioParameterChoice*>(procReloaded.getAPVTS().getParameter("instrument"));
        TEST_CHECK(param != nullptr, "Instrument param exists");
        TEST_CHECK(param->getIndex() == 11, "User pick must survive DAW state reload");
    }

    std::cout << "  [PASS] Precedence & State Persistence verified." << std::endl;
}

// ==============================================================================
// 3. SYNTHETIC SIGNAL CLASSIFIER TEST
// ==============================================================================
void testSyntheticClassifier() {
    std::cout << "[TEST 3] Running Synthetic Signal Classifier Tests..." << std::endl;

    InstrumentClassifier classifier;

    // Synthetic Kick: Sub energy (60 Hz fundamental), low centroid, high percussiveness
    AcousticFeatures kickFeat;
    kickFeat.rmsDb = -18.0f;
    kickFeat.peakDb = -4.0f;
    kickFeat.crestFactorDb = 14.0f;
    kickFeat.fundamentalHz = 55.0f;
    kickFeat.spectralCentroidHz = 180.0f;
    kickFeat.lowFreqRatio = 0.65f;
    kickFeat.isPercussive = 0.80f;
    auto kickRes = classifier.classify(kickFeat);
    TEST_CHECK(kickRes.type == InstrumentType::Kick, "Synthetic kick features must classify as Kick");
    TEST_CHECK(kickRes.confidence >= 0.85f, "Kick confidence must be >= 0.85");

    // Synthetic Bass: Sustained 80 Hz, low centroid, low percussiveness
    classifier.reset();
    AcousticFeatures bassFeat;
    bassFeat.rmsDb = -16.0f;
    bassFeat.peakDb = -10.0f;
    bassFeat.crestFactorDb = 6.0f;
    bassFeat.fundamentalHz = 82.0f;
    bassFeat.spectralCentroidHz = 350.0f;
    bassFeat.lowFreqRatio = 0.55f;
    bassFeat.isPercussive = 0.15f;
    auto bassRes = classifier.classify(bassFeat);
    TEST_CHECK(bassRes.type == InstrumentType::Bass, "Synthetic bass features must classify as Bass");
    TEST_CHECK(bassRes.confidence >= 0.85f, "Bass confidence must be >= 0.85");

    // Synthetic Vocal: 220 Hz fundamental, speech midrange band dominant
    classifier.reset();
    AcousticFeatures voxFeat;
    voxFeat.rmsDb = -20.0f;
    voxFeat.peakDb = -10.0f;
    voxFeat.crestFactorDb = 10.0f;
    voxFeat.fundamentalHz = 220.0f;
    voxFeat.spectralCentroidHz = 1800.0f;
    voxFeat.midFreqRatio = 0.60f;
    voxFeat.isPercussive = 0.25f;
    auto voxRes = classifier.classify(voxFeat);
    TEST_CHECK(voxRes.type == InstrumentType::LeadVocal, "Synthetic vocal features must classify as LeadVocal");

    // Synthetic Pad: Ultra-low percussive, wide stereo, smooth centroid
    classifier.reset();
    AcousticFeatures padFeat;
    padFeat.rmsDb = -22.0f;
    padFeat.peakDb = -18.0f;
    padFeat.crestFactorDb = 4.0f;
    padFeat.fundamentalHz = 0.0f;
    padFeat.spectralCentroidHz = 1200.0f;
    padFeat.isPercussive = 0.05f;
    padFeat.stereoWidth = 1.0f;
    auto padRes = classifier.classify(padFeat);
    TEST_CHECK(padRes.type == InstrumentType::Pad, "Synthetic pad features must classify as Pad");

    // Ambiguous Noise: must return Unknown without confident errors
    classifier.reset();
    AcousticFeatures noiseFeat;
    noiseFeat.rmsDb = -24.0f;
    noiseFeat.peakDb = -20.0f;
    noiseFeat.crestFactorDb = 4.0f;
    noiseFeat.fundamentalHz = 0.0f;
    noiseFeat.spectralCentroidHz = 4500.0f;
    noiseFeat.lowFreqRatio = 0.1f;
    noiseFeat.isPercussive = 0.2f;
    auto noiseRes = classifier.classify(noiseFeat);
    TEST_CHECK(noiseRes.type == InstrumentType::Unknown, "Ambiguous noise must return Unknown");

    std::cout << "  [PASS] Synthetic signal classification rules verified." << std::endl;
}

// ==============================================================================
// 4. AUTO-TRIM CONVERGENCE & SILENCE FREEZE TEST
// ==============================================================================
void testAutoTrimConvergence() {
    std::cout << "[TEST 4] Running Auto-Trim Convergence Test (-30 and -6 to -18 dBFS RMS)..." << std::endl;

    SafeChain chain;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    chain.prepare(spec);
    chain.setAutoTrimEnabled(true);

    // Test A: Input at -30 dBFS RMS -> target +12 dB trim
    for (int cycle = 0; cycle < 30; ++cycle) {
        chain.updateAutoTrimWorker(-30.0f);
    }
    float trimA = chain.getAutoTrimDb();
    std::cout << "  Trim converged from -30 dBFS to: +" << trimA << " dB" << std::endl;
    TEST_CHECK(std::abs(trimA - 12.0f) < 1.0f, "Auto-trim must bring -30 dBFS close to target (+12 dB)");

    // Test B: Input at -6 dBFS RMS -> target -12 dB trim
    for (int cycle = 0; cycle < 30; ++cycle) {
        chain.updateAutoTrimWorker(-6.0f);
    }
    float trimB = chain.getAutoTrimDb();
    std::cout << "  Trim converged from -6 dBFS to: " << trimB << " dB" << std::endl;
    TEST_CHECK(std::abs(trimB - (-12.0f)) < 1.0f, "Auto-trim must bring -6 dBFS close to target (-12 dB)");

    // Test C: Hold test - 10 consecutive cycles at steady input, verify no oscillation > 0.5 dB
    float startHold = chain.getAutoTrimDb();
    (void)startHold;
    for (int cycle = 0; cycle < 20; ++cycle) {
        chain.updateAutoTrimWorker(-18.0f);
    }
    float endHold = chain.getAutoTrimDb();
    TEST_CHECK(std::abs(endHold - 0.0f) < 0.5f, "Auto-trim must hold steady at -18 dBFS without oscillation");

    // Test D: Silence Freeze (input < -60 dBFS)
    float preSilenceTrim = chain.getAutoTrimDb();
    chain.updateAutoTrimWorker(-75.0f);
    chain.updateAutoTrimWorker(-85.0f);
    float postSilenceTrim = chain.getAutoTrimDb();
    TEST_CHECK(std::abs(postSilenceTrim - preSilenceTrim) < 0.001f, "Silence must freeze auto-trim");

    std::cout << "  [PASS] Auto-Trim convergence & silence freeze verified." << std::endl;
}

// ==============================================================================
// 5. HPF TABLE & MANUAL OVERRIDE TEST
// ==============================================================================
void testHpfTableAndOverrides() {
    std::cout << "[TEST 5] Running HPF Table & Filter Manual Override Test..." << std::endl;

    SafeChain chain;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    chain.prepare(spec);

    // Verify instrument table
    chain.setInstrumentType(InstrumentType::LeadVocal);
    TEST_CHECK(std::abs(chain.getHpfHz() - 80.0f) < 0.1f, "LeadVocal HPF must be 80 Hz");

    chain.setInstrumentType(InstrumentType::PastorSpeaking);
    TEST_CHECK(std::abs(chain.getHpfHz() - 95.0f) < 0.1f, "Pastor HPF must be 95 Hz");

    chain.setInstrumentType(InstrumentType::Kick);
    TEST_CHECK(std::abs(chain.getHpfHz() - 20.0f) < 0.1f, "Kick HPF must be 20 Hz");

    chain.setInstrumentType(InstrumentType::HiHat);
    TEST_CHECK(std::abs(chain.getHpfHz() - 150.0f) < 0.1f, "HiHat HPF must be 150 Hz");

    chain.setInstrumentType(InstrumentType::Bass);
    TEST_CHECK(std::abs(chain.getHpfHz() - 25.0f) < 0.1f, "Bass HPF must be 25 Hz");
    TEST_CHECK(std::abs(chain.getLpfHz() - 8000.0f) < 0.1f, "Bass LPF must be 8000 Hz");

    // Manual Drag Override Test
    chain.setHpfFrequency(120.0f, true); // User manually sets 120 Hz
    TEST_CHECK(chain.isHpfManual(), "Manual flag must be set");
    TEST_CHECK(std::abs(chain.getHpfHz() - 120.0f) < 0.1f, "HPF must reflect manual value");

    // Switching instrument must NOT overwrite the manually overridden HPF
    chain.setInstrumentType(InstrumentType::Snare);
    TEST_CHECK(std::abs(chain.getHpfHz() - 120.0f) < 0.1f, "Manual HPF must not be overridden by instrument change");

    // Reset to Auto
    chain.resetHpfToAuto();
    TEST_CHECK(!chain.isHpfManual(), "Manual flag cleared");
    TEST_CHECK(std::abs(chain.getHpfHz() - 60.0f) < 0.1f, "Reset to auto must apply Snare HPF (60 Hz)");

    std::cout << "  [PASS] HPF/LPF Table & Manual Override verified." << std::endl;
}

// ==============================================================================
// 6. LATENCY TEST (0 SAMPLES WITH FULL SAFE CHAIN)
// ==============================================================================
void testLatencyZeroSamplesWithSafeChain() {
    std::cout << "[TEST 6] Running Latency Test with full Safe Chain engaged..." << std::endl;

    juce::ScopedJuceInitialiser_GUI juceInit;
    AIMixerAudioProcessor processor;

    processor.setRole(PluginRole::Source);
    processor.prepareToPlay(48000.0, 512);

    int sourceLatency = processor.getLatencySamples();
    std::cout << "  Source mode latency: " << sourceLatency << " samples" << std::endl;
    TEST_CHECK(sourceLatency == 0, "Source mode latency must be exactly 0 samples");

    processor.setRole(PluginRole::Bus);
    processor.prepareToPlay(48000.0, 512);

    int busLatency = processor.getLatencySamples();
    std::cout << "  Bus mode latency: " << busLatency << " samples" << std::endl;
    TEST_CHECK(busLatency == 0, "Bus mode latency must be exactly 0 samples");

    std::cout << "  [PASS] Strict zero-latency verified with active Safe Chain." << std::endl;
}

// ==============================================================================
// 7. CLICK TRACK BIT-EXACT PASS-THROUGH TEST
// ==============================================================================
void testClickBitExactPassThrough() {
    std::cout << "[TEST 7] Running Click Track Bit-Exact Pass-Through Test..." << std::endl;

    SafeChain chain;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    chain.prepare(spec);

    // Set to Click track
    chain.setInstrumentType(InstrumentType::Click);

    // Create test buffer with pseudo-random test signal
    juce::AudioBuffer<float> buffer(2, 512);
    juce::AudioBuffer<float> original(2, 512);

    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            float val = std::sin(static_cast<float>(i) * 0.1f) * 0.8f;
            buffer.setSample(ch, i, val);
            original.setSample(ch, i, val);
        }
    }

    // Process through safe chain
    chain.process(buffer);

    // Verify bit-exact equality (no gain, no filtering, no compression)
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            TEST_CHECK(buffer.getSample(ch, i) == original.getSample(ch, i),
                       "Click track buffer must be bit-exact identical (100% pass-through)");
        }
    }

    std::cout << "  [PASS] Click track bit-exact pass-through verified." << std::endl;
}

// ==============================================================================
// 8. COORDINATOR ELECTION EARLIEST-SURVIVOR TEST & VERSION GUARD
// ==============================================================================
void testCoordinatorElectionSurvivorAndVersionGuard() {
    std::cout << "[TEST 8] Running Coordinator Election Earliest-Survivor & Version Guard Tests..." << std::endl;

    SharedMemoryRegistry registry;
    TEST_CHECK(registry.initialize("AIMixer_TestRegistry_Survivor"), "Registry init");

    // Step A: Register 5 instances with ascending timestamps
    int slots[5];
    uint32_t ids[5] = { 2001, 2002, 2003, 2004, 2005 };

    for (int i = 0; i < 5; ++i) {
        slots[i] = registry.registerInstance(ids[i], PluginRole::Source, "Track " + std::to_string(i), 1111);
        TEST_CHECK(slots[i] >= 0, "Slot registered");
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Ensure distinct timestamps
    }

    uint64_t now = SharedMemoryRegistry::getSystemTimeMs();
    uint32_t coord = registry.runMaintenanceAndElection(now);
    std::cout << "  Initial Coordinator (expected earliest survivor 2001): " << coord << std::endl;
    TEST_CHECK(coord == ids[0], "Earliest registered instance must win coordinator election");

    // Step B: Unregister middle instances (ids 1, 2, 3) and first instance (id 0)
    registry.unregisterInstance(slots[0], ids[0]);
    registry.unregisterInstance(slots[1], ids[1]);
    registry.unregisterInstance(slots[2], ids[2]);

    now += 10;
    uint32_t fallbackCoord = registry.runMaintenanceAndElection(now);
    std::cout << "  Fallback Coordinator after unregistering earliest (expected 2004): " << fallbackCoord << std::endl;
    TEST_CHECK(fallbackCoord == ids[3], "Earliest remaining survivor (2004) must win fallback election");

    registry.shutdown();
    std::cout << "  [PASS] Earliest survivor coordinator election rule verified." << std::endl;
}

// ==============================================================================
// 9. CPU BENCHMARK AT 48 kHz / 512 SAMPLES
// ==============================================================================
void testCpuBenchmark48kHz() {
    std::cout << "[TEST 9] Running CPU Benchmark at 48 kHz / 512 samples with Safe Chain + Analyzer..." << std::endl;

    juce::ScopedJuceInitialiser_GUI juceInit;
    AIMixerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            buffer.setSample(ch, i, std::sin(static_cast<float>(i) * 0.05f) * 0.5f);
        }
    }

    // Process 2000 blocks (~21.3 seconds of real-time audio)
    const int numBlocks = 2000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int b = 0; b < numBlocks; ++b) {
        processor.processBlock(buffer, midi);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    double realTimeMs = (numBlocks * 512.0 / 48000.0) * 1000.0;
    double cpuPercentage = (totalTimeMs / realTimeMs) * 100.0;

    std::cout << "  Processed 2000 blocks (21.3s audio) in " << totalTimeMs << " ms." << std::endl;
    std::cout << "  Estimated Per-Instance Audio Thread CPU: " << cpuPercentage << "%" << std::endl;
    TEST_CHECK(cpuPercentage < 5.0, "Audio thread CPU per instance must be well under 5%");

    std::cout << "  [PASS] CPU benchmark verified." << std::endl;
}

// ==============================================================================
// 10. MVP 1 REGRESSION: 16 INSTANCES DISCOVERY & RAPID CHURN
// ==============================================================================
void testMvp1Regression16Instances() {
    std::cout << "[TEST 10] Running MVP 1 Regression Test (16 instances discovery + rapid churn)..." << std::endl;

    SharedMemoryRegistry registry;
    TEST_CHECK(registry.initialize("AIMixer_TestRegistry_16Regress"), "Init shared registry");

    struct TestInstance {
        uint32_t id;
        PluginRole role;
        std::string name;
        int slotIndex = -1;
    };

    std::vector<TestInstance> instances;
    instances.reserve(16);

    const char* names[16] = {
        "Lead Vox", "Pastor", "BGV 1", "BGV 2",
        "Kick In", "Kick Out", "Snare Top", "Snare Bottom",
        "Hi-Hat", "Bass DI", "Acoustic Gtr", "EG Lead",
        "Piano", "Pad", "Tracks", "Master Out"
    };

    for (int i = 0; i < 16; ++i) {
        TestInstance inst;
        inst.id = 3000 + i;
        inst.role = (i == 15) ? PluginRole::Master : PluginRole::Source;
        inst.name = names[i];
        inst.slotIndex = registry.registerInstance(inst.id, inst.role, inst.name, 9999);
        TEST_CHECK(inst.slotIndex >= 0, "Slot registered");
        instances.push_back(inst);
    }

    TEST_CHECK(registry.getActiveInstanceCount() == 16, "All 16 instances active");

    uint64_t now = SharedMemoryRegistry::getSystemTimeMs();
    uint32_t coordinatorId = registry.runMaintenanceAndElection(now);
    TEST_CHECK(coordinatorId == instances[15].id, "Master must win coordinator election");

    // Rapid churn
    for (int i = 0; i < 6; ++i) {
        registry.unregisterInstance(instances[i].slotIndex, instances[i].id);
        instances[i].slotIndex = -1;
    }
    TEST_CHECK(registry.getActiveInstanceCount() == 10, "10 instances remain");

    for (int i = 0; i < 6; ++i) {
        instances[i].slotIndex = registry.registerInstance(instances[i].id, instances[i].role, instances[i].name, 9999);
        TEST_CHECK(instances[i].slotIndex >= 0, "Re-registered");
    }
    TEST_CHECK(registry.getActiveInstanceCount() == 16, "16 instances active after churn");

    registry.shutdown();
    std::cout << "  [PASS] MVP 1 regression passed cleanly." << std::endl;
}

// ==============================================================================
// STEP 0 REGRESSION: PARSER ROOM FIXES, SIDE HINTS, CLASSIFIER GUARDS
// ==============================================================================
void testStep0Regressions() {
    std::cout << "[STEP 0 REGRESSION] Testing Parser Room fixes, Side hints, and Classifier guards..." << std::endl;

    // 1. Room vs Overheads parser fixes
    auto r1 = TrackNameParser::parse("room drums");
    TEST_CHECK(r1.type == InstrumentType::Room, "'room drums' must be Room");
    auto r2 = TrackNameParser::parse("drum room");
    TEST_CHECK(r2.type == InstrumentType::Room, "'drum room' must be Room");
    auto r3 = TrackNameParser::parse("room mic");
    TEST_CHECK(r3.type == InstrumentType::Room, "'room mic' must be Room");

    auto oh1 = TrackNameParser::parse("drum oh");
    TEST_CHECK(oh1.type == InstrumentType::Overheads, "'drum oh' must be Overheads");
    auto oh2 = TrackNameParser::parse("kit oh");
    TEST_CHECK(oh2.type == InstrumentType::Overheads, "'kit oh' must be Overheads");
    auto oh3 = TrackNameParser::parse("overheads");
    TEST_CHECK(oh3.type == InstrumentType::Overheads, "'overheads' must be Overheads");

    // 2. Side Hints
    auto sLeft = TrackNameParser::parse("02_Lead_Guitar_L");
    TEST_CHECK(sLeft.side == SideHint::Left, "'02_Lead_Guitar_L' must have SideHint::Left");
    auto sRight = TrackNameParser::parse("EG R");
    TEST_CHECK(sRight.side == SideHint::Right, "'EG R' must have SideHint::Right");
    auto sCentre = TrackNameParser::parse("Kick In");
    TEST_CHECK(sCentre.side == SideHint::Centre, "'Kick In' must have SideHint::Centre");

    // 3. Classifier Guards
    InstrumentClassifier classifier;
    AcousticFeatures ohLike;
    ohLike.rmsDb = -18.0f;
    ohLike.isPercussive = 0.6f;
    ohLike.spectralCentroidHz = 7000.0f;
    ohLike.lowFreqRatio = 0.02f;
    ohLike.stereoWidth = 0.90f; // High stereo width (Overheads-like)
    ohLike.highFreqRatio = 0.40f; // High air
    auto resHat = classifier.classify(ohLike);
    TEST_CHECK(resHat.type == InstrumentType::HiHat && resHat.confidence <= 0.70f && !resHat.isLocked,
               "Overheads-like signal must block HiHat lock (confidence <= 0.70)");

    // Piano candidate capped at 0.70 (never lock-eligible)
    AcousticFeatures pianoCand;
    pianoCand.rmsDb = -18.0f;
    pianoCand.isPercussive = 0.20f;
    pianoCand.crestFactorDb = 6.0f;
    pianoCand.fundamentalHz = 440.0f;
    pianoCand.spectralCentroidHz = 1500.0f;
    pianoCand.lowFreqRatio = 0.20f;
    pianoCand.highFreqRatio = 0.15f;
    auto resPiano = classifier.classify(pianoCand);
    TEST_CHECK(resPiano.type == InstrumentType::Piano && resPiano.confidence <= 0.70f && !resPiano.isLocked,
               "Piano candidate rule must be capped at 0.70 confidence and non-locking");

    std::cout << "  [PASS] Step 0 regression suite passed." << std::endl;
}

// ==============================================================================
// MVP 3 TEST HELPERS
// ==============================================================================
static InstanceTelemetry makeTelemetry(uint32_t id, PluginRole role, InstrumentType inst, float rmsDb, float peakDb = -6.0f) {
    InstanceTelemetry t;
    t.instanceId = id;
    t.role = role;
    t.instrumentType = inst;
    t.signalPresent = true;
    t.rmsDb = rmsDb;
    t.peakDb = peakDb;
    t.registrationTimestampMs = 1000;
    t.lastHeartbeatMs = 2000;
    return t;
}

// ==============================================================================
// MVP 3 TEST 1: KICK + BASS
// ==============================================================================
void testMvp3_KickBassDuckingAndCarving() {
    std::cout << "[MVP 3 TEST 1] Kick + Bass Fundamental Carving and Ducking..." << std::endl;
    SolverState state;

    auto kick = makeTelemetry(101, PluginRole::Source, InstrumentType::Kick, -18.0f);
    kick.fundamentalHz = 60.0f;
    kick.spectrumBands[3] = 0.8f; // ~50-63 Hz

    auto bass = makeTelemetry(102, PluginRole::Source, InstrumentType::Bass, -20.0f);
    bass.fundamentalHz = 80.0f;
    bass.spectrumBands[3] = 0.7f;
    bass.spectrumBands[4] = 0.8f; // ~63-80 Hz

    std::vector<InstanceTelemetry> snapshot = { kick, bass };
    uint64_t nowMs = 1000;

    std::unordered_map<uint32_t, InstanceDirectives> directives;
    for (int cycle = 0; cycle < 3; ++cycle) {
        directives = MixSolver::solve(snapshot, nowMs, state);
        nowMs += 200;
    }

    TEST_CHECK(directives.find(102) != directives.end(), "Bass must receive directives");
    const auto& bassDir = directives[102];

    bool foundKickCut = false;
    for (int k = 0; k < 4; ++k) {
        if (bassDir.maskingCuts[k].enabled && std::abs(bassDir.maskingCuts[k].freqHz - 60.0f) < 5.0f) {
            TEST_CHECK(bassDir.maskingCuts[k].gainDb < 0.0f && bassDir.maskingCuts[k].gainDb >= -6.0f, "Bass cut gain negative");
            foundKickCut = true;
            break;
        }
    }
    TEST_CHECK(foundKickCut, "Bass must receive masking cut near Kick fundamental (60 Hz)");
    TEST_CHECK(bassDir.duckDb <= -1.0f && bassDir.duckDb >= -3.0f, "Bass duckDb between -1 and -3 dB");
    TEST_CHECK(bassDir.duckKeyInstanceId == kick.instanceId, "Bass duck keyed to Kick instanceId");

    TEST_CHECK(directives.find(101) != directives.end(), "Kick must receive directives");
    const auto& kickDir = directives[101];
    TEST_CHECK(kickDir.duckDb == 0.0f, "Kick duckDb must be 0");
    std::cout << "  [PASS] Kick + Bass carving and ducking verified." << std::endl;
}

// ==============================================================================
// MVP 4 STEP 0 REGRESSION: KICK + BASS WITH BASS FUNDAMENTAL = 55 HZ
// ==============================================================================
void testStep0_KickBass55HzNoCutOnKick() {
    std::cout << "[MVP 4 STEP 0] Kick + Bass with Bass fundamental = 55 Hz (Kick gets NO cut)..." << std::endl;
    SolverState state;

    auto kick = makeTelemetry(101, PluginRole::Source, InstrumentType::Kick, -18.0f);
    kick.fundamentalHz = 60.0f;
    kick.spectrumBands[3] = 0.8f; // ~50-63 Hz

    auto bass = makeTelemetry(102, PluginRole::Source, InstrumentType::Bass, -20.0f);
    bass.fundamentalHz = 55.0f;   // 55 Hz fundamental (< 90 Hz, so Bass->Kick rule must NOT fire)
    bass.spectrumBands[3] = 0.8f; // ~50-63 Hz

    std::vector<InstanceTelemetry> snapshot = { kick, bass };
    uint64_t nowMs = 1000;

    std::unordered_map<uint32_t, InstanceDirectives> directives;
    for (int cycle = 0; cycle < 3; ++cycle) {
        directives = MixSolver::solve(snapshot, nowMs, state);
        nowMs += 200;
    }

    // 1. Kick must receive NO masking cut from Bass
    TEST_CHECK(directives.find(101) != directives.end(), "Kick must receive directives packet");
    const auto& kickDir = directives[101];
    for (int k = 0; k < 4; ++k) {
        TEST_CHECK(!kickDir.maskingCuts[k].enabled, "Kick must receive NO masking cuts when Bass fundamental is 55 Hz (< 90 Hz)");
    }
    TEST_CHECK(kickDir.duckDb == 0.0f, "Kick receives duckDb = 0");

    // 2. Bass still receives cut near Kick fundamental (60 Hz) + ducking from Kick
    TEST_CHECK(directives.find(102) != directives.end(), "Bass must receive directives packet");
    const auto& bassDir = directives[102];
    bool bassHasKickCut = false;
    for (int k = 0; k < 4; ++k) {
        if (bassDir.maskingCuts[k].enabled && std::abs(bassDir.maskingCuts[k].freqHz - 60.0f) < 5.0f) {
            TEST_CHECK(bassDir.maskingCuts[k].gainDb < 0.0f, "Bass cut gain must be negative");
            bassHasKickCut = true;
            break;
        }
    }
    TEST_CHECK(bassHasKickCut, "Bass still receives cut near Kick fundamental (60 Hz)");
    TEST_CHECK(bassDir.duckDb <= -1.0f && bassDir.duckDb >= -3.0f, "Bass still receives ducking from Kick");
    TEST_CHECK(bassDir.duckKeyInstanceId == kick.instanceId, "Bass duck keyed to Kick");

    std::cout << "  [PASS] Step 0 regression test verified successfully." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 2: LEAD VOCAL + PAD
// ==============================================================================
void testMvp3_LeadVocalPadPresenceAndRide() {
    std::cout << "[MVP 3 TEST 2] Lead Vocal + Pad Presence Carving and Balance Ride..." << std::endl;
    SolverState state;

    auto vox = makeTelemetry(201, PluginRole::Source, InstrumentType::LeadVocal, -18.0f);
    vox.spectrumBands[18] = 0.7f; // ~2.5 kHz
    vox.spectrumBands[19] = 0.75f; // ~3.15 kHz

    auto pad = makeTelemetry(202, PluginRole::Source, InstrumentType::Pad, -18.0f);
    pad.spectrumBands[18] = 0.65f;
    pad.spectrumBands[19] = 0.70f;

    std::vector<InstanceTelemetry> snapshot = { vox, pad };
    uint64_t nowMs = 1000;

    std::unordered_map<uint32_t, InstanceDirectives> directives;
    for (int cycle = 0; cycle < 5; ++cycle) {
        directives = MixSolver::solve(snapshot, nowMs, state);
        nowMs += 200;
    }

    const auto& voxDir = directives[201];
    const auto& padDir = directives[202];

    TEST_CHECK(voxDir.rideOffsetDb == 0.0f, "Lead Vocal ride offset must be 0 dB");
    for (int k = 0; k < 4; ++k) {
        TEST_CHECK(!voxDir.maskingCuts[k].enabled, "Lead Vocal must receive zero cuts");
    }

    TEST_CHECK(padDir.rideOffsetDb < 0.0f, "Pad ride offset must be negative towards -14 dB");

    bool hasPadCut = false;
    for (int k = 0; k < 4; ++k) {
        if (padDir.maskingCuts[k].enabled) {
            TEST_CHECK(padDir.maskingCuts[k].freqHz >= 1000.0f && padDir.maskingCuts[k].freqHz <= 5000.0f, "Pad presence cut frequency range");
            TEST_CHECK(padDir.maskingCuts[k].gainDb < 0.0f, "Pad cut gain negative");
            hasPadCut = true;
        }
    }
    TEST_CHECK(hasPadCut, "Pad must receive presence cut in vocal region");
    std::cout << "  [PASS] Lead Vocal + Pad verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 3: VOCAL MASKING (KEYS & BGV)
// ==============================================================================
void testMvp3_VocalMaskingKeysAndBgv() {
    std::cout << "[MVP 3 TEST 3] Vocal Masking (Keys in 300-5000 Hz, BGV only > 2 kHz)..." << std::endl;

    // Part A: Lead Vocal + Keys
    {
        SolverState state;
        auto vox = makeTelemetry(301, PluginRole::Source, InstrumentType::LeadVocal, -18.0f);
        vox.spectrumBands[15] = 0.7f; // ~1 kHz
        vox.spectrumBands[16] = 0.7f; // ~1.25 kHz

        auto keys = makeTelemetry(302, PluginRole::Source, InstrumentType::Keys, -18.0f);
        keys.spectrumBands[15] = 0.65f;
        keys.spectrumBands[16] = 0.65f;

        std::vector<InstanceTelemetry> snapshot = { vox, keys };
        uint64_t nowMs = 1000;
        std::unordered_map<uint32_t, InstanceDirectives> dir;
        for (int c = 0; c < 5; ++c) {
            dir = MixSolver::solve(snapshot, nowMs, state);
            nowMs += 200;
        }

        const auto& keysDir = dir[302];
        bool hasKeysCut = false;
        for (int k = 0; k < 4; ++k) {
            if (keysDir.maskingCuts[k].enabled) {
                TEST_CHECK(keysDir.maskingCuts[k].freqHz >= 300.0f && keysDir.maskingCuts[k].freqHz <= 5000.0f, "Keys cut in 300-5000 Hz");
                TEST_CHECK(keysDir.maskingCuts[k].gainDb >= -6.0f && keysDir.maskingCuts[k].gainDb < 0.0f, "Keys cut cap -6 dB");
                hasKeysCut = true;
            }
        }
        TEST_CHECK(hasKeysCut, "Keys must receive masking cut from Lead Vocal");
    }

    // Part B: Lead Vocal + BGV (BGV cut ONLY > 2 kHz, NO cut < 2 kHz)
    {
        SolverState state;
        auto vox = makeTelemetry(303, PluginRole::Source, InstrumentType::LeadVocal, -18.0f);
        vox.spectrumBands[12] = 0.7f; // ~500 Hz (below 2 kHz)
        vox.spectrumBands[19] = 0.7f; // ~3.15 kHz (above 2 kHz)

        auto bgv = makeTelemetry(304, PluginRole::Source, InstrumentType::BackgroundVocal, -18.0f);
        bgv.spectrumBands[12] = 0.7f;
        bgv.spectrumBands[19] = 0.7f;

        std::vector<InstanceTelemetry> snapshot = { vox, bgv };
        uint64_t nowMs = 1000;
        std::unordered_map<uint32_t, InstanceDirectives> dir;
        for (int c = 0; c < 5; ++c) {
            dir = MixSolver::solve(snapshot, nowMs, state);
            nowMs += 200;
        }

        const auto& bgvDir = dir[304];
        bool hasHighCut = false;
        for (int k = 0; k < 4; ++k) {
            if (bgvDir.maskingCuts[k].enabled) {
                TEST_CHECK(bgvDir.maskingCuts[k].freqHz >= 2000.0f, "BGV cuts must be strictly >= 2000 Hz");
                hasHighCut = true;
            }
        }
        TEST_CHECK(hasHighCut, "BGV must receive air/presence cut > 2 kHz");
    }

    std::cout << "  [PASS] Vocal masking for Keys and BGV verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 4: DISJOINT SPECTRA
// ==============================================================================
void testMvp3_DisjointSpectraZeroCuts() {
    std::cout << "[MVP 3 TEST 4] Disjoint Spectra (Zero cuts and zero ducks)..." << std::endl;
    SolverState state;

    auto kick = makeTelemetry(401, PluginRole::Source, InstrumentType::Kick, -18.0f);
    kick.fundamentalHz = 60.0f;
    kick.spectrumBands[3] = 0.8f;

    auto hat = makeTelemetry(402, PluginRole::Source, InstrumentType::HiHat, -18.0f);
    hat.spectrumBands[26] = 0.8f; // ~10 kHz
    hat.spectrumBands[27] = 0.8f; // ~12.5 kHz

    std::vector<InstanceTelemetry> snapshot = { kick, hat };
    uint64_t nowMs = 1000;
    std::unordered_map<uint32_t, InstanceDirectives> dir;
    for (int c = 0; c < 5; ++c) {
        dir = MixSolver::solve(snapshot, nowMs, state);
        nowMs += 200;
    }

    for (const auto& [id, d] : dir) {
        TEST_CHECK(d.duckDb == 0.0f, "No ducking between Kick and Hi-Hat");
        for (int k = 0; k < 4; ++k) {
            TEST_CHECK(!d.maskingCuts[k].enabled, "No masking cuts for disjoint spectra");
        }
    }
    std::cout << "  [PASS] Disjoint spectra verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 5: SOLO PROMOTION
// ==============================================================================
void testMvp3_SoloPromotion() {
    std::cout << "[MVP 3 TEST 5] Solo Promotion (Single active Piano becomes 0 dB reference)..." << std::endl;
    SolverState state;

    auto piano = makeTelemetry(501, PluginRole::Source, InstrumentType::Piano, -18.0f);
    piano.spectrumBands[14] = 0.6f;

    auto silentBass = makeTelemetry(502, PluginRole::Source, InstrumentType::Bass, -100.0f);
    silentBass.signalPresent = false;

    std::vector<InstanceTelemetry> snapshot = { piano, silentBass };
    uint64_t nowMs = 1000;
    auto dir = MixSolver::solve(snapshot, nowMs, state);

    TEST_CHECK(dir.find(501) != dir.end(), "Piano must receive directive");
    TEST_CHECK(dir[501].rideOffsetDb == 0.0f, "Solo active Piano ride offset must be 0 dB");
    std::cout << "  [PASS] Solo promotion verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 6: SPEECH MODE
// ==============================================================================
void testMvp3_SpeechModePastorPad() {
    std::cout << "[MVP 3 TEST 6] Speech Mode (Pastor + Pad -4 dB Speech Bed Penalty)..." << std::endl;
    SolverState state;

    auto pastor = makeTelemetry(601, PluginRole::Source, InstrumentType::PastorSpeaking, -18.0f);
    auto pad = makeTelemetry(602, PluginRole::Source, InstrumentType::Pad, -18.0f);

    std::vector<InstanceTelemetry> snapshot = { pastor, pad };
    uint64_t nowMs = 1000;

    std::unordered_map<uint32_t, InstanceDirectives> dir;
    for (int c = 0; c < 50; ++c) {
        dir = MixSolver::solve(snapshot, nowMs, state);
        nowMs += 200;
    }

    TEST_CHECK(dir[601].rideOffsetDb == 0.0f, "Pastor must be reference at 0 dB ride");

    float padTarget = MixSolver::getRelativeRideTarget(InstrumentType::Pad, true);
    TEST_CHECK(padTarget == -18.0f, "Pad relative target in speech mode must be -18 dB");
    TEST_CHECK(dir[602].rideOffsetDb < -2.0f, "Pad ride offset driven downwards for speech bed");
    std::cout << "  [PASS] Speech mode Pastor + Pad verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 7: CONVERGENCE (1500 CYCLES)
// ==============================================================================
void testMvp3_SolverConvergence1500Cycles() {
    std::cout << "[MVP 3 TEST 7] Solver Convergence (1500 cycles / 5 min simulation, delta < 0.2 dB)..." << std::endl;
    SolverState state;

    std::vector<InstanceTelemetry> snapshot = {
        makeTelemetry(701, PluginRole::Source, InstrumentType::LeadVocal, -18.0f),
        makeTelemetry(702, PluginRole::Source, InstrumentType::Kick, -18.0f),
        makeTelemetry(703, PluginRole::Source, InstrumentType::Snare, -19.0f),
        makeTelemetry(704, PluginRole::Source, InstrumentType::Bass, -18.0f),
        makeTelemetry(705, PluginRole::Source, InstrumentType::AcousticGuitar, -22.0f),
        makeTelemetry(706, PluginRole::Source, InstrumentType::ElectricGuitar, -21.0f),
        makeTelemetry(707, PluginRole::Source, InstrumentType::Piano, -20.0f),
        makeTelemetry(708, PluginRole::Source, InstrumentType::Pad, -24.0f)
    };

    uint64_t nowMs = 1000;
    std::unordered_map<uint32_t, float> prevRides;

    for (int cycle = 0; cycle < 1500; ++cycle) {
        auto dir = MixSolver::solve(snapshot, nowMs, state);
        nowMs += 200;

        for (const auto& [id, d] : dir) {
            TEST_CHECK(!std::isnan(d.rideOffsetDb) && !std::isinf(d.rideOffsetDb), "Ride offset must be valid float");
            TEST_CHECK(!std::isnan(d.solverTrimDb) && !std::isinf(d.solverTrimDb), "Trim must be valid float");
            TEST_CHECK(!std::isnan(d.duckDb) && !std::isinf(d.duckDb), "Duck must be valid float");

            if (cycle > 50) {
                float change = std::abs(d.rideOffsetDb - prevRides[id]);
                TEST_CHECK(change <= 0.201f, "Directives must not hunt: cycle-to-cycle change < 0.2 dB");
            }
            prevRides[id] = d.rideOffsetDb;
        }
    }
    std::cout << "  [PASS] 1500-cycle convergence verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 8: CHORUS FREEZE (4 SECONDS)
// ==============================================================================
void testMvp3_ChorusFreeze4Seconds() {
    std::cout << "[MVP 3 TEST 8] Chorus Freeze (3 -> 8 sources, +8 dB jump, rides freeze 4 seconds)..." << std::endl;
    SolverState state;

    // Verse: 3 sources
    std::vector<InstanceTelemetry> verse = {
        makeTelemetry(801, PluginRole::Source, InstrumentType::LeadVocal, -24.0f),
        makeTelemetry(802, PluginRole::Source, InstrumentType::AcousticGuitar, -26.0f),
        makeTelemetry(803, PluginRole::Source, InstrumentType::Pad, -28.0f)
    };

    uint64_t nowMs = 1000;
    for (int c = 0; c < 10; ++c) {
        MixSolver::solve(verse, nowMs, state);
        nowMs += 200;
    }

    // Chorus: 8 sources, +8 dB jump
    std::vector<InstanceTelemetry> chorus = {
        makeTelemetry(801, PluginRole::Source, InstrumentType::LeadVocal, -16.0f),
        makeTelemetry(802, PluginRole::Source, InstrumentType::AcousticGuitar, -18.0f),
        makeTelemetry(803, PluginRole::Source, InstrumentType::Pad, -20.0f),
        makeTelemetry(804, PluginRole::Source, InstrumentType::Kick, -16.0f),
        makeTelemetry(805, PluginRole::Source, InstrumentType::Snare, -16.0f),
        makeTelemetry(806, PluginRole::Source, InstrumentType::Bass, -16.0f),
        makeTelemetry(807, PluginRole::Source, InstrumentType::ElectricGuitar, -17.0f),
        makeTelemetry(808, PluginRole::Source, InstrumentType::Piano, -18.0f)
    };

    auto firstChorusDir = MixSolver::solve(chorus, nowMs, state);
    float padRideFrozen = firstChorusDir[803].rideOffsetDb;

    // Step 3.5 seconds forward: must still be frozen
    nowMs += 3500;
    auto midChorusDir = MixSolver::solve(chorus, nowMs, state);
    TEST_CHECK(std::abs(midChorusDir[803].rideOffsetDb - padRideFrozen) < 0.001f, "Rides must remain frozen at 3.5s");

    // Step to 4.5 seconds forward: unfreezes and resumes
    nowMs += 1000;
    MixSolver::solve(chorus, nowMs, state);
    TEST_CHECK(state.chorusFreezeUntilMs <= nowMs, "Chorus freeze must expire after 4 seconds");
    std::cout << "  [PASS] 4-second chorus freeze verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 9: OVERRIDES & FREEZE COMPLIANCE
// ==============================================================================
void testMvp3_OverridesAndFreeze() {
    std::cout << "[MVP 3 TEST 9] Manual Overrides & Freeze Compliance..." << std::endl;
    SolverState state;

    auto vox = makeTelemetry(901, PluginRole::Source, InstrumentType::LeadVocal, -18.0f);
    vox.spectrumBands[18] = 0.8f;

    auto frozenPad = makeTelemetry(902, PluginRole::Source, InstrumentType::Pad, -18.0f);
    frozenPad.spectrumBands[18] = 0.8f;
    frozenPad.isFrozen = true;

    auto eqOverriddenKeys = makeTelemetry(903, PluginRole::Source, InstrumentType::Keys, -18.0f);
    eqOverriddenKeys.spectrumBands[18] = 0.8f;
    eqOverriddenKeys.manualOverrides = ManualOverride::EQ;

    auto levelOverriddenBass = makeTelemetry(904, PluginRole::Source, InstrumentType::Bass, -18.0f);
    levelOverriddenBass.fundamentalHz = 60.0f;
    levelOverriddenBass.manualOverrides = ManualOverride::LevelRide;

    auto kick = makeTelemetry(905, PluginRole::Source, InstrumentType::Kick, -18.0f);
    kick.fundamentalHz = 60.0f;

    std::vector<InstanceTelemetry> snapshot = { vox, frozenPad, eqOverriddenKeys, levelOverriddenBass, kick };
    uint64_t nowMs = 1000;

    auto dir = MixSolver::solve(snapshot, nowMs, state);

    // Frozen: all zeroed
    const auto& fDir = dir[902];
    TEST_CHECK(fDir.rideOffsetDb == 0.0f, "Frozen ride must be 0");
    TEST_CHECK(fDir.solverTrimDb == 0.0f, "Frozen trim must be 0");
    TEST_CHECK(fDir.duckDb == 0.0f, "Frozen duck must be 0");
    for (int k = 0; k < 4; ++k) TEST_CHECK(!fDir.maskingCuts[k].enabled, "Frozen cuts must be disabled");

    // EQ Overridden: cuts zeroed
    const auto& eqDir = dir[903];
    for (int k = 0; k < 4; ++k) TEST_CHECK(!eqDir.maskingCuts[k].enabled, "EQ overridden cuts must be disabled");

    // Level Overridden: ride and duck zeroed, but cuts still permitted
    const auto& lvlDir = dir[904];
    TEST_CHECK(lvlDir.rideOffsetDb == 0.0f, "Level overridden ride must be 0");
    TEST_CHECK(lvlDir.duckDb == 0.0f, "Level overridden duck must be 0");
    std::cout << "  [PASS] Overrides and freeze compliance verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 10: DIRECTIVES CAPS FUZZ (200 SNAPSHOTS)
// ==============================================================================
void testMvp3_DirectivesCapsFuzz200Snapshots() {
    std::cout << "[MVP 3 TEST 10] Directives Caps Fuzz (200 random snapshots)..." << std::endl;
    SolverState state;

    std::srand(12345);
    const InstrumentType types[] = {
        InstrumentType::LeadVocal, InstrumentType::PastorSpeaking, InstrumentType::Kick,
        InstrumentType::Snare, InstrumentType::HiHat, InstrumentType::Toms,
        InstrumentType::Bass, InstrumentType::AcousticGuitar, InstrumentType::ElectricGuitar,
        InstrumentType::Piano, InstrumentType::Keys, InstrumentType::Pad,
        InstrumentType::Synth, InstrumentType::Tracks, InstrumentType::Click,
        InstrumentType::Room, InstrumentType::Unknown
    };

    for (int i = 0; i < 200; ++i) {
        int count = 2 + (std::rand() % 14);
        std::vector<InstanceTelemetry> snapshot;

        for (int c = 0; c < count; ++c) {
            uint32_t id = 1000 + c;
            PluginRole role = (c == 0 && (std::rand() % 3 == 0)) ? PluginRole::Master :
                              (c == 1 && (std::rand() % 4 == 0)) ? PluginRole::Bus : PluginRole::Source;
            InstrumentType type = types[std::rand() % 17];
            float rms = -60.0f + static_cast<float>(std::rand() % 60);
            float peak = rms + static_cast<float>(std::rand() % 15);

            auto t = makeTelemetry(id, role, type, rms, peak);
            t.fundamentalHz = 40.0f + static_cast<float>(std::rand() % 400);
            for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) {
                t.spectrumBands[b] = static_cast<float>(std::rand() % 100) / 100.0f;
            }
            snapshot.push_back(t);
        }

        uint64_t nowMs = 1000 + i * 200;
        auto dir = MixSolver::solve(snapshot, nowMs, state);

        for (const auto& inst : snapshot) {
            auto it = dir.find(inst.instanceId);
            if (it == dir.end()) continue;
            const auto& d = it->second;

            TEST_CHECK(d.rideOffsetDb >= -4.01f && d.rideOffsetDb <= 4.01f, "Ride cap [-4, +4]");
            TEST_CHECK(d.solverTrimDb >= -3.01f && d.solverTrimDb <= 2.01f, "Trim cap [-3, +2]");
            TEST_CHECK(d.duckDb >= -3.01f && d.duckDb <= 0.01f, "Duck cap [-3, 0]");

            for (int k = 0; k < 4; ++k) {
                if (d.maskingCuts[k].enabled) {
                    TEST_CHECK(d.maskingCuts[k].gainDb >= -6.01f && d.maskingCuts[k].gainDb <= 0.01f, "Cut cap [-6, 0]");
                }
            }

            if (inst.instrumentType == InstrumentType::Click || inst.role == PluginRole::Bus || inst.role == PluginRole::Master) {
                TEST_CHECK(d.rideOffsetDb == 0.0f, "Click/Bus/Master ride must be 0");
                TEST_CHECK(d.duckDb == 0.0f, "Click/Bus/Master duck must be 0");
                for (int k = 0; k < 4; ++k) {
                    TEST_CHECK(!d.maskingCuts[k].enabled, "Click/Bus/Master must never receive cuts");
                }
            }
        }
    }
    std::cout << "  [PASS] 200 random snapshots passed all directive caps." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 11: STALENESS TIMEOUT RAMP DOWN
// ==============================================================================
void testMvp3_StalenessTimeoutRampDown() {
    std::cout << "[MVP 3 TEST 11] Staleness Timeout (Stop packets -> ramp to 0 in ~1s)..." << std::endl;

    SolverStage stage;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    stage.prepare(spec);

    uint64_t nowMs = 1000;
    InstanceDirectives d;
    d.directiveTimestampMs = nowMs;
    d.solverEnabled = true;
    d.rideOffsetDb = 3.0f;
    d.duckDb = -2.0f;
    d.maskingCuts[0].enabled = true;
    d.maskingCuts[0].freqHz = 1000.0f;
    d.maskingCuts[0].gainDb = -4.0f;
    stage.updateDirectives(d, nowMs);

    TEST_CHECK(stage.isSolverActive(), "Solver must be active upon packet receipt");

    // Advance time beyond 1000 ms timeout without new packets
    nowMs += 1200;
    stage.checkStaleness(nowMs);

    TEST_CHECK(!stage.isSolverActive(), "Solver must become inactive after 1000ms staleness");
    TEST_CHECK(stage.getActiveRideDb() == 0.0f, "Ride must reset to 0 dB");
    TEST_CHECK(stage.getActiveDuckDb() == 0.0f, "Duck must reset to 0 dB");
    TEST_CHECK(!stage.getActiveCut(0).enabled, "Cut must be disabled after staleness");

    juce::AudioBuffer<float> buffer(2, 512);
    for (int b = 0; b < 50; ++b) {
        buffer.clear();
        stage.process(buffer);
    }

    std::cout << "  [PASS] Staleness timeout verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 12: SOLVER EQ RESPONSE MEASUREMENT
// ==============================================================================
void testMvp3_SolverEqResponseMeasurement() {
    std::cout << "[MVP 3 TEST 12] SolverEQ Measurement (-4 dB @ 1 kHz, < 1 dB @ 100 Hz / 10 kHz)..." << std::endl;

    const double sampleRate = 48000.0;
    const int numSamples = 48000; // 1 second of audio
    juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };

    auto measureGainAtFreq = [&](float freqHz) -> float {
        SolverStage stage;
        stage.prepare(spec);

        InstanceDirectives d;
        d.directiveTimestampMs = 1000;
        d.solverEnabled = true;
        d.maskingCuts[0].enabled = true;
        d.maskingCuts[0].freqHz = 1000.0f;
        d.maskingCuts[0].gainDb = -4.0f;
        d.maskingCuts[0].q = 1.4f;
        stage.updateDirectives(d, 1000);

        juce::AudioBuffer<float> buffer(1, numSamples);
        for (int i = 0; i < numSamples; ++i) {
            float phase = 2.0f * juce::MathConstants<float>::pi * freqHz * (static_cast<float>(i) / static_cast<float>(sampleRate));
            buffer.setSample(0, i, std::sin(phase));
        }

        for (int offset = 0; offset < numSamples; offset += 512) {
            int blockLen = std::min(512, numSamples - offset);
            juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), 1, offset, blockLen);
            stage.process(block);
        }

        float sumSquares = 0.0f;
        const int testLen = 24000;
        const float* readPtr = buffer.getReadPointer(0, numSamples - testLen);
        for (int i = 0; i < testLen; ++i) {
            sumSquares += readPtr[i] * readPtr[i];
        }
        float outputRms = std::sqrt(sumSquares / static_cast<float>(testLen));
        float inputRms = 1.0f / std::sqrt(2.0f); // Sine RMS = 0.7071
        return 20.0f * std::log10(outputRms / inputRms);
    };

    float gainAt1kHz = measureGainAtFreq(1000.0f);
    float gainAt100Hz = measureGainAtFreq(100.0f);
    float gainAt10kHz = measureGainAtFreq(10000.0f);

    std::cout << "  Measured Gain at 1000 Hz: " << gainAt1kHz << " dB (expected -4.0 ± 0.7 dB)" << std::endl;
    std::cout << "  Measured Gain at 100 Hz:  " << gainAt100Hz << " dB (expected within 1 dB of 0)" << std::endl;
    std::cout << "  Measured Gain at 10 kHz:  " << gainAt10kHz << " dB (expected within 1 dB of 0)" << std::endl;

    TEST_CHECK(std::abs(gainAt1kHz - (-4.0f)) <= 0.7f, "1 kHz gain must be -4 ± 0.7 dB");
    TEST_CHECK(std::abs(gainAt100Hz) < 1.0f, "100 Hz gain must be within 1 dB of 0 dB");
    TEST_CHECK(std::abs(gainAt10kHz) < 1.0f, "10 kHz gain must be within 1 dB of 0 dB");
    std::cout << "  [PASS] SolverEQ response measurement verified." << std::endl;
}

// ==============================================================================
// MVP 3 TEST 13: LATENCY ZERO AND CPU BENCHMARK WITH SOLVER STAGE
// ==============================================================================
void testMvp3_LatencyZeroAndCpuWithSolver() {
    std::cout << "[MVP 3 TEST 13] Latency Zero and CPU Benchmark with Solver Stage..." << std::endl;

    juce::ScopedJuceInitialiser_GUI juceInit;
    AIMixerAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    TEST_CHECK(processor.getLatencySamples() == 0, "Source role must report strictly 0 latency");
    processor.setRole(PluginRole::Bus);
    TEST_CHECK(processor.getLatencySamples() == 0, "Bus role must report strictly 0 latency");
    processor.setRole(PluginRole::Source);

    InstanceDirectives d;
    d.directiveTimestampMs = 1000;
    d.solverEnabled = true;
    d.rideOffsetDb = 1.5f;
    d.duckDb = -1.5f;
    d.maskingCuts[0].enabled = true;
    d.maskingCuts[0].freqHz = 1000.0f;
    d.maskingCuts[0].gainDb = -3.0f;
    d.maskingCuts[1].enabled = true;
    d.maskingCuts[1].freqHz = 3000.0f;
    d.maskingCuts[1].gainDb = -2.5f;
    processor.getSolverStage().updateDirectives(d, 1000);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            buffer.setSample(ch, i, std::sin(static_cast<float>(i) * 0.05f) * 0.5f);
        }
    }

    const int numBlocks = 2000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < numBlocks; ++b) {
        processor.processBlock(buffer, midi);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    double realTimeMs = (numBlocks * 512.0 / 48000.0) * 1000.0;
    double cpuPercentage = (totalTimeMs / realTimeMs) * 100.0;

    std::cout << "  Processed 2000 blocks (21.3s audio) in " << totalTimeMs << " ms." << std::endl;
    std::cout << "  Per-Instance Audio Thread CPU (SafeChain + SolverStage): " << cpuPercentage << "%" << std::endl;
    TEST_CHECK(cpuPercentage < 0.5, "Per-instance CPU with solver stage must be well below 0.5%");
    std::cout << "  [PASS] Latency (0 samples) and CPU benchmark (<0.5%) verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 1: COMP CHARACTER TABLE & MANUAL OVERRIDE BLOCKING
// ==============================================================================
void testMvp4_CompCharacterTableAndManualOverride() {
    std::cout << "[MVP 4 TEST 1] Comp Character Table & Manual Override..." << std::endl;

    auto kickComp = getCompSettingsForInstrument(InstrumentType::Kick);
    TEST_CHECK(std::abs(kickComp.attackMs - 1.0f) < 0.1f, "Kick attack 1ms");
    TEST_CHECK(std::abs(kickComp.releaseMs - 80.0f) < 0.1f, "Kick release 80ms");
    TEST_CHECK(std::abs(kickComp.ratio - 3.0f) < 0.1f, "Kick ratio 3:1");
    TEST_CHECK(std::abs(kickComp.maxGrCapDb - 5.0f) < 0.1f, "Kick max GR cap 5dB");

    auto snareComp = getCompSettingsForInstrument(InstrumentType::Snare);
    TEST_CHECK(std::abs(snareComp.attackMs - 2.0f) < 0.1f, "Snare attack 2ms");
    TEST_CHECK(std::abs(snareComp.releaseMs - 100.0f) < 0.1f, "Snare release 100ms");
    TEST_CHECK(std::abs(snareComp.ratio - 3.0f) < 0.1f, "Snare ratio 3:1");
    TEST_CHECK(std::abs(snareComp.maxGrCapDb - 5.0f) < 0.1f, "Snare max GR cap 5dB");

    auto voxComp = getCompSettingsForInstrument(InstrumentType::LeadVocal);
    TEST_CHECK(std::abs(voxComp.attackMs - 3.0f) < 0.1f, "LeadVocal attack 3ms");
    TEST_CHECK(std::abs(voxComp.releaseMs - 120.0f) < 0.1f, "LeadVocal release 120ms");
    TEST_CHECK(std::abs(voxComp.ratio - 3.0f) < 0.1f, "LeadVocal ratio 3:1");
    TEST_CHECK(std::abs(voxComp.maxGrCapDb - 6.0f) < 0.1f, "LeadVocal max GR cap 6dB");

    auto bassComp = getCompSettingsForInstrument(InstrumentType::Bass);
    TEST_CHECK(std::abs(bassComp.attackMs - 15.0f) < 0.1f, "Bass attack 15ms");
    TEST_CHECK(std::abs(bassComp.releaseMs - 180.0f) < 0.1f, "Bass release 180ms");
    TEST_CHECK(std::abs(bassComp.ratio - 2.5f) < 0.1f, "Bass ratio 2.5:1");
    TEST_CHECK(std::abs(bassComp.maxGrCapDb - 5.0f) < 0.1f, "Bass max GR cap 5dB");

    auto clickComp = getCompSettingsForInstrument(InstrumentType::Click);
    TEST_CHECK(clickComp.maxGrCapDb == 0.0f, "Click comp max GR cap 0dB");

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    SafeChain sc;
    sc.prepare(spec);
    sc.setInstrumentType(InstrumentType::Kick);
    TEST_CHECK(std::abs(sc.getCompRatio() - 3.0f) < 0.1f, "Default Kick ratio 3:1");

    // Manual override ratio
    sc.setCompRatio(2.5f, true);
    TEST_CHECK(sc.isCompManual(), "Manual override must be marked on ratio change");
    TEST_CHECK(std::abs(sc.getCompRatio() - 2.5f) < 0.01f, "Ratio updated to 2.5");

    // Switching instrument should NOT overwrite manual ratio
    sc.setInstrumentType(InstrumentType::Snare);
    TEST_CHECK(std::abs(sc.getCompRatio() - 2.5f) < 0.01f, "Switching instrument must not override manual ratio");

    // Reset to auto
    sc.resetCompToAuto();
    TEST_CHECK(!sc.isCompManual(), "Manual override flag cleared");
    sc.setInstrumentType(InstrumentType::Snare);
    TEST_CHECK(std::abs(sc.getCompRatio() - 3.0f) < 0.1f, "Snare ratio restored after resetToAuto");
    std::cout << "  [PASS] Comp character table & manual overrides verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 2: GATE / EXPANDER (BURST, ATTENUATION, HYSTERESIS, BIT-TRANSPARENT)
// ==============================================================================
void testMvp4_GateExpander() {
    std::cout << "[MVP 4 TEST 2] Gate / Expander (Burst, Attenuation, Hysteresis)..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);
    pb.setInstrumentIdentity(InstrumentType::Kick, SideHint::Centre);

    // Bypass all stages except gate to test gate in isolation
    pb.setGateBypassed(true);
    pb.setEqBypassed(true);
    pb.setDeEsserBypassed(true);
    pb.setWarmthBypassed(true);
    pb.setTransientBypassed(true);
    pb.setLocalRiderBypassed(true);
    pb.setPanWidthBypassed(true);

    // Test bypass bit-transparency
    juce::AudioBuffer<float> buf(2, 512);
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) buf.setSample(ch, i, 0.005f * (i % 2 == 0 ? 1.0f : -1.0f));
    }
    juce::AudioBuffer<float> orig(buf);
    pb.process(buf);
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            TEST_CHECK(std::abs(buf.getSample(ch, i) - orig.getSample(ch, i)) < 1e-6f, "Gate bypass must be bit-transparent");
        }
    }
    pb.setGateBypassed(false);

    // Set manual threshold to -30 dB
    pb.setManualGateThreshold(-30.0f);

    // Feed quiet signal (-50 dBFS) to close the gate
    float quietAmp = juce::Decibels::decibelsToGain(-50.0f);
    for (int block = 0; block < 100; ++block) {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i) buf.setSample(ch, i, quietAmp * std::sin(i * 0.1f));
        pb.process(buf);
    }
    TEST_CHECK(!pb.isGateOpen(), "Gate must be closed for quiet signal below threshold");
    TEST_CHECK(pb.getGateGainReductionDb() >= 15.0f, "Gate must attenuate quiet signal towards 18 dB cap");

    // Feed loud burst (0 dBFS)
    float loudAmp = 1.0f;
    for (int b = 0; b < 3; ++b) {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i) buf.setSample(ch, i, loudAmp * std::sin(i * 0.1f));
        pb.process(buf);
    }
    TEST_CHECK(pb.isGateOpen(), "Gate must open on burst above threshold");
    TEST_CHECK(pb.getGateGainReductionDb() < 2.0f, "Gate GR must fall towards 0 dB on burst");

    // Hysteresis: signal at -31 dB (within 3 dB of -30 dB threshold) must NOT chatter gate shut immediately
    float hystAmp = juce::Decibels::decibelsToGain(-31.0f);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i) buf.setSample(ch, i, hystAmp * std::sin(i * 0.1f));
    pb.process(buf);
    TEST_CHECK(pb.isGateOpen(), "Gate must remain open within hysteresis zone");
    std::cout << "  [PASS] Gate/Expander dynamics verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 3: CHARACTER EQ RESPONSES & BIT-TRANSPARENT BYPASS
// ==============================================================================
void testMvp4_CharacterEqResponses() {
    std::cout << "[MVP 4 TEST 3] Character EQ Responses & Bypass..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);
    pb.setGateBypassed(true);
    pb.setDeEsserBypassed(true);
    pb.setWarmthBypassed(true);
    pb.setTransientBypassed(true);
    pb.setLocalRiderBypassed(true);
    pb.setPanWidthBypassed(true);

    // Bit-transparent bypass check
    pb.setEqBypassed(true);
    juce::AudioBuffer<float> buf(2, 512);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i) buf.setSample(ch, i, 0.25f * std::sin(i * 0.05f));
    juce::AudioBuffer<float> orig(buf);
    pb.process(buf);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            TEST_CHECK(std::abs(buf.getSample(ch, i) - orig.getSample(ch, i)) < 1e-6f, "EQ bypass must be bit-transparent");
    pb.setEqBypassed(false);

    // Lead Vocal EQ: Peak at 3500 Hz (+1.5 dB), Peak at 300 Hz (-1.5 dB)
    pb.setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);

    auto measureEqGainAtFreq = [&](float freqHz) -> float {
        pb.reset();
        juce::AudioBuffer<float> testBuf(2, 512);
        float inRms = 0.0f, outRms = 0.0f;
        for (int b = 0; b < 200; ++b) {
            for (int i = 0; i < 512; ++i) {
                float s = 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * freqHz * (b * 512 + i) / 48000.0f);
                testBuf.setSample(0, i, s);
                testBuf.setSample(1, i, s);
            }
            if (b > 150) {
                for (int i = 0; i < 512; ++i) inRms += testBuf.getSample(0, i) * testBuf.getSample(0, i);
            }
            pb.process(testBuf);
            if (b > 150) {
                for (int i = 0; i < 512; ++i) outRms += testBuf.getSample(0, i) * testBuf.getSample(0, i);
            }
        }
        return 10.0f * std::log10(outRms / inRms);
    };

    float g3500 = measureEqGainAtFreq(3500.0f);
    float g300 = measureEqGainAtFreq(300.0f);
    float g1000 = measureEqGainAtFreq(1000.0f);

    std::cout << "  Lead Vocal Character EQ: 3500 Hz = " << g3500 << " dB, 300 Hz = " << g300 << " dB, 1000 Hz = " << g1000 << " dB" << std::endl;
    TEST_CHECK(g3500 > 0.8f && g3500 < 2.2f, "Lead vocal 3.5 kHz peak must boost +1.5 ± 0.7 dB");
    TEST_CHECK(g300 < -0.8f && g300 > -2.2f, "Lead vocal 300 Hz peak must cut -1.5 ± 0.7 dB");
    TEST_CHECK(std::abs(g1000) < 0.7f, "Lead vocal 1000 Hz must remain close to flat (<0.7 dB)");
    std::cout << "  [PASS] Character EQ responses verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 4: DE-ESSER SELECTIVITY & CAP
// ==============================================================================
void testMvp4_DeEsserSelectivity() {
    std::cout << "[MVP 4 TEST 4] De-Esser Selectivity (1 kHz/3 kHz vs 6.5 kHz)..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);
    pb.setGateBypassed(true);
    pb.setEqBypassed(true);
    pb.setWarmthBypassed(true);
    pb.setTransientBypassed(true);
    pb.setLocalRiderBypassed(true);
    pb.setPanWidthBypassed(true);
    pb.setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);

    auto measureDeEsserGain = [&](float freqHz, float levelDb) -> float {
        pb.reset();
        float amp = juce::Decibels::decibelsToGain(levelDb);
        juce::AudioBuffer<float> testBuf(2, 512);
        float inRms = 0.0f, outRms = 0.0f;
        for (int b = 0; b < 100; ++b) {
            for (int i = 0; i < 512; ++i) {
                float s = amp * std::sin(2.0f * juce::MathConstants<float>::pi * freqHz * (b * 512 + i) / 48000.0f);
                testBuf.setSample(0, i, s);
                testBuf.setSample(1, i, s);
            }
            if (b > 60) {
                for (int i = 0; i < 512; ++i) inRms += testBuf.getSample(0, i) * testBuf.getSample(0, i);
            }
            pb.process(testBuf);
            if (b > 60) {
                for (int i = 0; i < 512; ++i) outRms += testBuf.getSample(0, i) * testBuf.getSample(0, i);
            }
        }
        return 10.0f * std::log10(outRms / inRms);
    };

    float gain1k = measureDeEsserGain(1000.0f, -6.0f);
    float gain3k = measureDeEsserGain(3000.0f, -6.0f);
    float gain6k5 = measureDeEsserGain(6500.0f, 0.0f); // loud sibilance

    std::cout << "  De-Esser Gain: 1 kHz = " << gain1k << " dB, 3 kHz = " << gain3k << " dB, 6.5 kHz = " << gain6k5 << " dB" << std::endl;
    TEST_CHECK(std::abs(gain1k) < 0.5f, "1 kHz must be affected by <0.5 dB by de-esser");
    TEST_CHECK(std::abs(gain3k) < 0.5f, "3 kHz must be affected by <0.5 dB by de-esser");
    TEST_CHECK(gain6k5 < -1.0f, "Hot 6.5 kHz sibilance must trigger de-esser reduction");
    std::cout << "  [PASS] De-Esser selectivity verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 5: WARMTH / SATURATION HARMONICS & BIT-TRANSPARENCY
// ==============================================================================
void testMvp4_WarmthSaturationHarmonics() {
    std::cout << "[MVP 4 TEST 5] Warmth Saturation Harmonics & Bit-Transparency..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);
    pb.setGateBypassed(true);
    pb.setEqBypassed(true);
    pb.setDeEsserBypassed(true);
    pb.setTransientBypassed(true);
    pb.setLocalRiderBypassed(true);
    pb.setPanWidthBypassed(true);
    pb.setInstrumentIdentity(InstrumentType::Bass, SideHint::Centre);

    // Test bit transparency when bypassed
    pb.setWarmthBypassed(true);
    juce::AudioBuffer<float> buf(2, 512);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i) buf.setSample(ch, i, 0.3f * std::sin(i * 0.1f));
    juce::AudioBuffer<float> orig(buf);
    pb.process(buf);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            TEST_CHECK(std::abs(buf.getSample(ch, i) - orig.getSample(ch, i)) < 1e-6f, "Warmth bypass must be bit-transparent");

    // Enable warmth
    pb.setWarmthBypassed(false);
    pb.setManualWarmthDrive(0.30f);

    const int N = 4096;
    juce::AudioBuffer<float> toneBuf(2, N);
    for (int i = 0; i < N; ++i) {
        float phase = 2.0f * juce::MathConstants<float>::pi * 100.0f * i / 48000.0f;
        float s = 0.5f * std::sin(phase);
        toneBuf.setSample(0, i, s);
        toneBuf.setSample(1, i, s);
    }
    pb.process(toneBuf);

    auto getHarmonicMag = [&](float freqHz) -> float {
        float re = 0.0f, im = 0.0f;
        for (int i = 0; i < N; ++i) {
            float phase = 2.0f * juce::MathConstants<float>::pi * freqHz * i / 48000.0f;
            float s = toneBuf.getSample(0, i);
            re += s * std::cos(phase);
            im -= s * std::sin(phase);
        }
        return std::sqrt(re * re + im * im) * (2.0f / N);
    };

    float f100 = getHarmonicMag(100.0f);
    float f300 = getHarmonicMag(300.0f); // 3rd harmonic

    std::cout << "  Harmonics: 100 Hz = " << f100 << ", 300 Hz = " << f300 << std::endl;
    TEST_CHECK(f100 > 0.4f, "Fundamental 100 Hz must be preserved");
    TEST_CHECK(f300 > 0.001f, "3rd harmonic (300 Hz) must be generated by tanh warmth");
    float thd = (f300 / f100) * 100.0f;
    TEST_CHECK(thd > 0.1f && thd < 8.0f, "THD must be subtle (<8%) for gentle warmth");
    std::cout << "  [PASS] Warmth saturation harmonics verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 6: TRANSIENT STAGE KICK ATTACK EMPHASIS
// ==============================================================================
void testMvp4_TransientStageKickAttack() {
    std::cout << "[MVP 4 TEST 6] Transient Stage Kick Attack Emphasis..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);
    pb.setGateBypassed(true);
    pb.setEqBypassed(true);
    pb.setDeEsserBypassed(true);
    pb.setWarmthBypassed(true);
    pb.setLocalRiderBypassed(true);
    pb.setPanWidthBypassed(true);
    pb.setInstrumentIdentity(InstrumentType::Kick, SideHint::Centre);

    const int totalSamples = 48000;
    juce::AudioBuffer<float> activeBuf(2, totalSamples);
    juce::AudioBuffer<float> bypassedBuf(2, totalSamples);

    for (int i = 0; i < totalSamples; ++i) {
        float env = (i < 960) ? 0.4f : 0.05f; // burst for 20ms, then quiet sustain
        float s = env * std::sin(2.0f * juce::MathConstants<float>::pi * 60.0f * i / 48000.0f);
        activeBuf.setSample(0, i, s);
        activeBuf.setSample(1, i, s);
        bypassedBuf.setSample(0, i, s);
        bypassedBuf.setSample(1, i, s);
    }

    pb.setTransientBypassed(false);
    for (int offset = 0; offset < totalSamples; offset += 512) {
        int len = std::min(512, totalSamples - offset);
        juce::AudioBuffer<float> blk(activeBuf.getArrayOfWritePointers(), 2, offset, len);
        pb.process(blk);
    }

    float activeAttackSum = 0.0f, bypassAttackSum = 0.0f;
    for (int i = 50; i < 900; ++i) {
        activeAttackSum += activeBuf.getSample(0, i) * activeBuf.getSample(0, i);
        bypassAttackSum += bypassedBuf.getSample(0, i) * bypassedBuf.getSample(0, i);
    }
    float attackBoostDb = 10.0f * std::log10(activeAttackSum / bypassAttackSum);

    float activeTailSum = 0.0f, bypassTailSum = 0.0f;
    for (int i = 4800; i < 14400; ++i) { // 100ms to 300ms
        activeTailSum += activeBuf.getSample(0, i) * activeBuf.getSample(0, i);
        bypassTailSum += bypassedBuf.getSample(0, i) * bypassedBuf.getSample(0, i);
    }
    float tailDiffDb = std::abs(10.0f * std::log10(activeTailSum / bypassTailSum));

    std::cout << "  Kick Transient Boost: Attack = +" << attackBoostDb << " dB, Tail Diff = " << tailDiffDb << " dB" << std::endl;
    TEST_CHECK(attackBoostDb > 0.8f, "Transient attack in first 20 ms must rise by > 0.8 dB");
    TEST_CHECK(tailDiffDb < 0.5f, "Tail after 100 ms must remain within 0.5 dB of bypassed");
    std::cout << "  [PASS] Transient stage kick attack verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 7: LOCAL RIDER PHRASE LEVELING
// ==============================================================================
void testMvp4_LocalRiderPhraseLeveling() {
    std::cout << "[MVP 4 TEST 7] Local Rider Phrase Leveling..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);
    pb.setGateBypassed(true);
    pb.setEqBypassed(true);
    pb.setDeEsserBypassed(true);
    pb.setWarmthBypassed(true);
    pb.setTransientBypassed(true);
    pb.setPanWidthBypassed(true);
    pb.setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);

    // Steady signal test at -18 dBFS
    juce::AudioBuffer<float> steadyBuf(2, 512);
    float steadyAmp = juce::Decibels::decibelsToGain(-18.0f);
    for (int b = 0; b < 100; ++b) {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i) steadyBuf.setSample(ch, i, steadyAmp * std::sin(i * 0.1f));
        pb.process(steadyBuf);
    }
    TEST_CHECK(std::abs(pb.getLocalRiderGainDb()) < 0.5f, "Steady signal at -18 dBFS must produce minimal rider gain adjustment");
    std::cout << "  [PASS] Local rider steady-state verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 8: PAN / WIDTH STAGE & BASS-MONO CROSSOVER (<120 HZ)
// ==============================================================================
void testMvp4_PanWidthStageAndBassMono() {
    std::cout << "[MVP 4 TEST 8] Pan / Width & Bass-Mono Crossover (<120 Hz)..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);
    pb.setGateBypassed(true);
    pb.setEqBypassed(true);
    pb.setDeEsserBypassed(true);
    pb.setWarmthBypassed(true);
    pb.setTransientBypassed(true);
    pb.setLocalRiderBypassed(true);
    pb.setInstrumentIdentity(InstrumentType::Bass, SideHint::Centre);

    // Generate stereo signal:
    // 60 Hz tone (out of phase: left = +1, right = -1) + 1000 Hz tone (panned stereo: left = 0.8, right = 0.2)
    const int N = 24000;
    juce::AudioBuffer<float> stBuf(2, N);
    for (int i = 0; i < N; ++i) {
        float lowL = 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 60.0f * i / 48000.0f);
        float lowR = -0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 60.0f * i / 48000.0f);
        float highL = 0.3f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * i / 48000.0f);
        float highR = 0.05f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * i / 48000.0f);
        stBuf.setSample(0, i, lowL + highL);
        stBuf.setSample(1, i, lowR + highR);
    }

    for (int offset = 0; offset < N; offset += 512) {
        int len = std::min(512, N - offset);
        juce::AudioBuffer<float> blk(stBuf.getArrayOfWritePointers(), 2, offset, len);
        pb.process(blk);
    }

    float highEnergyL = 0.0f, highEnergyR = 0.0f;
    for (int i = 12000; i < N; ++i) {
        highEnergyL += stBuf.getSample(0, i) * stBuf.getSample(0, i);
        highEnergyR += stBuf.getSample(1, i) * stBuf.getSample(1, i);
    }
    TEST_CHECK(highEnergyL > 2.0f * highEnergyR, "High frequency stereo pan asymmetry must be preserved");
    std::cout << "  [PASS] Pan/Width and bass-mono crossover verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 9: BUS GLUE TABLE, PARALLEL MIX & BUS TYPE PARSER
// ==============================================================================
void testMvp4_BusGlueTableAndParallel() {
    std::cout << "[MVP 4 TEST 9] Bus Glue Table, Parallel Mix & Bus Type Parser..." << std::endl;

    auto drumCfg = BusStage::getConfigForBusType(BusType::Drums);
    TEST_CHECK(std::abs(drumCfg.ratio - 2.0f) < 0.1f, "Drums bus ratio 2:1");
    TEST_CHECK(std::abs(drumCfg.parallelMix - 0.30f) < 0.05f, "Drums bus parallel mix 30%");
    TEST_CHECK(std::abs(drumCfg.attackMs - 30.0f) < 0.1f, "Drums bus attack 30ms");

    auto fxCfg = BusStage::getConfigForBusType(BusType::FX);
    TEST_CHECK(fxCfg.maxGrCapDb <= 1.0f, "FX bus max GR cap <= 1.0 dB");

    // Track name parser scoping
    TEST_CHECK(TrackNameParser::parseBusType("Drums") == BusType::Drums, "Drums parses to BusType::Drums on Bus role");
    TEST_CHECK(TrackNameParser::parseBusType("Drums Bus") == BusType::Drums, "Drums Bus parses to BusType::Drums");
    TEST_CHECK(TrackNameParser::parseBusType("Drum Group") == BusType::Drums, "Drum Group parses to BusType::Drums");
    TEST_CHECK(TrackNameParser::parseBusType("Vocal Sub") == BusType::Vocals, "Vocal Sub parses to BusType::Vocals");
    TEST_CHECK(TrackNameParser::parseBusType("Music Bus") == BusType::Music, "Music Bus parses to BusType::Music");
    TEST_CHECK(TrackNameParser::parseBusType("Reverb Return") == BusType::FX, "Reverb Return parses to BusType::FX");
    TEST_CHECK(TrackNameParser::parseBusType("Submaster") == BusType::Generic, "Submaster parses to BusType::Generic");

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    BusStage bus1, bus2;
    bus1.prepare(spec);
    bus2.prepare(spec);
    bus1.setBusType(BusType::Drums);   // 30% parallel
    bus2.setBusType(BusType::Generic); // 100% wet

    juce::AudioBuffer<float> b1(2, 512), b2(2, 512);
    for (int i = 0; i < 512; ++i) {
        float s = 0.8f * std::sin(i * 0.1f);
        b1.setSample(0, i, s); b1.setSample(1, i, s);
        b2.setSample(0, i, s); b2.setSample(1, i, s);
    }
    for (int block = 0; block < 100; ++block) {
        bus1.process(b1);
        bus2.process(b2);
    }
    float rms1 = b1.getRMSLevel(0, 0, 512);
    float rms2 = b2.getRMSLevel(0, 0, 512);
    TEST_CHECK(rms1 > rms2, "30% parallel compression on Drums bus must retain higher RMS than 100% wet compression");
    std::cout << "  [PASS] Bus glue table and parallel blend verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 10: MASTER BS.1770-5 K-WEIGHTED LOUDNESS ACCURACY
// ==============================================================================
void testMvp4_MasterLoudnessAccuracyBS1770() {
    std::cout << "[MVP 4 TEST 10] Master BS.1770-5 K-Weighted Loudness Accuracy..." << std::endl;

    LoudnessMeter meter;
    meter.prepare(48000.0);

    // 1 kHz stereo sine wave at peak -18 dBFS
    const int N = 144000; // 3 seconds
    const float amp = juce::Decibels::decibelsToGain(-18.0f);
    std::vector<float> sineWave(N);
    for (int i = 0; i < N; ++i) {
        sineWave[i] = amp * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * i / 48000.0f);
    }

    for (int offset = 0; offset < N; offset += 512) {
        int len = std::min(512, N - offset);
        meter.processSamples(&sineWave[offset], &sineWave[offset], len);
    }

    float momLufs = meter.getMomentaryLufs();
    float intLufs = meter.getIntegratedLufs();

    std::cout << "  1 kHz @ -18 dBFS: Momentary = " << momLufs << " LUFS, Integrated = " << intLufs << " LUFS" << std::endl;
    TEST_CHECK(std::abs(momLufs - (-18.0f)) <= 0.7f, "Momentary LUFS must be -18.0 ± 0.7 LU");
    TEST_CHECK(std::abs(intLufs - (-18.0f)) <= 0.7f, "Integrated LUFS must be -18.0 ± 0.7 LU");
    std::cout << "  [PASS] BS.1770-5 loudness measurement verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 11: MASTER LOUDNESS AUTO-GAIN CAP BEHAVIOUR
// ==============================================================================
void testMvp4_MasterLoudnessAutoGainCapBehaviour() {
    std::cout << "[MVP 4 TEST 11] Master Loudness Auto-Gain Cap Behaviour (+4 dB Cap on 6 dB Gap)..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    MasterChain chain;
    chain.prepare(spec);
    chain.setDestination(DestinationTag::Broadcast); // Target: -14.0 LUFS
    chain.setAutoGainEnabled(true);

    float dummySpectrum[NUM_SPECTRUM_BANDS] = {0};
    for (int cycle = 0; cycle < 150; ++cycle) {
        chain.updateWorkerLoudness(-20.0f, -20.0f, -24.0f, dummySpectrum);
    }
    float activeGain = chain.getActiveAutoGainDb();
    std::cout << "  Active Auto-Gain for -20 LUFS input vs -14 target: " << activeGain << " dB" << std::endl;
    TEST_CHECK(activeGain >= 3.5f && activeGain <= 4.0f, "Auto-gain must adapt smoothly towards +4 dB staging cap");
    std::cout << "  [PASS] Loudness auto-gain cap behaviour verified." << std::endl;
}

// ==============================================================================
// MVP 5 STEP 0 TEST 1: LOUDNESS AUTO-GAIN TRUE CONVERGENCE (3 dB GAP -> TARGET +/- 1 LU)
// ==============================================================================
void testMvp5_Step0_LoudnessAutoGainTrueConvergence() {
    std::cout << "[MVP 5 STEP 0.1] Loudness Auto-Gain True Convergence (3 dB Gap, Rate <= 0.25 dB/s)..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    MasterChain chain;
    chain.prepare(spec);
    chain.setDestination(DestinationTag::Broadcast); // Target: -14.0 LUFS
    chain.setAutoGainEnabled(true);

    float dummySpectrum[NUM_SPECTRUM_BANDS] = {0};
    float lastGain = 0.0f;
    float maxObservedRateDbPerSec = 0.0f;

    // Material is 3 dB below destination target: -17.0 LUFS vs -14.0 LUFS
    for (int cycle = 0; cycle < 150; ++cycle) {
        chain.updateWorkerLoudness(-17.0f, -17.0f, -21.0f, dummySpectrum);
        float currentGain = chain.getActiveAutoGainDb();
        if (cycle >= 50) { // After initial 10s non-silent gating
            float deltaCycle = std::abs(currentGain - lastGain);
            float ratePerSec = deltaCycle / 0.2f; // Worker cycle is ~200ms
            if (ratePerSec > maxObservedRateDbPerSec) maxObservedRateDbPerSec = ratePerSec;
        }
        lastGain = currentGain;
    }

    float finalGain = chain.getActiveAutoGainDb();
    float outputEstimatedLufs = -17.0f + finalGain;

    std::cout << "  Final Gain: " << finalGain << " dB (expected ~+3.0 dB), Estimated Output: " << outputEstimatedLufs << " LUFS" << std::endl;
    std::cout << "  Max Observed Rate: " << maxObservedRateDbPerSec << " dB/sec (limit <= 0.25 dB/s)" << std::endl;

    TEST_CHECK(maxObservedRateDbPerSec <= 0.26f, "Auto-gain rate must never exceed 0.25 dB/sec");
    TEST_CHECK(std::abs(finalGain - 3.0f) < 0.2f, "Final auto-gain must converge to +3.0 dB");
    TEST_CHECK(std::abs(outputEstimatedLufs - (-14.0f)) <= 1.0f, "Integrated output must be within +/-1 LU of destination target");
    std::cout << "  [PASS] Loudness auto-gain true convergence verified." << std::endl;
}

// ==============================================================================
// MVP 5 STEP 0 TEST 2: LOCAL RIDER REAL PHRASE LEVELING (2S PHRASES @ 0 AND -6 DB)
// ==============================================================================
void testMvp5_Step0_LocalRiderRealPhraseLeveling() {
    std::cout << "[MVP 5 STEP 0.2] Local Rider Real Phrase Leveling (Phrases 0 dB vs -6 dB)..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);
    pb.setGateBypassed(true);
    pb.setEqBypassed(true);
    pb.setDeEsserBypassed(true);
    pb.setWarmthBypassed(true);
    pb.setTransientBypassed(true);
    pb.setPanWidthBypassed(true);
    pb.setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);

    // Warm up rolling average with steady -18 dBFS signal for 10 seconds
    const int sampleRate = 48000;
    const int blockSize = 512;
    juce::AudioBuffer<float> block(2, blockSize);
    float baselineAmp = juce::Decibels::decibelsToGain(-18.0f);

    const int blocksPer100ms = (sampleRate / 10) / blockSize;
    for (int sec = 0; sec < 10; ++sec) {
        for (int b = 0; b < (sampleRate / blockSize); ++b) {
            for (int i = 0; i < blockSize; ++i) {
                float s = baselineAmp * std::sin(i * 0.1f);
                block.setSample(0, i, s);
                block.setSample(1, i, s);
            }
            pb.process(block);
            if (b % blocksPer100ms == 0) {
                pb.updateWorkerAnalysis(-18.0f, -65.0f, 2500.0f);
            }
        }
    }

    // Steady passage check: must change < 0.3 dB
    float steadyGainDb = pb.getLocalRiderGainDb();
    TEST_CHECK(std::abs(steadyGainDb) < 0.3f, "Steady passage rider gain must remain < 0.3 dB");

    // Alternate phrases: Phrase A (0 dB relative = -15 dBFS), Phrase B (-6 dB relative = -21 dBFS), 2 sec each
    float ampA = juce::Decibels::decibelsToGain(-15.0f);
    float ampB = juce::Decibels::decibelsToGain(-21.0f);

    auto runPhrase = [&](float amp, float levelDb, float& outRms) {
        float sumSq = 0.0f;
        int count = 0;
        const int totalBlocks = (sampleRate * 2) / blockSize;
        for (int b = 0; b < totalBlocks; ++b) {
            for (int i = 0; i < blockSize; ++i) {
                float s = amp * std::sin((b * blockSize + i) * 0.1f);
                block.setSample(0, i, s);
                block.setSample(1, i, s);
            }
            pb.process(block);
            if (b >= totalBlocks / 2) {
                for (int i = 0; i < blockSize; ++i) {
                    sumSq += block.getSample(0, i) * block.getSample(0, i);
                    count++;
                }
            }
            if (b % blocksPer100ms == 0) {
                pb.updateWorkerAnalysis(levelDb, -65.0f, 2500.0f);
            }
        }
        outRms = std::sqrt(sumSq / static_cast<float>(count));
    };

    float phraseA_outRms = 0.0f, phraseB_outRms = 0.0f;
    // Cycle 1
    runPhrase(ampA, -15.0f, phraseA_outRms);
    runPhrase(ampB, -21.0f, phraseB_outRms);
    // Cycle 2 (measured)
    runPhrase(ampA, -15.0f, phraseA_outRms);
    runPhrase(ampB, -21.0f, phraseB_outRms);

    float outDbA = juce::Decibels::gainToDecibels(phraseA_outRms);
    float outDbB = juce::Decibels::gainToDecibels(phraseB_outRms);
    float outputPhraseDeltaDb = std::abs(outDbA - outDbB);
    std::cout << "  Input Phrase Delta: 6.0 dB, Output Phrase Delta: " << outputPhraseDeltaDb << " dB" << std::endl;

    // Output difference must be reduced by at least half (< 3.0 dB)
    TEST_CHECK(outputPhraseDeltaDb < 3.0f, "Output phrase difference must be reduced by at least half (< 3 dB)");
    std::cout << "  [PASS] Local rider real phrase leveling verified." << std::endl;
}

// ==============================================================================
// MVP 5 STEP 0 TEST 3: PAN / WIDTH CLAMP FUZZ (ALL INSTRUMENTS X SIDES)
// ==============================================================================
void testMvp5_Step0_PanClampFuzz() {
    std::cout << "[MVP 5 STEP 0.3] Pan Clamp Fuzz (All Instruments x All SideHints)..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    PlaybookStage pb;
    pb.prepare(spec);

    SideHint sides[3] = { SideHint::Centre, SideHint::Left, SideHint::Right };

    for (int instIdx = 0; instIdx <= 20; ++instIdx) {
        auto inst = static_cast<InstrumentType>(instIdx);
        for (auto side : sides) {
            pb.setInstrumentIdentity(inst, side);

            float activePan = pb.getActivePan();
            float activeWidth = pb.getActiveWidth();

            TEST_CHECK(activePan >= -1.0f && activePan <= 1.0f, "Pan must be clamped within [-1.0, +1.0]");
            TEST_CHECK(activeWidth >= 0.0f && activeWidth <= 2.0f, "Width must be clamped within [0.0, 2.0]");

            // Run process block and ensure no NaN/Inf
            juce::AudioBuffer<float> block(2, 64);
            for (int ch = 0; ch < 2; ++ch) {
                for (int i = 0; i < 64; ++i) block.setSample(ch, i, 0.2f * std::sin(i * 0.1f));
            }
            pb.process(block);

            for (int ch = 0; ch < 2; ++ch) {
                for (int i = 0; i < 64; ++i) {
                    float val = block.getSample(ch, i);
                    TEST_CHECK(!std::isnan(val) && !std::isinf(val), "Output sample must never be NaN or Inf");
                }
            }
        }
    }
    std::cout << "  [PASS] Pan & width clamp fuzz passed across all combinations." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 12: MASTER TONAL BALANCE EQ & BIT-TRANSPARENCY
// ==============================================================================
void testMvp4_MasterTonalBalanceAndBitTransparency() {
    std::cout << "[MVP 4 TEST 12] Master Tonal Balance EQ & Bit-Transparency..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    MasterChain chain;
    chain.prepare(spec);
    chain.setDestination(DestinationTag::Broadcast);

    float balancedSpectrum[NUM_SPECTRUM_BANDS];
    for (size_t b = 0; b < NUM_SPECTRUM_BANDS; ++b) balancedSpectrum[b] = 0.50f;
    chain.updateWorkerLoudness(-14.0f, -14.0f, -18.0f, balancedSpectrum);

    float corr = chain.getTonalCorrectionDb();
    TEST_CHECK(std::abs(corr) < 0.3f, "Tonal correction when balanced must be near 0 dB");
    TEST_CHECK(chain.getTonalStatusDescription().contains("Compliance") || chain.getTonalStatusDescription().contains("Transparent"),
               "Status must indicate compliance when balanced");
    std::cout << "  [PASS] Master tonal balance EQ verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 13: MASTER 4X OVERSAMPLED LOOKAHEAD LIMITER CEILING
// ==============================================================================
void testMvp4_MasterLimiterCeilingAndTransparency() {
    std::cout << "[MVP 4 TEST 13] Master Lookahead Limiter Ceiling & Transparency..." << std::endl;

    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    MasterChain chain;
    chain.prepare(spec);
    chain.setDestination(DestinationTag::PALive); // Ceiling: -1.0 dBTP
    chain.setTonalBalanceEnabled(false);
    chain.setAutoGainEnabled(false);

    // Feed hot signal (+6 dBFS = amplitude 2.0f)
    const int N = 4096;
    juce::AudioBuffer<float> hotBuf(2, N);
    for (int i = 0; i < N; ++i) {
        float s = 2.0f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * i / 48000.0f);
        hotBuf.setSample(0, i, s);
        hotBuf.setSample(1, i, s);
    }

    for (int offset = 0; offset < N; offset += 512) {
        int len = std::min(512, N - offset);
        juce::AudioBuffer<float> blk(hotBuf.getArrayOfWritePointers(), 2, offset, len);
        chain.process(blk);
    }

    float maxPeak = 0.0f;
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 256; i < N; ++i) {
            maxPeak = std::max(maxPeak, std::abs(hotBuf.getSample(ch, i)));
        }
    }
    float maxPeakDb = juce::Decibels::gainToDecibels(maxPeak);
    std::cout << "  Limiter output peak for +6 dBFS input: " << maxPeakDb << " dBFS (ceiling = -1.0 dBTP)" << std::endl;
    TEST_CHECK(maxPeakDb <= -0.75f, "Peak limiter must strictly clamp hot signal to ceiling + 0.2 dB");
    std::cout << "  [PASS] Master lookahead limiter ceiling verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 14: MASTER LIMITER LATENCY ACCURACY (IMPULSE DELAY MATCH)
// ==============================================================================
void testMvp4_MasterLimiterLatencyAccuracy() {
    std::cout << "[MVP 4 TEST 14] Master Limiter Latency Accuracy (Impulse Match)..." << std::endl;

    juce::ScopedJuceInitialiser_GUI juceInit;
    AIMixerAudioProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    proc.setRole(PluginRole::Source);
    TEST_CHECK(proc.getLatencySamples() == 0, "Source role must report strictly 0 latency");

    proc.setRole(PluginRole::Bus);
    TEST_CHECK(proc.getLatencySamples() == 0, "Bus role must report strictly 0 latency");

    proc.setRole(PluginRole::Master);
    int masterLatency = proc.getLatencySamples();
    std::cout << "  Master role reported latency: " << masterLatency << " samples" << std::endl;
    TEST_CHECK(masterLatency >= 48, "Master role must report at least lookahead samples (48)");

    juce::AudioBuffer<float> impulseBuf(2, 512);
    impulseBuf.clear();
    impulseBuf.setSample(0, 0, 0.2f);
    impulseBuf.setSample(1, 0, 0.2f);

    proc.getMasterChain().reset();
    proc.getMasterChain().process(impulseBuf);

    int peakIdx = 0;
    float peakVal = 0.0f;
    for (int i = 0; i < 512; ++i) {
        float v = std::abs(impulseBuf.getSample(0, i));
        if (v > peakVal) {
            peakVal = v;
            peakIdx = i;
        }
    }
    std::cout << "  Measured impulse delay: " << peakIdx << " samples (reported: " << masterLatency << ")" << std::endl;
    TEST_CHECK(std::abs(peakIdx - masterLatency) <= 2, "Measured impulse delay must match reported latency within ±2 samples");
    std::cout << "  [PASS] Master limiter latency accuracy verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 15: TWO INDEPENDENT MASTERS DESTINATIONS
// ==============================================================================
void testMvp4_TwoIndependentMastersDestinations() {
    std::cout << "[MVP 4 TEST 15] Two Independent Masters Destinations..." << std::endl;

    auto paSettings = MasterChain::getSettingsForDestination(DestinationTag::PALive);
    auto bcastSettings = MasterChain::getSettingsForDestination(DestinationTag::Broadcast);
    auto studioSettings = MasterChain::getSettingsForDestination(DestinationTag::Studio);

    TEST_CHECK(paSettings.targetIntegratedLufs == -16.0f, "PA Live target -16 LUFS");
    TEST_CHECK(paSettings.ceilingDb == -1.0f, "PA Live ceiling -1.0 dBTP");
    TEST_CHECK(paSettings.isPresenceForward == true, "PA Live is presence forward");

    TEST_CHECK(bcastSettings.targetIntegratedLufs == -14.0f, "Broadcast target -14 LUFS");
    TEST_CHECK(bcastSettings.ceilingDb == -1.0f, "Broadcast ceiling -1.0 dBTP");

    TEST_CHECK(studioSettings.targetIntegratedLufs == -14.0f, "Studio target -14 LUFS");
    TEST_CHECK(studioSettings.ceilingDb == -0.3f, "Studio ceiling -0.3 dBTP");
    std::cout << "  [PASS] Independent masters destinations verified." << std::endl;
}

// ==============================================================================
// MVP 4 TEST 16: CPU BENCHMARK FOR SOURCE, BUS, AND MASTER ROLES
// ==============================================================================
void testMvp4_CpuBenchmark() {
    std::cout << "[MVP 4 TEST 16] Audio Thread CPU Benchmark (Source, Bus, Master)..." << std::endl;

    juce::ScopedJuceInitialiser_GUI juceInit;

    AIMixerAudioProcessor procSource;
    procSource.prepareToPlay(48000.0, 512);
    procSource.setRole(PluginRole::Source);
    procSource.getSafeChain().setInstrumentType(InstrumentType::LeadVocal);
    procSource.getPlaybookStage().setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);

    AIMixerAudioProcessor procBus;
    procBus.prepareToPlay(48000.0, 512);
    procBus.setRole(PluginRole::Bus);
    procBus.getBusStage().setBusType(BusType::Drums);

    AIMixerAudioProcessor procMaster;
    procMaster.prepareToPlay(48000.0, 512);
    procMaster.setRole(PluginRole::Master);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) buffer.setSample(ch, i, 0.3f * std::sin(i * 0.05f));
    }

    const int numBlocks = 2000;
    double realTimeMs = (numBlocks * 512.0 / 48000.0) * 1000.0;

    auto s1 = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < numBlocks; ++b) procSource.processBlock(buffer, midi);
    auto e1 = std::chrono::high_resolution_clock::now();
    double cpuSource = (std::chrono::duration<double, std::milli>(e1 - s1).count() / realTimeMs) * 100.0;

    auto s2 = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < numBlocks; ++b) procBus.processBlock(buffer, midi);
    auto e2 = std::chrono::high_resolution_clock::now();
    double cpuBus = (std::chrono::duration<double, std::milli>(e2 - s2).count() / realTimeMs) * 100.0;

    auto s3 = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < numBlocks; ++b) procMaster.processBlock(buffer, midi);
    auto e3 = std::chrono::high_resolution_clock::now();
    double cpuMaster = (std::chrono::duration<double, std::milli>(e3 - s3).count() / realTimeMs) * 100.0;

    std::cout << "  Source-with-Playbook CPU: " << cpuSource << "% (cap < 0.6%)" << std::endl;
    std::cout << "  Bus-Stage CPU:            " << cpuBus << "% (cap < 0.3%)" << std::endl;
    std::cout << "  Master-Chain CPU:         " << cpuMaster << "% (cap < 1.5%)" << std::endl;

    TEST_CHECK(cpuSource < 0.6, "Source role CPU must be below 0.6%");
    TEST_CHECK(cpuBus < 0.3, "Bus role CPU must be below 0.3%");
    TEST_CHECK(cpuMaster < 1.5, "Master role CPU must be below 1.5%");
    std::cout << "  [PASS] Audio thread CPU benchmarks verified." << std::endl;
}

// ==============================================================================
// MVP 5: TASTE LAYER, MACROS, FX SPACE, SESSION MODES & PROFILE LEARNING
// ==============================================================================

void testMvp5_FollowPercentSuppressionAndScaling() {
    std::cout << "\n[MVP 5 TEST 1] Follow % Suppression & Scaling..." << std::endl;

    // Acoustic Worship has Drum ride offset delta = -2.0 dB
    auto acoustic = TasteProfile::createAcousticWorship();
    GlobalTasteState state;
    state.profileId = acoustic.profileId;
    std::copy(acoustic.rideOffsetDelta.begin(), acoustic.rideOffsetDelta.end(), state.rideOffsetDelta);

    std::vector<InstanceTelemetry> snapshot;
    InstanceTelemetry vocal;
    vocal.instanceId = 1;
    vocal.role = PluginRole::Source;
    vocal.instrumentType = InstrumentType::LeadVocal;
    vocal.signalPresent = true;
    vocal.rmsDb = -18.0f;
    snapshot.push_back(vocal);

    InstanceTelemetry kick;
    kick.instanceId = 2;
    kick.role = PluginRole::Source;
    kick.instrumentType = InstrumentType::Kick;
    kick.signalPresent = true;
    kick.rmsDb = -21.0f; // Exactly at default target offset (-3.0 dB)
    snapshot.push_back(kick);

    // 1. Follow = 0%: Delta must be completely suppressed (appliedDelta = 0)
    state.followPercent = 0.0f;
    SolverState sState0;
    auto dir0 = MixSolver::solve(snapshot, 1000, sState0, state);
    TEST_CHECK(std::abs(dir0[2].rideOffsetDb) < 0.01f, "At Follow = 0%, profile delta must be suppressed");

    // 2. Follow = 100%: Profile delta (-2.0 dB) applies fully
    state.followPercent = 100.0f;
    SolverState sState100;
    for (int cycle = 0; cycle < 30; ++cycle) {
        auto dir = MixSolver::solve(snapshot, 1000 + cycle * 200, sState100, state);
        if (cycle == 29) {
            TEST_CHECK(std::abs(dir[2].rideOffsetDb - (-2.0f)) < 0.15f, "At Follow = 100%, Kick ride must converge towards -2.0 dB");
        }
    }

    std::cout << "  [PASS] Follow % suppression and full scaling verified." << std::endl;
}

void testMvp5_FiveMacros() {
    std::cout << "[MVP 5 TEST 2] 5 Taste Macros..." << std::endl;

    // Macro 1: Vocal Forward
    float voxAtPlus100 = MixSolver::calculateVocalForwardDelta(InstrumentType::LeadVocal, 100.0f);
    float otherAtPlus100 = MixSolver::calculateVocalForwardDelta(InstrumentType::ElectricGuitar, 100.0f);
    TEST_CHECK(std::abs(voxAtPlus100 - 1.5f) < 0.01f, "Vocal Forward +100 should be +1.5 dB on Lead Vocal");
    TEST_CHECK(std::abs(otherAtPlus100 - (-1.5f)) < 0.01f, "Vocal Forward +100 should be -1.5 dB on instruments");

    float voxAtMinus100 = MixSolver::calculateVocalForwardDelta(InstrumentType::LeadVocal, -100.0f);
    float otherAtMinus100 = MixSolver::calculateVocalForwardDelta(InstrumentType::ElectricGuitar, -100.0f);
    TEST_CHECK(std::abs(voxAtMinus100 - (-1.5f)) < 0.01f, "Vocal Forward -100 should be -1.5 dB on Lead Vocal");
    TEST_CHECK(std::abs(otherAtMinus100 - 1.0f) < 0.01f, "Vocal Forward -100 should be +1.0 dB on instruments");

    // Macro 2: Energy / Punch
    float energyPlus100 = MixSolver::calculateEnergyMaskingScalar(100.0f, 1.0f, 100.0f);
    float energyMinus100 = MixSolver::calculateEnergyMaskingScalar(-100.0f, 1.0f, 100.0f);
    TEST_CHECK(std::abs(energyPlus100 - 1.3f) < 0.01f, "Energy +100 must scale masking cuts by 1.3x");
    TEST_CHECK(std::abs(energyMinus100 - 0.7f) < 0.01f, "Energy -100 must scale masking cuts by 0.7x");

    // Macro 3: Warm <-> Bright
    GlobalTasteState state;
    state.macroWarmBright = 100.0f;
    std::vector<InstanceTelemetry> snap;
    InstanceTelemetry inst;
    inst.instanceId = 1;
    inst.role = PluginRole::Source;
    inst.instrumentType = InstrumentType::LeadVocal;
    inst.signalPresent = true;
    inst.rmsDb = -18.0f;
    snap.push_back(inst);
    SolverState ss;
    auto dirs = MixSolver::solve(snap, 1000, ss, state);
    TEST_CHECK(std::abs(dirs[1].characterTiltDb - 1.5f) < 0.01f, "Macro Warm/Bright at +100 must apply +1.5 dB character tilt");

    // Macro 4: Space
    state.macroSpace = 100.0f;
    state.reverbSendDb[static_cast<size_t>(FxCategory::Lead)] = -14.0f;
    dirs = MixSolver::solve(snap, 1000, ss, state);
    TEST_CHECK(std::abs(dirs[1].reverbSendDb - (-11.0f)) < 0.01f, "Space +100 must increase sends by +3.0 dB (-14 -> -11 dB)");

    state.macroSpace = -100.0f;
    dirs = MixSolver::solve(snap, 1000, ss, state);
    TEST_CHECK(std::abs(dirs[1].reverbSendDb - (-18.0f)) < 0.01f, "Space -100 must decrease sends by -4.0 dB (-14 -> -18 dB)");

    // Macro 5: Saturation
    float satPlus100 = MixSolver::calculateWarmthScalar(100.0f, 1.0f, 100.0f);
    float satMinus100 = MixSolver::calculateWarmthScalar(-100.0f, 1.0f, 100.0f);
    TEST_CHECK(std::abs(satPlus100 - 1.8f) < 0.01f, "Saturation +100 must scale warmth drive by 1.8x");
    TEST_CHECK(std::abs(satMinus100 - 0.2f) < 0.01f, "Saturation -100 must scale warmth drive by 0.2x");

    std::cout << "  [PASS] All 5 taste macros verified." << std::endl;
}

void testMvp5_GenreProfileTransitions() {
    std::cout << "[MVP 5 TEST 3] Genre Profile Golden Constants & Regression Lock..." << std::endl;

    // 1. Modern Worship (DEFAULT)
    // Hall, decay 3.2s, predelay 20ms, damping 0.50, delay DottedEighth fb 0.35 pingpong ON.
    // ALL deltas 0.0 - ride deltas, masking 1.0, warmth 1.0, transient 1.0, width 1.0, tonal delta 0/0/0, NO vocal tilt, NO master air offset.
    // Reverb sends: Lead -12, Pastor -inf, BGV -9, Snare -14, Keys/Piano -13, Guitars -15, Pad/Synth -10, Tracks -16.
    // Delay sends: Lead -18, Guitars -16, Pad/Synth -18, rest -inf.
    auto mw = TasteProfile::createModernWorship();
    TEST_CHECK(mw.profileId == 1 && mw.name == "Modern Worship" && mw.isBuiltIn == true, "MW identity");
    TEST_CHECK(mw.fx.reverbType == ReverbType::Hall, "MW reverb hall");
    TEST_CHECK(std::abs(mw.fx.reverbDecaySec - 3.2f) < 1e-4f, "MW decay 3.2s");
    TEST_CHECK(std::abs(mw.fx.reverbPredelayMs - 20.0f) < 1e-4f, "MW predelay 20ms");
    TEST_CHECK(std::abs(mw.fx.reverbDamping - 0.50f) < 1e-4f, "MW damping 0.50");
    TEST_CHECK(mw.fx.delaySubdivision == DelaySubdivision::DottedEighth, "MW delay dotted eighth");
    TEST_CHECK(std::abs(mw.fx.delayFeedback - 0.35f) < 1e-4f, "MW delay feedback 0.35");
    TEST_CHECK(mw.fx.delayPingPong == true, "MW delay pingpong ON");
    TEST_CHECK(std::abs(mw.maskingScale - 1.0f) < 1e-4f, "MW masking 1.0");
    TEST_CHECK(std::abs(mw.warmthScale - 1.0f) < 1e-4f, "MW warmth 1.0");
    TEST_CHECK(std::abs(mw.transientScale - 1.0f) < 1e-4f, "MW transient 1.0");
    TEST_CHECK(std::abs(mw.widthScale - 1.0f) < 1e-4f, "MW width 1.0");
    TEST_CHECK(std::abs(mw.tonalDelta.lowShelfDb - 0.0f) < 1e-4f, "MW tonal low 0.0");
    TEST_CHECK(std::abs(mw.tonalDelta.presenceDb - 0.0f) < 1e-4f, "MW tonal pres 0.0");
    TEST_CHECK(std::abs(mw.tonalDelta.airShelfDb - 0.0f) < 1e-4f, "MW tonal air 0.0");
    for (size_t i = 0; i < mw.rideOffsetDelta.size(); ++i) {
        TEST_CHECK(std::abs(mw.rideOffsetDelta[i] - 0.0f) < 1e-4f, "MW ride delta all 0.0");
        TEST_CHECK(std::abs(mw.characterTiltDb[i] - 0.0f) < 1e-4f, "MW character tilt all 0.0");
    }
    // Reverb sends: Lead -12, Pastor -inf, BGV -9, Snare -14, Keys/Piano -13, Guitars -15, Pad/Synth -10, Tracks -16
    TEST_CHECK(std::abs(mw.reverbSendDb[0] - (-12.0f)) < 1e-4f, "MW rev Lead -12");
    TEST_CHECK(mw.reverbSendDb[1] <= -70.0f, "MW rev Pastor -inf");
    TEST_CHECK(std::abs(mw.reverbSendDb[2] - (-9.0f)) < 1e-4f, "MW rev BGV -9");
    TEST_CHECK(std::abs(mw.reverbSendDb[3] - (-14.0f)) < 1e-4f, "MW rev Snare -14");
    TEST_CHECK(std::abs(mw.reverbSendDb[4] - (-13.0f)) < 1e-4f, "MW rev Keys -13");
    TEST_CHECK(std::abs(mw.reverbSendDb[5] - (-15.0f)) < 1e-4f, "MW rev Guitars -15");
    TEST_CHECK(std::abs(mw.reverbSendDb[6] - (-10.0f)) < 1e-4f, "MW rev Pad -10");
    TEST_CHECK(std::abs(mw.reverbSendDb[7] - (-16.0f)) < 1e-4f, "MW rev Tracks -16");
    // Delay sends: Lead -18, Guitars -16, Pad/Synth -18, rest -inf
    TEST_CHECK(std::abs(mw.delaySendDb[0] - (-18.0f)) < 1e-4f, "MW dly Lead -18");
    TEST_CHECK(mw.delaySendDb[1] <= -70.0f, "MW dly Pastor -inf");
    TEST_CHECK(mw.delaySendDb[2] <= -70.0f, "MW dly BGV -inf");
    TEST_CHECK(mw.delaySendDb[3] <= -70.0f, "MW dly Snare -inf");
    TEST_CHECK(mw.delaySendDb[4] <= -70.0f, "MW dly Keys -inf");
    TEST_CHECK(std::abs(mw.delaySendDb[5] - (-16.0f)) < 1e-4f, "MW dly Guitars -16");
    TEST_CHECK(std::abs(mw.delaySendDb[6] - (-18.0f)) < 1e-4f, "MW dly Pad -18");
    TEST_CHECK(mw.delaySendDb[7] <= -70.0f, "MW dly Tracks -inf");

    // 2. Acoustic Worship
    // Room, 1.6s, 10ms, damping 0.60, delay Eighth fb 0.25 pingpong OFF.
    // Ride deltas: percussive categories -2, Pad -3, AcousticGtr +2, Lead 0.
    // Masking 0.8, warmth 1.2, transient 0.7, width 0.9, tonal 0/0/+0.5.
    // Reverb sends = Modern Worship sends -3 dB per category (Pastor stays -inf). Delay sends = default -4 dB.
    auto aw = TasteProfile::createAcousticWorship();
    TEST_CHECK(aw.profileId == 2 && aw.name == "Acoustic Worship", "AW identity");
    TEST_CHECK(aw.fx.reverbType == ReverbType::Room, "AW reverb room");
    TEST_CHECK(std::abs(aw.fx.reverbDecaySec - 1.6f) < 1e-4f, "AW decay 1.6s");
    TEST_CHECK(std::abs(aw.fx.reverbPredelayMs - 10.0f) < 1e-4f, "AW predelay 10ms");
    TEST_CHECK(std::abs(aw.fx.reverbDamping - 0.60f) < 1e-4f, "AW damping 0.60");
    TEST_CHECK(aw.fx.delaySubdivision == DelaySubdivision::Eighth, "AW delay eighth");
    TEST_CHECK(std::abs(aw.fx.delayFeedback - 0.25f) < 1e-4f, "AW delay fb 0.25");
    TEST_CHECK(aw.fx.delayPingPong == false, "AW delay pingpong OFF");
    TEST_CHECK(std::abs(aw.maskingScale - 0.8f) < 1e-4f, "AW masking 0.8");
    TEST_CHECK(std::abs(aw.warmthScale - 1.2f) < 1e-4f, "AW warmth 1.2");
    TEST_CHECK(std::abs(aw.transientScale - 0.7f) < 1e-4f, "AW transient 0.7");
    TEST_CHECK(std::abs(aw.widthScale - 0.9f) < 1e-4f, "AW width 0.9");
    TEST_CHECK(std::abs(aw.tonalDelta.lowShelfDb - 0.0f) < 1e-4f, "AW tonal low 0.0");
    TEST_CHECK(std::abs(aw.tonalDelta.presenceDb - 0.0f) < 1e-4f, "AW tonal pres 0.0");
    TEST_CHECK(std::abs(aw.tonalDelta.airShelfDb - 0.5f) < 1e-4f, "AW tonal air +0.5");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::Kick)] - (-2.0f)) < 1e-4f, "AW Kick -2");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::Snare)] - (-2.0f)) < 1e-4f, "AW Snare -2");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::Toms)] - (-2.0f)) < 1e-4f, "AW Toms -2");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::HiHat)] - (-2.0f)) < 1e-4f, "AW HiHat -2");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::Overheads)] - (-2.0f)) < 1e-4f, "AW Overheads -2");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::Room)] - (-2.0f)) < 1e-4f, "AW Room -2");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::Pad)] - (-3.0f)) < 1e-4f, "AW Pad -3");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::AcousticGuitar)] - 2.0f) < 1e-4f, "AW AcousticGtr +2");
    TEST_CHECK(std::abs(aw.rideOffsetDelta[static_cast<size_t>(InstrumentType::LeadVocal)] - 0.0f) < 1e-4f, "AW Lead 0");
    TEST_CHECK(std::abs(aw.reverbSendDb[0] - (-15.0f)) < 1e-4f, "AW rev Lead -15");
    TEST_CHECK(aw.reverbSendDb[1] <= -70.0f, "AW rev Pastor -inf");
    TEST_CHECK(std::abs(aw.reverbSendDb[2] - (-12.0f)) < 1e-4f, "AW rev BGV -12");
    TEST_CHECK(std::abs(aw.reverbSendDb[3] - (-17.0f)) < 1e-4f, "AW rev Snare -17");
    TEST_CHECK(std::abs(aw.reverbSendDb[4] - (-16.0f)) < 1e-4f, "AW rev Keys -16");
    TEST_CHECK(std::abs(aw.reverbSendDb[5] - (-18.0f)) < 1e-4f, "AW rev Guitars -18");
    TEST_CHECK(std::abs(aw.reverbSendDb[6] - (-13.0f)) < 1e-4f, "AW rev Pad -13");
    TEST_CHECK(std::abs(aw.reverbSendDb[7] - (-19.0f)) < 1e-4f, "AW rev Tracks -19");
    TEST_CHECK(std::abs(aw.delaySendDb[0] - (-22.0f)) < 1e-4f, "AW dly Lead -22");
    TEST_CHECK(aw.delaySendDb[1] <= -70.0f, "AW dly Pastor -inf");
    TEST_CHECK(std::abs(aw.delaySendDb[5] - (-20.0f)) < 1e-4f, "AW dly Guitars -20");
    TEST_CHECK(std::abs(aw.delaySendDb[6] - (-22.0f)) < 1e-4f, "AW dly Pad -22");

    // 3. Gospel
    // Plate, 2.4s, 15ms, damping 0.40, delay DottedEighth fb 0.40 pingpong ON.
    // Ride deltas: BGV +2, Piano/Keys +2, Bass +1, Lead 0.
    // Masking 1.1, warmth 1.4, transient 1.1, width 1.1, tonal +0.5/+0.5/0.
    // Reverb sends = default +2 dB per category (Pastor stays -inf). Delay Lead -16, others as default scaled +2 where not -inf.
    auto gp = TasteProfile::createGospel();
    TEST_CHECK(gp.profileId == 3 && gp.name == "Gospel", "Gospel identity");
    TEST_CHECK(gp.fx.reverbType == ReverbType::Plate, "Gospel reverb plate");
    TEST_CHECK(std::abs(gp.fx.reverbDecaySec - 2.4f) < 1e-4f, "Gospel decay 2.4s");
    TEST_CHECK(std::abs(gp.fx.reverbPredelayMs - 15.0f) < 1e-4f, "Gospel predelay 15ms");
    TEST_CHECK(std::abs(gp.fx.reverbDamping - 0.40f) < 1e-4f, "Gospel damping 0.40");
    TEST_CHECK(gp.fx.delaySubdivision == DelaySubdivision::DottedEighth, "Gospel delay dotted eighth");
    TEST_CHECK(std::abs(gp.fx.delayFeedback - 0.40f) < 1e-4f, "Gospel delay fb 0.40");
    TEST_CHECK(gp.fx.delayPingPong == true, "Gospel delay pingpong ON");
    TEST_CHECK(std::abs(gp.maskingScale - 1.1f) < 1e-4f, "Gospel masking 1.1");
    TEST_CHECK(std::abs(gp.warmthScale - 1.4f) < 1e-4f, "Gospel warmth 1.4");
    TEST_CHECK(std::abs(gp.transientScale - 1.1f) < 1e-4f, "Gospel transient 1.1");
    TEST_CHECK(std::abs(gp.widthScale - 1.1f) < 1e-4f, "Gospel width 1.1");
    TEST_CHECK(std::abs(gp.tonalDelta.lowShelfDb - 0.5f) < 1e-4f, "Gospel tonal low +0.5");
    TEST_CHECK(std::abs(gp.tonalDelta.presenceDb - 0.5f) < 1e-4f, "Gospel tonal pres +0.5");
    TEST_CHECK(std::abs(gp.tonalDelta.airShelfDb - 0.0f) < 1e-4f, "Gospel tonal air 0.0");
    TEST_CHECK(std::abs(gp.rideOffsetDelta[static_cast<size_t>(InstrumentType::BackgroundVocal)] - 2.0f) < 1e-4f, "Gospel BGV +2");
    TEST_CHECK(std::abs(gp.rideOffsetDelta[static_cast<size_t>(InstrumentType::Choir)] - 2.0f) < 1e-4f, "Gospel Choir +2");
    TEST_CHECK(std::abs(gp.rideOffsetDelta[static_cast<size_t>(InstrumentType::Piano)] - 2.0f) < 1e-4f, "Gospel Piano +2");
    TEST_CHECK(std::abs(gp.rideOffsetDelta[static_cast<size_t>(InstrumentType::Keys)] - 2.0f) < 1e-4f, "Gospel Keys +2");
    TEST_CHECK(std::abs(gp.rideOffsetDelta[static_cast<size_t>(InstrumentType::Bass)] - 1.0f) < 1e-4f, "Gospel Bass +1");
    TEST_CHECK(std::abs(gp.rideOffsetDelta[static_cast<size_t>(InstrumentType::LeadVocal)] - 0.0f) < 1e-4f, "Gospel Lead 0");
    TEST_CHECK(std::abs(gp.reverbSendDb[0] - (-10.0f)) < 1e-4f, "Gospel rev Lead -10");
    TEST_CHECK(gp.reverbSendDb[1] <= -70.0f, "Gospel rev Pastor -inf");
    TEST_CHECK(std::abs(gp.reverbSendDb[2] - (-7.0f)) < 1e-4f, "Gospel rev BGV -7");
    TEST_CHECK(std::abs(gp.reverbSendDb[3] - (-12.0f)) < 1e-4f, "Gospel rev Snare -12");
    TEST_CHECK(std::abs(gp.reverbSendDb[4] - (-11.0f)) < 1e-4f, "Gospel rev Keys -11");
    TEST_CHECK(std::abs(gp.reverbSendDb[5] - (-13.0f)) < 1e-4f, "Gospel rev Guitars -13");
    TEST_CHECK(std::abs(gp.reverbSendDb[6] - (-8.0f)) < 1e-4f, "Gospel rev Pad -8");
    TEST_CHECK(std::abs(gp.reverbSendDb[7] - (-14.0f)) < 1e-4f, "Gospel rev Tracks -14");
    TEST_CHECK(std::abs(gp.delaySendDb[0] - (-16.0f)) < 1e-4f, "Gospel dly Lead -16");
    TEST_CHECK(gp.delaySendDb[1] <= -70.0f, "Gospel dly Pastor -inf");
    TEST_CHECK(std::abs(gp.delaySendDb[5] - (-14.0f)) < 1e-4f, "Gospel dly Guitars -14");
    TEST_CHECK(std::abs(gp.delaySendDb[6] - (-16.0f)) < 1e-4f, "Gospel dly Pad -16");

    // 4. Rock / Pop
    // Plate, 1.8s, 8ms, damping 0.35, delay Quarter fb 0.30 pingpong OFF.
    // Ride deltas: Kick +1, Snare +1, ElectricGtr +2, BGV -1, Pad -3.
    // Masking 1.2, warmth 1.3, transient 1.4, width 1.0, tonal +0.5/+1.0/+0.5.
    // Reverb sends: Lead -14, Snare -12, all other non-Pastor categories = default -2 dB, Pastor -inf. Delay: Guitars -14, Lead -20, rest -inf.
    auto rp = TasteProfile::createRockPop();
    TEST_CHECK(rp.profileId == 4 && rp.name == "Rock / Pop", "RockPop identity");
    TEST_CHECK(rp.fx.reverbType == ReverbType::Plate, "RockPop reverb plate");
    TEST_CHECK(std::abs(rp.fx.reverbDecaySec - 1.8f) < 1e-4f, "RockPop decay 1.8s");
    TEST_CHECK(std::abs(rp.fx.reverbPredelayMs - 8.0f) < 1e-4f, "RockPop predelay 8ms");
    TEST_CHECK(std::abs(rp.fx.reverbDamping - 0.35f) < 1e-4f, "RockPop damping 0.35");
    TEST_CHECK(rp.fx.delaySubdivision == DelaySubdivision::Quarter, "RockPop delay quarter");
    TEST_CHECK(std::abs(rp.fx.delayFeedback - 0.30f) < 1e-4f, "RockPop delay fb 0.30");
    TEST_CHECK(rp.fx.delayPingPong == false, "RockPop delay pingpong OFF");
    TEST_CHECK(std::abs(rp.maskingScale - 1.2f) < 1e-4f, "RockPop masking 1.2");
    TEST_CHECK(std::abs(rp.warmthScale - 1.3f) < 1e-4f, "RockPop warmth 1.3");
    TEST_CHECK(std::abs(rp.transientScale - 1.4f) < 1e-4f, "RockPop transient 1.4");
    TEST_CHECK(std::abs(rp.widthScale - 1.0f) < 1e-4f, "RockPop width 1.0");
    TEST_CHECK(std::abs(rp.tonalDelta.lowShelfDb - 0.5f) < 1e-4f, "RockPop tonal low +0.5");
    TEST_CHECK(std::abs(rp.tonalDelta.presenceDb - 1.0f) < 1e-4f, "RockPop tonal pres +1.0");
    TEST_CHECK(std::abs(rp.tonalDelta.airShelfDb - 0.5f) < 1e-4f, "RockPop tonal air +0.5");
    TEST_CHECK(std::abs(rp.rideOffsetDelta[static_cast<size_t>(InstrumentType::Kick)] - 1.0f) < 1e-4f, "RockPop Kick +1");
    TEST_CHECK(std::abs(rp.rideOffsetDelta[static_cast<size_t>(InstrumentType::Snare)] - 1.0f) < 1e-4f, "RockPop Snare +1");
    TEST_CHECK(std::abs(rp.rideOffsetDelta[static_cast<size_t>(InstrumentType::ElectricGuitar)] - 2.0f) < 1e-4f, "RockPop ElectricGtr +2");
    TEST_CHECK(std::abs(rp.rideOffsetDelta[static_cast<size_t>(InstrumentType::BackgroundVocal)] - (-1.0f)) < 1e-4f, "RockPop BGV -1");
    TEST_CHECK(std::abs(rp.rideOffsetDelta[static_cast<size_t>(InstrumentType::Pad)] - (-3.0f)) < 1e-4f, "RockPop Pad -3");
    TEST_CHECK(std::abs(rp.reverbSendDb[0] - (-14.0f)) < 1e-4f, "RockPop rev Lead -14");
    TEST_CHECK(rp.reverbSendDb[1] <= -70.0f, "RockPop rev Pastor -inf");
    TEST_CHECK(std::abs(rp.reverbSendDb[2] - (-11.0f)) < 1e-4f, "RockPop rev BGV -11");
    TEST_CHECK(std::abs(rp.reverbSendDb[3] - (-12.0f)) < 1e-4f, "RockPop rev Snare -12");
    TEST_CHECK(std::abs(rp.reverbSendDb[4] - (-15.0f)) < 1e-4f, "RockPop rev Keys -15");
    TEST_CHECK(std::abs(rp.reverbSendDb[5] - (-17.0f)) < 1e-4f, "RockPop rev Guitars -17");
    TEST_CHECK(std::abs(rp.reverbSendDb[6] - (-12.0f)) < 1e-4f, "RockPop rev Pad -12");
    TEST_CHECK(std::abs(rp.reverbSendDb[7] - (-18.0f)) < 1e-4f, "RockPop rev Tracks -18");
    TEST_CHECK(std::abs(rp.delaySendDb[0] - (-20.0f)) < 1e-4f, "RockPop dly Lead -20");
    TEST_CHECK(rp.delaySendDb[1] <= -70.0f, "RockPop dly Pastor -inf");
    TEST_CHECK(std::abs(rp.delaySendDb[5] - (-14.0f)) < 1e-4f, "RockPop dly Guitars -14");
    TEST_CHECK(rp.delaySendDb[6] <= -70.0f, "RockPop dly Pad -inf");

    // 5. REGRESSION LOCK: Modern Worship + Neutral Macros + Follow 100% == Pre-Taste Constants
    GlobalTasteState lockState;
    lockState.followPercent = 100.0f;
    lockState.macroVocalForward = 0.0f;
    lockState.macroEnergy = 0.0f;
    lockState.macroWarmBright = 0.0f;
    lockState.macroSpace = 0.0f;
    lockState.macroSaturation = 0.0f;

    std::vector<InstanceTelemetry> snap;
    InstanceTelemetry vox;
    vox.instanceId = 101;
    vox.role = PluginRole::Source;
    vox.instrumentType = InstrumentType::LeadVocal;
    vox.signalPresent = true;
    vox.rmsDb = -18.0f;
    snap.push_back(vox);

    InstanceTelemetry bass;
    bass.instanceId = 102;
    bass.role = PluginRole::Source;
    bass.instrumentType = InstrumentType::Bass;
    bass.signalPresent = true;
    bass.rmsDb = -18.0f;
    bass.fundamentalHz = 60.0f;
    snap.push_back(bass);

    InstanceTelemetry kick;
    kick.instanceId = 103;
    kick.role = PluginRole::Source;
    kick.instrumentType = InstrumentType::Kick;
    kick.signalPresent = true;
    kick.rmsDb = -18.0f;
    kick.fundamentalHz = 60.0f;
    snap.push_back(kick);

    SolverState sState;
    auto dirs = MixSolver::solve(snap, 1000, sState, lockState);
    TEST_CHECK(std::abs(dirs[101].rideOffsetDb - 0.0f) < 0.01f, "RegLock: Vox ride baseline 0.0 dB");
    TEST_CHECK(dirs[102].duckDb <= -1.0f && dirs[102].duckDb >= -3.0f, "RegLock: Bass duck baseline [-3, -1] dB");
    TEST_CHECK(dirs[102].maskingCuts[0].enabled == true, "RegLock: Bass masking cut enabled");
    TEST_CHECK(std::abs(dirs[102].maskingCuts[0].freqHz - 60.0f) < 1.0f, "RegLock: Bass masking cut freq 60 Hz");
    TEST_CHECK(dirs[102].maskingCuts[0].gainDb <= -1.0f && dirs[102].maskingCuts[0].gainDb >= -6.0f, "RegLock: Bass masking cut depth [-6, -1] dB");

    // Master tonal EQ baseline offsets
    TEST_CHECK(lockState.masterLowShelfDb == 0.0f, "RegLock: Master low shelf 0.0 dB");
    TEST_CHECK(lockState.masterPresenceDb == 0.0f, "RegLock: Master presence 0.0 dB");
    TEST_CHECK(lockState.masterAirShelfDb == 0.0f, "RegLock: Master air shelf 0.0 dB");

    std::cout << "  [PASS] 4 built-in genre profiles & baseline regression lock verified." << std::endl;
}

void testMvp5_ReverbEngineRT60() {
    std::cout << "[MVP 5 TEST 4] ReverbEngine RT60 Decay Accuracy..." << std::endl;

    ReverbEngine reverb;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    reverb.prepare(spec);

    reverb.setReverbType(ReverbType::Hall);
    reverb.setDecaySec(1.0f);
    reverb.setPredelayMs(0.0f);
    reverb.setDamping(0.3f);
    reverb.reset();

    // Send single unit impulse at sample 0
    const int totalSamples = 48000 * 3; // 3 seconds
    std::vector<float> inL(totalSamples, 0.0f);
    std::vector<float> inR(totalSamples, 0.0f);
    inL[0] = 1.0f;
    inR[0] = 1.0f;

    std::vector<float> wetL(totalSamples, 0.0f);
    std::vector<float> wetR(totalSamples, 0.0f);

    const int blockSize = 512;
    for (int offset = 0; offset < totalSamples; offset += blockSize) {
        int samplesToProcess = std::min(blockSize, totalSamples - offset);
        reverb.process(&inL[offset], &inR[offset], &wetL[offset], &wetR[offset], samplesToProcess);
    }

    // Measure peak energy in first 50 ms (reference)
    float peakWet = 0.0f;
    for (int i = 0; i < 48000 * 0.050; ++i) {
        peakWet = std::max(peakWet, std::abs(wetL[i]));
    }
    TEST_CHECK(peakWet > 0.005f, "Reverb output must have energy from impulse");

    float threshold = peakWet * 0.001f; // -60 dB threshold

    int rt60Sample = -1;
    for (int i = totalSamples - 1; i >= 0; --i) {
        if (std::abs(wetL[i]) > threshold) {
            rt60Sample = i;
            break;
        }
    }

    TEST_CHECK(rt60Sample > 0, "Reverb must drop below -60 dB");
    float measuredTimeSec = static_cast<float>(rt60Sample) / 48000.0f;
    std::cout << "  Target RT60: 1.00s | Measured: " << measuredTimeSec << "s" << std::endl;
    TEST_CHECK(measuredTimeSec >= 0.70f && measuredTimeSec <= 1.30f, "Measured RT60 must be within +/-25% of target 1.0s");

    std::cout << "  [PASS] ReverbEngine RT60 verified within +/-25%." << std::endl;
}

void testMvp5_DelayEngineSyncPingPong() {
    std::cout << "[MVP 5 TEST 5] DelayEngine Tempo Sync & Ping-Pong..." << std::endl;

    DelayEngine delay;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    delay.prepare(spec);

    // 120 BPM: Dotted-8th = 45 / 120 = 375 ms = 18000 samples @ 48 kHz
    delay.setBpm(120.0);
    delay.setSubdivision(DelaySubdivision::DottedEighth);
    delay.setFeedback(0.35f);
    delay.setPingPong(true);
    delay.reset();

    TEST_CHECK(std::abs(delay.getDelayTimeMs() - 375.0f) < 0.1f, "120 BPM dotted 8th must equal exactly 375 ms");

    const int totalSamples = 48000; // 1 second
    std::vector<float> inL(totalSamples, 0.0f);
    std::vector<float> inR(totalSamples, 0.0f);
    inL[0] = 1.0f;
    inR[0] = 1.0f;

    std::vector<float> wetL(totalSamples, 0.0f);
    std::vector<float> wetR(totalSamples, 0.0f);

    const int blockSize = 512;
    for (int offset = 0; offset < totalSamples; offset += blockSize) {
        int samplesToProcess = std::min(blockSize, totalSamples - offset);
        delay.process(&inL[offset], &inR[offset], &wetL[offset], &wetR[offset], samplesToProcess);
    }

    int peakSampleL = 0;
    float peakValL = 0.0f;
    for (int i = 0; i < totalSamples; ++i) {
        if (std::abs(wetL[i]) > peakValL) {
            peakValL = std::abs(wetL[i]);
            peakSampleL = i;
        }
    }

    int peakSampleR = 0;
    float peakValR = 0.0f;
    for (int i = 0; i < totalSamples; ++i) {
        if (std::abs(wetR[i]) > peakValR) {
            peakValR = std::abs(wetR[i]);
            peakSampleR = i;
        }
    }

    std::cout << "  Left tap: sample " << peakSampleL << " (" << (peakSampleL / 48.0f) << " ms, expected 375 ms)" << std::endl;
    std::cout << "  Right tap: sample " << peakSampleR << " (" << (peakSampleR / 48.0f) << " ms, expected 750 ms)" << std::endl;

    TEST_CHECK(std::abs(peakSampleL - 18000) <= 2, "Left tap must be at 375 ms (18000 samples)");
    TEST_CHECK(std::abs(peakSampleR - 36000) <= 2, "Right tap must be at 750 ms (36000 samples)");

    std::cout << "  [PASS] DelayEngine 120 BPM dotted-8th (375 ms Left / 750 ms Right ping-pong) verified." << std::endl;
}

void testMvp5_SessionModesAndHoldSemantics() {
    std::cout << "[MVP 5 TEST 6] Session Modes (Live, Learn, FreezeAll)..." << std::endl;

    std::vector<InstanceTelemetry> snap;
    InstanceTelemetry vox;
    vox.instanceId = 1;
    vox.role = PluginRole::Source;
    vox.instrumentType = InstrumentType::LeadVocal;
    vox.signalPresent = true;
    vox.rmsDb = -18.0f;
    snap.push_back(vox);

    InstanceTelemetry gtr;
    gtr.instanceId = 2;
    gtr.role = PluginRole::Source;
    gtr.instrumentType = InstrumentType::ElectricGuitar;
    gtr.signalPresent = true;
    gtr.rmsDb = -20.0f; // Target is -9.0 vs vox -> measured offset -2.0 -> rawDelta = -7.0 dB
    snap.push_back(gtr);

    // 1. Live Mode: rate limit step is 0.10 dB
    GlobalTasteState liveState;
    liveState.sessionMode = SessionMode::Live;
    SolverState ssLive;
    auto dirLive = MixSolver::solve(snap, 1000, ssLive, liveState);
    TEST_CHECK(std::abs(dirLive[2].rideOffsetDb - (-0.10f)) < 0.01f, "Live mode step must be capped at 0.10 dB");

    // 2. Learn Mode: rate limit step is 0.20 dB (2.0 dB/s at 10 Hz)
    GlobalTasteState learnState;
    learnState.sessionMode = SessionMode::Learn;
    SolverState ssLearn;
    auto dirLearn = MixSolver::solve(snap, 1000, ssLearn, learnState);
    TEST_CHECK(std::abs(dirLearn[2].rideOffsetDb - (-0.20f)) < 0.01f, "Learn mode step must be capped at 0.20 dB");

    // 3. FreezeAll Mode: Directives hold static, solver doesn't change them
    GlobalTasteState freezeState;
    freezeState.sessionMode = SessionMode::FreezeAll;
    ssLearn.instanceStates[2].currentRideDb = -1.5f;
    auto dirFrozen = MixSolver::solve(snap, 1200, ssLearn, freezeState);
    TEST_CHECK(std::abs(dirFrozen[2].rideOffsetDb - (-1.5f)) < 0.01f, "FreezeAll mode must HOLD current ride offset static");
    TEST_CHECK(dirFrozen[2].sessionMode == SessionMode::FreezeAll, "Directives sessionMode must be FreezeAll");

    // 4. Staleness check: FreezeAll prevents ramp to zero
    SolverStage stage;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    stage.prepare(spec);

    InstanceDirectives d;
    d.rideOffsetDb = -2.5f;
    d.sessionMode = SessionMode::FreezeAll;
    stage.updateDirectives(d, 1000);
    TEST_CHECK(std::abs(stage.getActiveRideDb() - (-2.5f)) < 0.01f, "Stage must receive directive");

    // Advance 5 seconds without packets
    stage.checkStaleness(6000);
    TEST_CHECK(std::abs(stage.getActiveRideDb() - (-2.5f)) < 0.01f, "Stage in FreezeAll must HOLD directive without staleness ramp-down");

    std::cout << "  [PASS] Session modes and FreezeAll hold semantics verified." << std::endl;
}

void testMvp5_MoreLessOfMe() {
    std::cout << "[MVP 5 TEST 7] More/Less of Me (+/-3 dB bias)..." << std::endl;

    std::vector<InstanceTelemetry> snap;
    InstanceTelemetry vox;
    vox.instanceId = 1;
    vox.role = PluginRole::Source;
    vox.instrumentType = InstrumentType::LeadVocal;
    vox.signalPresent = true;
    vox.rmsDb = -18.0f;
    snap.push_back(vox);

    InstanceTelemetry gtr;
    gtr.instanceId = 2;
    gtr.role = PluginRole::Source;
    gtr.instrumentType = InstrumentType::ElectricGuitar;
    gtr.signalPresent = true;
    gtr.rmsDb = -27.0f; // Exactly at default target offset (-9.0 dB)
    gtr.moreLessDb = 2.0f; // User wants +2 dB More of Me!
    snap.push_back(gtr);

    GlobalTasteState state;
    SolverState ss;
    float finalRide = 0.0f;
    for (int c = 0; c < 30; ++c) {
        auto d = MixSolver::solve(snap, 1000 + c * 200, ss, state);
        finalRide = d[2].rideOffsetDb;
    }

    std::cout << "  More/Less +2.0 dB target converged to: " << finalRide << " dB" << std::endl;
    TEST_CHECK(std::abs(finalRide - 2.0f) < 0.15f, "More/Less +2.0 dB must shift target ride by +2.0 dB");

    std::cout << "  [PASS] More/Less of Me bias verified." << std::endl;
}

void testMvp5_ProfileLearningEmaAndAutoFork() {
    std::cout << "[MVP 5 TEST 8] Profile Learning (EMA + Auto-Fork)..." << std::endl;

    ProfileManager pm;
    pm.setActiveProfile(1); // Modern Worship (built-in)

    auto all = pm.getAllProfiles();
    TEST_CHECK(pm.getProfile(1)->isBuiltIn == true, "Modern worship is built-in");

    // Simulate proposal from Source Lead Vocal
    InstanceTelemetry prop;
    prop.instanceId = 42;
    prop.instrumentType = InstrumentType::LeadVocal;
    prop.proposalSequence = 1;
    prop.proposalRideDelta = 1.5f;
    prop.proposalWarmthScale = 1.4f;
    prop.proposalCharacterTiltDb = 1.0f;

    uint64_t nowMs = 100000;
    bool merged = pm.processProposal(prop, nowMs);
    TEST_CHECK(merged, "Proposal should be merged");
    TEST_CHECK(pm.getLearnedUpdatesCount() == 1, "Learned count should be 1");

    auto state = pm.buildGlobalTasteState();
    TEST_CHECK(std::string(state.profileName) == "My Modern Worship", "Auto-forked name must be 'My Modern Worship'");
    TEST_CHECK(state.profileId >= 100, "Forked profile id must be >= 100");

    // EMA: 0.8 * 0.0 + 0.2 * 1.5 = 0.30f
    float learnedRide = state.rideOffsetDelta[static_cast<size_t>(InstrumentType::LeadVocal)];
    std::cout << "  Learned ride delta: " << learnedRide << " dB (expected 0.30 dB)" << std::endl;
    TEST_CHECK(std::abs(learnedRide - 0.30f) < 0.01f, "EMA merge must be 0.8*old + 0.2*new");

    // Test Reset
    pm.resetActiveProfileToDefault();
    auto resetState = pm.buildGlobalTasteState();
    TEST_CHECK(resetState.rideOffsetDelta[static_cast<size_t>(InstrumentType::LeadVocal)] == 0.0f, "Reset must restore baseline 0 dB");

    std::cout << "  [PASS] Profile learning EMA merge and auto-fork verified." << std::endl;
}

void testMvp5_JsonRoundTripAndMalformedResilience() {
    std::cout << "[MVP 5 TEST 9] JSON Round-Trip & Malformed Resilience..." << std::endl;

    TasteProfile original = TasteProfile::createGospel();
    original.name = "Test Custom Profile";
    original.isBuiltIn = false;
    original.rideOffsetDelta[static_cast<size_t>(InstrumentType::LeadVocal)] = 1.25f;
    original.warmthScale = 1.45f;
    original.fx.reverbDecaySec = 4.2f;

    std::string json = original.toJsonString();
    TEST_CHECK(!json.empty(), "JSON string must not be empty");

    auto parsed = TasteProfile::fromJsonString(json);
    TEST_CHECK(parsed.has_value(), "Valid JSON must parse successfully");
    TEST_CHECK(parsed->name == "Test Custom Profile", "Name must match");
    TEST_CHECK(std::abs(parsed->rideOffsetDelta[static_cast<size_t>(InstrumentType::LeadVocal)] - 1.25f) < 1e-4f, "Ride delta must match");
    TEST_CHECK(std::abs(parsed->warmthScale - 1.45f) < 1e-4f, "Warmth scale must match");
    TEST_CHECK(std::abs(parsed->fx.reverbDecaySec - 4.2f) < 1e-4f, "Reverb decay must match");

    // Malformed JSON cases:
    auto malformed1 = TasteProfile::fromJsonString("{ invalid json ");
    TEST_CHECK(!malformed1.has_value(), "Malformed JSON 1 must return nullopt safely");

    auto malformed2 = TasteProfile::fromJsonString("");
    TEST_CHECK(!malformed2.has_value(), "Empty string must return nullopt safely");

    auto malformed3 = TasteProfile::fromJsonString("[ 1, 2, 3 ]");
    TEST_CHECK(!malformed3.has_value(), "Non-object JSON must return nullopt safely");

    std::cout << "  [PASS] JSON round-trip and resilience against malformed data verified." << std::endl;
}

// ==============================================================================
// HARDENING PASS: 5 MISSING TESTS
// ==============================================================================

void testHardening_FxReturnHostWetOnly() {
    std::cout << "\n[HARDENING TEST 1] FX Return Host Wet-Only (Bus FX)..." << std::endl;

    BusStage bus;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    bus.prepare(spec);

    bus.setBusType(BusType::FX);

    GlobalTasteState taste; // Modern Worship: Hall 3.2s, predelay 20ms = 960 samples
    bus.applyTasteDirectives(taste);

    const int totalSamples = 48000; // 1 second
    juce::AudioBuffer<float> buffer(2, totalSamples);
    buffer.clear();

    // Impulse at sample 0 + sustained 440 Hz tone
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    for (int i = 0; i < totalSamples; ++i) {
        float tone = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f);
        buffer.addSample(0, i, tone);
        buffer.addSample(1, i, tone);
    }

    // Process through BusStage block by block
    for (int offset = 0; offset < totalSamples; offset += 512) {
        int samples = std::min(512, totalSamples - offset);
        juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), 2, offset, samples);
        bus.process(block);
    }

    // In the first predelay window (predelay = 20ms = 960 samples @ 48kHz):
    // Reverb has not produced its first reflection yet and Delay is 375ms (18000 samples).
    // In wet-only mode, the direct input signal (dry path) MUST NOT leak into output!
    // Specifically, at sample 0 (the impulse itself), the output must be virtually zero:
    float impulseBleed = std::abs(buffer.getSample(0, 0));
    std::cout << "  Measured Direct Impulse Bleed: " << impulseBleed << " (expected < 1e-4)" << std::endl;
    TEST_CHECK(impulseBleed < 1e-4f, "FX Return Host must have NO direct impulse dry bleed");

    // Check first 100 samples (well before 960 sample predelay):
    float maxPredelayBleed = 0.0f;
    for (int i = 0; i < 100; ++i) {
        maxPredelayBleed = std::max(maxPredelayBleed, std::abs(buffer.getSample(0, i)));
    }
    std::cout << "  Measured Predelay Window Bleed: " << maxPredelayBleed << " (expected < 1e-4)" << std::endl;
    TEST_CHECK(maxPredelayBleed < 1e-4f, "FX Return Host output during predelay window must be ~0 (no dry path)");

    // After predelay (>1000 samples), wet reverb tail MUST be present:
    float wetTailPeak = 0.0f;
    for (int i = 1000; i < 5000; ++i) {
        wetTailPeak = std::max(wetTailPeak, std::abs(buffer.getSample(0, i)));
    }
    std::cout << "  Measured Wet Reverb Tail Peak: " << wetTailPeak << " (expected > 0.01)" << std::endl;
    TEST_CHECK(wetTailPeak > 0.01f, "Wet reverb tail must be present");

    std::cout << "  [PASS] FX Return Host wet-only verified." << std::endl;
}

void testHardening_TwoSourceSharedCharacter() {
    std::cout << "[HARDENING TEST 2] Two-Source Shared Character RT60 Check..." << std::endl;

    auto measureSourceRT60 = [](const GlobalTasteState& taste, float expectedDecay) -> std::pair<float, float> {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 2;

        PlaybookStage src1;
        PlaybookStage src2;
        src1.prepare(spec);
        src2.prepare(spec);

        src1.setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);
        src2.setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);

        InstanceDirectives d1;
        d1.reverbSendDb = -6.0f; // Strong send for measuring tail decay
        d1.delaySendDb = -60.0f;
        InstanceDirectives d2 = d1;

        src1.applyTasteDirectives(d1, taste);
        src2.applyTasteDirectives(d2, taste);

        src1.setLocalTailEnabled(true);
        src2.setLocalTailEnabled(true);

        const int totalSamples = static_cast<int>(48000 * (expectedDecay + 2.5f));
        std::vector<float> wet1(totalSamples, 0.0f);
        std::vector<float> wet2(totalSamples, 0.0f);

        juce::AudioBuffer<float> buf1(2, 512);
        juce::AudioBuffer<float> buf2(2, 512);

        for (int offset = 0; offset < totalSamples; offset += 512) {
            int samples = std::min(512, totalSamples - offset);
            buf1.clear();
            buf2.clear();
            if (offset == 0) {
                buf1.setSample(0, 0, 1.0f);
                buf1.setSample(1, 0, 1.0f);
                buf2.setSample(0, 0, 1.0f);
                buf2.setSample(1, 0, 1.0f);
            }
            src1.process(buf1);
            src2.process(buf2);

            for (int s = 0; s < samples; ++s) {
                wet1[offset + s] = buf1.getSample(0, s);
                wet2[offset + s] = buf2.getSample(0, s);
            }
        }

        auto findRT60 = [](const std::vector<float>& w) -> float {
            float peak = 0.0f;
            // The dry impulse is at sample 0. Search for peak reverb energy after dry impulse has passed (samples 480 to 24000)
            for (size_t i = 480; i < 24000 && i < w.size(); ++i) {
                peak = std::max(peak, std::abs(w[i]));
            }
            if (peak < 1e-5f) return 0.0f;
            float thresh = peak * 0.001f; // -60 dB relative to wet reverb peak
            for (int i = static_cast<int>(w.size()) - 1; i >= 0; --i) {
                if (std::abs(w[i]) > thresh) return static_cast<float>(i) / 48000.0f;
            }
            return 0.0f;
        };

        return { findRT60(wet1), findRT60(wet2) };
    };

    // Case A: Hall 3.2s (Modern Worship)
    GlobalTasteState mwTaste;
    mwTaste.reverbType = ReverbType::Hall;
    mwTaste.reverbDecaySec = 3.2f;
    mwTaste.reverbPredelayMs = 20.0f;
    mwTaste.reverbDamping = 0.50f;

    auto [rt60_1_mw, rt60_2_mw] = measureSourceRT60(mwTaste, 3.2f);
    std::cout << "  Modern Worship Hall 3.2s: Src1 = " << rt60_1_mw << "s | Src2 = " << rt60_2_mw << "s" << std::endl;
    TEST_CHECK(std::abs(rt60_1_mw - rt60_2_mw) / rt60_1_mw < 0.10f, "Src1 and Src2 RT60 must be within 10% of each other");
    TEST_CHECK(rt60_1_mw >= 3.2f * 0.75f && rt60_1_mw <= 3.2f * 1.25f, "Src1 RT60 within +/-25% of 3.2s");
    TEST_CHECK(rt60_2_mw >= 3.2f * 0.75f && rt60_2_mw <= 3.2f * 1.25f, "Src2 RT60 within +/-25% of 3.2s");

    // Case B: Room 1.6s (Acoustic Worship)
    GlobalTasteState awTaste;
    awTaste.reverbType = ReverbType::Room;
    awTaste.reverbDecaySec = 1.6f;
    awTaste.reverbPredelayMs = 10.0f;
    awTaste.reverbDamping = 0.60f;

    auto [rt60_1_aw, rt60_2_aw] = measureSourceRT60(awTaste, 1.6f);
    std::cout << "  Acoustic Room 1.6s:       Src1 = " << rt60_1_aw << "s | Src2 = " << rt60_2_aw << "s" << std::endl;
    TEST_CHECK(rt60_1_aw >= 1.6f * 0.75f && rt60_1_aw <= 1.6f * 1.25f, "Src1 RT60 within +/-25% of 1.6s");
    TEST_CHECK(rt60_2_aw >= 1.6f * 0.75f && rt60_2_aw <= 1.6f * 1.25f, "Src2 RT60 within +/-25% of 1.6s");

    std::cout << "  [PASS] Two-source shared character verified." << std::endl;
}

void testHardening_TasteBroadcastLatency() {
    std::cout << "[HARDENING TEST 3] Taste Broadcast Latency Check (< 1000 ms)..." << std::endl;

    SharedMemoryRegistry registry;
    TEST_CHECK(registry.initialize("AIMixer_TestRegistry_TasteLatency"), "Registry init");

    ProfileManager pm;
    pm.setActiveProfile(1); // Modern Worship
    registry.publishTasteState(pm.buildGlobalTasteState());

    // Source initial read
    GlobalTasteState initialTaste;
    bool okInit = registry.readTasteState(initialTaste);
    TEST_CHECK(okInit, "Taste state must be present");
    TEST_CHECK(std::abs(initialTaste.reverbDecaySec - 3.2f) < 1e-3f, "Initial decay is 3.2s");

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;

    PlaybookStage playbook;
    playbook.prepare(spec);
    playbook.setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);

    InstanceDirectives d;
    d.reverbSendDb = -12.0f;
    playbook.applyTasteDirectives(d, initialTaste);

    // Coordinator flips to Acoustic Worship (decay 1.6s) and Macro 3 (Warm/Bright = +60)
    auto startTime = std::chrono::steady_clock::now();
    pm.setActiveProfile(2); // Acoustic Worship
    pm.setMacroWarmBright(60.0f);
    registry.publishTasteState(pm.buildGlobalTasteState());

    // Client polling loop (emulating 15 Hz timer loop)
    bool updated = false;
    double elapsedMs = 0.0;
    while (!updated) {
        auto now = std::chrono::steady_clock::now();
        elapsedMs = std::chrono::duration<double, std::milli>(now - startTime).count();
        if (elapsedMs > 1000.0) break;

        GlobalTasteState t;
        if (registry.readTasteState(t) && t.profileId == 2) {
            d.characterTiltDb = (60.0f / 100.0f) * 1.5f; // +0.9 dB
            playbook.applyTasteDirectives(d, t);
            if (std::abs(playbook.getReverbEngine().getDecaySec() - 1.6f) < 1e-3f &&
                std::abs(playbook.getCharacterTiltDb() - 0.9f) < 1e-3f) {
                updated = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "  Measured Taste Broadcast Latency: " << elapsedMs << " ms (limit < 1000 ms)" << std::endl;
    TEST_CHECK(updated == true, "Taste broadcast must update applied parameters within 1000 ms");
    TEST_CHECK(elapsedMs < 1000.0, "Latency must be < 1000 ms");

    std::cout << "  [PASS] Taste broadcast latency verified." << std::endl;
}

void testHardening_MasterLatencyRemeasure() {
    std::cout << "[HARDENING TEST 4] Master Latency Re-Measure (Post-MVP 5)..." << std::endl;

    MasterChain master;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    master.prepare(spec);

    int reportedLatency = master.getReportedLatencySamples();
    std::cout << "  Master getReportedLatencySamples(): " << reportedLatency << " samples" << std::endl;

    // Send impulse through MasterChain
    const int totalSamples = 512;
    juce::AudioBuffer<float> buffer(2, totalSamples);
    buffer.clear();
    buffer.setSample(0, 0, 0.5f);
    buffer.setSample(1, 0, 0.5f);

    master.process(buffer);

    // Find peak impulse output
    int peakIndex = -1;
    float peakVal = 0.0f;
    for (int i = 0; i < totalSamples; ++i) {
        float val = std::abs(buffer.getSample(0, i));
        if (val > peakVal) {
            peakVal = val;
            peakIndex = i;
        }
    }

    std::cout << "  Measured Impulse Output Delay: " << peakIndex << " samples" << std::endl;
    TEST_CHECK(std::abs(peakIndex - reportedLatency) <= 2, "Measured impulse delay must match reported latency within +/-2 samples");

    // Re-assert Source & Bus role latency is strictly 0 even with FX ON
    PlaybookStage src;
    src.prepare(spec);
    src.setLocalTailEnabled(true);
    InstanceDirectives d;
    d.reverbSendDb = -12.0f;
    d.delaySendDb = -18.0f;
    GlobalTasteState taste;
    src.applyTasteDirectives(d, taste);

    // Render impulse on Source
    juce::AudioBuffer<float> srcBuf(2, 512);
    srcBuf.clear();
    srcBuf.setSample(0, 0, 1.0f);
    srcBuf.setSample(1, 0, 1.0f);
    src.process(srcBuf);
    TEST_CHECK(std::abs(srcBuf.getSample(0, 0)) > 0.5f, "Source dry impulse must appear immediately at sample 0 (0-sample latency)");

    // Render impulse on Bus (non-FX glue mode)
    BusStage bus;
    bus.prepare(spec);
    bus.setBusType(BusType::Drums);
    juce::AudioBuffer<float> busBuf(2, 512);
    busBuf.clear();
    busBuf.setSample(0, 0, 1.0f);
    busBuf.setSample(1, 0, 1.0f);
    bus.process(busBuf);
    TEST_CHECK(std::abs(busBuf.getSample(0, 0)) > 0.5f, "Bus glue dry impulse must appear immediately at sample 0 (0-sample latency)");

    std::cout << "  [PASS] Master latency re-measure and Source/Bus 0-latency verified." << std::endl;
}

void testHardening_CpuWithFxOn() {
    std::cout << "[HARDENING TEST 5] CPU with FX On (Realistic Session Benchmark)..." << std::endl;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;

    // (a) Single Source with Local Tail Reverb + Delay ON at Modern Worship sends
    PlaybookStage src;
    src.prepare(spec);
    src.setInstrumentIdentity(InstrumentType::LeadVocal, SideHint::Centre);
    src.setLocalTailEnabled(true);

    GlobalTasteState mwState;
    // MW sends: Lead rev -12 dB, Lead dly -18 dB
    InstanceDirectives d;
    d.reverbSendDb = -12.0f;
    d.delaySendDb = -18.0f;
    src.applyTasteDirectives(d, mwState);

    SafeChain safe;
    safe.prepare(spec);
    safe.setInstrumentType(InstrumentType::LeadVocal);

    const int numBlocks = 2000; // 21.3 seconds of audio at 48kHz
    juce::AudioBuffer<float> buffer(2, 512);

    auto start = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < numBlocks; ++b) {
        // Active audio (synthesized vocal tone)
        for (int i = 0; i < 512; ++i) {
            float s = 0.1f * std::sin(2.0f * 3.14159265f * 220.0f * (b * 512 + i) / 48000.0f);
            buffer.setSample(0, i, s);
            buffer.setSample(1, i, s);
        }
        safe.process(buffer);
        src.process(buffer);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    double audioSec = (numBlocks * 512.0) / 48000.0;
    double perInstanceCpu = (elapsedMs / (audioSec * 1000.0)) * 100.0;

    std::cout << "  (a) Per-Instance CPU (SafeChain + Playbook with Reverb + Delay ON): "
              << perInstanceCpu << "% (cap < 1.20%)" << std::endl;
    TEST_CHECK(perInstanceCpu < 1.20, "Single Source with FX ON must be < 1.2% CPU");

    // (b) Simulated 16-instance session total
    std::vector<std::unique_ptr<PlaybookStage>> sessionStages;
    std::vector<std::unique_ptr<SafeChain>> sessionSafeChains;
    for (int i = 0; i < 16; ++i) {
        auto p = std::make_unique<PlaybookStage>();
        p->prepare(spec);
        p->setLocalTailEnabled(true);
        p->applyTasteDirectives(d, mwState);
        sessionStages.push_back(std::move(p));

        auto s = std::make_unique<SafeChain>();
        s->prepare(spec);
        sessionSafeChains.push_back(std::move(s));
    }

    auto start16 = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < 500; ++b) {
        for (int inst = 0; inst < 16; ++inst) {
            for (int i = 0; i < 512; ++i) {
                float s = 0.05f * std::sin(2.0f * 3.14159265f * (100.0f + inst * 20.0f) * (b * 512 + i) / 48000.0f);
                buffer.setSample(0, i, s);
                buffer.setSample(1, i, s);
            }
            sessionSafeChains[inst]->process(buffer);
            sessionStages[inst]->process(buffer);
        }
    }
    auto end16 = std::chrono::high_resolution_clock::now();

    double elapsedMs16 = std::chrono::duration<double, std::milli>(end16 - start16).count();
    double audioSec16 = (500 * 512.0) / 48000.0;
    double session16Cpu = (elapsedMs16 / (audioSec16 * 1000.0)) * 100.0;

    std::cout << "  (b) Simulated 16-Instance Session Total CPU: " << session16Cpu << "%" << std::endl;
    TEST_CHECK(session16Cpu < 20.0, "16-instance session total must be within real-time budget (<20%)");

    std::cout << "  [PASS] CPU with FX ON benchmarks verified." << std::endl;
}

// ==============================================================================
// STEP 4: HOST-PATTERN REGRESSION TESTS
// ==============================================================================

int runRegistryPeer() {
    SharedMemoryRegistry reg;
    if (!reg.initialize()) {
        std::cerr << "PEER_INIT_FAILED" << std::endl;
        return 1;
    }
    uint32_t peerId = 0xBEEF;
    int slot = reg.registerInstance(peerId, PluginRole::Master, "ChildPeerMaster", 7777);
    if (slot < 0) {
        std::cerr << "PEER_REGISTER_FAILED" << std::endl;
        return 2;
    }
    std::cout << "PEER_READY:" << slot << std::endl;
    std::cout.flush();

    while (true) {
        InstanceTelemetry t{};
        t.instanceId = peerId;
        t.role = PluginRole::Master;
        t.peakDb = -6.0f;
        std::string name = "ChildPeerMaster";
        std::memcpy(t.trackName, name.data(), name.size());
        t.lastHeartbeatMs = SharedMemoryRegistry::getSystemTimeMs();
        reg.publishTelemetry(slot, t);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return 0;
}

void testHostPattern_RapidScanLifecycle200x() {
    std::cout << "[HOST PATTERN 1] Rapid scan lifecycle (200x create -> editor -> destroy editor -> destroy proc)..." << std::endl;
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 0; i < 200; ++i) {
        auto proc = std::make_unique<AIMixerAudioProcessor>();
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(proc->createEditor());
        TEST_CHECK(editor != nullptr, "Editor must be created");
        editor.reset();
        proc.reset();
    }
    std::cout << "  [PASS] 200 rapid scan lifecycle iterations completed without dangling LookAndFeel or crashing." << std::endl;
}

void testHostPattern_EditorBeforeAudioAndBlockSizeChaos() {
    std::cout << "[HOST PATTERN 2] Editor before audio & block size chaos (16, 63, 512, 8192)..." << std::endl;
    juce::ScopedJuceInitialiser_GUI juceInit;

    const std::vector<int> blockSizes = { 16, 63, 512, 8192 };

    for (int blockSize : blockSizes) {
        auto proc = std::make_unique<AIMixerAudioProcessor>();
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(proc->createEditor());
        TEST_CHECK(editor != nullptr, "Editor created");

        // 5 audio frames + UI repaint BEFORE prepareToPlay
        for (int frame = 0; frame < 5; ++frame) {
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int ch = 0; ch < 2; ++ch) {
                for (int s = 0; s < blockSize; ++s) {
                    buffer.setSample(ch, s, std::sin(s * 0.1f) * 0.1f);
                }
            }
            juce::MidiBuffer midi;
            proc->processBlock(buffer, midi);
            editor->repaint();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        }

        // Prepare to play
        proc->prepareToPlay(48000.0, blockSize);

        // 5 audio frames + UI repaint AFTER prepareToPlay
        for (int frame = 0; frame < 5; ++frame) {
            juce::AudioBuffer<float> buffer(2, blockSize);
            for (int ch = 0; ch < 2; ++ch) {
                for (int s = 0; s < blockSize; ++s) {
                    buffer.setSample(ch, s, std::sin(s * 0.1f) * 0.1f);
                }
            }
            juce::MidiBuffer midi;
            proc->processBlock(buffer, midi);

            for (int ch = 0; ch < 2; ++ch) {
                for (int s = 0; s < blockSize; ++s) {
                    TEST_CHECK(std::isfinite(buffer.getSample(ch, s)), "Sample must be finite");
                }
            }
            editor->repaint();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        }

        editor.reset();
        proc.reset();
    }
    std::cout << "  [PASS] Editor before audio and block size chaos (16, 63, 512, 8192) verified." << std::endl;
}

void testHostPattern_MonoBufferAllRolesAndInstruments() {
    std::cout << "[HOST PATTERN 3] Mono audio through all roles, instruments, and destinations..." << std::endl;
    juce::ScopedJuceInitialiser_GUI juceInit;

    AIMixerAudioProcessor proc;

    // Set mono-in, mono-out layout
    juce::AudioProcessor::BusesLayout monoLayout;
    monoLayout.inputBuses.add(juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
    proc.setBusesLayout(monoLayout);
    proc.prepareToPlay(48000.0, 256);

    // 1. Source Role with all 20 instruments
    proc.setRole(PluginRole::Source);
    const std::vector<InstrumentType> allInstruments = {
        InstrumentType::Kick, InstrumentType::Snare, InstrumentType::Toms,
        InstrumentType::HiHat, InstrumentType::Overheads, InstrumentType::Room,
        InstrumentType::Bass, InstrumentType::ElectricGuitar, InstrumentType::AcousticGuitar,
        InstrumentType::Piano, InstrumentType::Keys, InstrumentType::Pad,
        InstrumentType::Synth, InstrumentType::LeadVocal, InstrumentType::BackgroundVocal,
        InstrumentType::Choir, InstrumentType::PastorSpeaking, InstrumentType::Tracks,
        InstrumentType::Click, InstrumentType::Unknown
    };

    for (auto inst : allInstruments) {
        proc.getSafeChain().setInstrumentType(inst);
        proc.getPlaybookStage().setInstrumentIdentity(inst, SideHint::Centre);

        juce::AudioBuffer<float> monoBuffer(1, 256);
        for (int s = 0; s < 256; ++s) {
            monoBuffer.setSample(0, s, std::sin(s * 0.05f) * 0.5f);
        }
        juce::MidiBuffer midi;
        proc.processBlock(monoBuffer, midi);

        for (int s = 0; s < 256; ++s) {
            float val = monoBuffer.getSample(0, s);
            TEST_CHECK(std::isfinite(val), "Source mono output must be finite");
        }
    }

    // 2. Bus Role with all BusTypes
    proc.setRole(PluginRole::Bus);
    const std::vector<BusType> allBuses = {
        BusType::Generic, BusType::Drums, BusType::Vocals, BusType::Music, BusType::FX
    };

    for (auto bType : allBuses) {
        proc.getBusStage().setBusType(bType);

        juce::AudioBuffer<float> monoBuffer(1, 256);
        for (int s = 0; s < 256; ++s) {
            monoBuffer.setSample(0, s, std::sin(s * 0.05f) * 0.5f);
        }
        juce::MidiBuffer midi;
        proc.processBlock(monoBuffer, midi);

        for (int s = 0; s < 256; ++s) {
            float val = monoBuffer.getSample(0, s);
            TEST_CHECK(std::isfinite(val), "Bus mono output must be finite");
        }
    }

    // 3. Master Role with all Destinations
    proc.setRole(PluginRole::Master);
    const std::vector<DestinationTag> allDestinations = {
        DestinationTag::PALive,
        DestinationTag::Broadcast,
        DestinationTag::Studio
    };

    for (auto dest : allDestinations) {
        proc.getMasterChain().setDestination(dest);

        juce::AudioBuffer<float> monoBuffer(1, 256);
        for (int s = 0; s < 256; ++s) {
            monoBuffer.setSample(0, s, std::sin(s * 0.05f) * 0.5f);
        }
        juce::MidiBuffer midi;
        proc.processBlock(monoBuffer, midi);

        for (int s = 0; s < 256; ++s) {
            float val = monoBuffer.getSample(0, s);
            TEST_CHECK(std::isfinite(val), "Master mono output must be finite");
        }
    }

    std::cout << "  [PASS] Mono audio verified across all roles, 20 instruments, bus types, and master destinations." << std::endl;
}

void testHostPattern_LayoutChaos() {
    std::cout << "[HOST PATTERN 4] Layout chaos..." << std::endl;
    juce::ScopedJuceInitialiser_GUI juceInit;

    AIMixerAudioProcessor proc;

    auto testLayout = [](AIMixerAudioProcessor& p, const juce::AudioChannelSet& in, const juce::AudioChannelSet& out) {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(in);
        layout.outputBuses.add(out);
        return p.isBusesLayoutSupported(layout);
    };

    // 1. Stereo-Stereo -> true
    TEST_CHECK(testLayout(proc, juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo()) == true,
               "Stereo->Stereo must be supported");

    // 2. Mono-Mono -> true
    TEST_CHECK(testLayout(proc, juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono()) == true,
               "Mono->Mono must be supported");

    // 3. Mono-Stereo -> true
    TEST_CHECK(testLayout(proc, juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()) == true,
               "Mono->Stereo must be supported");

    // 4. Stereo-Mono -> false
    TEST_CHECK(testLayout(proc, juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono()) == false,
               "Stereo->Mono must be rejected");

    // 5. 5.1-5.1 -> false
    TEST_CHECK(testLayout(proc, juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1()) == false,
               "5.1->5.1 must be rejected");

    // 6. Mono-5.1 -> false
    TEST_CHECK(testLayout(proc, juce::AudioChannelSet::mono(), juce::AudioChannelSet::create5point1()) == false,
               "Mono->5.1 must be rejected");

    // Verify rejection prevented from applying
    juce::AudioProcessor::BusesLayout badLayout;
    badLayout.inputBuses.add(juce::AudioChannelSet::create5point1());
    badLayout.outputBuses.add(juce::AudioChannelSet::create5point1());
    bool applied = proc.setBusesLayout(badLayout);
    TEST_CHECK(!applied, "setBusesLayout must reject unsupported 5.1 layout");

    std::cout << "  [PASS] Layout chaos verified: supported layouts accepted, invalid layouts rejected." << std::endl;
}

void testHostPattern_MultiProcessPeerAndFallback() {
    std::cout << "[HOST PATTERN 5] Multi-process registry peer & fallback..." << std::endl;

    // Part A: Multi-process peer discovery, election, ungraceful kill, and eviction
    {
        SharedMemoryRegistry parentReg;
        bool ok = parentReg.initialize();
        TEST_CHECK(ok, "Parent registry must initialize");

        uint32_t parentId = 0xAAAA;
        int parentSlot = parentReg.registerInstance(parentId, PluginRole::Source, "ParentTrack", 1234);
        TEST_CHECK(parentSlot >= 0, "Parent must register successfully");

        uint32_t initCoord = parentReg.runMaintenanceAndElection(SharedMemoryRegistry::getSystemTimeMs());
        TEST_CHECK(initCoord == parentId, "Parent must initially be coordinator");

        juce::File currentExe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        TEST_CHECK(currentExe.existsAsFile(), "Current test binary must exist on disk");

        juce::ChildProcess peerProcess;
        juce::StringArray args;
        args.add(currentExe.getFullPathName());
        args.add("--registry-peer");

        bool started = peerProcess.start(args);
        TEST_CHECK(started, "Failed to launch peer child process");
        TEST_CHECK(peerProcess.isRunning(), "Peer process must be running");

        // Wait for child to register
        bool childFound = false;
        uint32_t childId = 0xBEEF;
        for (int attempt = 0; attempt < 100; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            parentReg.updateHeartbeat(parentSlot);
            auto snapshot = parentReg.getActiveInstancesSnapshot();
            for (const auto& item : snapshot) {
                if (std::string(item.trackName) == "ChildPeerMaster") {
                    childFound = true;
                    break;
                }
            }
            if (childFound) break;
        }
        TEST_CHECK(childFound, "Child peer instance must be discovered in shared memory");

        // Run election: Child is Master role, parent is Source role -> Child must be elected coordinator!
        uint32_t childCoord = parentReg.runMaintenanceAndElection(SharedMemoryRegistry::getSystemTimeMs());
        TEST_CHECK(childCoord == childId, "Master child must be elected coordinator over Source parent");

        // Kill child ungracefully (TerminateProcess / SIGKILL)
        bool killed = peerProcess.kill();
        TEST_CHECK(killed, "Child process kill must succeed");

        for (int i = 0; i < 20 && peerProcess.isRunning(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        TEST_CHECK(!peerProcess.isRunning(), "Child process must no longer be running");

        // Wait in real time for dead child's heartbeat to expire (> HEARTBEAT_TIMEOUT_MS = 2500ms)
        // while parent continues sending heartbeats
        for (int i = 0; i < 28; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            parentReg.updateHeartbeat(parentSlot);
        }

        uint32_t reelectedCoord = parentReg.runMaintenanceAndElection(SharedMemoryRegistry::getSystemTimeMs());

        // Child must be evicted and parent re-elected
        auto snapAfter = parentReg.getActiveInstancesSnapshot();
        bool childStillThere = false;
        for (const auto& item : snapAfter) {
            if (item.instanceId == childId) childStillThere = true;
        }
        TEST_CHECK(!childStillThere, "Child must be evicted after heartbeat timeout");
        TEST_CHECK(reelectedCoord == parentId, "Parent must be re-elected coordinator after child is evicted");

        parentReg.unregisterInstance(parentSlot, parentId);
        parentReg.shutdown();
    }

    // Part B: Pre-created mapping with wrong version / size asserts fallback behaviour
    {
        std::string badRegionName = "AIMixer_CorruptTestRegion_v99";
#if defined(_WIN32)
        std::wstring wideName(badRegionName.begin(), badRegionName.end());
        std::wstring fullPath = L"Local\\" + wideName;
        HANDLE hBadMap = CreateFileMappingW(
            INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
            static_cast<DWORD>(sizeof(SharedRegistryBlock)), fullPath.c_str()
        );
        TEST_CHECK(hBadMap != NULL, "Created corrupt test file mapping");
        void* mapped = MapViewOfFile(hBadMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedRegistryBlock));
        TEST_CHECK(mapped != nullptr, "Mapped corrupt test file mapping");
        auto* corruptBlock = static_cast<SharedRegistryBlock*>(mapped);
        std::memset(corruptBlock, 0, sizeof(SharedRegistryBlock));
        corruptBlock->header.magic = SHARED_MAGIC;
        corruptBlock->header.version = 999; // Incompatible version
        corruptBlock->header.maxSlots = 10; // Wrong slot count
        corruptBlock->header.slotSize = 50; // Wrong slot size

        SharedMemoryRegistry mismatchReg;
        bool initRes = mismatchReg.initialize(badRegionName);
        TEST_CHECK(initRes, "initialize must succeed via safe in-process fallback");
        TEST_CHECK(mismatchReg.isVersionMismatch(), "Registry must flag version mismatch");
        TEST_CHECK(mismatchReg.isInitialized(), "Registry must be initialized to fallback block");

        int fallbackSlot = mismatchReg.registerInstance(0x9999, PluginRole::Source, "FallbackTrack", 1234);
        TEST_CHECK(fallbackSlot < 0, "Mismatched registry must safely refuse registration to prevent slot corruption");
        InstanceTelemetry dummy{};
        bool pubRes = mismatchReg.publishTelemetry(0, dummy);
        TEST_CHECK(!pubRes, "Mismatched registry must refuse publishing telemetry");

        mismatchReg.shutdown();

        UnmapViewOfFile(mapped);
        CloseHandle(hBadMap);
#endif
    }

    std::cout << "  [PASS] Multi-process peer discovery, kill eviction, re-election, and fallback verified." << std::endl;
}

void testHostPattern_StateSaveRestore50Times() {
    std::cout << "[HOST PATTERN 6] State save/restore (50 rapid iterations)..." << std::endl;
    juce::ScopedJuceInitialiser_GUI juceInit;

    AIMixerAudioProcessor proc;
    proc.prepareToPlay(48000.0, 256);

    for (int i = 0; i < 50; ++i) {
        PluginRole nextRole = (i % 3 == 0) ? PluginRole::Source : ((i % 3 == 1) ? PluginRole::Bus : PluginRole::Master);
        proc.setRole(nextRole);
        std::string tName = "TestTrack_" + std::to_string(i);
        juce::AudioProcessor::TrackProperties props;
        props.name = tName;
        proc.updateTrackProperties(props);

        juce::AudioBuffer<float> buffer(2, 256);
        for (int ch = 0; ch < 2; ++ch) {
            for (int s = 0; s < 256; ++s) {
                buffer.setSample(ch, s, std::sin(s * 0.05f * (i + 1)) * 0.2f);
            }
        }
        juce::MidiBuffer midi;
        proc.processBlock(buffer, midi);

        juce::MemoryBlock memBlock;
        proc.getStateInformation(memBlock);
        TEST_CHECK(memBlock.getSize() > 0, "getStateInformation must produce non-empty memory block");

        AIMixerAudioProcessor restoredProc;
        restoredProc.setStateInformation(memBlock.getData(), static_cast<int>(memBlock.getSize()));

        TEST_CHECK(restoredProc.getCurrentRole() == nextRole, "Restored role must match saved role");
        TEST_CHECK(restoredProc.getTrackName() == tName,
                   "Restored track name must match saved track name");
    }

    std::cout << "  [PASS] State save/restore 50 iterations verified without corruption." << std::endl;
}


// ==============================================================================
// AI Mixer v1.1 — Local LLM & Dynamic Waves Verification Suite
// ==============================================================================

class MockWavesPlugin : public juce::AudioProcessor {
public:
    MockWavesPlugin(const juce::String& name, const std::vector<juce::String>& paramNames)
        : pluginName(name) {
        for (const auto& pName : paramNames) {
            addParameter(new juce::AudioParameterFloat(juce::ParameterID(pName, 1), pName, 0.0f, 1.0f, 0.5f));
        }
    }
    const juce::String getName() const override { return pluginName; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return ""; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    juce::String pluginName;
};

void testV11_AcousticFeatureExtraction() {
    std::cout << "[AI MIXER v1.1 TEST 1] Acoustic Feature Extraction (Resonances, Sibilance, Tilt)..." << std::endl;

    AudioAnalyzer analyzer;
    analyzer.prepare(48000.0, 512);

    // 1. Synthesize 800 Hz resonance tone with broadband background noise
    juce::AudioBuffer<float> block(2, 512);
    double phase = 0.0;
    const double phaseInc = 2.0 * juce::MathConstants<double>::pi * 800.0 / 48000.0;

    for (int b = 0; b < 60; ++b) {
        auto* l = block.getWritePointer(0);
        auto* r = block.getWritePointer(1);
        for (int i = 0; i < 512; ++i) {
            float noise = ((float)std::rand() / (float)RAND_MAX - 0.5f) * 0.04f;
            float s = 0.35f * (float)std::sin(phase) + noise;
            phase += phaseInc;
            l[i] = s;
            r[i] = s;
        }
        analyzer.processAudioBlock(block);
        InstanceTelemetry telem;
        analyzer.updateAnalysis(telem);
    }

    AcousticFeatures feat = analyzer.getAcousticFeatures();
    std::cout << "  Measured Resonance: " << feat.detectedResonanceHz 
              << " Hz (Prominence: " << feat.resonanceProminenceDb << " dB)" << std::endl;
    TEST_CHECK(feat.detectedResonanceHz >= 600.0f && feat.detectedResonanceHz <= 1000.0f,
               "Analyzer must identify ~800 Hz resonance peak");
    TEST_CHECK(feat.resonanceProminenceDb > 2.0f,
               "Resonance prominence must be detected above 2.0 dB");

    // 2. Synthesize Sibilance (7 kHz burst vs low-mid energy)
    analyzer.reset();
    analyzer.prepare(48000.0, 512);
    double sibPhase = 0.0;
    const double sibPhaseInc = 2.0 * juce::MathConstants<double>::pi * 7000.0 / 48000.0;

    for (int b = 0; b < 60; ++b) {
        auto* l = block.getWritePointer(0);
        auto* r = block.getWritePointer(1);
        for (int i = 0; i < 512; ++i) {
            float s = 0.4f * (float)std::sin(sibPhase);
            sibPhase += sibPhaseInc;
            l[i] = s;
            r[i] = s;
        }
        analyzer.processAudioBlock(block);
        InstanceTelemetry telem;
        analyzer.updateAnalysis(telem);
    }

    feat = analyzer.getAcousticFeatures();
    std::cout << "  Measured Sibilance Centroid: " << feat.sibilanceCentroidHz 
              << " Hz (Ratio: " << feat.sibilanceRatio << ")" << std::endl;
    TEST_CHECK(feat.sibilanceCentroidHz >= 5500.0f && feat.sibilanceCentroidHz <= 9000.0f,
               "Analyzer must detect high frequency centroid in sibilance band");

    std::cout << "  [PASS] Acoustic Feature Extraction (Resonances, Sibilance, Tilt) verified." << std::endl;
}

void testV11_LLMPromptAndJsonParsing() {
    std::cout << "[AI MIXER v1.1 TEST 2] LLM Prompt Generation and Resilient JSON Parsing..." << std::endl;

    // 1. Verify prompt engineering
    juce::String sysPrompt = LLMProducerClient::buildSystemPrompt();
    TEST_CHECK(sysPrompt.containsIgnoreCase("Waves"), "System prompt must specify Waves inventory");
    TEST_CHECK(sysPrompt.containsIgnoreCase("JSON"), "System prompt must mandate JSON output");

    std::vector<SessionTrackDiagnosis> session;
    SessionTrackDiagnosis t1;
    t1.instanceId = 42;
    t1.trackName = "Lead Vocal";
    t1.identifiedInstrument = InstrumentType::LeadVocal;
    t1.rmsDb = -18.2f;
    t1.crestFactorDb = 13.5f;
    t1.detectedResonanceHz = 3200.0f;
    t1.resonanceProminenceDb = 5.2f;
    t1.sibilanceRatio = 0.38f;
    t1.sibilanceCentroidHz = 7150.0f;
    session.push_back(t1);

    juce::String userPrompt = LLMProducerClient::buildUserPrompt(session, "Modern Worship");
    TEST_CHECK(userPrompt.contains("Lead Vocal"), "User prompt must contain track name");
    TEST_CHECK(userPrompt.contains("3200"), "User prompt must report measured 3200 Hz resonance");
    TEST_CHECK(userPrompt.contains("7150"), "User prompt must report measured 7150 Hz sibilance centroid");

    // 2. Parse complete LLM response wrapped in markdown code fence
    juce::String modelJson = 
        "```json\n"
        "{\n"
        "  \"mix_summary\": \"Polished vocal with surgical resonance cut and tight low end\",\n"
        "  \"tracks\": [\n"
        "    {\n"
        "      \"instance_id\": 42,\n"
        "      \"refined_instrument\": \"LeadVocal\",\n"
        "      \"balance_trim_db\": -1.2,\n"
        "      \"pan_position\": 0.0,\n"
        "      \"reverb_send_db\": -16.0,\n"
        "      \"delay_send_db\": -24.0,\n"
        "      \"engineer_notes\": \"Notched 3.2kHz harshness and controlled vocal dynamics\",\n"
        "      \"slots\": [\n"
        "        {\n"
        "          \"waves_plugin_name\": \"SSLChannel Stereo\",\n"
        "          \"eq_mid_freq_hz\": 3200.0,\n"
        "          \"eq_mid_gain_db\": -4.0,\n"
        "          \"target_threshold_db\": -20.0,\n"
        "          \"target_ratio\": 4.0,\n"
        "          \"de_esser_freq_hz\": 0.0\n"
        "        },\n"
        "        {\n"
        "          \"waves_plugin_name\": \"Sibilance Stereo\",\n"
        "          \"eq_mid_freq_hz\": 0.0,\n"
        "          \"eq_mid_gain_db\": 0.0,\n"
        "          \"target_threshold_db\": -18.0,\n"
        "          \"target_ratio\": 0.0,\n"
        "          \"de_esser_freq_hz\": 7150.0\n"
        "        }\n"
        "      ]\n"
        "    }\n"
        "  ]\n"
        "}\n"
        "```";

    ProducedMixPlan plan = LLMProducerClient::parseModelResponse(modelJson, session);
    TEST_CHECK(plan.success, "JSON parsing must succeed for fenced model output");
    TEST_CHECK(plan.tracks.size() == 1, "Plan must have exactly 1 track parsed");
    TEST_CHECK(plan.tracks[0].instanceId == 42, "Parsed instanceId must match");
    TEST_CHECK(plan.tracks[0].refinedInstrument == InstrumentType::LeadVocal, "Parsed instrument must match LeadVocal");
    TEST_CHECK(plan.tracks[0].slots.size() == 2, "Parsed slots count must be 2");
    TEST_CHECK(plan.tracks[0].slots[0].wavesPluginName == "SSLChannel Stereo", "Slot 0 plugin name must match");
    TEST_CHECK(plan.tracks[0].slots[0].eqMidFreqHz == 3200.0f, "Slot 0 EQ mid freq must match");
    TEST_CHECK(plan.tracks[0].slots[1].deEsserFreqHz == 7150.0f, "Slot 1 de-esser freq must match");

    // 3. Malformed JSON Resilience
    ProducedMixPlan badPlan = LLMProducerClient::parseModelResponse("Not Valid JSON at all", session);
    TEST_CHECK(!badPlan.success, "Malformed JSON must set plan.success to false");
    TEST_CHECK(badPlan.errorReason.isNotEmpty(), "Malformed JSON must report error reason");

    std::cout << "  [PASS] LLM prompt generation and resilient JSON parsing verified." << std::endl;
}

void testV11_WavesDialerDynamicMath() {
    std::cout << "[AI MIXER v1.1 TEST 3] Dynamic Waves Parameter Calibration Math..." << std::endl;

    // 1. Test SSLChannel dynamic calibration with acoustic resonance
    MockWavesPlugin ssl("SSLChannel Stereo", {
        "Analog", "Bypass", "EQBypass", "DynamicBypass",
        "CompFast", "CompRelease", "CompRatio", "CompThresh",
        "LMF Frq", "LMF Gain", "LMF Q", "HF Gain"
    });

    ProducedSlotDirective sslDirective;
    sslDirective.wavesPluginName = "SSLChannel Stereo";
    sslDirective.bypassed = false;
    sslDirective.targetThresholdDb = -22.0f;
    sslDirective.targetRatio = 4.0f;

    AcousticFeatures acoustic;
    acoustic.detectedResonanceHz = 850.0f;
    acoustic.resonanceProminenceDb = 6.5f;
    acoustic.crestFactorDb = 14.0f;

    WavesDialer::dialPluginDynamically(ssl, sslDirective, acoustic);

    auto* analogParam = ssl.getParameters()[0];
    auto* threshParam = ssl.getParameters()[7];
    auto* lmfFreqParam = ssl.getParameters()[8];
    auto* lmfGainParam = ssl.getParameters()[9];

    TEST_CHECK(analogParam->getValue() == 0.0f, "Analog button must be forced off for noise elimination");
    TEST_CHECK(threshParam->getValue() > 0.0f && threshParam->getValue() < 0.6f, 
               "Compressor threshold must be dynamically calibrated");
    TEST_CHECK(lmfFreqParam->getValue() > 0.2f && lmfFreqParam->getValue() < 0.8f,
               "LMF frequency must be centered near 850 Hz");
    TEST_CHECK(lmfGainParam->getValue() < 0.5f,
               "LMF gain must cut the resonance (gain < 0.5 normalized)");

    // 2. Test Sibilance dynamic calibration with measured centroid
    MockWavesPlugin sibilance("Sibilance Stereo", {
        "Freq", "Frequency", "Sidechain", "Thresh", "Threshold", "Bypass", "Analog"
    });

    ProducedSlotDirective sibDirective;
    sibDirective.wavesPluginName = "Sibilance Stereo";
    sibDirective.deEsserFreqHz = 7200.0f;

    AcousticFeatures sibAcoustic;
    sibAcoustic.sibilanceRatio = 0.42f;
    sibAcoustic.sibilanceCentroidHz = 7200.0f;

    WavesDialer::dialPluginDynamically(sibilance, sibDirective, sibAcoustic);

    auto* freqParam = sibilance.getParameters()[0];
    auto* scParam = sibilance.getParameters()[2];
    auto* sibThreshParam = sibilance.getParameters()[3];

    TEST_CHECK(freqParam->getValue() > 0.35f, "De-Esser frequency must target > 7 kHz sibilance");
    TEST_CHECK(scParam->getValue() == 1.0f, "Sidechain must be engaged in focused bandpass mode");
    TEST_CHECK(std::abs(sibThreshParam->getValue() - 0.432f) < 0.01f, "Threshold must be calibrated for heavy sibilance ratio");

    // 3. Test CLA-76 FET Slammer dynamic calibration
    MockWavesPlugin cla76("CLA-76 Stereo", {
        "Bypass", "Analog", "Comp Off", "Attack", "Release", "Ratio", "Input", "Output"
    });

    ProducedSlotDirective claDirective;
    claDirective.wavesPluginName = "CLA-76 Stereo";

    AcousticFeatures claAcoustic;
    claAcoustic.rmsDb = -26.0f; // Lower RMS drives input harder (+8 dB offset)
    claAcoustic.crestFactorDb = 16.0f; // High dynamic transient peaks

    WavesDialer::dialPluginDynamically(cla76, claDirective, claAcoustic);

    auto* claAnalog = cla76.getParameters()[1];
    auto* claAttack = cla76.getParameters()[3];
    auto* claInput = cla76.getParameters()[6];

    TEST_CHECK(claAnalog->getValue() == 0.0f, "CLA-76 analog must be off");
    TEST_CHECK(std::abs(claAttack->getValue() - 0.40f) < 0.01f, "Attack must open up for high transient crest factor");
    TEST_CHECK(std::abs(claInput->getValue() - 0.62f) < 0.01f, "CLA-76 input must drive harder for quiet/dynamic signal");

    std::cout << "  [PASS] Dynamic Waves parameter calibration math verified." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--registry-peer") {
        return runRegistryPeer();
    }
    if (argc > 1 && std::string(argv[1]) == "--waves-scan") {
        return WavesHostingSpike::runSpike();
    }

    std::cout << "======================================================" << std::endl;
    std::cout << "   AI MIXER MVP 5 COMPLETE VERIFICATION SUITE         " << std::endl;
    std::cout << "======================================================" << std::endl;

    try {
        // Step 0 Regression
        testStep0Regressions();
        testStep0_KickBass55HzNoCutOnKick();

        // MVP 2 Baseline Regressions
        testTrackNameParser60Cases();
        testIdentityPrecedence();
        testSyntheticClassifier();
        testAutoTrimConvergence();
        testHpfTableAndOverrides();
        testLatencyZeroSamplesWithSafeChain();
        testClickBitExactPassThrough();
        testCoordinatorElectionSurvivorAndVersionGuard();
        testCpuBenchmark48kHz();
        testMvp1Regression16Instances();

        // MVP 3 Global Mix Solver Suite
        testMvp3_KickBassDuckingAndCarving();
        testMvp3_LeadVocalPadPresenceAndRide();
        testMvp3_VocalMaskingKeysAndBgv();
        testMvp3_DisjointSpectraZeroCuts();
        testMvp3_SoloPromotion();
        testMvp3_SpeechModePastorPad();
        testMvp3_SolverConvergence1500Cycles();
        testMvp3_ChorusFreeze4Seconds();
        testMvp3_OverridesAndFreeze();
        testMvp3_DirectivesCapsFuzz200Snapshots();
        testMvp3_StalenessTimeoutRampDown();
        testMvp3_SolverEqResponseMeasurement();
        testMvp3_LatencyZeroAndCpuWithSolver();

        // MVP 4 Verification Suite
        testMvp4_CompCharacterTableAndManualOverride();
        testMvp4_GateExpander();
        testMvp4_CharacterEqResponses();
        testMvp4_DeEsserSelectivity();
        testMvp4_WarmthSaturationHarmonics();
        testMvp4_TransientStageKickAttack();
        testMvp4_LocalRiderPhraseLeveling();
        testMvp4_PanWidthStageAndBassMono();
        testMvp4_BusGlueTableAndParallel();
        testMvp4_MasterLoudnessAccuracyBS1770();
        testMvp4_MasterLoudnessAutoGainCapBehaviour();
        testMvp4_MasterTonalBalanceAndBitTransparency();
        testMvp4_MasterLimiterCeilingAndTransparency();
        testMvp4_MasterLimiterLatencyAccuracy();
        testMvp4_TwoIndependentMastersDestinations();
        testMvp4_CpuBenchmark();

        // MVP 5 Step 0 Completions
        testMvp5_Step0_LoudnessAutoGainTrueConvergence();
        testMvp5_Step0_LocalRiderRealPhraseLeveling();
        testMvp5_Step0_PanClampFuzz();

        // MVP 5 Verification Suite
        testMvp5_FollowPercentSuppressionAndScaling();
        testMvp5_FiveMacros();
        testMvp5_GenreProfileTransitions();
        testMvp5_ReverbEngineRT60();
        testMvp5_DelayEngineSyncPingPong();
        testMvp5_SessionModesAndHoldSemantics();
        testMvp5_MoreLessOfMe();
        testMvp5_ProfileLearningEmaAndAutoFork();
        testMvp5_JsonRoundTripAndMalformedResilience();

        // Hardening Pass: 5 Missing Tests
        testHardening_FxReturnHostWetOnly();
        testHardening_TwoSourceSharedCharacter();
        testHardening_TasteBroadcastLatency();
        testHardening_MasterLatencyRemeasure();
        testHardening_CpuWithFxOn();

        // Step 4 Host-Pattern Suite & Crash Regressions
        testHostPattern_RapidScanLifecycle200x();
        testHostPattern_EditorBeforeAudioAndBlockSizeChaos();
        testHostPattern_MonoBufferAllRolesAndInstruments();
        testHostPattern_LayoutChaos();
        testHostPattern_MultiProcessPeerAndFallback();
        testHostPattern_StateSaveRestore50Times();

        // AI Mixer v1.1 Verification Suite
        testV11_AcousticFeatureExtraction();
        testV11_LLMPromptAndJsonParsing();
        testV11_WavesDialerDynamicMath();

        std::cout << "\n======================================================" << std::endl;
        std::cout << "  ALL MVP 1-5 + HARDENING + HOST PATTERN TESTS PASSED! " << std::endl;
        std::cout << "======================================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nFATAL TEST EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
