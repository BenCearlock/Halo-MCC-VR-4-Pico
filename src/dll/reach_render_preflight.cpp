#include "reach_render_preflight.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
    constexpr size_t kMaxPeSections = 96;

    struct ExecutableRange
    {
        uintptr_t address = 0;
        uintptr_t rva = 0;
        size_t size = 0;
    };

    struct LoadedPe
    {
        std::array<ExecutableRange, kMaxPeSections> executable{};
        size_t executableCount = 0;
    };

    struct ExpectedCall
    {
        uintptr_t siteRva;
        uintptr_t targetRva;
    };

    constexpr ExpectedCall kExpectedCalls[] = {
        {kReachNormalSetupCallRva, kReachNormalSetupTargetRva},
        {kReachNormalOuterCallRva, kReachMainRenderViewRva},
        {kReachScreenshotOuterCallRva, kReachMainRenderViewRva},
        {kReachPlayerViewRenderCallerRva, kReachPlayerViewRenderRva},
        {kReachOuterMainRenderCallRva, kReachOuterMainRenderTargetRva},
        {kReachOuterPresentCallRva, kReachOuterPresentTargetRva},
    };

    struct FixedRange
    {
        uintptr_t rva;
        size_t size;
    };

    constexpr FixedRange kFixedRanges[] = {
        {kReachPlayerViewArrayRva,
         kReachPlayerViewCount * kReachPlayerViewStride},
        {kReachDefaultWorkspaceRva, kReachRenderScopeSnapshotSize},
        {kReachActiveViewRva, sizeof(uintptr_t)},
        {kReachCameraStackDepthRva, sizeof(int32_t)},
        {kReachCameraStackPointersRva, 4 * sizeof(uintptr_t)},
        {kReachRenderCameraOwnerRva, sizeof(uintptr_t)},
        {kReachSelectedSpecializationRva, sizeof(uint32_t)},
        {kReachDisplaySwapchainRva, sizeof(uintptr_t)},
        {kReachDisplayGroupRva,
         kReachDisplaySurfaceArrayOffset + sizeof(uintptr_t)},
        {kReachDisplaySelectedRtvRva, sizeof(uintptr_t)},
    };

    bool IsReadableProtection(DWORD protection) noexcept
    {
        if (protection & (PAGE_GUARD | PAGE_NOACCESS))
            return false;
        switch (protection & 0xFFu)
        {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
        }
    }

    bool IsExecutableProtection(DWORD protection) noexcept
    {
        if (protection & (PAGE_GUARD | PAGE_NOACCESS))
            return false;
        switch (protection & 0xFFu)
        {
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
        }
    }

    bool IsMappedImageRange(
        uintptr_t moduleBase, uintptr_t address, size_t size,
        bool executable) noexcept
    {
        if (!moduleBase || !address || !size ||
            size > std::numeric_limits<uintptr_t>::max() - address)
            return false;
        const uintptr_t end = address + size;
        uintptr_t cursor = address;
        while (cursor < end)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(
                    reinterpret_cast<const void*>(cursor), &info,
                    sizeof(info)) != sizeof(info))
                return false;
            const uintptr_t regionBase =
                reinterpret_cast<uintptr_t>(info.BaseAddress);
            if (!info.RegionSize ||
                info.RegionSize >
                    std::numeric_limits<uintptr_t>::max() - regionBase)
                return false;
            const uintptr_t regionEnd = regionBase + info.RegionSize;
            if (cursor < regionBase || cursor >= regionEnd ||
                info.State != MEM_COMMIT || info.Type != MEM_IMAGE ||
                reinterpret_cast<uintptr_t>(info.AllocationBase) !=
                    moduleBase ||
                !IsReadableProtection(info.Protect) ||
                (executable && !IsExecutableProtection(info.Protect)))
                return false;
            cursor = std::min(end, regionEnd);
        }
        return true;
    }

    bool BoundedRva(
        uintptr_t rva, size_t span, size_t imageSize) noexcept
    {
        return rva < imageSize && span && span <= imageSize - rva;
    }

    bool ParseLoadedPe(
        uintptr_t base, size_t size, LoadedPe& image) noexcept
    {
        if (size != kReachRetailImageSize ||
            !IsMappedImageRange(base, base, sizeof(IMAGE_DOS_HEADER), false))
            return false;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0)
            return false;
        const size_t ntOffset = static_cast<size_t>(dos->e_lfanew);
        if (ntOffset > size || sizeof(IMAGE_NT_HEADERS64) > size - ntOffset ||
            !IsMappedImageRange(
                base, base + ntOffset, sizeof(IMAGE_NT_HEADERS64), false))
            return false;
        const auto* nt =
            reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + ntOffset);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->FileHeader.TimeDateStamp != kReachRetailPeTimestamp ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt->OptionalHeader.SizeOfImage != kReachRetailImageSize ||
            nt->FileHeader.NumberOfSections == 0 ||
            nt->FileHeader.NumberOfSections > kMaxPeSections ||
            nt->FileHeader.SizeOfOptionalHeader <
                sizeof(IMAGE_OPTIONAL_HEADER64))
            return false;

        const size_t sectionOffset = ntOffset +
            offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
            nt->FileHeader.SizeOfOptionalHeader;
        const size_t sectionBytes =
            static_cast<size_t>(nt->FileHeader.NumberOfSections) *
            sizeof(IMAGE_SECTION_HEADER);
        if (sectionOffset > size || sectionBytes > size - sectionOffset ||
            !IsMappedImageRange(
                base, base + sectionOffset, sectionBytes, false))
            return false;

        const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(
            base + sectionOffset);
        struct SectionSpan { uintptr_t begin; uintptr_t end; };
        std::array<SectionSpan, kMaxPeSections> spans{};
        size_t spanCount = 0;
        for (uint16_t index = 0;
             index < nt->FileHeader.NumberOfSections; ++index)
        {
            const uintptr_t rva = sections[index].VirtualAddress;
            const size_t virtualSize = sections[index].Misc.VirtualSize;
            if (!virtualSize)
                continue;
            if (!BoundedRva(rva, virtualSize, size))
                return false;
            const uintptr_t end = rva + virtualSize;
            for (size_t prior = 0; prior < spanCount; ++prior)
            {
                if (rva < spans[prior].end &&
                    spans[prior].begin < end)
                    return false;
            }
            spans[spanCount++] = {rva, end};

            if ((sections[index].Characteristics &
                 IMAGE_SCN_MEM_EXECUTE) == 0)
                continue;
            if (image.executableCount == image.executable.size() ||
                !IsMappedImageRange(
                    base, base + rva, virtualSize, true))
                return false;
            image.executable[image.executableCount++] = {
                base + rva, rva, virtualSize};
        }
        return image.executableCount != 0;
    }

    bool ExecutableContains(
        const LoadedPe& image, uintptr_t rva, size_t size) noexcept
    {
        for (size_t index = 0; index < image.executableCount; ++index)
        {
            const auto& range = image.executable[index];
            if (rva >= range.rva && rva - range.rva <= range.size &&
                size <= range.size - (rva - range.rva))
                return true;
        }
        return false;
    }

    struct PatternResult
    {
        uint32_t count = 0;
        uintptr_t first = 0;
    };

    PatternResult ScanExecutable(
        const LoadedPe& image, const uint8_t* pattern,
        const uint8_t* mask, size_t patternSize) noexcept
    {
        PatternResult result{};
        for (size_t section = 0;
             section < image.executableCount; ++section)
        {
            const auto& range = image.executable[section];
            const auto* bytes =
                reinterpret_cast<const uint8_t*>(range.address);
            if (range.size < patternSize)
                continue;
            for (size_t offset = 0;
                 offset <= range.size - patternSize; ++offset)
            {
                size_t index = 0;
                for (; index < patternSize; ++index)
                {
                    if (mask[index] &&
                        bytes[offset + index] != pattern[index])
                        break;
                }
                if (index != patternSize)
                    continue;
                if (!result.count)
                    result.first = range.address + offset;
                if (result.count !=
                    std::numeric_limits<uint32_t>::max())
                    ++result.count;
            }
        }
        return result;
    }

    class Sha256
    {
    public:
        ~Sha256()
        {
            if (m_hash)
                BCryptDestroyHash(m_hash);
            if (m_algorithm)
                BCryptCloseAlgorithmProvider(m_algorithm, 0);
        }

        bool Initialize()
        {
            if (BCryptOpenAlgorithmProvider(
                    &m_algorithm, BCRYPT_SHA256_ALGORITHM,
                    nullptr, 0) < 0)
                return false;
            DWORD objectSize = 0;
            DWORD received = 0;
            if (BCryptGetProperty(
                    m_algorithm, BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&objectSize),
                    sizeof(objectSize), &received, 0) < 0 ||
                received != sizeof(objectSize) || !objectSize)
                return false;
            m_object.resize(objectSize);
            return BCryptCreateHash(
                       m_algorithm, &m_hash, m_object.data(),
                       static_cast<ULONG>(m_object.size()),
                       nullptr, 0, 0) >= 0;
        }

        bool Add(const void* bytes, size_t size)
        {
            const auto* cursor = static_cast<const uint8_t*>(bytes);
            while (size)
            {
                const ULONG part = static_cast<ULONG>(
                    std::min<size_t>(size, 1024 * 1024));
                if (BCryptHashData(
                        m_hash, const_cast<PUCHAR>(cursor), part, 0) < 0)
                    return false;
                cursor += part;
                size -= part;
            }
            return true;
        }

        bool Finish(std::array<uint8_t, 32>& output)
        {
            return BCryptFinishHash(
                       m_hash, output.data(),
                       static_cast<ULONG>(output.size()), 0) >= 0;
        }

    private:
        BCRYPT_ALG_HANDLE m_algorithm = nullptr;
        BCRYPT_HASH_HANDLE m_hash = nullptr;
        std::vector<uint8_t> m_object;
    };

    std::string Hex(const std::array<uint8_t, 32>& bytes)
    {
        constexpr char digits[] = "0123456789ABCDEF";
        std::string result(bytes.size() * 2, '0');
        for (size_t index = 0; index < bytes.size(); ++index)
        {
            result[index * 2] = digits[bytes[index] >> 4];
            result[index * 2 + 1] = digits[bytes[index] & 0x0F];
        }
        return result;
    }

    bool HashBytes(
        const void* bytes, size_t size,
        const char* expected)
    {
        Sha256 hash;
        std::array<uint8_t, 32> digest{};
        return hash.Initialize() && hash.Add(bytes, size) &&
            hash.Finish(digest) && Hex(digest) == expected;
    }

    bool HashBackingFile(HMODULE module)
    {
        std::array<wchar_t, 32768> path{};
        const DWORD length = GetModuleFileNameW(
            module, path.data(), static_cast<DWORD>(path.size()));
        if (!length || length >= path.size())
            return false;
        HANDLE file = CreateFileW(
            path.data(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        Sha256 hash;
        bool ok = hash.Initialize();
        std::array<uint8_t, 64 * 1024> buffer{};
        while (ok)
        {
            DWORD received = 0;
            if (!ReadFile(
                    file, buffer.data(),
                    static_cast<DWORD>(buffer.size()), &received, nullptr))
            {
                ok = false;
                break;
            }
            if (!received)
                break;
            ok = hash.Add(buffer.data(), received);
        }
        CloseHandle(file);
        std::array<uint8_t, 32> digest{};
        return ok && hash.Finish(digest) &&
            Hex(digest) == kReachRetailModuleSha256;
    }

    bool CheckCall(
        uintptr_t base, size_t imageSize,
        const ExpectedCall& expected) noexcept
    {
        if (!BoundedRva(expected.siteRva, 5, imageSize))
            return false;
        const auto* call = reinterpret_cast<const uint8_t*>(
            base + expected.siteRva);
        const uintptr_t instruction = base + expected.siteRva;
        if (call[0] != 0xE8 || instruction >
                std::numeric_limits<uintptr_t>::max() - 5)
            return false;
        int32_t displacement = 0;
        std::memcpy(&displacement, call + 1, sizeof(displacement));
        const uintptr_t next = instruction + 5;
        uintptr_t actualTarget = 0;
        if (displacement >= 0)
        {
            const uintptr_t positive =
                static_cast<uint32_t>(displacement);
            if (positive >
                std::numeric_limits<uintptr_t>::max() - next)
                return false;
            actualTarget = next + positive;
        }
        else
        {
            const uintptr_t magnitude = static_cast<uintptr_t>(
                -(static_cast<int64_t>(displacement)));
            if (magnitude > next)
                return false;
            actualTarget = next - magnitude;
        }
        return actualTarget == base + expected.targetRva;
    }

    bool CheckFixedRanges(uintptr_t base, size_t imageSize) noexcept
    {
        for (const auto& range : kFixedRanges)
        {
            if (!BoundedRva(range.rva, range.size, imageSize) ||
                !IsMappedImageRange(
                    base, base + range.rva, range.size, false))
                return false;
        }
        return true;
    }
}

ReachLoadedImageModulePin::~ReachLoadedImageModulePin()
{
    Reset();
}

bool ReachLoadedImageModulePin::Valid() const noexcept
{
    return m_module != nullptr;
}

bool ReachLoadedImageModulePin::IsCurrent(
    uintptr_t expectedBase) const noexcept
{
    return m_module && expectedBase &&
        reinterpret_cast<uintptr_t>(m_module) == expectedBase &&
        GetModuleHandleW(L"haloreach.dll") ==
            reinterpret_cast<HMODULE>(m_module);
}

bool ReachLoadedImageModulePin::Acquire(
    uintptr_t expectedBase) noexcept
{
    Reset();
    HMODULE module = nullptr;
    if (!expectedBase ||
        !GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(expectedBase), &module) ||
        reinterpret_cast<uintptr_t>(module) != expectedBase)
    {
        if (module)
            FreeLibrary(module);
        return false;
    }
    m_module = module;
    return true;
}

