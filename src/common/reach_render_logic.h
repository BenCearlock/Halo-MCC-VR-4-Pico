#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

// Reach-only render evidence and allocation-free policy. This file contains no
// Windows, COM, MinHook, logging, or engine writes, so the exact routing and
// rollback rules can be exhaustively tested before any detour is authorized.

inline constexpr size_t kReachRetailImageSize = 0x04EDA000;
inline constexpr uintptr_t kReachMainRenderViewRva = 0x000C31F4;
inline constexpr uintptr_t kReachNormalOuterReturnRva = 0x000C3735;
inline constexpr uintptr_t kReachScreenshotOuterReturnRva = 0x001D3869;
inline constexpr uintptr_t kReachPlayerViewRenderRva = 0x0026C6DC;
inline constexpr uintptr_t kReachPlayerViewRenderCallerRva = 0x000C33C4;
inline constexpr uintptr_t kReachPlayerViewRenderReturnRva = 0x000C33C9;
inline constexpr uintptr_t kReachFrustumHelperRva = 0x00287F58;
inline constexpr uintptr_t kReachPlayerViewArrayRva = 0x029F2B90;
inline constexpr size_t kReachPlayerViewStride = 0x0A40;
inline constexpr uint32_t kReachPlayerViewCount = 4;
inline constexpr uintptr_t kReachDefaultWorkspaceRva = 0x00C9FAE0;
inline constexpr size_t kReachRenderScopeSnapshotSize = 0x02B0;
inline constexpr uintptr_t kReachCameraStackCallbackRva = 0x0026BFD4;
inline constexpr uintptr_t kReachRenderCameraOwnerRva = 0x04E38A90;
inline constexpr uintptr_t kReachSelectedSpecializationRva = 0x04E38A08;
inline constexpr uintptr_t kReachPlayerViewCameraStateOffset = 0x03B0;
inline constexpr uintptr_t kReachPlayerViewCurrentMatricesOffset = 0x0490;
inline constexpr uintptr_t kReachPlayerViewPreviousMatricesOffset = 0x0760;
inline constexpr uintptr_t kReachLastWindowFlagOffset = 0x0A30;
inline constexpr size_t kReachPlayerViewCameraStateSize = 0x00C8;
inline constexpr size_t kReachPlayerViewMatrixBlockSize = 0x02D0;
inline constexpr uint64_t kReachRenderFreshnessMaxGapMs = 500;
inline constexpr uint64_t kReachRenderSafetyIntervalMs = 1000;

inline constexpr size_t kReachMainRenderViewBodySize = 515;
inline constexpr char kReachMainRenderViewBodySha256[] =
    "95DF3EFFF9AC6EE29887D1272CCA8D7BF3E58F87041BAD8032107825B733FE89";
inline constexpr size_t kReachPlayerViewRenderBodySize = 2314;
inline constexpr char kReachPlayerViewRenderBodySha256[] =
    "2628D1189621EACED7C95A1F295815D70E7783054F1C3CBA46799F838CC33C60";

inline constexpr std::array<uint8_t, 32> kReachMainRenderViewAob{
    0x40, 0x53, 0x56, 0x57, 0x48, 0x81, 0xEC, 0x80,
    0x00, 0x00, 0x00, 0x0F, 0x29, 0x74, 0x24, 0x70,
    0x48, 0x8B, 0x05, 0x05, 0x6E, 0xA3, 0x00, 0x48,
    0x33, 0xC4, 0x48, 0x89, 0x44, 0x24, 0x68, 0x41,
};

inline constexpr std::array<uint8_t, 69> kReachPlayerViewRenderAob{
    0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48,
    0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20, 0x55,
    0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
    0x48, 0x8D, 0xA8, 0x28, 0xFF, 0xFF, 0xFF, 0x48,
    0x81, 0xEC, 0xB0, 0x01, 0x00, 0x00, 0x0F, 0x29,
    0x70, 0xC8, 0x0F, 0x29, 0x78, 0xB8, 0x48, 0x8B,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x33, 0xC4,
    0x48, 0x89, 0x85, 0x80, 0x00, 0x00, 0x00, 0x8B,
    0x81, 0xA4, 0x03, 0x00, 0x00,
};

inline constexpr auto kReachPlayerViewRenderAobMask = []
{
    std::array<uint8_t, kReachPlayerViewRenderAob.size()> mask{};
    for (auto& byte : mask)
        byte = 0xFF;
    for (size_t index = 49; index <= 52; ++index)
        mask[index] = 0;
    return mask;
}();

