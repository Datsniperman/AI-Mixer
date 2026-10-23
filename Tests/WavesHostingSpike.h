#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Source/Common/LoadTrace.h"
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <iomanip>

namespace aimixer {

class WavesHostingSpike {
public:
    static int runSpike() {
        std::cout << "\n======================================================\n";
        std::cout << "   AI MIXER v1.1 — WAVES HOSTING SPIKE VERIFICATION   \n";
        std::cout << "======================================================\n" << std::endl;

        traceLog("waves-spike: starting initialization");
        juce::ScopedJuceInitialiser_GUI guiInit;

        juce::AudioPluginFormatManager formatManager;
        formatManager.addFormat(new juce::VST3PluginFormat());

        // ----------------------------------------------------------------------
        // STEP 1: SCAN AND ENUMERATE
        // ----------------------------------------------------------------------
        std::cout << ">>> STEP 1: SCAN AND ENUMERATE VST3 PLUGINS <<<" << std::endl;
        juce::KnownPluginList pluginList;

        juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                    .getChildFile("AI Mixer");
        appDataDir.createDirectory();

        juce::File deadMansPedal = appDataDir.getChildFile("scanner-deadman.txt");
        if (deadMansPedal.existsAsFile()) {
            std::cout << "[Dead-Man's Pedal] Previous pedal file found: " 
                      << deadMansPedal.getFullPathName() << std::endl;
        }

        juce::FileSearchPath searchPath;
        searchPath.add(juce::File("C:\\Program Files\\Common Files\\VST3"));

        std::cout << "Scanning search paths (root non-recursive to bypass interactive dongles):" << std::endl;
        for (int i = 0; i < searchPath.getNumPaths(); ++i) {
            std::cout << "  - " << searchPath[i].getFullPathName() << std::endl;
        }

        auto* vst3Format = formatManager.getFormat(0);
        juce::PluginDirectoryScanner scanner(pluginList, *vst3Format, searchPath, false, deadMansPedal);

        juce::String nameBeingScanned;
        int fileScanCount = 0;
        traceLog("waves-spike: starting PluginDirectoryScanner");

        while (scanner.scanNextFile(true, nameBeingScanned)) {
            fileScanCount++;
            std::cout << "  [" << fileScanCount << "] Scanned: " << nameBeingScanned << std::endl;
            traceLog("waves-spike: scanned file: %s", nameBeingScanned.toRawUTF8());
        }

        // Save XML
        juce::File xmlFile = appDataDir.getChildFile("known-plugins.xml");
        std::unique_ptr<juce::XmlElement> xml = pluginList.createXml();
        if (xml != nullptr) {
            xml->writeTo(xmlFile);
            std::cout << "\nKnownPluginList written to: " << xmlFile.getFullPathName() << std::endl;
            traceLog("waves-spike: known-plugins.xml written");
        }

        auto allTypes = pluginList.getTypes();
        std::cout << "\nScan Summary:" << std::endl;
        std::cout << "  Total Plugins Found: " << allTypes.size() << std::endl;

        std::vector<juce::PluginDescription> wavesPlugins;
        for (const auto& desc : allTypes) {
            bool isWaves = desc.manufacturerName.containsIgnoreCase("Waves") ||
                           desc.fileOrIdentifier.containsIgnoreCase("WaveShell");
            if (isWaves) {
                wavesPlugins.push_back(desc);
            }
        }

        std::cout << "  Total Waves Plugins Found: " << wavesPlugins.size() << std::endl;

        bool shellEnumerationSuccessful = false;
        if (!wavesPlugins.empty()) {
            std::cout << "\n--- ENUMERATED WAVES PLUGINS ---" << std::endl;
            for (size_t i = 0; i < wavesPlugins.size(); ++i) {
                const auto& d = wavesPlugins[i];
                std::cout << std::setw(3) << (i + 1) << ". "
                          << d.name << " | Mfr: " << d.manufacturerName
                          << " | UID: " << juce::String::toHexString(d.uniqueId)
                          << " | File: " << juce::File(d.fileOrIdentifier).getFileName()
                          << std::endl;

                // Check if sub-plugin names differ from the shell filename
                if (!d.name.containsIgnoreCase("WaveShell") || d.name.containsIgnoreCase("CLA") ||
                    d.name.containsIgnoreCase("SSL") || d.name.containsIgnoreCase("Scheps") ||
                    d.name.containsIgnoreCase("Renaissance") || d.name.containsIgnoreCase("Vox")) {
                    shellEnumerationSuccessful = true;
                }
            }
        }

        if (wavesPlugins.empty()) {
            std::cerr << "\n[CRITICAL FAILURE] No Waves plugins found in scan paths!" << std::endl;
            traceLog("waves-spike: CRITICAL FAILURE - no waves plugins found");
            return 1;
        }

        if (!shellEnumerationSuccessful) {
            std::cerr << "\n[DEAL-BREAKER] WaveShell did NOT enumerate individual sub-plugins! Only the shell itself appeared." << std::endl;
            traceLog("waves-spike: DEAL-BREAKER - sub-plugins not enumerated");
            return 2;
        }

        std::cout << "\n[STEP 1 PASS] WaveShell enumerated " << wavesPlugins.size() 
                  << " individual sub-plugins successfully!" << std::endl;

        // ----------------------------------------------------------------------
        // STEP 2: HOST ONE WAVES PLUGIN END-TO-END
        // ----------------------------------------------------------------------
        std::cout << "\n>>> STEP 2: HOST ONE WAVES PLUGIN END-TO-END <<<" << std::endl;

        // Candidate preference order: SSL E-Channel, Scheps Omni Channel, CLA-76, R-Vox
        juce::PluginDescription chosenStereoDesc;
        juce::PluginDescription chosenMonoDesc;
        juce::String chosenFamily;

        auto findPluginDesc = [&](const juce::String& searchName) -> bool {
            bool found = false;
            for (const auto& d : wavesPlugins) {
                if (d.name.containsIgnoreCase(searchName)) {
                    if (d.name.containsIgnoreCase("Stereo") || !d.name.containsIgnoreCase("Mono")) {
                        if (chosenStereoDesc.name.isEmpty()) chosenStereoDesc = d;
                    }
                    if (d.name.containsIgnoreCase("Mono")) {
                        if (chosenMonoDesc.name.isEmpty()) chosenMonoDesc = d;
                    }
                    found = true;
                }
            }
            if (found && chosenStereoDesc.name.isNotEmpty()) {
                chosenFamily = searchName;
                return true;
            }
            return false;
        };

        if (!findPluginDesc("SSLChannel") && !findPluginDesc("SSL E-Channel") && !findPluginDesc("SSL EV2")) {
            if (!findPluginDesc("Scheps Omni Channel")) {
                if (!findPluginDesc("CLA-76")) {
                    if (!findPluginDesc("RVox")) {
                        chosenStereoDesc = wavesPlugins[0];
                        chosenFamily = wavesPlugins[0].name;
                    }
                }
            }
        }

        std::cout << "Selected Target Plugin Family: " << chosenFamily << std::endl;
        std::cout << "  Stereo Variant: " << chosenStereoDesc.name << " (UID: " << juce::String::toHexString(chosenStereoDesc.uniqueId) << ")" << std::endl;
        if (chosenMonoDesc.name.isNotEmpty()) {
            std::cout << "  Mono Variant:   " << chosenMonoDesc.name << " (UID: " << juce::String::toHexString(chosenMonoDesc.uniqueId) << ")" << std::endl;
        }

        auto createInstance = [&](const juce::PluginDescription& desc, double rate, int block)
            -> std::unique_ptr<juce::AudioPluginInstance> {
            juce::String err;
            auto inst = formatManager.createPluginInstance(desc, rate, block, err);
            if (!inst) {
                std::cerr << "Failed to instantiate " << desc.name << ": " << err << std::endl;
                traceLog("waves-spike: failed to instantiate %s: %s", desc.name.toRawUTF8(), err.toRawUTF8());
            }
            return inst;
        };

        traceLog("waves-spike: creating stereo instance of %s", chosenStereoDesc.name.toRawUTF8());
        auto stereoPlugin = createInstance(chosenStereoDesc, 48000.0, 512);
        if (!stereoPlugin) {
            std::cerr << "[CRITICAL FAILURE] Could not instantiate chosen plugin!" << std::endl;
            return 3;
        }

        std::cout << "\nInstance Created Successfully: " << stereoPlugin->getName() << std::endl;
        traceLog("waves-spike: stereo instance created successfully");

        // Prepare at 48kHz / 512
        stereoPlugin->prepareToPlay(48000.0, 512);
        traceLog("waves-spike: prepared at 48kHz / 512");

        // Run 10 seconds of -18 dBFS sine buffer through stereo
        std::cout << "Processing 10 seconds of -18 dBFS audio in STEREO at 48 kHz / 512..." << std::endl;
        juce::AudioBuffer<float> stereoBuffer(2, 512);
        juce::MidiBuffer midi;
        const int numBlocks10Sec = static_cast<int>((10.0 * 48000.0) / 512.0); // 937 blocks

        auto startStereoTime = std::chrono::high_resolution_clock::now();
        float phase = 0.0f;
        const float phaseDelta = (2.0f * 3.14159265f * 1000.0f) / 48000.0f;
        const float targetAmp = 0.12589f; // -18 dBFS

        for (int b = 0; b < numBlocks10Sec; ++b) {
            for (int s = 0; s < 512; ++s) {
                float sampleVal = targetAmp * std::sin(phase);
                phase += phaseDelta;
                stereoBuffer.setSample(0, s, sampleVal);
                stereoBuffer.setSample(1, s, sampleVal);
            }
            try {
                stereoPlugin->processBlock(stereoBuffer, midi);
            } catch (const std::exception& e) {
                std::cerr << "Exception in processBlock: " << e.what() << std::endl;
                traceLog("waves-spike: exception in stereo processBlock: %s", e.what());
                return 4;
            } catch (...) {
                std::cerr << "Unknown exception in processBlock!" << std::endl;
                traceLog("waves-spike: unknown exception in stereo processBlock");
                return 4;
            }
        }
        auto endStereoTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsedStereo = endStereoTime - startStereoTime;
        double stereoCpuPercent = (elapsedStereo.count() / 10.0) * 100.0;
        std::cout << "  Stereo 10s Audio Processed in: " << (elapsedStereo.count() * 1000.0) << " ms"
                  << " (Estimated CPU: " << std::fixed << std::setprecision(2) << stereoCpuPercent << "%)" << std::endl;
        traceLog("waves-spike: stereo 10s audio processed in %.2f ms (CPU: %.2f%%)", elapsedStereo.count() * 1000.0, stereoCpuPercent);

        // Prepare at 44.1kHz / 1024
        stereoPlugin->releaseResources();
        stereoPlugin->prepareToPlay(44100.0, 1024);
        traceLog("waves-spike: prepared at 44.1kHz / 1024");
        juce::AudioBuffer<float> buffer44k(2, 1024);
        buffer44k.clear();
        stereoPlugin->processBlock(buffer44k, midi);
        std::cout << "  Prepared & Processed successfully at 44.1 kHz / 1024." << std::endl;

        // Mono test
        if (chosenMonoDesc.name.isNotEmpty()) {
            std::cout << "Processing 10 seconds of -18 dBFS audio in MONO at 48 kHz / 512..." << std::endl;
            traceLog("waves-spike: creating mono instance of %s", chosenMonoDesc.name.toRawUTF8());
            auto monoPlugin = createInstance(chosenMonoDesc, 48000.0, 512);
            if (monoPlugin) {
                monoPlugin->prepareToPlay(48000.0, 512);
                juce::AudioBuffer<float> monoBuffer(1, 512);
                for (int b = 0; b < numBlocks10Sec; ++b) {
                    for (int s = 0; s < 512; ++s) {
                        monoBuffer.setSample(0, s, targetAmp * std::sin(phase));
                        phase += phaseDelta;
                    }
                    monoPlugin->processBlock(monoBuffer, midi);
                }
                std::cout << "  Mono 10s Audio Processed successfully without exception!" << std::endl;
                traceLog("waves-spike: mono 10s audio processed cleanly");
            }
        }

        // Dump Parameters
        std::cout << "\n--- PARAMETER DUMP FOR " << stereoPlugin->getName() << " ---" << std::endl;
        const auto& params = stereoPlugin->getParameters();
        std::cout << "Total Parameters Exposed: " << params.size() << std::endl;

        int meaningfulCount = 0;
        int genericCount = 0;

        for (int i = 0; i < params.size(); ++i) {
            auto* p = params[i];
            juce::String name = p->getName(100);
            float val = p->getValue();
            juce::String text = p->getText(val, 100);

            // Determine if name is human-meaningful or generic (like Param 1, #01, etc.)
            bool isGeneric = name.startsWithIgnoreCase("Param") ||
                             name.startsWithIgnoreCase("#") ||
                             name.isEmpty();
            if (isGeneric) genericCount++; else meaningfulCount++;

            std::cout << "  [" << std::setw(3) << i << "] "
                      << std::left << std::setw(32) << name
                      << " = " << std::fixed << std::setprecision(4) << val
                      << " (" << (text.isNotEmpty() ? text : juce::String(val, 2)) << ")"
                      << " [" << (isGeneric ? "GENERIC" : "MEANINGFUL") << "]"
                      << std::endl;
        }

        std::cout << "\nParameter Quality Assessment:" << std::endl;
        std::cout << "  Meaningful Names: " << meaningfulCount << " / " << params.size() << std::endl;
        std::cout << "  Generic Names:    " << genericCount << " / " << params.size() << std::endl;

        // Parameter Set & Read-back Proof (3 parameters)
        std::cout << "\n--- PARAMETER SET & READ-BACK PROOF ---" << std::endl;
        int testParamIndices[3] = { -1, -1, -1 };
        int foundCount = 0;
        for (int i = 0; i < params.size() && foundCount < 3; ++i) {
            if (!params[i]->getName(100).startsWithIgnoreCase("Param")) {
                testParamIndices[foundCount++] = i;
            }
        }
        if (foundCount < 3) {
            testParamIndices[0] = 0;
            testParamIndices[1] = 1;
            testParamIndices[2] = 2;
        }

        for (int idx = 0; idx < 3; ++idx) {
            int pIdx = testParamIndices[idx];
            auto* p = params[pIdx];
            juce::String pName = p->getName(100);
            float originalVal = p->getValue();
            float targetVal = (originalVal > 0.5f) ? 0.25f : 0.75f;

            p->setValueNotifyingHost(targetVal);
            float readBackVal = p->getValue();

            std::cout << "  Param [" << pIdx << "] '" << pName << "':"
                      << " Original=" << originalVal
                      << " | SetTarget=" << targetVal
                      << " | ReadBack=" << readBackVal
                      << " | Delta=" << std::abs(targetVal - readBackVal)
                      << (std::abs(targetVal - readBackVal) < 0.05f ? " [PROVEN OK]" : " [DISCREPANCY]")
                      << std::endl;
            traceLog("waves-spike: param set/read-back: %s, target=%.4f, read=%.4f", pName.toRawUTF8(), targetVal, readBackVal);
        }

        // GUI Editor Creation & 10-Second Live Window
        std::cout << "\n--- GUI EDITOR HOSTING & 10-SECOND LIVE WINDOW TEST ---" << std::endl;
        traceLog("waves-spike: editor creation start");
        std::cout << "Creating editor component via createEditorIfNeeded()..." << std::endl;
        
        juce::AudioProcessorEditor* editor = nullptr;
        try {
            editor = stereoPlugin->createEditorIfNeeded();
        } catch (const std::exception& e) {
            std::cerr << "Exception in createEditorIfNeeded: " << e.what() << std::endl;
            traceLog("waves-spike: exception in createEditorIfNeeded: %s", e.what());
        } catch (...) {
            std::cerr << "Unknown exception in createEditorIfNeeded!" << std::endl;
            traceLog("waves-spike: unknown exception in createEditorIfNeeded");
        }

        if (editor != nullptr) {
            std::cout << "Editor created successfully! Size: " 
                      << editor->getWidth() << "x" << editor->getHeight() << std::endl;
            traceLog("waves-spike: editor created successfully (%dx%d)", editor->getWidth(), editor->getHeight());

            struct SpikeWindow : public juce::DocumentWindow {
                SpikeWindow(const juce::String& title, juce::Component* content)
                    : DocumentWindow(title, juce::Colours::black, DocumentWindow::allButtons) {
                    setContentOwned(content, true);
                    setUsingNativeTitleBar(true);
                    centreWithSize(content->getWidth(), content->getHeight());
                    setVisible(true);
                }
                void closeButtonPressed() override {
                    setVisible(false);
                }
            };

            traceLog("waves-spike: creating SpikeWindow on screen");
            auto window = std::make_unique<SpikeWindow>("AI Mixer Spike Host: " + stereoPlugin->getName(), editor);
            std::cout << "Window displayed on screen. Running Windows event loop for 10 seconds..." << std::endl;

            auto startTime = juce::Time::getMillisecondCounter();
            int secondsPassed = 0;
            while (juce::Time::getMillisecondCounter() - startTime < 10000) {
                juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                int currentSec = static_cast<int>((juce::Time::getMillisecondCounter() - startTime) / 1000);
                if (currentSec > secondsPassed) {
                    secondsPassed = currentSec;
                    std::cout << "  [Live Window] " << secondsPassed << "s / 10s alive..." << std::endl;
                }
            }

            traceLog("waves-spike: closing SpikeWindow start");
            std::cout << "10 seconds completed. Closing window and tearing down editor cleanly..." << std::endl;
            window->setVisible(false);
            window.reset(); // Destroys DocumentWindow and content (editor)
            traceLog("waves-spike: closing SpikeWindow end");
            std::cout << "Editor closed and destroyed cleanly without crash!" << std::endl;
        } else {
            std::cerr << "[WARNING] createEditorIfNeeded returned null!" << std::endl;
            traceLog("waves-spike: createEditorIfNeeded returned null");
        }

        // State Save / Restore Test
        std::cout << "\n--- STATE SAVE / RESTORE VERIFICATION ---" << std::endl;
        traceLog("waves-spike: state save start");
        juce::MemoryBlock savedState;
        stereoPlugin->getStateInformation(savedState);
        std::cout << "Saved plugin binary state size: " << savedState.getSize() << " bytes" << std::endl;
        traceLog("waves-spike: saved state size %d bytes", static_cast<int>(savedState.getSize()));

        std::vector<float> savedParamVals;
        for (int idx = 0; idx < 3; ++idx) {
            savedParamVals.push_back(params[testParamIndices[idx]]->getValue());
        }

        traceLog("waves-spike: destroying first instance");
        stereoPlugin.reset(); // Destroy instance

        traceLog("waves-spike: creating second instance for restore");
        auto restoredPlugin = createInstance(chosenStereoDesc, 48000.0, 512);
        if (!restoredPlugin) {
            std::cerr << "Second instance recreation failed!" << std::endl;
            return 5;
        }

        traceLog("waves-spike: applying saved state");
        restoredPlugin->setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
        traceLog("waves-spike: applied saved state");

        const auto& restoredParams = restoredPlugin->getParameters();
        bool paramsMatch = true;
        for (int idx = 0; idx < 3; ++idx) {
            int pIdx = testParamIndices[idx];
            float restoredVal = restoredParams[pIdx]->getValue();
            float expectedVal = savedParamVals[idx];
            float diff = std::abs(restoredVal - expectedVal);
            std::cout << "  Param [" << pIdx << "] '" << restoredParams[pIdx]->getName(100) << "':"
                      << " Expected=" << expectedVal
                      << " | Restored=" << restoredVal
                      << " | Delta=" << diff
                      << (diff < 0.01f ? " [IDENTICAL]" : " [MISMATCH]")
                      << std::endl;
            if (diff >= 0.01f) paramsMatch = false;
        }

        juce::MemoryBlock reSavedState;
        restoredPlugin->getStateInformation(reSavedState);
        bool binaryStateMatches = (savedState == reSavedState);
        std::cout << "Binary State MemoryBlock match: " << (binaryStateMatches ? "EXACT (Byte-for-byte)" : "Different sizes/metadata") << std::endl;
        std::cout << "State Save/Restore Verdict: " << (paramsMatch ? "SUCCESS (Parameters 100% Preserved)" : "FAILURE") << std::endl;

        int reportedLatency = restoredPlugin->getLatencySamples();
        std::cout << "\nReported Latency: " << reportedLatency << " samples" << std::endl;

        // Parameter names of up to 5 other Waves plugins
        std::cout << "\n--- PARAMETER NAME SAMPLES FROM 5 OTHER WAVES PLUGINS ---" << std::endl;
        std::vector<juce::String> sampleCandidates = { "CLA-76", "RVox", "Scheps Omni", "SSL", "Renaissance Axx", "CLA-2A" };
        int sampleCount = 0;
        for (const auto& cand : sampleCandidates) {
            for (const auto& d : wavesPlugins) {
                if (d.name.containsIgnoreCase(cand) && !d.name.equalsIgnoreCase(chosenStereoDesc.name) && sampleCount < 5) {
                    auto pInst = createInstance(d, 48000.0, 512);
                    if (pInst) {
                        sampleCount++;
                        std::cout << "\n[" << sampleCount << "] Plugin: " << pInst->getName() 
                                  << " (" << pInst->getParameters().size() << " parameters):" << std::endl;
                        for (int pi = 0; pi < std::min(10, pInst->getParameters().size()); ++pi) {
                            std::cout << "    - " << pInst->getParameters()[pi]->getName(100) 
                                      << " (current=" << pInst->getParameters()[pi]->getValue() << ")" << std::endl;
                        }
                        if (pInst->getParameters().size() > 10) {
                            std::cout << "    ... [" << (pInst->getParameters().size() - 10) << " more parameters]" << std::endl;
                        }
                    }
                    break;
                }
            }
        }

        // ----------------------------------------------------------------------
        // STEP 3: CHAIN MATH & LATENCY AUDIT
        // ----------------------------------------------------------------------
        std::cout << "\n>>> STEP 3: CHAIN MATH & LATENCY AUDIT <<<" << std::endl;

        // Find 3 distinct Waves plugins for series chain
        std::vector<juce::PluginDescription> chainDescs;
        for (const auto& target : { "SSL", "CLA-76", "RVox", "Scheps", "Renaissance Axx" }) {
            for (const auto& d : wavesPlugins) {
                if (d.name.containsIgnoreCase(target) && d.name.containsIgnoreCase("Stereo")) {
                    bool alreadyAdded = false;
                    for (const auto& cd : chainDescs) {
                        if (cd.name == d.name) alreadyAdded = true;
                    }
                    if (!alreadyAdded) {
                        chainDescs.push_back(d);
                        break;
                    }
                }
            }
            if (chainDescs.size() >= 3) break;
        }

        if (chainDescs.size() < 3) {
            for (const auto& d : wavesPlugins) {
                if (chainDescs.size() >= 3) break;
                bool alreadyAdded = false;
                for (const auto& cd : chainDescs) if (cd.name == d.name) alreadyAdded = true;
                if (!alreadyAdded) chainDescs.push_back(d);
            }
        }

        std::cout << "Series Chain Configuration (3 Plugins):" << std::endl;
        int totalChainLatency = 0;
        for (size_t i = 0; i < chainDescs.size(); ++i) {
            auto inst = createInstance(chainDescs[i], 48000.0, 512);
            if (inst) {
                inst->prepareToPlay(48000.0, 512);
                int lat = inst->getLatencySamples();
                totalChainLatency += lat;
                std::cout << "  Slot " << (i + 1) << ": " << inst->getName() 
                          << " -> Latency: " << lat << " samples" << std::endl;
            }
        }
        std::cout << "Total Series Chain Reported Latency: " << totalChainLatency << " samples" 
                  << " (" << std::fixed << std::setprecision(2) << (totalChainLatency * 1000.0 / 48000.0) << " ms at 48kHz)" << std::endl;
        std::cout << "DAW Latency Compensation Requirement: setLatencySamples(" << totalChainLatency << ")" << std::endl;

        // Audit latency for all enumerated Waves plugins
        std::cout << "\nScanning latency of all enumerated Waves plugins..." << std::endl;
        std::vector<std::pair<juce::String, int>> nonZeroLatencyPlugins;
        std::vector<juce::String> zeroLatencyPlugins;

        for (size_t wi = 0; wi < wavesPlugins.size(); ++wi) {
            const auto& d = wavesPlugins[wi];
            try {
                auto inst = createInstance(d, 48000.0, 512);
                if (inst) {
                    inst->prepareToPlay(48000.0, 512);
                    int lat = inst->getLatencySamples();
                    if (lat > 0) {
                        nonZeroLatencyPlugins.push_back({ d.name, lat });
                    } else {
                        zeroLatencyPlugins.push_back(d.name);
                    }
                }
            } catch (...) {}
            if ((wi + 1) % 10 == 0 || wi + 1 == wavesPlugins.size()) {
                std::cout << "  [" << (wi + 1) << "/" << wavesPlugins.size() << "] Audited..." << std::endl;
            }
        }

        std::cout << "\nWaves Plugins with NON-ZERO Latency (Incompatible with Zero-Latency Live Mode):" << std::endl;
        if (nonZeroLatencyPlugins.empty()) {
            std::cout << "  None! All tested Waves plugins report 0 latency." << std::endl;
        } else {
            for (const auto& p : nonZeroLatencyPlugins) {
                std::cout << "  - " << std::left << std::setw(35) << p.first 
                          << ": " << p.second << " samples (" 
                          << std::fixed << std::setprecision(2) << (p.second * 1000.0 / 48000.0) << " ms)" << std::endl;
            }
        }

        std::cout << "\nWaves Plugins with ZERO Latency (Live Mode Eligible): " 
                  << zeroLatencyPlugins.size() << " plugins" << std::endl;

        std::cout << "\n======================================================\n";
        std::cout << "   WAVES HOSTING SPIKE COMPLETE                       \n";
        std::cout << "======================================================\n" << std::endl;

        return 0;
    }
};

} // namespace aimixer