void ReachLoadedImageModulePin::Reset() noexcept
{
    if (m_module)
    {
        FreeLibrary(reinterpret_cast<HMODULE>(m_module));
        m_module = nullptr;
    }
}

const char* ReachRender_LoadedImageFailureName(
    ReachLoadedImageFailure failure) noexcept
{
    switch (failure)
    {
    case ReachLoadedImageFailure::None: return "none";
    case ReachLoadedImageFailure::InvalidInput: return "invalid-input";
    case ReachLoadedImageFailure::ModuleReference: return "module-reference";
    case ReachLoadedImageFailure::BackingFileIdentity: return "backing-file-identity";
    case ReachLoadedImageFailure::PeIdentity: return "pe-identity";
    case ReachLoadedImageFailure::ExecutableSections: return "executable-sections";
    case ReachLoadedImageFailure::SignatureIdentity: return "signature-identity";
    case ReachLoadedImageFailure::BodyIdentity: return "body-identity";
    case ReachLoadedImageFailure::CallerEdges: return "caller-edges";
    case ReachLoadedImageFailure::FixedRanges: return "fixed-ranges";
    case ReachLoadedImageFailure::MappingChanged: return "mapping-changed";
    case ReachLoadedImageFailure::Publication: return "publication";
    default: return "unknown";
    }
}