inline constexpr std::array<uint8_t, 25> kReachFrustumHelperAob{
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x44, 0x0F,
    0xBF, 0x49, 0x62, 0x4C, 0x8B, 0xD9, 0x4C, 0x8B,
    0x41, 0x38, 0x48, 0x8B, 0xDA, 0x0F, 0xBF, 0x51,
    0x50,
};

inline size_t CountReachMaskedPattern(
    const uint8_t* data, size_t dataSize, const uint8_t* pattern,
    const uint8_t* mask, size_t patternSize) noexcept
{
    if (!data || !pattern || !mask || !patternSize || dataSize < patternSize)
        return 0;

    size_t count = 0;
    for (size_t offset = 0; offset <= dataSize - patternSize; ++offset)
    {
        size_t index = 0;
        for (; index < patternSize; ++index)
        {
            if (mask[index] && data[offset + index] != pattern[index])
                break;
        }
        if (index == patternSize)
            ++count;
    }
    return count;
}

struct ReachRenderCandidateProof
{
    bool retailIdentity = false;
    uint32_t mainRenderViewMatchCount = 0;
    bool mainRenderViewAtExpectedRva = false;
    bool mainRenderViewBodyHash = false;
    uint32_t playerViewRenderMatchCount = 0;
    bool playerViewRenderAtExpectedRva = false;
    bool playerViewRenderBodyHash = false;
    uint32_t frustumHelperMatchCount = 0;
    bool frustumHelperAtExpectedRva = false;
    bool frustumHelperExecutableRange = false;
    bool exactOuterCallerEdges = false;
    bool exactInnerCallerEdge = false;
    bool fixedDataRanges = false;
};

inline bool ReachRenderCandidateProofComplete(
    const ReachRenderCandidateProof& proof) noexcept
{
    return proof.retailIdentity &&
        proof.mainRenderViewMatchCount == 1 &&
        proof.mainRenderViewAtExpectedRva &&
        proof.mainRenderViewBodyHash &&
        proof.playerViewRenderMatchCount == 1 &&
        proof.playerViewRenderAtExpectedRva &&
        proof.playerViewRenderBodyHash &&
        proof.frustumHelperMatchCount == 1 &&
        proof.frustumHelperAtExpectedRva &&
        proof.frustumHelperExecutableRange &&
        proof.exactOuterCallerEdges &&
        proof.exactInnerCallerEdge &&
        proof.fixedDataRanges;
}

struct ReachModuleEpoch
{
    uintptr_t moduleBase = 0;
    uint32_t generation = 0;
};

inline bool ReachModuleEpochValid(const ReachModuleEpoch& epoch) noexcept
{
    return epoch.moduleBase != 0 && epoch.generation != 0;
}

inline bool ReachSameModuleEpoch(
    const ReachModuleEpoch& left, const ReachModuleEpoch& right) noexcept
{
    return ReachModuleEpochValid(left) &&
        left.moduleBase == right.moduleBase &&
        left.generation == right.generation;
}

class ReachPreflightToken
{
public:
    ReachPreflightToken() noexcept = default;

    static ReachPreflightToken Create(
        const ReachModuleEpoch& epoch,
        const ReachRenderCandidateProof& proof) noexcept
    {
        return ReachModuleEpochValid(epoch) &&
                ReachRenderCandidateProofComplete(proof)
            ? ReachPreflightToken(epoch)
            : ReachPreflightToken{};
    }

    bool Complete() const noexcept { return m_complete; }
    ReachModuleEpoch Epoch() const noexcept { return m_epoch; }

private:
    explicit ReachPreflightToken(const ReachModuleEpoch& epoch) noexcept
        : m_epoch(epoch), m_complete(true)
    {
    }

    ReachModuleEpoch m_epoch{};
    bool m_complete = false;
};

class ReachFreshCameraToken
{
public:
    ReachFreshCameraToken() noexcept = default;

    bool Stable() const noexcept { return m_stable; }
    ReachModuleEpoch Epoch() const noexcept { return m_epoch; }
    uint64_t PreparedFrameSerial() const noexcept
    {
        return m_preparedFrameSerial;
    }
    uint64_t Nonce() const noexcept { return m_nonce; }
    uint64_t ObservedAtMs() const noexcept { return m_observedAtMs; }

private:
    ReachFreshCameraToken(
        const ReachModuleEpoch& epoch, uint64_t preparedFrameSerial,
        uint64_t nonce, uint64_t observedAtMs) noexcept
        : m_epoch(epoch),
          m_preparedFrameSerial(preparedFrameSerial),
          m_nonce(nonce),
          m_observedAtMs(observedAtMs),
          m_stable(true)
    {
    }

