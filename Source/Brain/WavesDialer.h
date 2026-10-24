#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Common/Types.h"
#include "LLMProducerClient.h"
#include <string>

namespace aimixer {

class WavesDialer {
public:
    // Helper to find a parameter by name (case-insensitive substring)
    static juce::AudioProcessorParameter* findParam(juce::AudioProcessor& inst, const juce::String& namePattern) {
        for (auto* p : inst.getParameters()) {
            if (p != nullptr && p->getName(64).containsIgnoreCase(namePattern)) {
                return p;
            }
        }
        return nullptr;
    }

    // Set normalized value safely with host notification
    static void setParam(juce::AudioProcessor& inst, const juce::String& namePattern, float val) {
        if (auto* p = findParam(inst, namePattern)) {
            p->setValueNotifyingHost(std::clamp(val, 0.0f, 1.0f));
        }
    }

    // ==============================================================================
    // Dynamic Autonomous Calibration: Dials ANY Waves plugin based on measured
    // acoustic diagnostics (resonances, sibilance bursts, crest factor, and LLM directives)
    // ==============================================================================
    static void dialPluginDynamically(juce::AudioProcessor& inst,
                                      const ProducedSlotDirective& directive,
                                      const AcousticFeatures& acoustic) {
        juce::String name = inst.getName();
        setParam(inst, "Bypass", directive.bypassed ? 1.0f : 0.0f);
        setParam(inst, "Analog", 0.0f);
        setParam(inst, "ANLG", 0.0f);

        // --- 1. SSL Channel & SSL EQ ---
        if (name.containsIgnoreCase("SSLChannel") || name.containsIgnoreCase("SSLEQ") || name.containsIgnoreCase("SSLGChannel")) {
            setParam(inst, "EQBypass", 0.0f);
            setParam(inst, "DynamicBypass", 0.0f);

            bool isFast = (acoustic.isPercussive > 0.4f || acoustic.crestFactorDb > 13.0f);
            setParam(inst, "CompFast", isFast ? 1.0f : 0.0f);
            setParam(inst, "CompRelease", 0.5f);

            if (directive.targetThresholdDb != 0.0f) {
                float normThresh = std::clamp((directive.targetThresholdDb + 30.0f) / 30.0f, 0.1f, 0.9f);
                setParam(inst, "CompThresh", normThresh);
            }

            float targetResHz = (directive.eqMidFreqHz > 20.0f) ? directive.eqMidFreqHz : acoustic.detectedResonanceHz;
            float targetCutDb = (directive.eqMidGainDb != 0.0f) ? directive.eqMidGainDb
                              : (acoustic.resonanceProminenceDb > 2.5f ? -std::clamp(acoustic.resonanceProminenceDb, 2.0f, 6.0f) : 0.0f);

            if (targetResHz >= 200.0f && targetResHz <= 2500.0f) {
                float normFreq = std::clamp(std::log10(targetResHz / 200.0f) / std::log10(2500.0f / 200.0f), 0.0f, 1.0f);
                float normGain = std::clamp(0.5f + (targetCutDb / 30.0f), 0.0f, 1.0f);
                setParam(inst, "LMF Frq", normFreq);
                setParam(inst, "LMF Gain", normGain);
                setParam(inst, "LMF Q", 0.6f);
            } else if (targetResHz > 2500.0f) {
                float normFreq = std::clamp(std::log10(targetResHz / 1500.0f) / std::log10(16000.0f / 1500.0f), 0.0f, 1.0f);
                float normGain = std::clamp(0.5f + (targetCutDb / 30.0f), 0.0f, 1.0f);
                setParam(inst, "HMF Frq", normFreq);
                setParam(inst, "HMF Gain", normGain);
                setParam(inst, "HMF Q", 0.6f);
            }
            return;
        }

        // --- 2. F6 Dynamic EQ ---
        if (name.containsIgnoreCase("F6")) {
            setParam(inst, "Bypass", 0.0f);
            float targetResHz = (directive.eqMidFreqHz > 20.0f) ? directive.eqMidFreqHz : acoustic.detectedResonanceHz;
            if (targetResHz > 40.0f) {
                setParam(inst, "Band 3 Freq", std::clamp(targetResHz / 20000.0f, 0.01f, 1.0f));
                setParam(inst, "Band 3 Gain", std::clamp(0.5f + (directive.eqMidGainDb / 36.0f), 0.1f, 0.9f));
                setParam(inst, "Band 3 Q", 0.7f);
                setParam(inst, "Band 3 Threshold", 0.45f);
            }
            return;
        }

        // --- 3. CLA-76 FET Peak Tamer ---
        if (name.containsIgnoreCase("CLA-76")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Analog", 0.0f);
            setParam(inst, "Comp Off", 0.0f);

            if (acoustic.crestFactorDb > 14.0f || acoustic.isPercussive > 0.5f) {
                setParam(inst, "Attack", 0.40f);
                setParam(inst, "Release", 0.85f);
            } else {
                setParam(inst, "Attack", 0.65f);
                setParam(inst, "Release", 0.70f);
            }
            setParam(inst, "Ratio", 0.25f);
            float inputDrive = std::clamp(0.50f + ((-18.0f - acoustic.rmsDb) * 0.015f), 0.35f, 0.75f);
            setParam(inst, "Input", inputDrive);
            setParam(inst, "Output", 0.50f);
            return;
        }

        // --- 4. RVox Vocal Leveler ---
        if (name.containsIgnoreCase("RVox")) {
            setParam(inst, "Bypass", 0.0f);
            float compThresh = (directive.targetThresholdDb != 0.0f) ? directive.targetThresholdDb
                             : (acoustic.rmsDb + (acoustic.crestFactorDb * 0.35f));
            float normComp = std::clamp((compThresh + 36.0f) / 36.0f, 0.40f, 0.95f);
            setParam(inst, "Compression", normComp);
            setParam(inst, "Gate", 0.15f);
            setParam(inst, "Gain", 0.88f);
            return;
        }

        // --- 5. CLA-2A / CLA-3A Opto Compressor ---
        if (name.containsIgnoreCase("CLA-2A") || name.containsIgnoreCase("CLA-3A")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Analog", 0.0f);
            float prVal = std::clamp(0.50f + (acoustic.crestFactorDb * 0.012f), 0.40f, 0.75f);
            setParam(inst, "Peak Reduction", prVal);
            setParam(inst, "Gain", 0.52f);
            return;
        }

        // --- 6. De-Esser / Sibilance ---
        if (name.containsIgnoreCase("DeEsser") || name.containsIgnoreCase("Sibilance") || name.containsIgnoreCase("RDeEsser")) {
            setParam(inst, "Bypass", 0.0f);
            float targetSibFreq = (directive.deEsserFreqHz > 3000.0f) ? directive.deEsserFreqHz
                                : (acoustic.sibilanceCentroidHz > 3000.0f ? acoustic.sibilanceCentroidHz : 6800.0f);
            float normFreq = std::clamp(std::log10(targetSibFreq / 2000.0f) / std::log10(12000.0f / 2000.0f), 0.1f, 0.9f);
            setParam(inst, "Freq", normFreq);
            setParam(inst, "Frequency", normFreq);
            setParam(inst, "Sidechain", 1.0f);
            setParam(inst, "FilterType", 0.0f);
            float thresh = std::clamp(0.60f - (acoustic.sibilanceRatio * 0.4f), 0.25f, 0.65f);
            setParam(inst, "Thresh", thresh);
            setParam(inst, "Threshold", thresh);
            return;
        }

        // --- 7. RBass / Submarine (Sub-Harmonics tuned to Fundamental) ---
        if (name.containsIgnoreCase("RBass") || name.containsIgnoreCase("Submarine") || name.containsIgnoreCase("MaxxBass")) {
            setParam(inst, "Bypass", 0.0f);
            float f0 = (acoustic.fundamentalHz > 30.0f && acoustic.fundamentalHz < 200.0f) ? acoustic.fundamentalHz : 60.0f;
            float normPitch = std::clamp(std::log10(f0 / 30.0f) / std::log10(200.0f / 30.0f), 0.1f, 0.9f);
            setParam(inst, "Freq", normPitch);
            float intensity = (acoustic.subEnergyRatio < 0.10f) ? 0.60f : 0.40f;
            setParam(inst, "Intensity", intensity);
            setParam(inst, "Original Bass", 0.50f);
            return;
        }

        // --- 8. Magma Lil Tube / Kramer Tape (Harmonic Warming) ---
        if (name.containsIgnoreCase("Magma") || name.containsIgnoreCase("Kramer Tape") || name.containsIgnoreCase("Saturator")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Noise", 0.0f);
            float drive = (acoustic.harmonicRichness < 0.25f) ? 0.48f : 0.25f;
            setParam(inst, "Drive", drive);
            setParam(inst, "Record Level", drive);
            return;
        }

        // --- 9. Smack Attack (Transient Punch) ---
        if (name.containsIgnoreCase("Smack Attack") || name.containsIgnoreCase("TransX")) {
            setParam(inst, "Bypass", 0.0f);
            float attackBoost = (acoustic.attackSharpness < 0.4f) ? 0.65f : 0.50f;
            setParam(inst, "Attack", attackBoost);
            return;
        }

        // --- 10. SSLComp & API-2500 (Bus VCA Glue) ---
        if (name.containsIgnoreCase("SSLComp") || name.containsIgnoreCase("SSL G-Master") || name.containsIgnoreCase("API-2500")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "ANLG", 0.0f);
            setParam(inst, "IN", 1.0f);
            setParam(inst, "Attack", 0.50f);
            setParam(inst, "Release", 0.75f);
            setParam(inst, "Thresh", 0.55f);
            setParam(inst, "SC Filter", 1.0f);
            return;
        }
    }

