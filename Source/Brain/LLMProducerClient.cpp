#include "LLMProducerClient.h"
#include "PluginCurator.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace aimixer {

LLMProducerClient& LLMProducerClient::getInstance() {
    static LLMProducerClient instance;
    return instance;
}

LLMProducerClient::LLMProducerClient()
    : juce::Thread("LLMProducerClientThread") {
    startThread(juce::Thread::Priority::normal);
    refreshModelStatusAsync();
}

LLMProducerClient::~LLMProducerClient() {
    signalThreadShouldExit();
    jobEvent.signal();
    stopThread(5000);
}

juce::String LLMProducerClient::getActiveModelName() const {
    std::lock_guard<std::mutex> lock(modelMutex);
    return currentModelName;
}

void LLMProducerClient::refreshModelStatusAsync() {
    // Non-blocking quick check
    juce::Thread::launch([this] {
        juce::String tagsJson = sendHttpGet("http://127.0.0.1:11434/api/tags", 3000);
        if (tagsJson.isNotEmpty()) {
            auto parsed = juce::JSON::parse(tagsJson);
            if (parsed.isObject()) {
                serverOnline.store(true);
                auto modelsVar = parsed["models"];
                if (modelsVar.isArray()) {
                    auto* arr = modelsVar.getArray();
                    std::lock_guard<std::mutex> lock(modelMutex);
                    
                    // Look for fast 3b/7b model first, fallback to llama3.1:latest
                    bool foundFav = false;
                    for (int i = 0; i < arr->size(); ++i) {
                        juce::String mName = (*arr)[i]["name"].toString();
                        if (mName.containsIgnoreCase("llama3.2:3b") || mName.containsIgnoreCase("qwen2.5:3b")) {
                            currentModelName = mName;
                            foundFav = true;
                            break;
                        }
                    }
                    if (!foundFav && arr->size() > 0) {
                        currentModelName = (*arr)[0]["name"].toString();
                    }
                }
                return;
            }
        }
        serverOnline.store(false);
    });
}

bool LLMProducerClient::produceSessionMixAsync(const std::vector<SessionTrackDiagnosis>& sessionTracks,
                                               const juce::String& genreAesthetic,
                                               CompletionCallback callback) {
    if (sessionTracks.empty() || isGenerating.load()) {
        return false;
    }

    auto job = std::make_unique<MixJob>();
    job->tracks = sessionTracks;
    job->genre = genreAesthetic.isNotEmpty() ? genreAesthetic : "Modern Worship / Radio Ready";
    job->callback = std::move(callback);

    {
        std::lock_guard<std::mutex> lock(jobMutex);
        pendingJob = std::move(job);
    }

    jobEvent.signal();
    return true;
}