    ReachModuleEpoch m_epoch{};
    uint64_t m_preparedFrameSerial = 0;
    uint64_t m_nonce = 0;
    uint64_t m_observedAtMs = 0;
    bool m_stable = false;

    friend class ReachRenderFreshnessGate;
};

class ReachPreparedFrameToken
{
public:
    ReachPreparedFrameToken() noexcept = default;

    static ReachPreparedFrameToken Create(
        const ReachModuleEpoch& epoch, uint64_t serial,
        bool ready) noexcept
    {
        return ReachModuleEpochValid(epoch) && serial && ready
            ? ReachPreparedFrameToken(epoch, serial)
            : ReachPreparedFrameToken{};
    }

    bool Ready() const noexcept { return m_ready; }
    ReachModuleEpoch Epoch() const noexcept { return m_epoch; }
    uint64_t Serial() const noexcept { return m_serial; }

private:
    ReachPreparedFrameToken(
        const ReachModuleEpoch& epoch, uint64_t serial) noexcept
        : m_epoch(epoch), m_serial(serial), m_ready(true)
    {
    }

    ReachModuleEpoch m_epoch{};
    uint64_t m_serial = 0;
    bool m_ready = false;
};

class ReachDirectCopyToken
{
public:
    ReachDirectCopyToken() noexcept = default;

    static ReachDirectCopyToken Create(
        const ReachModuleEpoch& epoch, uint64_t preparedFrameSerial,
        bool ready) noexcept
    {
        return ReachModuleEpochValid(epoch) && preparedFrameSerial && ready
            ? ReachDirectCopyToken(epoch, preparedFrameSerial)
            : ReachDirectCopyToken{};
    }

    bool Ready() const noexcept { return m_ready; }
    ReachModuleEpoch Epoch() const noexcept { return m_epoch; }
    uint64_t PreparedFrameSerial() const noexcept
    {
        return m_preparedFrameSerial;
    }

private:
    ReachDirectCopyToken(
        const ReachModuleEpoch& epoch,
        uint64_t preparedFrameSerial) noexcept
        : m_epoch(epoch),
          m_preparedFrameSerial(preparedFrameSerial),
          m_ready(true)
    {
    }

    ReachModuleEpoch m_epoch{};
    uint64_t m_preparedFrameSerial = 0;
    bool m_ready = false;
};

enum class ReachCleanupDisposition : uint8_t
{
    None = 0,
    Completed,
    Aborted,
};

class ReachCleanupToken
{
public:
    ReachCleanupToken() noexcept = default;

    bool Valid() const noexcept
    {
        return m_disposition != ReachCleanupDisposition::None;
    }
    ReachModuleEpoch Epoch() const noexcept { return m_epoch; }
    uint64_t PreparedFrameSerial() const noexcept
    {
        return m_preparedFrameSerial;
    }
    ReachCleanupDisposition Disposition() const noexcept
    {
        return m_disposition;
    }

private:
    ReachCleanupToken(
        const ReachModuleEpoch& epoch, uint64_t preparedFrameSerial,
        ReachCleanupDisposition disposition) noexcept
        : m_epoch(epoch),
          m_preparedFrameSerial(preparedFrameSerial),
          m_disposition(disposition)
    {
    }

    ReachModuleEpoch m_epoch{};
    uint64_t m_preparedFrameSerial = 0;
    ReachCleanupDisposition m_disposition = ReachCleanupDisposition::None;

    friend class ReachRollbackGate;
};

enum class ReachOuterRenderCaller : uint8_t
{
    Unknown = 0,
    NormalPlayer,
    ScreenshotTileBloom,
};

inline bool ReachAddressFromRva(
    uintptr_t moduleBase, size_t moduleSize, uintptr_t rva,
    uintptr_t& address) noexcept
{
    if (!moduleBase || rva >= moduleSize ||
        rva > std::numeric_limits<uintptr_t>::max() - moduleBase)
    {
        return false;
    }
    address = moduleBase + rva;
    return true;
}