    // ==============================================================================
    // Autonomous Calibration: Dials the plugin into an optimal sonic sweet spot
    // ==============================================================================
    static void dialPluginForInstrument(juce::AudioProcessor& inst,
                                        InstrumentType instrument,
                                        BusType busType,
                                        float inputRmsDb = -18.0f) {
        juce::ignoreUnused(busType, inputRmsDb);
        juce::String name = inst.getName();

        // 1. SSL Channel (Console Preamp, Filters, VCA Dynamics & 4-Band Black/Brown EQ)
        if (name.containsIgnoreCase("SSLChannel") || name.containsIgnoreCase("SSLGChannel")) {
            // Analog button OFF (eliminates 60Hz hum hiss in clean DAW sessions)
            setParam(inst, "Analog", 0.0f);
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "EQBypass", 0.0f);
            setParam(inst, "DynamicBypass", 0.0f);

            // Turn Fast Attack ON for percussive instruments
            bool isFast = (instrument == InstrumentType::Snare || instrument == InstrumentType::Kick ||
                           instrument == InstrumentType::Toms || instrument == InstrumentType::ElectricGuitar);
            setParam(inst, "CompFast", isFast ? 1.0f : 0.0f);
            setParam(inst, "CompRelease", 0.5f); // 400ms auto
            setParam(inst, "CompRatio", 0.25f);  // 4:1 ratio

            // Threshold: calibrated so typical -18 dBFS signal catches 2.5 - 4 dB of gain reduction
            setParam(inst, "CompThresh", 0.45f);

            // Instrument-specific EQ shaping
            switch (instrument) {
                case InstrumentType::Kick:
                    setParam(inst, "HP Frq", 0.15f);    // ~35 Hz HPF to clear sub-rumble
                    setParam(inst, "LP Frq", 0.0f);     // Open
                    setParam(inst, "LF Frq", 0.22f);    // ~60 Hz weight
                    setParam(inst, "LF Gain", 0.65f);   // +3.5 dB sub thump
                    setParam(inst, "LMF Frq", 0.26f);   // ~350 Hz boxiness
                    setParam(inst, "LMF Gain", 0.35f);  // -3.5 dB mud scoop
                    setParam(inst, "LMF Q", 0.45f);
                    setParam(inst, "HMF Frq", 0.62f);   // ~4.2 kHz click
                    setParam(inst, "HMF Gain", 0.62f);  // +3 dB beater attack
                    break;

                case InstrumentType::Snare:
                    setParam(inst, "HP Frq", 0.25f);    // ~80 Hz HPF
                    setParam(inst, "LF Frq", 0.40f);    // ~200 Hz body
                    setParam(inst, "LF Gain", 0.62f);   // +3 dB body punch
                    setParam(inst, "LMF Frq", 0.35f);   // ~600 Hz ring
                    setParam(inst, "LMF Gain", 0.40f);  // -2.5 dB notch
                    setParam(inst, "HMF Frq", 0.60f);   // ~3.5 kHz crack
                    setParam(inst, "HMF Gain", 0.65f);  // +3.5 dB crack
                    setParam(inst, "HF Frq", 0.55f);    // ~10 kHz air
                    setParam(inst, "HF Gain", 0.58f);   // +2 dB top-end sizzle
                    // Noise gate for snare isolation
                    setParam(inst, "ExpGate", 1.0f);
                    setParam(inst, "ExpThresh", 0.32f);
                    break;

                case InstrumentType::Bass:
                    setParam(inst, "HP Frq", 0.12f);    // ~30 Hz sub clean
                    setParam(inst, "LF Frq", 0.30f);    // ~100 Hz punch
                    setParam(inst, "LF Gain", 0.58f);   // +2 dB solid bottom
                    setParam(inst, "LMF Frq", 0.45f);   // ~800 Hz finger snap
                    setParam(inst, "LMF Gain", 0.58f);  // +2 dB attack clarity
                    setParam(inst, "HF Frq", 0.35f);
                    setParam(inst, "HF Gain", 0.45f);   // -1.5 dB fret hiss reduction
                    break;

                case InstrumentType::LeadVocal:
                case InstrumentType::PastorSpeaking:
                case InstrumentType::BackgroundVocal:
                    setParam(inst, "HP Frq", 0.32f);    // ~100 Hz vocal HPF
                    setParam(inst, "LMF Frq", 0.30f);   // ~300 Hz proximity mud
                    setParam(inst, "LMF Gain", 0.42f);  // -2 dB mud scoop
                    setParam(inst, "LMF Q", 0.35f);
                    setParam(inst, "HMF Frq", 0.55f);   // ~3 kHz intelligibility
                    setParam(inst, "HMF Gain", 0.58f);  // +2 dB presence
                    setParam(inst, "HF Frq", 0.65f);    // ~12 kHz air
                    setParam(inst, "HF Gain", 0.62f);   // +3 dB sheen
                    break;

                case InstrumentType::ElectricGuitar:
                    setParam(inst, "HP Frq", 0.28f);    // ~90 Hz HPF
                    setParam(inst, "LMF Frq", 0.40f);   // ~500 Hz cab resonance
                    setParam(inst, "LMF Gain", 0.42f);  // -2 dB scoop
                    setParam(inst, "HMF Frq", 0.50f);   // ~2.5 kHz guitar cut
                    setParam(inst, "HMF Gain", 0.60f);  // +2.5 dB bite
                    break;

                case InstrumentType::AcousticGuitar:
                    setParam(inst, "HP Frq", 0.30f);    // ~95 Hz HPF
                    setParam(inst, "LMF Frq", 0.26f);   // ~250 Hz boom
                    setParam(inst, "LMF Gain", 0.40f);  // -2.5 dB boom reduction
                    setParam(inst, "HF Frq", 0.60f);    // ~10 kHz sparkle
                    setParam(inst, "HF Gain", 0.62f);   // +3 dB air
                    break;

                default:
                    // Transparent studio channel strip setup
                    setParam(inst, "HP Frq", 0.20f);    // 50 Hz HPF
                    setParam(inst, "LF Gain", 0.50f);
                    setParam(inst, "LMF Gain", 0.50f);
                    setParam(inst, "HMF Gain", 0.50f);
                    setParam(inst, "HF Gain", 0.50f);
                    break;
            }
            return;
        }