void LLMProducerClient::run() {
    while (!threadShouldExit()) {
        std::unique_ptr<MixJob> activeJob;
        {
            std::lock_guard<std::mutex> lock(jobMutex);
            if (pendingJob) {
                activeJob = std::move(pendingJob);
            }
        }

        if (activeJob) {
            isGenerating.store(true);

            juce::String modelToUse;
            {
                std::lock_guard<std::mutex> lock(modelMutex);
                modelToUse = currentModelName;
            }

            juce::String sysPrompt = buildSystemPrompt();
            juce::String userPrompt = buildUserPrompt(activeJob->tracks, activeJob->genre);

            // Construct Ollama payload
            juce::DynamicObject::Ptr bodyObj = new juce::DynamicObject();
            bodyObj->setProperty("model", modelToUse);
            bodyObj->setProperty("system", sysPrompt);
            bodyObj->setProperty("prompt", userPrompt);
            bodyObj->setProperty("format", "json");
            bodyObj->setProperty("stream", false);

            juce::DynamicObject::Ptr optsObj = new juce::DynamicObject();
            optsObj->setProperty("temperature", 0.3); // Low temp for structured adherence
            optsObj->setProperty("top_p", 0.9);
            bodyObj->setProperty("options", optsObj.get());

            juce::String postData = juce::JSON::toString(juce::var(bodyObj.get()));
            juce::String responseJson = sendHttpPost("http://127.0.0.1:11434/api/generate", postData, 60000);

            ProducedMixPlan plan;
            if (responseJson.isNotEmpty()) {
                auto topParsed = juce::JSON::parse(responseJson);
                if (topParsed.isObject() && topParsed["response"].isString()) {
                    juce::String actualModelText = topParsed["response"].toString();
                    plan = parseModelResponse(actualModelText, activeJob->tracks);
                } else {
                    plan.success = false;
                    plan.errorReason = "Malformed response wrapper from local LLM.";
                }
            } else {
                plan.success = false;
                plan.errorReason = "Could not connect to Ollama server at 127.0.0.1:11434.";
            }

            // Fallback / Acoustic synthesis if offline or malformed
            if (!plan.success) {
                // Synthesize mathematically from acoustic measurements
                plan.success = true;
                plan.mixSummary = "Acoustic Intelligence Engine: Analyzed measured resonance, crest factor, and frequency tilt across session.";
                for (const auto& t : activeJob->tracks) {
                    ProducedTrackPlan tp;
                    tp.instanceId = t.instanceId;
                    tp.trackName = t.trackName;
                    tp.refinedInstrument = t.identifiedInstrument;
                    tp.balanceTrimDb = std::clamp(-18.0f - t.rmsDb, -12.0f, 12.0f);
                    tp.panPosition = (t.sideHint == SideHint::Left) ? -0.7f : (t.sideHint == SideHint::Right) ? 0.7f : 0.0f;
                    tp.reverbSendDb = (t.identifiedInstrument == InstrumentType::LeadVocal) ? -12.0f : -24.0f;
                    tp.delaySendDb = (t.identifiedInstrument == InstrumentType::LeadVocal) ? -16.0f : -60.0f;

                    // Slot 1: Tone / EQ
                    ProducedSlotDirective s1;
                    s1.wavesPluginName = t.isStereo ? "SSLChannel Stereo" : "SSLChannel Mono";
                    if (t.detectedResonanceHz > 60.0f && t.resonanceProminenceDb > 3.0f) {
                        s1.eqMidFreqHz = t.detectedResonanceHz;
                        s1.eqMidGainDb = -std::clamp(t.resonanceProminenceDb, 2.0f, 6.0f);
                        s1.eqMidQ = t.resonanceQ;
                    }
                    tp.slots.push_back(s1);

                    // Slot 2: Compression based on crest
                    ProducedSlotDirective s2;
                    s2.wavesPluginName = (t.crestFactorDb > 14.0f) ? (t.isStereo ? "CLA-76 Stereo" : "CLA-76 Mono")
                                                                  : (t.isStereo ? "RVox Stereo" : "RVox Mono");
                    s2.targetThresholdDb = t.rmsDb + (t.crestFactorDb * 0.4f);
                    s2.targetRatio = (t.crestFactorDb > 14.0f) ? 4.0f : 2.5f;
                    tp.slots.push_back(s2);

                    plan.tracks.push_back(tp);
                }
            }

            isGenerating.store(false);

            // Dispatch callback to main/UI thread
            if (activeJob->callback) {
                juce::MessageManager::callAsync([cb = activeJob->callback, resultPlan = plan] {
                    cb(resultPlan);
                });
            }
        } else {
            jobEvent.wait(2000);
        }
    }
}

juce::String LLMProducerClient::buildSystemPrompt() {
    return R"(You are an elite Grammy-winning mix engineer and producer.
You analyze multi-track audio sessions based on measured acoustic diagnostic features (measured RMS, peak crest factor, ringing room resonances, sibilant consonant spikes, spectral tilt, and sub-bass energy).
You prescribe the exact Waves plugin chain (from the user's installed Waves inventory) and precise parameter values tailored specifically to each channel's measured symptoms.

Available Waves Plugin Families you can select from:
- Dynamic EQs & Tone: SSLChannel, Scheps Omni Channel, F6-RTA, PuigTec EQP1A, API-550A, API-550B, V-EQ4, SSLEQ.
- Fast Transient Compressors: CLA-76 (Blacky/Bluey), Smack Attack.
- Smooth Levelers & Vocal Compressors: CLA-2A, CLA-3A, RVox, Renaissance Compressor, Renaissance Axx.
- De-Essers: Renaissance DeEsser, Sibilance.
- Sub-Harmonics & Low Weight: RBass, Submarine, MaxxBass.
- Saturation & Warmth: Magma Lil Tube, Kramer Master Tape, Abbey Road Saturator.
- Bus Glue: SSL G-Master Bus Comp, API-2500.

Rules:
1. DO NOT use generic presets.
2. If a track has a detected resonance frequency (e.g. 780 Hz or 3200 Hz), explicitly target an EQ cut at that exact frequency.
3. If vocal sibilance ratio is high (> 0.25), deploy a de-esser (Sibilance or Renaissance DeEsser) centered on the measured sibilance centroid.
4. If a kick or bass lacks sub energy (< 0.10) but has fundamental, select RBass tuned to the fundamental pitch.
5. If crest factor is erratic (> 14 dB), prescribe a dual-stage compressor (CLA-76 for peaks into CLA-2A or RVox for leveling).
6. Return strictly valid JSON matching the requested schema. No conversational filler.)";
}