inline ReachOuterRenderCaller ClassifyReachOuterRenderCaller(
    uintptr_t moduleBase, size_t moduleSize, uintptr_t returnAddress) noexcept
{
    if (moduleSize != kReachRetailImageSize)
        return ReachOuterRenderCaller::Unknown;

    uintptr_t normalReturn = 0;
    uintptr_t screenshotReturn = 0;
    if (!ReachAddressFromRva(moduleBase, moduleSize,
                            kReachNormalOuterReturnRva, normalReturn) ||
        !ReachAddressFromRva(moduleBase, moduleSize,
                            kReachScreenshotOuterReturnRva, screenshotReturn))
    {
        return ReachOuterRenderCaller::Unknown;
    }
    if (returnAddress == normalReturn)
        return ReachOuterRenderCaller::NormalPlayer;
    if (returnAddress == screenshotReturn)
        return ReachOuterRenderCaller::ScreenshotTileBloom;
    return ReachOuterRenderCaller::Unknown;
}

class ReachRenderFreshnessGate
{
public:
    bool AdvanceEpoch(const ReachModuleEpoch& epoch) noexcept
    {
        if (ReachModuleEpochValid(m_epoch) ||
            !ReachModuleEpochValid(epoch) ||
            epoch.generation <= m_highestGeneration)
        {
            return false;
        }
        m_epoch = epoch;
        m_highestGeneration = epoch.generation;
        m_lastObservedSerial = 0;
        ResetWindow();
        return true;
    }

    ReachFreshCameraToken Observe(
        uint64_t nowMs, const ReachModuleEpoch& epoch,
        uint64_t preparedFrameSerial,
        bool exactNormalSlotZero, bool cameraValid) noexcept
    {
        if (!ReachSameModuleEpoch(epoch, m_epoch))
            return {};

        m_currentToken = {};
        if (!preparedFrameSerial ||
            preparedFrameSerial <= m_lastObservedSerial)
        {
            ResetWindow();
            return {};
        }
        m_lastObservedSerial = preparedFrameSerial;

        if (!nowMs || !exactNormalSlotZero || !cameraValid)
        {
            ResetWindow();
            return {};
        }

        if (!m_transactionCount || nowMs <= m_lastMs ||
            nowMs - m_lastMs >= kReachRenderFreshnessMaxGapMs)
        {
            m_transactionCount = 1;
            m_firstMs = nowMs;
            m_lastMs = nowMs;
            return {};
        }

        if (m_transactionCount != std::numeric_limits<uint32_t>::max())
            ++m_transactionCount;
        m_lastMs = nowMs;
        if (m_lastMs - m_firstMs <= kReachRenderSafetyIntervalMs ||
            m_nonceCounter == std::numeric_limits<uint64_t>::max())
        {
            return {};
        }

        ++m_nonceCounter;
        m_currentToken = ReachFreshCameraToken(
            m_epoch, preparedFrameSerial, m_nonceCounter, nowMs);
        return m_currentToken;
    }

    bool IsCurrent(const ReachFreshCameraToken& token) const noexcept
    {
        return token.Stable() && m_currentToken.Stable() &&
            ReachSameModuleEpoch(token.Epoch(), m_epoch) &&
            ReachSameModuleEpoch(token.Epoch(), m_currentToken.Epoch()) &&
            token.PreparedFrameSerial() ==
                m_currentToken.PreparedFrameSerial() &&
            token.Nonce() == m_currentToken.Nonce();
    }

    bool Consume(
        const ReachFreshCameraToken& fresh,
        const ReachPreparedFrameToken& preparedFrame,
        uint64_t nowMs) noexcept
    {
        if (!IsCurrent(fresh) || !preparedFrame.Ready() ||
            !ReachSameModuleEpoch(fresh.Epoch(), preparedFrame.Epoch()) ||
            fresh.PreparedFrameSerial() != preparedFrame.Serial() ||
            !nowMs || nowMs < fresh.ObservedAtMs() ||
            nowMs - fresh.ObservedAtMs() >= kReachRenderFreshnessMaxGapMs)
        {
            m_currentToken = {};
            return false;
        }
        m_currentToken = {};
        return true;
    }

    bool Teardown(const ReachModuleEpoch& epoch) noexcept
    {
        if (!ReachSameModuleEpoch(epoch, m_epoch))
            return false;
        m_epoch = {};
        m_lastObservedSerial = 0;
        ResetWindow();
        return true;
    }

    void ResetWindow() noexcept
    {
        m_transactionCount = 0;
        m_firstMs = 0;
        m_lastMs = 0;
        m_currentToken = {};
    }

