#pragma once

#include "../Common/SharedProtocol.h"
#include <memory>
#include <vector>
#include <string>
#include <optional>

namespace aimixer {

class SharedMemoryRegistry {
public:
    SharedMemoryRegistry();
    ~SharedMemoryRegistry();

    SharedMemoryRegistry(const SharedMemoryRegistry&) = delete;
    SharedMemoryRegistry& operator=(const SharedMemoryRegistry&) = delete;

    bool initialize(const std::string& customRegionName = "");
    void shutdown();
    bool isInitialized() const noexcept { return blockPtr != nullptr; }
    bool isVersionMismatch() const noexcept { return hasVersionMismatch; }

    int registerInstance(uint32_t instanceId, PluginRole role, const std::string& trackName, uint32_t processId);
    void unregisterInstance(int slotIndex, uint32_t instanceId);
    void updateHeartbeat(int slotIndex);

    bool publishTelemetry(int slotIndex, const InstanceTelemetry& telemetry);
    bool readTelemetry(int slotIndex, InstanceTelemetry& outTelemetry) const;

    bool publishDirectives(int slotIndex, const InstanceDirectives& directives);
    bool readDirectives(int slotIndex, InstanceDirectives& outDirectives) const;

    bool publishTasteState(const GlobalTasteState& taste);
    bool readTasteState(GlobalTasteState& outTaste) const;

    uint32_t runMaintenanceAndElection(uint64_t currentTimestampMs);

    std::vector<InstanceTelemetry> getActiveInstancesSnapshot() const;
    uint32_t getCoordinatorInstanceId() const;
    size_t getActiveInstanceCount() const;

    static uint64_t getSystemTimeMs() noexcept;

private:
    bool mapSharedMemory(const std::string& regionName);
    void unmapSharedMemory();

    SharedRegistryBlock* blockPtr = nullptr;
    bool isOwner = false;
    bool hasVersionMismatch = false;

#if defined(_WIN32)
    void* winSharedMemHandle = nullptr;
#else
    int posixShmFd = -1;
#endif

    std::unique_ptr<SharedRegistryBlock> fallbackLocalBlock;
};

} // namespace aimixer