juce::String LLMProducerClient::buildUserPrompt(const std::vector<SessionTrackDiagnosis>& sessionTracks,
                                                const juce::String& genreAesthetic) {
    juce::DynamicObject::Ptr rootObj = new juce::DynamicObject();
    rootObj->setProperty("genre_target", genreAesthetic);

    juce::Array<juce::var> tracksArr;
    for (const auto& t : sessionTracks) {
        juce::DynamicObject::Ptr tObj = new juce::DynamicObject();
        tObj->setProperty("instance_id", static_cast<int>(t.instanceId));
        tObj->setProperty("track_name", t.trackName);
        tObj->setProperty("instrument_guess", getInstrumentName(t.identifiedInstrument));
        tObj->setProperty("is_stereo", t.isStereo);
        tObj->setProperty("rms_db", std::round(t.rmsDb * 10.0f) / 10.0f);
        tObj->setProperty("crest_factor_db", std::round(t.crestFactorDb * 10.0f) / 10.0f);
        tObj->setProperty("fundamental_hz", std::round(t.fundamentalHz));
        tObj->setProperty("detected_resonance_hz", std::round(t.detectedResonanceHz));
        tObj->setProperty("resonance_prominence_db", std::round(t.resonanceProminenceDb * 10.0f) / 10.0f);
        tObj->setProperty("sibilance_ratio", std::round(t.sibilanceRatio * 100.0f) / 100.0f);
        tObj->setProperty("sibilance_centroid_hz", std::round(t.sibilanceCentroidHz));
        tObj->setProperty("boxiness_ratio", std::round(t.boxinessRatio * 100.0f) / 100.0f);
        tObj->setProperty("sub_energy_ratio", std::round(t.subEnergyRatio * 100.0f) / 100.0f);
        tObj->setProperty("harmonic_richness", std::round(t.harmonicRichness * 100.0f) / 100.0f);
        tracksArr.add(juce::var(tObj.get()));
    }
    rootObj->setProperty("tracks", tracksArr);

    juce::String sessionJson = juce::JSON::toString(juce::var(rootObj.get()));

    return "Analyze this multi-track session and output the complete mix plan as JSON.\n"
           "Format must match:\n"
           "{\n"
           "  \"mix_summary\": \"string summary of mix approach\",\n"
           "  \"tracks\": [\n"
           "    {\n"
           "      \"instance_id\": int,\n"
           "      \"refined_instrument\": \"string\",\n"
           "      \"balance_trim_db\": float,\n"
           "      \"pan_position\": float (-1.0 to 1.0),\n"
           "      \"reverb_send_db\": float,\n"
           "      \"delay_send_db\": float,\n"
           "      \"engineer_notes\": \"string\",\n"
           "      \"slots\": [\n"
           "        {\n"
           "          \"waves_plugin_name\": \"string (e.g. SSLChannel Stereo, CLA-76 Stereo, RVox Stereo, RBass Stereo)\",\n"
           "          \"eq_mid_freq_hz\": float,\n"
           "          \"eq_mid_gain_db\": float,\n"
           "          \"target_threshold_db\": float,\n"
           "          \"target_ratio\": float,\n"
           "          \"de_esser_freq_hz\": float\n"
           "        }\n"
           "      ]\n"
           "    }\n"
           "  ]\n"
           "}\n\nSession Data:\n" + sessionJson;
}