        // 2. CLA-76 (Ultra-Fast FET Peak Limiter / Slammer)
        if (name.containsIgnoreCase("CLA-76")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Analog", 0.0f); // Pure digital noise-free floor
            setParam(inst, "Comp Off", 0.0f);

            // Attack (fastest = 7 / 1.0, slowest = 1 / 0.0)
            // On a 76, clockwise is faster attack
            if (instrument == InstrumentType::Snare || instrument == InstrumentType::Kick) {
                setParam(inst, "Attack", 0.30f); // Medium-slow attack to let initial transient punch through
                setParam(inst, "Release", 0.85f); // Fast release to bring up body & sustain
                setParam(inst, "Ratio", 0.25f);   // 4:1
                setParam(inst, "Input", 0.55f);   // Drives 3-5 dB GR
                setParam(inst, "Output", 0.52f);  // Unity makeup
            } else if (instrument == InstrumentType::Bass) {
                setParam(inst, "Attack", 0.70f);  // Fast attack to pin plucks
                setParam(inst, "Release", 0.60f); // Smooth release
                setParam(inst, "Ratio", 0.25f);   // 4:1
                setParam(inst, "Input", 0.58f);   // Solid 4 dB clamp
                setParam(inst, "Output", 0.50f);
            } else {
                // Vocals & Guitars (Stage 1 peak tamer)
                setParam(inst, "Attack", 0.65f);  // Quick peak catch
                setParam(inst, "Release", 0.75f); // Fast release
                setParam(inst, "Ratio", 0.25f);   // 4:1
                setParam(inst, "Input", 0.52f);   // Shaves 2-3 dB peak tips
                setParam(inst, "Output", 0.50f);
            }
            return;
        }