    uint32_t TransactionCount() const noexcept { return m_transactionCount; }
    uint64_t CurrentSpanMs() const noexcept
    {
        return m_transactionCount && m_lastMs >= m_firstMs
            ? m_lastMs - m_firstMs
            : 0;
    }

private:
    ReachModuleEpoch m_epoch{};
    uint32_t m_highestGeneration = 0;
    uint64_t m_lastObservedSerial = 0;
    uint64_t m_nonceCounter = 0;
    ReachFreshCameraToken m_currentToken{};
    uint32_t m_transactionCount = 0;
    uint64_t m_firstMs = 0;
    uint64_t m_lastMs = 0;
};

struct ReachOuterRenderInput
{
    uintptr_t moduleBase = 0;
    size_t moduleSize = 0;
    uintptr_t returnAddress = 0;
    uintptr_t workspace = 0;
    uintptr_t playerView = 0;
    uint32_t playerWindowIndex = 0;
    int32_t cameraStackDepthBefore = -1;
    uint64_t nowMs = 0;
    ReachPreflightToken preflight{};
    ReachFreshCameraToken freshCamera{};
    ReachPreparedFrameToken preparedFrame{};
    bool teardownRequested = false;
};

class ReachRenderOwnerToken
{
public:
    ReachRenderOwnerToken() noexcept = default;

    bool Active() const noexcept { return m_active; }
    ReachModuleEpoch Epoch() const noexcept { return m_epoch; }
    uint64_t PreparedFrameSerial() const noexcept
    {
        return m_preparedFrameSerial;
    }
    uintptr_t Workspace() const noexcept { return m_workspace; }
    uintptr_t PlayerView() const noexcept { return m_playerView; }
    int32_t CameraStackDepthBefore() const noexcept
    {
        return m_cameraStackDepthBefore;
    }

private:
    bool m_active = false;
    ReachModuleEpoch m_epoch{};
    uint64_t m_preparedFrameSerial = 0;
    uintptr_t m_workspace = 0;
    uintptr_t m_playerView = 0;
    int32_t m_cameraStackDepthBefore = -1;

    friend class ReachRenderOwnerGate;
};

inline bool ReachNormalOuterInputMatches(
    const ReachOuterRenderInput& input) noexcept
{
    const ReachModuleEpoch epoch = input.preflight.Epoch();
    if (!input.preflight.Complete() || !input.freshCamera.Stable() ||
        !input.preparedFrame.Ready() || input.teardownRequested ||
        !input.nowMs ||
        !ReachModuleEpochValid(epoch) ||
        !ReachSameModuleEpoch(epoch, input.freshCamera.Epoch()) ||
        !ReachSameModuleEpoch(epoch, input.preparedFrame.Epoch()) ||
        !input.preparedFrame.Serial() ||
        input.freshCamera.PreparedFrameSerial() !=
            input.preparedFrame.Serial() ||
        input.moduleBase != epoch.moduleBase ||
        input.moduleSize != kReachRetailImageSize ||
        input.playerWindowIndex != 0 ||
        input.cameraStackDepthBefore < 0 ||
        input.cameraStackDepthBefore >= 3 ||
        ClassifyReachOuterRenderCaller(
            input.moduleBase, input.moduleSize, input.returnAddress) !=
            ReachOuterRenderCaller::NormalPlayer)
    {
        return false;
    }

    uintptr_t expectedWorkspace = 0;
    uintptr_t expectedPlayerView = 0;
    return ReachAddressFromRva(input.moduleBase, input.moduleSize,
                               kReachDefaultWorkspaceRva,
                               expectedWorkspace) &&
        ReachAddressFromRva(input.moduleBase, input.moduleSize,
                            kReachPlayerViewArrayRva,
                            expectedPlayerView) &&
        input.workspace == expectedWorkspace &&
        input.playerView == expectedPlayerView;
}

// Fixed-storage TLS-ready gate for the exact outer normal owner. It consumes a
// current freshness capability tied to the same OpenXR prepared-frame serial;
// nested, stale, and replayed owners stay stock.
class ReachRenderOwnerGate
{
public:
    bool AdvanceEpoch(const ReachModuleEpoch& epoch) noexcept
    {
        if (m_token.m_active || ReachModuleEpochValid(m_epoch) ||
            !ReachModuleEpochValid(epoch) ||
            epoch.generation <= m_highestGeneration)
        {
            return false;
        }
        m_epoch = epoch;
        m_highestGeneration = epoch.generation;
        m_lastCompletedSerial = 0;
        return true;
    }