bool ReachRender_RunLoadedImagePreflight(
    uintptr_t moduleBase, size_t moduleSize,
    ReachLoadedImagePreflight& result,
    ReachLoadedImageModulePin& pin)
{
    result = {};
    pin.Reset();
    result.failure = ReachLoadedImageFailure::InvalidInput;
    if (!moduleBase || moduleSize != kReachRetailImageSize)
        return false;

    if (!pin.Acquire(moduleBase))
    {
        result.failure = ReachLoadedImageFailure::ModuleReference;
        return false;
    }
    if (!HashBackingFile(reinterpret_cast<HMODULE>(pin.m_module)))
    {
        result.failure = ReachLoadedImageFailure::BackingFileIdentity;
        return false;
    }

    LoadedPe image{};
    if (!ParseLoadedPe(moduleBase, moduleSize, image))
    {
        result.failure = ReachLoadedImageFailure::PeIdentity;
        return false;
    }
    result.proof.retailIdentity = true;

    if (!ExecutableContains(
            image, kReachMainRenderViewRva,
            kReachMainRenderViewBodySize) ||
        !ExecutableContains(
            image, kReachPlayerViewRenderRva,
            kReachPlayerViewRenderBodySize) ||
        !ExecutableContains(
            image, kReachFrustumHelperRva,
            kReachFrustumHelperAob.size()))
    {
        result.failure = ReachLoadedImageFailure::ExecutableSections;
        return false;
    }
    result.proof.frustumHelperExecutableRange = true;

    std::array<uint8_t, kReachMainRenderViewAob.size()> exactMask{};
    exactMask.fill(0xFF);
    std::array<uint8_t, kReachFrustumHelperAob.size()> frustumMask{};
    frustumMask.fill(0xFF);
    const PatternResult main = ScanExecutable(
        image, kReachMainRenderViewAob.data(), exactMask.data(),
        kReachMainRenderViewAob.size());
    const PatternResult player = ScanExecutable(
        image, kReachPlayerViewRenderAob.data(),
        kReachPlayerViewRenderAobMask.data(),
        kReachPlayerViewRenderAob.size());
    const PatternResult frustum = ScanExecutable(
        image, kReachFrustumHelperAob.data(), frustumMask.data(),
        kReachFrustumHelperAob.size());
    result.proof.mainRenderViewMatchCount = main.count;
    result.proof.mainRenderViewAtExpectedRva =
        main.first == moduleBase + kReachMainRenderViewRva;
    result.proof.playerViewRenderMatchCount = player.count;
    result.proof.playerViewRenderAtExpectedRva =
        player.first == moduleBase + kReachPlayerViewRenderRva;
    result.proof.frustumHelperMatchCount = frustum.count;
    result.proof.frustumHelperAtExpectedRva =
        frustum.first == moduleBase + kReachFrustumHelperRva;
    if (main.count != 1 || player.count != 1 || frustum.count != 1 ||
        !result.proof.mainRenderViewAtExpectedRva ||
        !result.proof.playerViewRenderAtExpectedRva ||
        !result.proof.frustumHelperAtExpectedRva)
    {
        result.failure = ReachLoadedImageFailure::SignatureIdentity;
        return false;
    }

    result.proof.mainRenderViewBodyHash = HashBytes(
        reinterpret_cast<const void*>(
            moduleBase + kReachMainRenderViewRva),
        kReachMainRenderViewBodySize,
        kReachMainRenderViewBodySha256);
    result.proof.playerViewRenderBodyHash = HashBytes(
        reinterpret_cast<const void*>(
            moduleBase + kReachPlayerViewRenderRva),
        kReachPlayerViewRenderBodySize,
        kReachPlayerViewRenderBodySha256);
    if (!result.proof.mainRenderViewBodyHash ||
        !result.proof.playerViewRenderBodyHash)
    {
        result.failure = ReachLoadedImageFailure::BodyIdentity;
        return false;
    }

    for (const auto& expected : kExpectedCalls)
    {
        if (!ExecutableContains(image, expected.siteRva, 5) ||
            !CheckCall(moduleBase, moduleSize, expected))
        {
            result.failure = ReachLoadedImageFailure::CallerEdges;
            return false;
        }
    }
    result.proof.exactOuterCallerEdges = true;
    result.proof.exactInnerCallerEdge = true;

    result.proof.fixedDataRanges =
        CheckFixedRanges(moduleBase, moduleSize);
    if (!result.proof.fixedDataRanges)
    {
        result.failure = ReachLoadedImageFailure::FixedRanges;
        return false;
    }

    if (!pin.IsCurrent(moduleBase))
    {
        result.failure = ReachLoadedImageFailure::MappingChanged;
        return false;
    }
    if (!ReachRenderCandidateProofComplete(result.proof))
    {
        result.failure = ReachLoadedImageFailure::SignatureIdentity;
        return false;
    }
    result.failure = ReachLoadedImageFailure::None;
    return true;
}
