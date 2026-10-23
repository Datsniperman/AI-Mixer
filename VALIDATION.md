# AI Mixer v1.0.0 — Real-World Host Validation Guide

This validation runbook is designed for front-of-house (FOH), broadcast engineers, and audio developers testing **AI Mixer v1.0.0** inside real-world Digital Audio Workstations (DAWs) including **REAPER** and **Ableton Live**, or live audio hosts.

---

## 1. Installation & Plugin Scan

- [ ] **1.1 Locate Binary Artifacts**:
  - Windows VST3: `build/AIMixer_artefacts/Release/VST3/AI Mixer.vst3`
  - Windows Standalone: `build/AIMixer_artefacts/Release/Standalone/AI Mixer.exe`
- [ ] **1.2 Copy to System VST3 Directory**:
  - Windows: Copy `AI Mixer.vst3` to `C:\Program Files\Common Files\VST3\`
  - macOS: Copy `AI Mixer.vst3` to `/Library/Audio/Plug-Ins/VST3/`
- [ ] **1.3 Host Rescan**:
  - Open REAPER (`Options -> Preferences -> Plug-ins -> VST -> Re-scan`) or Ableton Live (`Preferences -> Plug-Ins -> Rescan`).
  - Verify `AI Mixer` (Developer: `aimixer`, Version `1.0.0`) appears in the plugin browser without errors.

---

## 2. 16-Track Real-World Session Template

- [ ] **2.1 Instantiate 16 Source Tracks**:
  Create 16 audio tracks using the standard parser naming scheme:
  1. `01 Kick In` (Instrument: Kick)
  2. `02 Snare Top` (Instrument: Snare)
  3. `03 Hi-Hat` (Instrument: HiHat)
  4. `04 Toms Rack` (Instrument: Tom)
  5. `05 Drum OH L` (Instrument: Overheads, Side: Left)
  6. `06 Drum OH R` (Instrument: Overheads, Side: Right)
  7. `07 Drum Room` (Instrument: Room, Side: Centre)
  8. `08 Bass DI` (Instrument: Bass)
  9. `09 Elec Gtr L` (Instrument: ElectricGuitar, Side: Left)
  10. `10 Elec Gtr R` (Instrument: ElectricGuitar, Side: Right)
  11. `11 Acoustic Gtr` (Instrument: AcousticGuitar)
  12. `12 Piano` (Instrument: Piano)
  13. `13 Synth Pad` (Instrument: Pad)
  14. `14 Lead Vocal` (Instrument: LeadVocal)
  15. `15 BGV Harmony` (Instrument: BackgroundVocal)
  16. `16 Pastor Wireless` (Instrument: PastorSpeaking)
- [ ] **2.2 Insert AI Mixer as First Insert**:
  Place `AI Mixer` as Slot 1 on every source track.
- [ ] **2.3 Create Buses and Master**:
  - Create `Drums Bus` -> insert `AI Mixer` -> set Role to `Bus` (Bus Type auto-detects `Drums`).
  - Create `Vocal Bus` -> insert `AI Mixer` -> set Role to `Bus` (Bus Type auto-detects `Vocals`).
  - On the DAW Master track -> insert `AI Mixer` -> set Role to `Master`.
- [ ] **2.4 Verify Discovery & Mix View**:
  - Open UI on the Master track.
  - Verify the 64-slot telemetry grid displays all 16 sources and 2 buses with real-time spectrum and RMS metering.
  - Confirm identity chips reflect the parsed instrument names and side hints (e.g. `Elec Gtr L` tagged `Electric Guitar [L]`).

---

## 3. Learn Mode Chorus / Verse Soundcheck

- [ ] **3.1 Switch Session Mode to Learn**:
  On the Master coordinator UI, toggle Session Mode to `Learn`.
- [ ] **3.2 Loop Song Passage (Chorus / Verse)**:
  Play back dynamic multitrack stems (2–3 minutes).
- [ ] **3.3 Verify Fast Adaptation**:
  - Observe that balance rides adapt at up to `2.0 dB/sec` (0.2 dB per solver tick).
  - Check that Auto-Trim levels active tracks toward `-18 dBFS RMS` within ~3 seconds without exceeding `+/-12 dB`.
  - Confirm vocal masking carve engages on Pad (around 1–3 kHz) and Keys when Lead Vocal sings.
  - Confirm dynamic ducking on Bass when Kick transients strike (1–3 dB gain reduction).

---

## 4. Live Mode Playback (Slow Rides & Smoothness)

- [ ] **4.1 Switch Session Mode to Live**:
  Toggle Session Mode from `Learn` to `Live`.
- [ ] **4.2 Verify Rate Limiting**:
  - Observe rides moving smoothly at `1.0 dB/sec` maximum (0.10 dB per solver tick).
  - Verify zero audible "zipper" noise or sudden gain pumping.
- [ ] **4.3 Inspect Solver Reasons**:
  In Master Mix View, inspect hover telemetry / status line to verify solver decision tags (`"Vocal Masking"`, `"Kick/Bass Ducking"`, `"Speech Bed Ducking"`).
- [ ] **4.4 Pastor Speech Bed Verification**:
  Unmute `16 Pastor Wireless`. When pastor speaks, verify Synth Pad and Acoustic Guitar receive an automatic -4 dB speech bed ducking penalty.

---

## 5. Freeze All Mid-Song Hold Test

- [ ] **5.1 Engage Freeze All**:
  During a loud chorus passage, press `Freeze All` on the coordinator UI.
- [ ] **5.2 Verify Static Hold**:
  - Directives, balance rides, masking cuts, and auto-trim gains immediately lock and freeze.
  - Dynamic adaptation ceases completely while audio streams without interruption.
- [ ] **5.3 Verify Individual Manual Override**:
  - Grab an EQ frequency slider or Compressor Threshold on `Lead Vocal`.
  - Verify that only the manipulated control switches to Manual mode while other parameters stay pinned.
  - Click `Reset to Auto` and verify parameter returns to AI control.

---

## 6. Coordinator Kill Mid-Playback Test (Fault Tolerance)

- [ ] **6.1 Start Playback**:
  Ensure multitrack audio is playing with the Master instance acting as Coordinator.
- [ ] **6.2 Delete Master Instance**:
  Remove the `AI Mixer` plugin from the Master track while audio is actively playing.
- [ ] **6.3 Verify Survivor Election**:
  - Within 1 second, the oldest surviving active Source instance (e.g. `01 Kick In`) detects missing Master heartbeats and elects itself fallback Coordinator.
  - The shared registry maintains instance registry state without audio interruption or clicks.
- [ ] **6.4 Staleness Timeout Verification**:
  If the coordinator machine shuts down entirely, verify that all Source instances detect heartbeat timeout after 2 seconds and smoothly ramp ride offsets to 0.0 dB over 1.0 second.

---

## 7. Dual Master Setup (PA Front of House + Broadcast Stream)

- [ ] **7.1 Configure PA Master**:
  - On Main PA Out, insert `AI Mixer` (Role: `Master`).
  - Set Destination Target to `Live PA (-18 LUFS, -1.0 dBTP)`.
- [ ] **7.2 Configure Broadcast Master**:
  - On Broadcast / Livestream Out, insert `AI Mixer` (Role: `Master`).
  - Set Destination Target to `Broadcast (-14 LUFS, -1.0 dBTP)`.
- [ ] **7.3 Verify True Peak Limiting & Auto-Gain**:
  - Confirm broadcast output integrates to `-14.0 LUFS +/- 1.0 LU`.
  - Confirm true-peak limiter holds ceiling strictly at `-1.0 dBTP` with 4x oversampling engaged without clipping or inter-sample overs.
- [ ] **7.4 Latency Check**:
  Confirm DAW reports 52 samples latency on both Master tracks; Source and Bus tracks remain strictly 0 samples.

---

## 8. Session Save & State Recall

- [ ] **8.1 Save DAW Project**:
  Save the DAW project (`Save Project As...`).
- [ ] **8.2 Close & Reopen Host**:
  Quit REAPER / Ableton completely, relaunch, and reopen the saved session.
- [ ] **8.3 Verify State Recall**:
  - All track roles (Source / Bus / Master) are preserved.
  - User identity overrides (if any) are retained.
  - Custom Taste profile, macro slider positions, and "More/Less of Me" biases restore accurately.

---

## 9. Standalone Mode & Dante / Ravenna Virtual Soundcheck

- [ ] **9.1 Launch Standalone**:
  Run `AI Mixer.exe` from the command line or desktop.
- [ ] **9.2 Audio Device Configuration**:
  In `Audio Device Settings`, select ASIO / Dante Virtual Soundcard (DVS) with 48 kHz / 128 or 256 samples buffer.
- [ ] **9.3 Verify Shared Memory Communication**:
  Launch a second instance or standalone process; verify they register in the shared registry and discover each other within 1 heartbeat cycle (1.0s).