    bool TryBegin(
        const ReachOuterRenderInput& input,
        ReachRenderFreshnessGate& freshness) noexcept
    {
        if (m_token.m_active || !ReachNormalOuterInputMatches(input))
            return false;

        if (!ReachSameModuleEpoch(input.preflight.Epoch(), m_epoch) ||
            input.preparedFrame.Serial() <= m_lastCompletedSerial ||
            !freshness.Consume(
                input.freshCamera, input.preparedFrame, input.nowMs))
            return false;

        m_token.m_active = true;
        m_token.m_epoch = input.preflight.Epoch();
        m_token.m_preparedFrameSerial = input.preparedFrame.Serial();
        m_token.m_workspace = input.workspace;
        m_token.m_playerView = input.playerView;
        m_token.m_cameraStackDepthBefore = input.cameraStackDepthBefore;
        return true;
    }

    bool Finish(
        const ReachRenderOwnerToken& token,
        const ReachCleanupToken& cleanup) noexcept
    {
        return cleanup.Valid() &&
            cleanup.Disposition() == ReachCleanupDisposition::Completed &&
            ReachSameModuleEpoch(cleanup.Epoch(), token.Epoch()) &&
            cleanup.PreparedFrameSerial() == token.PreparedFrameSerial() &&
            Release(token);
    }

    bool Abort(
        const ReachRenderOwnerToken& token,
        const ReachCleanupToken& cleanup) noexcept
    {
        // A partially entered serial is consumed so it cannot be replayed
        // after fallback or rollback.
        return cleanup.Valid() &&
            cleanup.Disposition() == ReachCleanupDisposition::Aborted &&
            ReachSameModuleEpoch(cleanup.Epoch(), token.Epoch()) &&
            cleanup.PreparedFrameSerial() == token.PreparedFrameSerial() &&
            Release(token);
    }

    bool Teardown(const ReachModuleEpoch& epoch) noexcept
    {
        if (m_token.m_active || !ReachSameModuleEpoch(epoch, m_epoch))
            return false;
        m_epoch = {};
        m_lastCompletedSerial = 0;
        return true;
    }

    ReachRenderOwnerToken Token() const noexcept { return m_token; }
    uint64_t LastCompletedSerial() const noexcept
    {
        return m_lastCompletedSerial;
    }

    bool IsCurrent(const ReachRenderOwnerToken& token) const noexcept
    {
        return m_token.m_active && token.m_active &&
            ReachSameModuleEpoch(m_token.m_epoch, token.m_epoch) &&
            m_token.m_preparedFrameSerial ==
                token.m_preparedFrameSerial &&
            m_token.m_workspace == token.m_workspace &&
            m_token.m_playerView == token.m_playerView &&
            m_token.m_cameraStackDepthBefore ==
                token.m_cameraStackDepthBefore;
    }

private:
    bool Release(const ReachRenderOwnerToken& token) noexcept
    {
        if (!IsCurrent(token))
            return false;
        m_lastCompletedSerial = token.m_preparedFrameSerial;
        m_token = {};
        return true;
    }

    ReachRenderOwnerToken m_token{};
    ReachModuleEpoch m_epoch{};
    uint32_t m_highestGeneration = 0;
    uint64_t m_lastCompletedSerial = 0;
};

struct ReachInnerRenderInput
{
    uintptr_t returnAddress = 0;
    uintptr_t playerView = 0;
    uintptr_t activeView = 0;
    int32_t cameraStackDepth = -1;
    uintptr_t topWorkspace = 0;
    uintptr_t workspaceCallback = 0;
    uintptr_t renderCameraOwner = 0;
    uint32_t selectedSpecialization = 0;
    bool primaryCameraValid = false;
    bool secondaryCameraValid = false;
    ReachPreparedFrameToken preparedFrame{};
    ReachDirectCopyToken directCopy{};
    bool teardownRequested = false;
};

