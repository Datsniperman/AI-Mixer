#include "SharedMemoryRegistry.h"
#include "../Common/LoadTrace.h"
#include <chrono>
#include <cstring>
#include <algorithm>
#include <iostream>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace aimixer {

namespace {
    std::unique_ptr<SharedRegistryBlock> gInProcessFallbackBlock;
}

uint64_t SharedMemoryRegistry::getSystemTimeMs() noexcept {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

SharedMemoryRegistry::SharedMemoryRegistry() = default;

SharedMemoryRegistry::~SharedMemoryRegistry() {
    shutdown();
}

bool SharedMemoryRegistry::initialize(const std::string& customRegionName) {
    if (blockPtr != nullptr) return true;

    std::string defaultRegion = "AIMixer_SharedRegistry_v" + std::to_string(PROTOCOL_VERSION) + "_" + std::to_string(sizeof(SharedRegistryBlock));
    std::string region = customRegionName.empty() ? defaultRegion : customRegionName;
    if (mapSharedMemory(region)) {
        return true;
    }

    if (!gInProcessFallbackBlock) {
        gInProcessFallbackBlock = std::make_unique<SharedRegistryBlock>();
        gInProcessFallbackBlock->header.magic = SHARED_MAGIC;
        gInProcessFallbackBlock->header.version = PROTOCOL_VERSION;
        gInProcessFallbackBlock->header.maxSlots = static_cast<uint32_t>(MAX_REGISTRY_INSTANCES);
        gInProcessFallbackBlock->header.slotSize = sizeof(InstanceSlot);
    }
    blockPtr = gInProcessFallbackBlock.get();
    return true;
}

void SharedMemoryRegistry::shutdown() {
    unmapSharedMemory();
    blockPtr = nullptr;
    hasVersionMismatch = false;
}

bool SharedMemoryRegistry::mapSharedMemory(const std::string& regionName) {
    traceLog("registry attach start: region=%s, structSize=%zu", regionName.c_str(), sizeof(SharedRegistryBlock));
#if defined(_WIN32)
    std::wstring wideName(regionName.begin(), regionName.end());
    std::wstring fullPath = L"Local\\" + wideName;
    std::wstring mutexPath = L"Local\\" + wideName + L"_Mutex";

    HANDLE hMutex = CreateMutexW(NULL, FALSE, mutexPath.c_str());
    DWORD waitRes = (hMutex != NULL) ? WaitForSingleObject(hMutex, 5000) : WAIT_FAILED;

    winSharedMemHandle = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(sizeof(SharedRegistryBlock)),
        fullPath.c_str()
    );

    if (winSharedMemHandle == NULL) {
        if (hMutex != NULL) {
            if (waitRes == WAIT_OBJECT_0) ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return false;
    }

    bool alreadyExisted = (GetLastError() == ERROR_ALREADY_EXISTS);
    void* mapped = MapViewOfFile(winSharedMemHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedRegistryBlock));
    if (mapped == nullptr) {
        CloseHandle(static_cast<HANDLE>(winSharedMemHandle));
        winSharedMemHandle = nullptr;
        if (hMutex != NULL) {
            if (waitRes == WAIT_OBJECT_0) ReleaseMutex(hMutex);
            CloseHandle(hMutex);
        }
        return false;
    }

    blockPtr = static_cast<SharedRegistryBlock*>(mapped);
    if (!alreadyExisted) {
        isOwner = true;
        std::memset(blockPtr, 0, sizeof(SharedRegistryBlock));
        blockPtr->header.magic = SHARED_MAGIC;
        blockPtr->header.version = PROTOCOL_VERSION;
        blockPtr->header.maxSlots = static_cast<uint32_t>(MAX_REGISTRY_INSTANCES);
        blockPtr->header.slotSize = sizeof(InstanceSlot);
    } else {
        // Version Guard & Size validation: check if existing block has a mismatch
        bool valid = (blockPtr->header.magic == SHARED_MAGIC &&
                      blockPtr->header.version == PROTOCOL_VERSION &&
                      blockPtr->header.maxSlots == MAX_REGISTRY_INSTANCES &&
                      blockPtr->header.slotSize == sizeof(InstanceSlot));
        if (!valid) {
            hasVersionMismatch = true;
            traceLog("registry version/size mismatch: magic=0x%08X, version=%u, maxSlots=%u, slotSize=%u",
                     blockPtr->header.magic, blockPtr->header.version,
                     blockPtr->header.maxSlots, blockPtr->header.slotSize);
            UnmapViewOfFile(blockPtr);
            CloseHandle(static_cast<HANDLE>(winSharedMemHandle));
            winSharedMemHandle = nullptr;
            blockPtr = nullptr;
            if (hMutex != NULL) {
                if (waitRes == WAIT_OBJECT_0) ReleaseMutex(hMutex);
                CloseHandle(hMutex);
            }
            return false;
        }
    }
    traceLog("registry attach done: %s, magic=0x%08X, version=%u, maxSlots=%u, slotSize=%u",
             alreadyExisted ? "attached-existing" : "created-new",
             blockPtr->header.magic, blockPtr->header.version,
             blockPtr->header.maxSlots, blockPtr->header.slotSize);

    if (hMutex != NULL) {
        if (waitRes == WAIT_OBJECT_0) ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    return true;

#else
    std::string shmPath = "/" + regionName;
    posixShmFd = shm_open(shmPath.c_str(), O_RDWR | O_CREAT, 0666);
    if (posixShmFd < 0) {
        return false;
    }

    struct stat shmStat{};
    fstat(posixShmFd, &shmStat);
    bool needInit = (shmStat.st_size < static_cast<off_t>(sizeof(SharedRegistryBlock)));

    if (needInit) {
        if (ftruncate(posixShmFd, sizeof(SharedRegistryBlock)) != 0) {
            close(posixShmFd);
            posixShmFd = -1;
            return false;
        }
    }

    void* mapped = mmap(NULL, sizeof(SharedRegistryBlock), PROT_READ | PROT_WRITE, MAP_SHARED, posixShmFd, 0);
    if (mapped == MAP_FAILED) {
        close(posixShmFd);
        posixShmFd = -1;
        return false;
    }

    blockPtr = static_cast<SharedRegistryBlock*>(mapped);
    if (needInit) {
        isOwner = true;
        std::memset(blockPtr, 0, sizeof(SharedRegistryBlock));
        blockPtr->header.magic = SHARED_MAGIC;
        blockPtr->header.version = PROTOCOL_VERSION;
        blockPtr->header.maxSlots = static_cast<uint32_t>(MAX_REGISTRY_INSTANCES);
        blockPtr->header.slotSize = sizeof(InstanceSlot);
    } else {
        bool valid = (blockPtr->header.magic == SHARED_MAGIC &&
                      blockPtr->header.version == PROTOCOL_VERSION &&
                      blockPtr->header.maxSlots == MAX_REGISTRY_INSTANCES &&
                      blockPtr->header.slotSize == sizeof(InstanceSlot));
        if (!valid) {
            hasVersionMismatch = true;
            traceLog("registry version/size mismatch: magic=0x%08X, version=%u, maxSlots=%u, slotSize=%u",
                     blockPtr->header.magic, blockPtr->header.version,
                     blockPtr->header.maxSlots, blockPtr->header.slotSize);
            munmap(blockPtr, sizeof(SharedRegistryBlock));
            close(posixShmFd);
            posixShmFd = -1;
            blockPtr = nullptr;
            return false;
        }
    }
    return true;
#endif
}

void SharedMemoryRegistry::unmapSharedMemory() {
#if defined(_WIN32)
    if (blockPtr != nullptr && winSharedMemHandle != nullptr) {
        UnmapViewOfFile(blockPtr);
    }
    if (winSharedMemHandle != nullptr) {
        CloseHandle(static_cast<HANDLE>(winSharedMemHandle));
        winSharedMemHandle = nullptr;
    }
#else
    if (blockPtr != nullptr && posixShmFd >= 0) {
        munmap(blockPtr, sizeof(SharedRegistryBlock));
    }
    if (posixShmFd >= 0) {
        close(posixShmFd);
        posixShmFd = -1;
    }
#endif
}

int SharedMemoryRegistry::registerInstance(uint32_t instanceId, PluginRole role, const std::string& trackName, uint32_t processId) {
    if (!blockPtr || hasVersionMismatch) return -1;

    const uint64_t now = getSystemTimeMs();

    for (size_t i = 0; i < MAX_REGISTRY_INSTANCES; ++i) {
        InstanceSlot& slot = blockPtr->slots[i];
        uint32_t expected = 0;
        if (slot.slotActive.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
            uint32_t seq = slot.sequence.load(std::memory_order_relaxed);
            slot.sequence.store(seq + 1, std::memory_order_release);
            std::atomic_thread_fence(std::memory_order_release);

            std::memset(&slot.telemetry, 0, sizeof(InstanceTelemetry));
            std::memset(&slot.directives, 0, sizeof(InstanceDirectives));

            slot.telemetry.instanceId = instanceId;
            slot.telemetry.processId = processId;
            slot.telemetry.role = role;
            slot.telemetry.registrationTimestampMs = now;
            slot.telemetry.lastHeartbeatMs = now;
            slot.telemetry.priority = (role == PluginRole::Master) ? 100 : ((role == PluginRole::Bus) ? 80 : 50);

            size_t nameLen = (std::min)(trackName.size(), sizeof(slot.telemetry.trackName) - 1);
            std::memcpy(slot.telemetry.trackName, trackName.data(), nameLen);
            slot.telemetry.trackName[nameLen] = '\0';

            std::atomic_thread_fence(std::memory_order_release);
            slot.sequence.store(seq + 2, std::memory_order_release);

            blockPtr->header.activeInstanceCount.fetch_add(1, std::memory_order_relaxed);
            return static_cast<int>(i);
        }
    }

    return -1;
}

void SharedMemoryRegistry::unregisterInstance(int slotIndex, uint32_t instanceId) {
    if (!blockPtr || slotIndex < 0 || slotIndex >= static_cast<int>(MAX_REGISTRY_INSTANCES)) return;

    InstanceSlot& slot = blockPtr->slots[slotIndex];
    if (slot.slotActive.load(std::memory_order_acquire) == 1) {
        if (slot.telemetry.instanceId == instanceId) {
            slot.slotActive.store(0, std::memory_order_release);
            blockPtr->header.activeInstanceCount.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

void SharedMemoryRegistry::updateHeartbeat(int slotIndex) {
    if (!blockPtr || hasVersionMismatch || slotIndex < 0 || slotIndex >= static_cast<int>(MAX_REGISTRY_INSTANCES)) return;

    InstanceSlot& slot = blockPtr->slots[slotIndex];
    if (slot.slotActive.load(std::memory_order_acquire) == 1) {
        slot.telemetry.lastHeartbeatMs = getSystemTimeMs();
    }
}

bool SharedMemoryRegistry::publishTelemetry(int slotIndex, const InstanceTelemetry& telemetry) {
    if (!blockPtr || hasVersionMismatch || slotIndex < 0 || slotIndex >= static_cast<int>(MAX_REGISTRY_INSTANCES)) return false;

    InstanceSlot& slot = blockPtr->slots[slotIndex];
    if (slot.slotActive.load(std::memory_order_acquire) == 0) return false;

    uint32_t seq = slot.sequence.load(std::memory_order_relaxed);
    slot.sequence.store(seq + 1, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_release);

    std::memcpy(&slot.telemetry, &telemetry, sizeof(InstanceTelemetry));

    std::atomic_thread_fence(std::memory_order_release);
    slot.sequence.store(seq + 2, std::memory_order_release);
    return true;
}

bool SharedMemoryRegistry::readTelemetry(int slotIndex, InstanceTelemetry& outTelemetry) const {
    if (!blockPtr || slotIndex < 0 || slotIndex >= static_cast<int>(MAX_REGISTRY_INSTANCES)) return false;

    const InstanceSlot& slot = blockPtr->slots[slotIndex];
    if (slot.slotActive.load(std::memory_order_acquire) == 0) return false;

    for (int retry = 0; retry < 5; ++retry) {
        uint32_t seq1 = slot.sequence.load(std::memory_order_acquire);
        if ((seq1 & 1) != 0) continue;

        std::atomic_thread_fence(std::memory_order_acquire);
        std::memcpy(&outTelemetry, &slot.telemetry, sizeof(InstanceTelemetry));
        std::atomic_thread_fence(std::memory_order_acquire);

        uint32_t seq2 = slot.sequence.load(std::memory_order_acquire);
        if (seq1 == seq2 && (seq2 & 1) == 0) {
            return true;
        }
    }
    return false;
}

bool SharedMemoryRegistry::publishTasteState(const GlobalTasteState& taste) {
    if (!blockPtr || hasVersionMismatch) return false;

    uint32_t seq = blockPtr->header.tasteSequence.load(std::memory_order_relaxed);
    blockPtr->header.tasteSequence.store(seq + 1, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_release);

    std::memcpy(&blockPtr->header.tasteState, &taste, sizeof(GlobalTasteState));

    std::atomic_thread_fence(std::memory_order_release);
    blockPtr->header.tasteSequence.store(seq + 2, std::memory_order_release);
    return true;
}

bool SharedMemoryRegistry::readTasteState(GlobalTasteState& outTaste) const {
    if (!blockPtr) return false;

    for (int retry = 0; retry < 5; ++retry) {
        uint32_t seq1 = blockPtr->header.tasteSequence.load(std::memory_order_acquire);
        if ((seq1 & 1) != 0) continue;

        std::atomic_thread_fence(std::memory_order_acquire);
        std::memcpy(&outTaste, &blockPtr->header.tasteState, sizeof(GlobalTasteState));
        std::atomic_thread_fence(std::memory_order_acquire);

        uint32_t seq2 = blockPtr->header.tasteSequence.load(std::memory_order_acquire);
        if (seq1 == seq2 && (seq2 & 1) == 0) {
            return true;
        }
    }
    return false;
}

bool SharedMemoryRegistry::publishDirectives(int slotIndex, const InstanceDirectives& directives) {
    if (!blockPtr || hasVersionMismatch || slotIndex < 0 || slotIndex >= static_cast<int>(MAX_REGISTRY_INSTANCES)) return false;

    InstanceSlot& slot = blockPtr->slots[slotIndex];
    if (slot.slotActive.load(std::memory_order_acquire) == 0) return false;

    uint32_t seq = slot.sequence.load(std::memory_order_relaxed);
    slot.sequence.store(seq + 1, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_release);

    std::memcpy(&slot.directives, &directives, sizeof(InstanceDirectives));

    std::atomic_thread_fence(std::memory_order_release);
    slot.sequence.store(seq + 2, std::memory_order_release);
    return true;
}

bool SharedMemoryRegistry::readDirectives(int slotIndex, InstanceDirectives& outDirectives) const {
    if (!blockPtr || slotIndex < 0 || slotIndex >= static_cast<int>(MAX_REGISTRY_INSTANCES)) return false;

    const InstanceSlot& slot = blockPtr->slots[slotIndex];
    if (slot.slotActive.load(std::memory_order_acquire) == 0) return false;

    for (int retry = 0; retry < 5; ++retry) {
        uint32_t seq1 = slot.sequence.load(std::memory_order_acquire);
        if ((seq1 & 1) != 0) continue;

        std::atomic_thread_fence(std::memory_order_acquire);
        std::memcpy(&outDirectives, &slot.directives, sizeof(InstanceDirectives));
        std::atomic_thread_fence(std::memory_order_acquire);

        uint32_t seq2 = slot.sequence.load(std::memory_order_acquire);
        if (seq1 == seq2 && (seq2 & 1) == 0) {
            return true;
        }
    }
    return false;
}

uint32_t SharedMemoryRegistry::runMaintenanceAndElection(uint64_t currentTimestampMs) {
    if (!blockPtr) return 0;

    blockPtr->header.lastSweepTimestampMs.store(currentTimestampMs, std::memory_order_relaxed);

    uint32_t masterCandidateId = 0;
    uint64_t masterCandidateTimestamp = UINT64_MAX;

    // Earliest registered active survivor (by registration timestamp)
    uint32_t earliestSurvivorId = 0;
    uint64_t earliestSurvivorTimestamp = UINT64_MAX;

    for (size_t i = 0; i < MAX_REGISTRY_INSTANCES; ++i) {
        InstanceSlot& slot = blockPtr->slots[i];
        if (slot.slotActive.load(std::memory_order_acquire) == 0) continue;

        if (currentTimestampMs > slot.telemetry.lastHeartbeatMs &&
            (currentTimestampMs - slot.telemetry.lastHeartbeatMs) > HEARTBEAT_TIMEOUT_MS) {
            slot.slotActive.store(0, std::memory_order_release);
            blockPtr->header.activeInstanceCount.fetch_sub(1, std::memory_order_relaxed);
            continue;
        }

        // Priority 1: Master role
        if (slot.telemetry.role == PluginRole::Master) {
            if (slot.telemetry.registrationTimestampMs < masterCandidateTimestamp) {
                masterCandidateTimestamp = slot.telemetry.registrationTimestampMs;
                masterCandidateId = slot.telemetry.instanceId;
            }
        }

        // Fallback: Earliest registered active survivor among all non-master instances
        if (slot.telemetry.registrationTimestampMs < earliestSurvivorTimestamp) {
            earliestSurvivorTimestamp = slot.telemetry.registrationTimestampMs;
            earliestSurvivorId = slot.telemetry.instanceId;
        }
    }

    uint32_t electedCoordinatorId = (masterCandidateId != 0) ? masterCandidateId : earliestSurvivorId;
    blockPtr->header.coordinatorInstanceId.store(electedCoordinatorId, std::memory_order_release);

    for (size_t i = 0; i < MAX_REGISTRY_INSTANCES; ++i) {
        InstanceSlot& slot = blockPtr->slots[i];
        if (slot.slotActive.load(std::memory_order_acquire) == 1) {
            slot.telemetry.isCoordinator = (slot.telemetry.instanceId == electedCoordinatorId);
        }
    }

    return electedCoordinatorId;
}

std::vector<InstanceTelemetry> SharedMemoryRegistry::getActiveInstancesSnapshot() const {
    std::vector<InstanceTelemetry> result;
    if (!blockPtr) return result;

    result.reserve(MAX_REGISTRY_INSTANCES);

    for (size_t i = 0; i < MAX_REGISTRY_INSTANCES; ++i) {
        InstanceTelemetry item;
        if (readTelemetry(static_cast<int>(i), item)) {
            result.push_back(item);
        }
    }

    std::sort(result.begin(), result.end(), [](const InstanceTelemetry& a, const InstanceTelemetry& b) {
        if (static_cast<int>(a.role) != static_cast<int>(b.role)) {
            return static_cast<int>(a.role) > static_cast<int>(b.role);
        }
        return a.priority > b.priority;
    });

    return result;
}

uint32_t SharedMemoryRegistry::getCoordinatorInstanceId() const {
    if (!blockPtr) return 0;
    return blockPtr->header.coordinatorInstanceId.load(std::memory_order_acquire);
}

size_t SharedMemoryRegistry::getActiveInstanceCount() const {
    if (!blockPtr) return 0;
    return blockPtr->header.activeInstanceCount.load(std::memory_order_relaxed);
}

} // namespace aimixer