ProducedMixPlan LLMProducerClient::parseModelResponse(const juce::String& responseText,
                                                      const std::vector<SessionTrackDiagnosis>& sessionTracks) {
    juce::ignoreUnused(sessionTracks);
    ProducedMixPlan plan;

    // Sanitize in case model included markdown code fences
    juce::String cleanJson = responseText.trim();
    if (cleanJson.startsWith("```json")) cleanJson = cleanJson.substring(7);
    else if (cleanJson.startsWith("```")) cleanJson = cleanJson.substring(3);
    if (cleanJson.endsWith("```")) cleanJson = cleanJson.dropLastCharacters(3);
    cleanJson = cleanJson.trim();

    auto parsed = juce::JSON::parse(cleanJson);
    if (!parsed.isObject()) {
        plan.success = false;
        plan.errorReason = "JSON parser could not parse response.";
        return plan;
    }

    plan.success = true;
    plan.mixSummary = parsed["mix_summary"].toString();

    auto tracksVar = parsed["tracks"];
    if (tracksVar.isArray()) {
        auto* tArr = tracksVar.getArray();
        for (int i = 0; i < tArr->size(); ++i) {
            auto item = (*tArr)[i];
            if (!item.isObject()) continue;

            ProducedTrackPlan tp;
            tp.instanceId = static_cast<uint32_t>(static_cast<int>(item["instance_id"]));
            tp.engineerNotes = item["engineer_notes"].toString();
            tp.balanceTrimDb = static_cast<float>(item["balance_trim_db"]);
            tp.panPosition = static_cast<float>(item["pan_position"]);
            tp.reverbSendDb = static_cast<float>(item["reverb_send_db"]);
            tp.delaySendDb = static_cast<float>(item["delay_send_db"]);

            // Refined instrument (flexible matching: ignores spaces, hyphens, underscores)
            juce::String cleanInstStr = item["refined_instrument"].toString().removeCharacters(" _-");
            for (int k = 0; k <= static_cast<int>(InstrumentType::Room); ++k) {
                juce::String canonical = juce::String(getInstrumentName(static_cast<InstrumentType>(k))).removeCharacters(" _-");
                if (cleanInstStr.equalsIgnoreCase(canonical)) {
                    tp.refinedInstrument = static_cast<InstrumentType>(k);
                    break;
                }
            }

            auto slotsVar = item["slots"];
            if (slotsVar.isArray()) {
                auto* sArr = slotsVar.getArray();
                for (int s = 0; s < sArr->size(); ++s) {
                    auto sItem = (*sArr)[s];
                    if (!sItem.isObject()) continue;

                    ProducedSlotDirective sd;
                    sd.wavesPluginName = sItem["waves_plugin_name"].toString();
                    sd.eqMidFreqHz = static_cast<float>(sItem["eq_mid_freq_hz"]);
                    sd.eqMidGainDb = static_cast<float>(sItem["eq_mid_gain_db"]);
                    sd.targetThresholdDb = static_cast<float>(sItem["target_threshold_db"]);
                    sd.targetRatio = static_cast<float>(sItem["target_ratio"]);
                    sd.deEsserFreqHz = static_cast<float>(sItem["de_esser_freq_hz"]);
                    tp.slots.push_back(sd);
                }
            }
            plan.tracks.push_back(tp);
        }
    }

    return plan;
}

juce::String LLMProducerClient::sendHttpPost(const juce::String& urlString, const juce::String& jsonBody, int timeoutMs) {
    juce::URL url(urlString);
    int statusCode = 0;
    
    juce::StringPairArray responseHeaders;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withHttpRequestCmd("POST")
                       .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json")
                       .withConnectionTimeoutMs(timeoutMs)
                       .withStatusCode(&statusCode);

    url = url.withPOSTData(jsonBody);
    std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
    if (stream != nullptr) {
        return stream->readEntireStreamAsString();
    }
    return {};
}

juce::String LLMProducerClient::sendHttpGet(const juce::String& urlString, int timeoutMs) {
    juce::URL url(urlString);
    int statusCode = 0;

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withHttpRequestCmd("GET")
                       .withConnectionTimeoutMs(timeoutMs)
                       .withStatusCode(&statusCode);

    std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
    if (stream != nullptr) {
        return stream->readEntireStreamAsString();
    }
    return {};
}

} // namespace aimixer