        // 3. CLA-2A (Legendary T4 Opto Leveling Amplifier)
        if (name.containsIgnoreCase("CLA-2A")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Analog", 0.0f);
            setParam(inst, "Comp/Limiter", 0.0f); // Compress mode (smooth curve)
            setParam(inst, "Hi Freq", 0.50f);     // Flat sidechain response

            // Peak Reduction: Calibrated to catch warm, musical 2 - 3.5 dB leveling
            setParam(inst, "Peak Reduction", 0.62f);
            setParam(inst, "Gain", 0.55f); // Output compensation
            return;
        }

        // 4. RVox / Renaissance Vox (Vocal Dyn / Gate / Rider)
        if (name.containsIgnoreCase("RVox")) {
            setParam(inst, "Bypass", 0.0f);
            // In RVox, Compression slider ranges 0 to -36 dB (1.0 = 0 dB / off, 0.0 = -36 dB / slammed)
            // 0.78 = ~-8 dB threshold, providing 2.5 - 4.5 dB of vocal leveling
            setParam(inst, "Compression", 0.78f);
            setParam(inst, "Gate", 0.15f); // Gentle background noise tamer
            setParam(inst, "Gain", 0.88f); // Clean makeup
            return;
        }

        // 5. Scheps 73 (Neve 1073 Console EQ & Preamp Color)
        if (name.containsIgnoreCase("Scheps 73")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "PreAmp On/Off", 1.0f);
            setParam(inst, "PreAmp Drive", 0.05f); // Subtle analog transformer saturation
            setParam(inst, "High On/Off", 1.0f);
            setParam(inst, "Mid On/Off", 1.0f);
            setParam(inst, "Low On/Off", 1.0f);

