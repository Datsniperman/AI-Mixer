#include "InstanceNode.h"
#include <juce_core/juce_core.h>
#include <algorithm>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace aimixer {

namespace {
    uint32_t getCurrentProcessId() noexcept {
#if defined(_WIN32)
        return static_cast<uint32_t>(GetCurrentProcessId());
#else
        return static_cast<uint32_t>(getpid());
#endif
    }
}

InstanceNode::InstanceNode(PluginRole initialRole, const std::string& initialTrackName)
    : currentRole(initialRole), trackName(initialTrackName) {
    // Generate random non-zero instance ID
    juce::Random rng(juce::Time::getHighResolutionTicks());
    instanceId = static_cast<uint32_t>(rng.nextInt({100000, 999999}));

    registry.initialize();
    ensureRegistered();

    // Start timer at 100ms (10 Hz) for heartbeats, election, and maintenance
    startTimer(100);
}

InstanceNode::~InstanceNode() {
    stopTimer();
    if (slotIndex >= 0) {
        registry.unregisterInstance(slotIndex, instanceId);
        slotIndex = -1;
    }
    registry.shutdown();
}

void InstanceNode::ensureRegistered() {
    if (slotIndex >= 0) return;

    slotIndex = registry.registerInstance(
        instanceId,
        currentRole,
        trackName,
        getCurrentProcessId()
    );
}

void InstanceNode::setRole(PluginRole newRole) {
    if (currentRole == newRole) return;
    currentRole = newRole;

    if (slotIndex >= 0) {
        // Update role in slot
        lastTelemetry.role = newRole;
        lastTelemetry.priority = (newRole == PluginRole::Master) ? 100 : ((newRole == PluginRole::Bus) ? 80 : 50);
        publishTelemetry(lastTelemetry);
        // Trigger election check immediately
        timerCallback();
    }
}

void InstanceNode::setTrackName(const std::string& newName) {
    if (trackName == newName) return;
    trackName = newName;

    if (slotIndex >= 0) {
        size_t len = (std::min)(trackName.size(), sizeof(lastTelemetry.trackName) - 1);
        std::memcpy(lastTelemetry.trackName, trackName.data(), len);
        lastTelemetry.trackName[len] = '\0';
        publishTelemetry(lastTelemetry);
    }
}

void InstanceNode::setInstrumentType(InstrumentType type, float confidence) {
    instrumentType = type;
    instrumentConfidence = confidence;
    lastTelemetry.instrumentType = type;
    lastTelemetry.instrumentConfidence = confidence;
}

void InstanceNode::publishTelemetry(InstanceTelemetry telemetry) {
    ensureRegistered();
    if (slotIndex < 0) return;

    telemetry.instanceId = instanceId;
    telemetry.processId = getCurrentProcessId();
    telemetry.role = currentRole;
    telemetry.lastHeartbeatMs = SharedMemoryRegistry::getSystemTimeMs();
    telemetry.isCoordinator = isElectedCoordinator;
    telemetry.instrumentType = instrumentType;
    telemetry.instrumentConfidence = instrumentConfidence;

    size_t nameLen = (std::min)(trackName.size(), sizeof(telemetry.trackName) - 1);
    std::memcpy(telemetry.trackName, trackName.data(), nameLen);
    telemetry.trackName[nameLen] = '\0';

    lastTelemetry = telemetry;
    registry.publishTelemetry(slotIndex, telemetry);
}

bool InstanceNode::fetchDirectives(InstanceDirectives& directives) {
    if (slotIndex < 0) return false;
    return registry.readDirectives(slotIndex, directives);
}

bool InstanceNode::publishTasteState(const GlobalTasteState& taste) {
    return registry.publishTasteState(taste);
}

bool InstanceNode::fetchTasteState(GlobalTasteState& taste) {
    return registry.readTasteState(taste);
}

std::vector<InstanceTelemetry> InstanceNode::getSessionSnapshot() const {
    return registry.getActiveInstancesSnapshot();
}

void InstanceNode::timerCallback() {
    ensureRegistered();
    if (slotIndex < 0) return;

    const uint64_t now = SharedMemoryRegistry::getSystemTimeMs();
    registry.updateHeartbeat(slotIndex);

    uint32_t electedId = registry.runMaintenanceAndElection(now);
    bool shouldBeCoordinator = (electedId == instanceId);

    if (shouldBeCoordinator != isElectedCoordinator) {
        isElectedCoordinator = shouldBeCoordinator;
        if (onCoordinatorChanged) {
            onCoordinatorChanged(isElectedCoordinator);
        }
    }
}

} // namespace aimixer
