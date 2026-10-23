#pragma once

#include "SharedMemoryRegistry.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <string>
#include <functional>

namespace aimixer {

class InstanceNode : private juce::Timer {
public:
    InstanceNode(PluginRole initialRole, const std::string& initialTrackName);
    ~InstanceNode() override;

    uint32_t getInstanceId() const noexcept { return instanceId; }
    int getSlotIndex() const noexcept { return slotIndex; }
    PluginRole getRole() const noexcept { return currentRole; }
    bool isCoordinator() const noexcept { return isElectedCoordinator; }

    void setRole(PluginRole newRole);
    void setTrackName(const std::string& newName);
    const std::string& getTrackName() const noexcept { return trackName; }
    void setInstrumentType(InstrumentType type, float confidence);

    // Publishes updated telemetry from DSP analyzer to shared memory (called at 10-20 Hz)
    void publishTelemetry(InstanceTelemetry telemetry);

    // Reads directives sent from the Coordinator
    bool fetchDirectives(InstanceDirectives& directives);

    // Taste State broadcast (Coordinator writes, non-coordinators read)
    bool publishTasteState(const GlobalTasteState& taste);
    bool fetchTasteState(GlobalTasteState& taste);

    // Obtains a snapshot of all active instances in the session (for Master UI)
    std::vector<InstanceTelemetry> getSessionSnapshot() const;

    SharedMemoryRegistry& getRegistry() { return registry; }

    // Callback when coordinator status changes
    std::function<void(bool /* isCoordinator */)> onCoordinatorChanged;

private:
    void timerCallback() override;
    void ensureRegistered();

    SharedMemoryRegistry registry;
    uint32_t instanceId = 0;
    int slotIndex = -1;
    PluginRole currentRole = PluginRole::Source;
    std::string trackName;
    InstrumentType instrumentType = InstrumentType::Unknown;
    float instrumentConfidence = 0.0f;
    bool isElectedCoordinator = false;

    InstanceTelemetry lastTelemetry;
};

} // namespace aimixer