            // 0.5 = 0 dB flat
            if (instrument == InstrumentType::LeadVocal || instrument == InstrumentType::BackgroundVocal) {
                setParam(inst, "High Gain L/M", 0.65f); // +3 dB 12kHz Neve air
                setParam(inst, "High Gain R/S", 0.65f);
                setParam(inst, "Mid Frequency L/M", 0.55f); // ~3.2 kHz
                setParam(inst, "Mid Gain L/M", 0.58f);  // +1.5 dB presence
                setParam(inst, "Mid Gain R/S", 0.58f);
            } else if (instrument == InstrumentType::AcousticGuitar) {
                setParam(inst, "High Gain L/M", 0.62f); // +2 dB air
                setParam(inst, "High Gain R/S", 0.62f);
                setParam(inst, "Mid Frequency L/M", 0.33f); // ~1.6 kHz
                setParam(inst, "Mid Gain L/M", 0.44f);  // -1.5 dB quack scoop
                setParam(inst, "Mid Gain R/S", 0.44f);
            } else {
                setParam(inst, "High Gain L/M", 0.58f);
                setParam(inst, "High Gain R/S", 0.58f);
            }
            return;
        }

        // 6. RDeEsser / DeEsser (Dynamic High-Frequency Tamer)
        if (name.containsIgnoreCase("RDeEsser") || name.containsIgnoreCase("DeEsser")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "FilterType", 0.0f); // Band-pass / Bell
            setParam(inst, "Freq", 0.55f);       // ~5.8 kHz - 6.5 kHz sibilance focus
            setParam(inst, "Threshold", 0.42f);  // Bites dynamic 'S' and 'T' sounds
            setParam(inst, "Range", 0.70f);      // Capped to -6 dB max reduction
            return;
        }

        // 7. Renaissance Bass / RBass (Subharmonic Synthesizer)
        if (name.containsIgnoreCase("RBass") || name.containsIgnoreCase("Renaissance Bass")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Freq", 0.38f);       // ~80 Hz tuned harmonic fundamental
            setParam(inst, "Intensity", 0.45f);  // Generates clean upper harmonics for small speakers
            setParam(inst, "Original Bass", 0.50f);
            return;
        }

        // 8. SSLComp / SSL G-Master Bus Compressor (The VCA Glue Compressor)
        if (name.containsIgnoreCase("SSLComp") || name.containsIgnoreCase("SSL G-Master")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "ANLG", 0.0f); // Clean noise floor
            setParam(inst, "IN", 1.0f);
            setParam(inst, "Ratio", 0.0f);        // 2:1 ratio for transparent subgroup glue (or 4:1)
            setParam(inst, "Attack", 0.50f);      // 30ms attack (lets transients punch cleanly through)
            setParam(inst, "Release", 0.75f);     // Auto release (adapts dynamically to groove)
            setParam(inst, "Thresh", 0.55f);      // Dialed for smooth 1.5 - 3.0 dB gain reduction glue
            setParam(inst, "Makeup", 0.55f);      // +1.5 dB makeup gain
            setParam(inst, "SC Filter", 1.0f);    // 185 Hz HPF sidechain so bass doesn't pump the mix!
            return;
        }

        // 9. API-2500 Bus Compressor
        if (name.containsIgnoreCase("API-2500")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Analog", 0.0f);
            setParam(inst, "In", 1.0f);
            setParam(inst, "Thrust", 1.0f);       // Loud/Med thrust (patent API low-end filter)
            setParam(inst, "Ratio", 0.25f);       // 2:1 or 3:1
            setParam(inst, "Attack", 0.50f);      // 30ms
            setParam(inst, "Release", 0.80f);     // Auto/fast
            setParam(inst, "Thresh", 0.45f);      // Smooth 2 dB bus glue
            return;
        }

        // 10. Kramer Tape / Abbey Road J37 (Harmonic Tape Saturation & Head Bump)
        if (name.containsIgnoreCase("Kramer Tape") || name.containsIgnoreCase("J37")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Noise", 0.0f);        // Zero hiss
            setParam(inst, "Speed", 1.0f);        // 15 IPS for warm, rich analog low-end punch
            setParam(inst, "Record Level", 0.52f);// Warm saturation without harsh distortion
            setParam(inst, "Flux", 0.35f);        // Musical harmonic excitement
            setParam(inst, "Delay Level", 0.0f);  // Delay bypassed (tone only)
            return;
        }

        // 11. L2 / L3 Limiter (True-Peak Mastering Ceiling)
        if (name.containsIgnoreCase("L2") || name.containsIgnoreCase("L3")) {
            setParam(inst, "Bypass", 0.0f);
            setParam(inst, "Ceiling Slider", 0.90f); // -0.8 dBTP ceiling
            setParam(inst, "Thresh Slider", 0.85f);  // Clean 1.5 - 2 dB true peak ceiling control
            setParam(inst, "ARC", 1.0f);             // Auto Release Control
            return;
        }
    }
};

} // namespace aimixer