inline bool ReachInnerScopeMatches(
    const ReachRenderOwnerGate& owner,
    const ReachRenderOwnerToken& token,
    const ReachInnerRenderInput& input) noexcept
{
    if (!owner.IsCurrent(token) ||
        !ReachModuleEpochValid(token.Epoch()) || !token.Workspace() ||
        !token.PlayerView() || token.CameraStackDepthBefore() < 0 ||
        token.CameraStackDepthBefore() >= 3 ||
        !input.preparedFrame.Ready() || !input.directCopy.Ready() ||
        !ReachSameModuleEpoch(
            input.preparedFrame.Epoch(), token.Epoch()) ||
        !ReachSameModuleEpoch(input.directCopy.Epoch(), token.Epoch()) ||
        input.preparedFrame.Serial() != token.PreparedFrameSerial() ||
        input.directCopy.PreparedFrameSerial() !=
            token.PreparedFrameSerial() ||
        input.playerView != token.PlayerView() ||
        input.activeView != token.PlayerView() ||
        input.cameraStackDepth != token.CameraStackDepthBefore() + 1 ||
        input.cameraStackDepth < 0 || input.cameraStackDepth > 3 ||
        input.topWorkspace != token.Workspace() ||
        input.selectedSpecialization != 0 ||
        !input.primaryCameraValid || !input.secondaryCameraValid ||
        input.teardownRequested)
    {
        return false;
    }

    uintptr_t expectedCallback = 0;
    uintptr_t expectedReturn = 0;
    if (!ReachAddressFromRva(token.Epoch().moduleBase, kReachRetailImageSize,
                            kReachCameraStackCallbackRva,
                            expectedCallback) ||
        !ReachAddressFromRva(token.Epoch().moduleBase, kReachRetailImageSize,
                            kReachPlayerViewRenderReturnRva,
                            expectedReturn) ||
        input.returnAddress != expectedReturn ||
        input.workspaceCallback != expectedCallback ||
        token.PlayerView() > std::numeric_limits<uintptr_t>::max() -
            kReachPlayerViewCameraStateOffset)
    {
        return false;
    }

    return input.renderCameraOwner ==
        token.PlayerView() + kReachPlayerViewCameraStateOffset;
}

enum class ReachRenderAction : uint8_t
{
    StockOnce = 0,
    StereoTransaction,
};

inline ReachRenderAction SelectReachRenderAction(
    bool runtimeHooksPermitted, const ReachPreflightToken& preflight,
    const ReachRenderOwnerGate& owner,
    const ReachRenderOwnerToken& token,
    const ReachInnerRenderInput& input) noexcept
{
    return runtimeHooksPermitted && preflight.Complete() &&
            owner.IsCurrent(token) &&
            ReachSameModuleEpoch(preflight.Epoch(), token.Epoch()) &&
            ReachInnerScopeMatches(owner, token, input)
        ? ReachRenderAction::StereoTransaction
        : ReachRenderAction::StockOnce;
}

inline int ReachEyeForPass(uint32_t pass, bool rightEyeFirst) noexcept
{
    if (pass > 1)
        return -1;
    const int firstEye = rightEyeFirst ? 1 : 0;
    return pass == 0 ? firstEye : 1 - firstEye;
}

struct ReachStereoPassPolicy
{
    bool valid = false;
    int eye = -1;
    bool writeLastWindow = false;
    uint8_t lastWindowInput = 0;
    bool restoreLastWindowAfterPass = true;
};

inline ReachStereoPassPolicy SelectReachStereoPassPolicy(
    uint32_t pass, bool rightEyeFirst, uint8_t originalStockValue) noexcept
{
    const int eye = ReachEyeForPass(pass, rightEyeFirst);
    if (eye < 0)
    {
        // Invalid pass indices authorize no write and require any pending
        // first-pass mutation to be rolled back.
        return {false, -1, false, originalStockValue, true};
    }

    // The first eye is an inserted pass and restores the stock byte for the
    // final eye. The final eye's actual post-call byte must persist.
    return {
        true, eye, true, pass == 0 ? uint8_t{0} : originalStockValue,
        pass == 0
    };
}

struct ReachRollbackLayout
{
    size_t workspaceSize;
    uintptr_t cameraStateOffset;
    size_t cameraStateSize;
    uintptr_t currentMatricesOffset;
    size_t currentMatricesSize;
    uintptr_t previousMatricesOffset;
    size_t previousMatricesSize;
    uintptr_t excludedLastWindowOffset;
};

inline constexpr ReachRollbackLayout kReachRollbackLayout{
    kReachRenderScopeSnapshotSize,
    kReachPlayerViewCameraStateOffset,
    kReachPlayerViewCameraStateSize,
    kReachPlayerViewCurrentMatricesOffset,
    kReachPlayerViewMatrixBlockSize,
    kReachPlayerViewPreviousMatricesOffset,
    kReachPlayerViewMatrixBlockSize,
    kReachLastWindowFlagOffset,
};

inline constexpr bool ReachSpanFits(
    uintptr_t offset, size_t size, size_t limit) noexcept
{
    return offset <= limit && size <= limit - offset;
}

static_assert(kReachRenderScopeSnapshotSize == 0x02B0);
static_assert(ReachSpanFits(
    kReachPlayerViewCameraStateOffset, kReachPlayerViewCameraStateSize,
    kReachPlayerViewStride));
static_assert(ReachSpanFits(
    kReachPlayerViewCurrentMatricesOffset, kReachPlayerViewMatrixBlockSize,
    kReachPlayerViewStride));
static_assert(ReachSpanFits(
    kReachPlayerViewPreviousMatricesOffset, kReachPlayerViewMatrixBlockSize,
    kReachPlayerViewStride));
static_assert(
    kReachPlayerViewCameraStateOffset + kReachPlayerViewCameraStateSize <=
    kReachPlayerViewCurrentMatricesOffset);
static_assert(
    kReachPlayerViewCurrentMatricesOffset + kReachPlayerViewMatrixBlockSize <=
    kReachPlayerViewPreviousMatricesOffset);
static_assert(
    kReachPlayerViewPreviousMatricesOffset + kReachPlayerViewMatrixBlockSize <=
    kReachLastWindowFlagOffset);
static_assert(kReachLastWindowFlagOffset < kReachPlayerViewStride);

// Pure cleanup state for a future detour. It tracks both passes as dirty until
// an explicit restore transition and is the only producer of the exact cleanup
// capability required to finish or abort the live owner.
class ReachRollbackGate
{
public:
    bool Bind(
        const ReachRenderOwnerGate& owner,
        const ReachRenderOwnerToken& token) noexcept
    {
        if (m_phase != Phase::Inactive || !owner.IsCurrent(token))
            return false;
        m_epoch = token.Epoch();
        m_preparedFrameSerial = token.PreparedFrameSerial();
        m_phase = Phase::ReadyClean;
        return true;
    }

    bool BeginFirstPass(const ReachRenderOwnerToken& token) noexcept
    {
        if (!Matches(token) || m_phase != Phase::ReadyClean)
            return false;
        m_phase = Phase::FirstDirty;
        return true;
    }

    bool MarkFirstPassRestored(
        const ReachRenderOwnerToken& token) noexcept
    {
        if (!Matches(token) || m_phase != Phase::FirstDirty)
            return false;
        m_phase = Phase::BetweenPassesClean;
        return true;
    }

    bool BeginFinalPass(const ReachRenderOwnerToken& token) noexcept
    {
        if (!Matches(token) || m_phase != Phase::BetweenPassesClean)
            return false;
        m_phase = Phase::FinalDirty;
        return true;
    }

    ReachCleanupToken MarkFinalPassRestoredAndComplete(
        const ReachRenderOwnerToken& token) noexcept
    {
        if (!Matches(token) || m_phase != Phase::FinalDirty)
            return {};
        m_phase = Phase::CompletedClean;
        return ReachCleanupToken(
            m_epoch, m_preparedFrameSerial,
            ReachCleanupDisposition::Completed);
    }

    bool MarkDirtyPassRestoredForAbort(
        const ReachRenderOwnerToken& token) noexcept
    {
        if (!Matches(token) ||
            (m_phase != Phase::FirstDirty &&
             m_phase != Phase::FinalDirty))
        {
            return false;
        }
        m_phase = Phase::AbortReadyClean;
        return true;
    }

    ReachCleanupToken AbortClean(
        const ReachRenderOwnerToken& token) noexcept
    {
        if (!Matches(token) ||
            (m_phase != Phase::ReadyClean &&
             m_phase != Phase::BetweenPassesClean &&
             m_phase != Phase::AbortReadyClean))
        {
            return {};
        }
        m_phase = Phase::AbortedClean;
        return ReachCleanupToken(
            m_epoch, m_preparedFrameSerial,
            ReachCleanupDisposition::Aborted);
    }

    bool NeedsRollback() const noexcept
    {
        return m_phase == Phase::FirstDirty ||
            m_phase == Phase::FinalDirty;
    }
    bool Finished() const noexcept
    {
        return m_phase == Phase::CompletedClean;
    }
    bool Aborted() const noexcept
    {
        return m_phase == Phase::AbortedClean;
    }

private:
    enum class Phase : uint8_t
    {
        Inactive = 0,
        ReadyClean,
        FirstDirty,
        BetweenPassesClean,
        FinalDirty,
        AbortReadyClean,
        CompletedClean,
        AbortedClean,
    };

    bool Matches(const ReachRenderOwnerToken& token) const noexcept
    {
        return m_phase != Phase::Inactive && token.Active() &&
            ReachSameModuleEpoch(m_epoch, token.Epoch()) &&
            m_preparedFrameSerial == token.PreparedFrameSerial();
    }

    ReachModuleEpoch m_epoch{};
    uint64_t m_preparedFrameSerial = 0;
    Phase m_phase = Phase::Inactive;
};
