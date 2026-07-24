#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Reach-only render evidence and allocation-free policy. This file contains no
// Windows, COM, MinHook, logging, or engine writes, so the exact routing and
// rollback rules can be exhaustively tested before any detour is authorized.

inline constexpr size_t kReachRetailImageSize = 0x04EDA000;
inline constexpr uint32_t kReachRetailPeTimestamp = 0x68A0EFE1;
inline constexpr char kReachRetailModuleSha256[] =
    "738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894";
inline constexpr uintptr_t kReachMainRenderViewRva = 0x000C31F4;
inline constexpr uintptr_t kReachNormalSetupCallRva = 0x000C36D6;
inline constexpr uintptr_t kReachNormalSetupTargetRva = 0x0026C204;
inline constexpr uintptr_t kReachNormalOuterCallRva = 0x000C3730;
inline constexpr uintptr_t kReachNormalOuterReturnRva = 0x000C3735;
inline constexpr uintptr_t kReachScreenshotOuterCallRva = 0x001D3864;
inline constexpr uintptr_t kReachScreenshotOuterReturnRva = 0x001D3869;
inline constexpr uintptr_t kReachPlayerViewRenderRva = 0x0026C6DC;
inline constexpr uintptr_t kReachPlayerViewRenderCallerRva = 0x000C33C4;
inline constexpr uintptr_t kReachPlayerViewRenderReturnRva = 0x000C33C9;
inline constexpr uintptr_t kReachOuterMainRenderCallRva = 0x000C2FAA;
inline constexpr uintptr_t kReachOuterMainRenderTargetRva = 0x000C33F8;
inline constexpr uintptr_t kReachOuterPresentCallRva = 0x000C3000;
inline constexpr uintptr_t kReachOuterPresentTargetRva = 0x0025113C;
inline constexpr uintptr_t kReachFrustumHelperRva = 0x00287F58;
// The remaining three stock camera-rebuild helpers, proven in
// REACH-SIGNATURE-EVIDENCE.md ("Inner stereo candidate and coherent rebuild
// constraints", steps 2-6). The projection builder is the direct call that
// follows the frustum helper at every one of its nine call sites; the
// camera-state updater and projection/matrix builder complete the stock
// pre-scope rebuild that writes player_view+0x3B0 and +0x490.
inline constexpr uintptr_t kReachProjectionBuilderRva = 0x002884BC;
inline constexpr uintptr_t kReachCameraStateUpdaterRva = 0x00286F9C;
inline constexpr uintptr_t kReachProjectionMatrixBuilderRva = 0x0028AF8C;
// Proven setup call sites (inside setup 0x26C204) that anchor the first two
// helpers by their exact rel32 target, so a mismatched image fails open.
inline constexpr uintptr_t kReachSetupFrustumCallRva = 0x0026C2FF;
inline constexpr uintptr_t kReachSetupProjectionCallRva = 0x0026C316;
inline constexpr uintptr_t kReachPlayerViewArrayRva = 0x029F2B90;
inline constexpr size_t kReachPlayerViewStride = 0x0A40;
inline constexpr uint32_t kReachPlayerViewCount = 4;
inline constexpr uintptr_t kReachDefaultWorkspaceRva = 0x00C9FAE0;
inline constexpr size_t kReachRenderScopeSnapshotSize = 0x02B0;
inline constexpr uintptr_t kReachCameraStackCallbackRva = 0x0026BFD4;
inline constexpr uintptr_t kReachActiveViewRva = 0x04E389A8;
inline constexpr uintptr_t kReachCameraStackDepthRva = 0x00B43ABC;
inline constexpr uintptr_t kReachCameraStackPointersRva = 0x00C878A8;
inline constexpr uintptr_t kReachRenderCameraOwnerRva = 0x04E38A90;
inline constexpr uintptr_t kReachSelectedSpecializationRva = 0x04E38A08;
inline constexpr uintptr_t kReachDisplaySwapchainRva = 0x04E38868;
inline constexpr uintptr_t kReachDisplayGroupRva = 0x00C8E520;
inline constexpr uintptr_t kReachDisplaySelectedRtvRva = 0x00CA02E0;
inline constexpr uintptr_t kReachDisplaySurfaceCountOffset = 0x58;
inline constexpr uintptr_t kReachDisplaySurfaceArrayOffset = 0x60;
inline constexpr size_t kReachDisplaySurfaceRecordSize = 0x88;
inline constexpr uintptr_t kReachDisplaySurfaceRtvOffset = 0x08;
inline constexpr uintptr_t kReachDisplaySurfaceSrvOffset = 0x18;
inline constexpr uint32_t kReachDisplaySurfaceCount = 4;
inline constexpr uint32_t kReachDisplayFormatR8G8B8A8Unorm = 28;
// Reach-native type-6 float debug variables. The pinned retail table contains
// one exact entry for each name; HREK independently corroborates the same two
// controls and authored values (docs/REACH-SIGNATURE-EVIDENCE.md). Production
// still resolves by name, then requires these exact value slots before writing.
inline constexpr uintptr_t kReachMotionBlurMaxEntryRva = 0x00B3A1C8;
inline constexpr uintptr_t kReachMotionBlurScaleEntryRva = 0x00B3A1E0;
inline constexpr uintptr_t kReachMotionBlurMaxValueRva = 0x00B44600;
inline constexpr uintptr_t kReachMotionBlurScaleValueRva = 0x00B44604;
// Retail apply_distortions divides the maximum by the scale at both sites.
// The scale must therefore remain positive even when the maximum is zeroed.
inline constexpr uintptr_t kReachMotionBlurMaxOverScaleDivideRva = 0x00287561;
inline constexpr uintptr_t kReachMotionBlurScaledMaxDivideRva = 0x002875AD;
inline constexpr float kReachMotionBlurMinimumUsableScale = 1.0e-6f;
// Reach's screen-aligned patchy-fog renderer is independently gated inside
// player_view_render. Retail tests bit 0x08 and skips the helper when it is set.
// HREK names the matching resources `_surface_patchy_fog_buffer0/1` and the
// matching parameter block `Patchy Fog Global Parameters`. The exact retail
// player_view body hash pins the test/jump bytes; the preflight additionally
// proves the helper call edge and mapped flag byte before publication.
inline constexpr uintptr_t kReachPatchyFogGateTestRva = 0x0026CC59;
inline constexpr uintptr_t kReachPatchyFogSkipJumpRva = 0x0026CC60;
inline constexpr uintptr_t kReachPatchyFogCallRva = 0x0026CC65;
inline constexpr uintptr_t kReachPatchyFogTargetRva = 0x0026EFEC;
inline constexpr uintptr_t kReachPatchyFogFlagsRva = 0x00CA0240;
inline constexpr uint8_t kReachPatchyFogSkipMask = 0x08;

inline constexpr uint8_t ReachPatchyFogSuppressedFlags(
    uint8_t flags) noexcept
{
    return static_cast<uint8_t>(flags | kReachPatchyFogSkipMask);
}

inline constexpr uint8_t ReachPatchyFogRestoredFlags(
    uint8_t current, uint8_t original) noexcept
{
    return static_cast<uint8_t>(
        (current & static_cast<uint8_t>(~kReachPatchyFogSkipMask)) |
        (original & kReachPatchyFogSkipMask));
}
inline constexpr uintptr_t kReachPlayerViewCameraStateOffset = 0x03B0;
inline constexpr uintptr_t kReachPlayerViewCurrentMatricesOffset = 0x0490;
inline constexpr uintptr_t kReachPlayerViewPreviousMatricesOffset = 0x0760;
inline constexpr uintptr_t kReachLastWindowFlagOffset = 0x0A30;
inline constexpr size_t kReachPlayerViewCameraStateSize = 0x00C8;
inline constexpr size_t kReachPlayerViewMatrixBlockSize = 0x02D0;
// Zeroed projection-offset pair inside the camera-state envelope; passed as the
// projection/matrix builder's fifth argument.
inline constexpr uintptr_t kReachPlayerViewProjectionOffsetPairOffset = 0x0470;
// Rasterizer-workspace sub-block layout (REACH-SIGNATURE-EVIDENCE.md, the 0x2B0
// render-scope snapshot table): primary compact +0x000/0x90, primary derived
// +0x090/0xC4, secondary compact +0x154/0x90, secondary derived +0x1E4/0xC4.
inline constexpr uintptr_t kReachCompactCameraSize = 0x0090;
inline constexpr uintptr_t kReachPrimaryDerivedOffset = 0x0090;
inline constexpr size_t kReachDerivedBlockSize = 0x00C4;
inline constexpr uintptr_t kReachSecondaryCompactOffset = 0x0154;
inline constexpr uintptr_t kReachSecondaryDerivedOffset = 0x01E4;
// The four camera blocks end immediately before the engine-owned camera-stack
// callback at +0x2A8. This is the exact bounded unit an outer camera owner may
// snapshot without including that callback.
inline constexpr size_t kReachCameraPairDataSize = 0x02A8;
static_assert(
    kReachSecondaryDerivedOffset + kReachDerivedBlockSize ==
    kReachCameraPairDataSize);
// Exact retail evidence for the pre-inner visibility consumer. The first call
// resolves the camera cluster from workspace+0x154; the second builds the
// visibility collection from workspace+0x154 and workspace+0x1E4.
inline constexpr uintptr_t kReachVisibilityClusterLookupCallRva = 0x000C3320;
inline constexpr uintptr_t kReachVisibilityClusterLookupTargetRva = 0x00273458;
inline constexpr uintptr_t kReachVisibilitySecondaryCompactLeaRva = 0x00273468;
inline constexpr uintptr_t kReachVisibilityBuildCallRva = 0x000C335C;
inline constexpr uintptr_t kReachVisibilityBuildTargetRva = 0x0027F408;
inline constexpr uintptr_t kReachVisibilitySecondaryDerivedLeaRva = 0x000C3339;
inline constexpr uintptr_t kReachVisibilitySecondaryCompactAddressRva =
    kReachDefaultWorkspaceRva + kReachSecondaryCompactOffset;
inline constexpr uintptr_t kReachVisibilitySecondaryDerivedAddressRva =
    kReachDefaultWorkspaceRva + kReachSecondaryDerivedOffset;
// Fields the projection/matrix builder consumes: the derived projection matrix
// at derived+0x78 and the compact render bounds at compact+0x4C.
inline constexpr uintptr_t kReachDerivedProjectionOffset = 0x0078;
inline constexpr uintptr_t kReachCompactRenderBoundsOffset = 0x004C;
inline constexpr uint64_t kReachRenderFreshnessMaxGapMs = 500;
inline constexpr uint64_t kReachRenderSafetyIntervalMs = 1000;

struct ReachSymmetricFovCover
{
    bool valid = false;
    float verticalFov = 0.0f;
    float requiredHalfHorizontal = 0.0f;
    float requiredHalfVertical = 0.0f;
};

struct ReachEyeCullFrustum
{
    float angleLeft = 0.0f;
    float angleRight = 0.0f;
    float angleUp = 0.0f;
    float angleDown = 0.0f;
    // OpenXR x/y/z/w orientation of this eye relative to the stereo midpoint.
    std::array<float, 4> relativeOrientation{0.0f, 0.0f, 0.0f, 1.0f};
};

// Converts the symmetric projection Reach actually rasterizes into the eye
// frustum used by the outer binocular cull. The raw asymmetric OpenXR FOV is
// intentionally not reused here: widening either axis for Reach's fixed-aspect
// projection widens the real raster corners that visibility must retain.
inline bool BuildReachSymmetricRasterCullFrustum(
    const ReachSymmetricFovCover& rasterCover,
    const std::array<float, 4>& relativeOrientation,
    uint32_t renderWidth, uint32_t renderHeight,
    ReachEyeCullFrustum& result) noexcept
{
    result = {};
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kMinimumQuaternionLengthSquared = 1.0e-12;
    if (!rasterCover.valid ||
        !std::isfinite(rasterCover.verticalFov) ||
        !std::isfinite(rasterCover.requiredHalfHorizontal) ||
        !std::isfinite(rasterCover.requiredHalfVertical) ||
        rasterCover.verticalFov <= 0.0001f ||
        rasterCover.verticalFov >= kPi ||
        rasterCover.requiredHalfHorizontal <= 0.0f ||
        rasterCover.requiredHalfVertical <= 0.0f ||
        !renderWidth || !renderHeight)
    {
        return false;
    }

    double quaternionLengthSquared = 0.0;
    for (float component : relativeOrientation)
    {
        if (!std::isfinite(component))
            return false;
        quaternionLengthSquared +=
            static_cast<double>(component) * component;
    }
    if (!std::isfinite(quaternionLengthSquared) ||
        quaternionLengthSquared <= kMinimumQuaternionLengthSquared)
    {
        return false;
    }

    const double aspect =
        static_cast<double>(renderWidth) / static_cast<double>(renderHeight);
    const double halfVertical =
        static_cast<double>(rasterCover.verticalFov) * 0.5;
    const double verticalTangent = std::tan(halfVertical);
    const double halfHorizontal =
        std::atan(verticalTangent * aspect);
    if (!std::isfinite(aspect) || aspect <= 0.0 ||
        !std::isfinite(verticalTangent) || verticalTangent <= 0.0 ||
        !std::isfinite(halfHorizontal) || halfHorizontal <= 0.0 ||
        halfHorizontal >= kPi * 0.5)
    {
        return false;
    }

    result.angleLeft = -static_cast<float>(halfHorizontal);
    result.angleRight = static_cast<float>(halfHorizontal);
    result.angleUp = static_cast<float>(halfVertical);
    result.angleDown = -static_cast<float>(halfVertical);
    result.relativeOrientation = relativeOrientation;
    return true;
}

// Reach's compact camera stores one vertical FOV and derives its horizontal
// scale from render_pixel_bounds. Choose the smallest symmetric frustum that
// covers all four OpenXR angles at that exact render aspect. This is the same
// conservative cover used by Halo 3/ODST, expressed in Reach's proven layout.
inline ReachSymmetricFovCover SelectReachSymmetricFovCover(
    float angleLeft, float angleRight, float angleUp, float angleDown,
    uint32_t renderWidth, uint32_t renderHeight) noexcept
{
    ReachSymmetricFovCover result{};
    if (!std::isfinite(angleLeft) || !std::isfinite(angleRight) ||
        !std::isfinite(angleUp) || !std::isfinite(angleDown) ||
        angleLeft >= 0.0f || angleRight <= 0.0f ||
        angleUp <= 0.0f || angleDown >= 0.0f ||
        !renderWidth || !renderHeight)
    {
        return result;
    }

    const float requiredHalfHorizontal =
        -angleLeft > angleRight ? -angleLeft : angleRight;
    const float requiredHalfVertical =
        angleUp > -angleDown ? angleUp : -angleDown;
    const float horizontalTangent = std::tan(requiredHalfHorizontal);
    const float verticalTangent = std::tan(requiredHalfVertical);
    const float aspect =
        static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
    if (!std::isfinite(horizontalTangent) ||
        !std::isfinite(verticalTangent) || !std::isfinite(aspect) ||
        horizontalTangent <= 0.0f || verticalTangent <= 0.0f ||
        aspect <= 0.0f)
    {
        return result;
    }

    const float horizontalCoverageTangent = horizontalTangent / aspect;
    const float selectedVerticalTangent =
        verticalTangent > horizontalCoverageTangent
            ? verticalTangent : horizontalCoverageTangent;
    const float verticalFov = 2.0f * std::atan(selectedVerticalTangent);
    if (!std::isfinite(verticalFov) ||
        verticalFov <= 0.0001f || verticalFov >= 3.1414928436f)
    {
        return result;
    }

    result.valid = true;
    result.verticalFov = verticalFov;
    result.requiredHalfHorizontal = requiredHalfHorizontal;
    result.requiredHalfVertical = requiredHalfVertical;
    return result;
}

// Builds one head-centre symmetric frustum that contains both complete OpenXR
// eye frusta. Every eye corner is rotated into midpoint space by its normalized
// relative-eye quaternion before the angular envelope is measured, so canted
// and asymmetric views cannot be clipped by an identity-orientation shortcut.
inline ReachSymmetricFovCover SelectReachStereoCullFovCover(
    const std::array<ReachEyeCullFrustum, 2>& eyes,
    uint32_t renderWidth, uint32_t renderHeight) noexcept
{
    ReachSymmetricFovCover result{};
    if (!renderWidth || !renderHeight)
        return result;

    const double aspect =
        static_cast<double>(renderWidth) / static_cast<double>(renderHeight);
    if (!std::isfinite(aspect) || aspect <= 0.0)
        return result;

    constexpr double kHalfPi = 1.57079632679489661923;
    constexpr double kMinimumQuaternionLengthSquared = 1.0e-12;
    double maximumHorizontalTangent = 0.0;
    double maximumVerticalTangent = 0.0;

    for (const ReachEyeCullFrustum& eye : eyes)
    {
        if (!std::isfinite(eye.angleLeft) ||
            !std::isfinite(eye.angleRight) ||
            !std::isfinite(eye.angleUp) ||
            !std::isfinite(eye.angleDown) ||
            eye.angleLeft >= 0.0f || eye.angleRight <= 0.0f ||
            eye.angleUp <= 0.0f || eye.angleDown >= 0.0f ||
            eye.angleLeft <= -kHalfPi || eye.angleRight >= kHalfPi ||
            eye.angleUp >= kHalfPi || eye.angleDown <= -kHalfPi)
        {
            return result;
        }

        double quaternion[4]{};
        double quaternionLengthSquared = 0.0;
        for (size_t component = 0; component < 4; ++component)
        {
            if (!std::isfinite(eye.relativeOrientation[component]))
                return result;
            quaternion[component] = eye.relativeOrientation[component];
            quaternionLengthSquared +=
                quaternion[component] * quaternion[component];
        }
        if (!std::isfinite(quaternionLengthSquared) ||
            quaternionLengthSquared <= kMinimumQuaternionLengthSquared)
        {
            return result;
        }
        const double inverseQuaternionLength =
            1.0 / std::sqrt(quaternionLengthSquared);
        for (double& component : quaternion)
            component *= inverseQuaternionLength;

        const double horizontalAngles[2]{
            eye.angleLeft, eye.angleRight};
        const double verticalAngles[2]{
            eye.angleDown, eye.angleUp};
        for (double horizontalAngle : horizontalAngles)
        {
            const double sourceX = std::tan(horizontalAngle);
            if (!std::isfinite(sourceX))
                return result;
            for (double verticalAngle : verticalAngles)
            {
                const double sourceY = std::tan(verticalAngle);
                if (!std::isfinite(sourceY))
                    return result;

                // Rotate (sourceX, sourceY, -1) using
                // v' = v + 2*w*cross(q.xyz,v) +
                //      2*cross(q.xyz,cross(q.xyz,v)).
                const double crossX =
                    quaternion[1] * -1.0 - quaternion[2] * sourceY;
                const double crossY =
                    quaternion[2] * sourceX + quaternion[0];
                const double crossZ =
                    quaternion[0] * sourceY -
                    quaternion[1] * sourceX;
                const double twiceCrossX = 2.0 * crossX;
                const double twiceCrossY = 2.0 * crossY;
                const double twiceCrossZ = 2.0 * crossZ;
                const double rotatedX = sourceX +
                    quaternion[3] * twiceCrossX +
                    quaternion[1] * twiceCrossZ -
                    quaternion[2] * twiceCrossY;
                const double rotatedY = sourceY +
                    quaternion[3] * twiceCrossY +
                    quaternion[2] * twiceCrossX -
                    quaternion[0] * twiceCrossZ;
                const double rotatedZ = -1.0 +
                    quaternion[3] * twiceCrossZ +
                    quaternion[0] * twiceCrossY -
                    quaternion[1] * twiceCrossX;
                const double forwardDepth = -rotatedZ;
                if (!std::isfinite(rotatedX) ||
                    !std::isfinite(rotatedY) ||
                    !std::isfinite(forwardDepth) ||
                    forwardDepth <= 0.0)
                {
                    return result;
                }

                const double horizontalTangent =
                    std::fabs(rotatedX) / forwardDepth;
                const double verticalTangent =
                    std::fabs(rotatedY) / forwardDepth;
                if (!std::isfinite(horizontalTangent) ||
                    !std::isfinite(verticalTangent))
                {
                    return result;
                }
                if (horizontalTangent > maximumHorizontalTangent)
                    maximumHorizontalTangent = horizontalTangent;
                if (verticalTangent > maximumVerticalTangent)
                    maximumVerticalTangent = verticalTangent;
            }
        }
    }

    const double selectedVerticalTangent =
        maximumVerticalTangent >
                maximumHorizontalTangent / aspect
            ? maximumVerticalTangent
            : maximumHorizontalTangent / aspect;
    const double requiredHalfHorizontal =
        std::atan(maximumHorizontalTangent);
    const double requiredHalfVertical =
        std::atan(maximumVerticalTangent);
    const double verticalFov = 2.0 * std::atan(selectedVerticalTangent);
    if (!std::isfinite(requiredHalfHorizontal) ||
        !std::isfinite(requiredHalfVertical) ||
        !std::isfinite(verticalFov) ||
        requiredHalfHorizontal <= 0.0 ||
        requiredHalfVertical <= 0.0 ||
        verticalFov <= 0.0001 ||
        verticalFov >= 3.1414928436)
    {
        return result;
    }

    result.valid = true;
    result.verticalFov = static_cast<float>(verticalFov);
    result.requiredHalfHorizontal =
        static_cast<float>(requiredHalfHorizontal);
    result.requiredHalfVertical =
        static_cast<float>(requiredHalfVertical);
    return result;
}

struct ReachProjectionHalfFovs
{
    bool valid = false;
    float horizontal = 0.0f;
    float vertical = 0.0f;
};

// The proven Reach derived block holds its projection matrix at +0x78. Decode
// the actual symmetric raster scales rather than telling OpenXR what we hoped
// the engine built. Invalid or under-covering matrices fail the eye transaction.
inline ReachProjectionHalfFovs DecodeReachProjectionHalfFovs(
    float projection00, float projection11) noexcept
{
    ReachProjectionHalfFovs result{};
    const float horizontalScale = std::fabs(projection00);
    const float verticalScale = std::fabs(projection11);
    if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale) ||
        horizontalScale <= 0.0001f || verticalScale <= 0.0001f)
    {
        return result;
    }

    const float horizontal = std::atan(1.0f / horizontalScale);
    const float vertical = std::atan(1.0f / verticalScale);
    if (!std::isfinite(horizontal) || !std::isfinite(vertical) ||
        horizontal <= 0.0f || vertical <= 0.0f)
    {
        return result;
    }
    result.valid = true;
    result.horizontal = horizontal;
    result.vertical = vertical;
    return result;
}

inline bool ReachProjectionCoversOpenXr(
    const ReachProjectionHalfFovs& projection,
    const ReachSymmetricFovCover& requested,
    float toleranceRadians = 0.001f) noexcept
{
    return projection.valid && requested.valid &&
        std::isfinite(toleranceRadians) && toleranceRadians >= 0.0f &&
        projection.horizontal + toleranceRadians >=
            requested.requiredHalfHorizontal &&
        projection.vertical + toleranceRadians >=
            requested.requiredHalfVertical;
}

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

class ReachPreflightPublication;

class ReachPreflightToken
{
public:
    ReachPreflightToken() noexcept = default;

    bool Complete() const noexcept { return m_complete; }
    ReachModuleEpoch Epoch() const noexcept { return m_epoch; }
    uint64_t PublicationNonce() const noexcept
    {
        return m_publicationNonce;
    }
    bool IsCurrent() const noexcept;

private:
    ReachPreflightToken(
        const ReachPreflightPublication* publication,
        const ReachModuleEpoch& epoch,
        uint64_t publicationNonce) noexcept
        : m_publication(publication),
          m_epoch(epoch),
          m_publicationNonce(publicationNonce),
          m_complete(true)
    {
    }

    const ReachPreflightPublication* m_publication = nullptr;
    ReachModuleEpoch m_epoch{};
    uint64_t m_publicationNonce = 0;
    bool m_complete = false;

    friend class ReachPreflightPublication;
};

// Single-writer, multi-reader publication capability for a completed loaded
// image preflight. Every successful publication receives a strictly newer
// nonce. Invalidation clears the live nonce before changing the epoch, so a
// copied token becomes unusable before the next publication can be observed.
class ReachPreflightPublication
{
public:
    ReachPreflightPublication() noexcept = default;
    ReachPreflightPublication(const ReachPreflightPublication&) = delete;
    ReachPreflightPublication& operator=(
        const ReachPreflightPublication&) = delete;

    bool Publish(
        const ReachModuleEpoch& epoch,
        const ReachRenderCandidateProof& proof) noexcept
    {
        Invalidate();
        if (!ReachModuleEpochValid(epoch) ||
            !ReachRenderCandidateProofComplete(proof))
        {
            return false;
        }

        const uint64_t nonce = NextNonce();
        if (!nonce)
            return false;
        m_moduleBase.store(epoch.moduleBase, std::memory_order_relaxed);
        m_generation.store(epoch.generation, std::memory_order_relaxed);
        m_currentNonce.store(nonce, std::memory_order_release);
        return true;
    }

    void Invalidate() noexcept
    {
        m_currentNonce.store(0, std::memory_order_release);
        m_moduleBase.store(0, std::memory_order_relaxed);
        m_generation.store(0, std::memory_order_relaxed);
    }

    ReachPreflightToken Get(
        const ReachModuleEpoch& epoch) const noexcept
    {
        if (!ReachModuleEpochValid(epoch))
            return {};
        for (unsigned attempt = 0; attempt < 4; ++attempt)
        {
            const uint64_t before =
                m_currentNonce.load(std::memory_order_acquire);
            if (!before)
                return {};
            const uintptr_t moduleBase =
                m_moduleBase.load(std::memory_order_relaxed);
            const uint32_t generation =
                m_generation.load(std::memory_order_relaxed);
            const uint64_t after =
                m_currentNonce.load(std::memory_order_acquire);
            if (before != after)
                continue;
            if (moduleBase != epoch.moduleBase ||
                generation != epoch.generation)
            {
                return {};
            }
            return ReachPreflightToken(this, epoch, before);
        }
        return {};
    }

    bool IsCurrent(const ReachPreflightToken& token) const noexcept
    {
        if (!token.Complete() || token.m_publication != this ||
            !token.m_publicationNonce)
        {
            return false;
        }
        for (unsigned attempt = 0; attempt < 4; ++attempt)
        {
            const uint64_t before =
                m_currentNonce.load(std::memory_order_acquire);
            if (!before || before != token.m_publicationNonce)
                return false;
            const uintptr_t moduleBase =
                m_moduleBase.load(std::memory_order_relaxed);
            const uint32_t generation =
                m_generation.load(std::memory_order_relaxed);
            const uint64_t after =
                m_currentNonce.load(std::memory_order_acquire);
            if (before != after)
                continue;
            return moduleBase == token.m_epoch.moduleBase &&
                generation == token.m_epoch.generation;
        }
        return false;
    }

    bool HasCurrent() const noexcept
    {
        return m_currentNonce.load(std::memory_order_acquire) != 0;
    }

    uint64_t LastPublicationNonce() const noexcept
    {
        return m_nonceCounter.load(std::memory_order_relaxed);
    }

private:
    uint64_t NextNonce() noexcept
    {
        uint64_t previous =
            m_nonceCounter.load(std::memory_order_relaxed);
        for (;;)
        {
            if (previous == std::numeric_limits<uint64_t>::max())
                return 0;
            if (m_nonceCounter.compare_exchange_weak(
                    previous, previous + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed))
            {
                return previous + 1;
            }
        }
    }

    std::atomic<uint64_t> m_nonceCounter{0};
    std::atomic<uint64_t> m_currentNonce{0};
    std::atomic<uintptr_t> m_moduleBase{0};
    std::atomic<uint32_t> m_generation{0};
};

inline bool ReachPreflightToken::IsCurrent() const noexcept
{
    return m_publication && m_publication->IsCurrent(*this);
}

inline bool IsPreflightCurrent(
    const ReachPreflightToken& token) noexcept
{
    return token.IsCurrent();
}

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

    bool Ready() const noexcept { return m_ready; }
    ReachModuleEpoch Epoch() const noexcept { return m_epoch; }
    uint64_t PreparedFrameSerial() const noexcept
    {
        return m_preparedFrameSerial;
    }
    uint64_t ResourceRevision() const noexcept
    {
        return m_resourceRevision;
    }
    uint64_t Nonce() const noexcept { return m_nonce; }

private:
    ReachDirectCopyToken(
        const ReachModuleEpoch& epoch,
        uint64_t preparedFrameSerial, uint64_t resourceRevision,
        uint64_t nonce) noexcept
        : m_epoch(epoch),
          m_preparedFrameSerial(preparedFrameSerial),
          m_resourceRevision(resourceRevision),
          m_nonce(nonce),
          m_ready(true)
    {
    }

    ReachModuleEpoch m_epoch{};
    uint64_t m_preparedFrameSerial = 0;
    uint64_t m_resourceRevision = 0;
    uint64_t m_nonce = 0;
    bool m_ready = false;

    friend class ReachDirectCopyGate;
};

struct ReachCopyShape
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 0;
    uint32_t arraySize = 0;
    uint32_t format = 0;
    uint32_t sampleCount = 0;
    uint32_t sampleQuality = 0;
};

inline bool ReachSameCopyShape(
    const ReachCopyShape& left, const ReachCopyShape& right) noexcept
{
    return left.width == right.width &&
        left.height == right.height &&
        left.mipLevels == right.mipLevels &&
        left.arraySize == right.arraySize &&
        left.format == right.format &&
        left.sampleCount == right.sampleCount &&
        left.sampleQuality == right.sampleQuality;
}

inline bool ReachDisplayCopyShapeValid(
    const ReachCopyShape& shape) noexcept
{
    return shape.width != 0 && shape.height != 0 &&
        shape.mipLevels == 1 && shape.arraySize == 1 &&
        shape.format == kReachDisplayFormatR8G8B8A8Unorm &&
        shape.sampleCount == 1 && shape.sampleQuality == 0;
}

struct ReachDisplayContinuity
{
    ReachModuleEpoch epoch{};
    uint64_t resourceRevision = 0;
    uint64_t lifecycleSerial = 0;
    uintptr_t swapchainIdentity = 0;
    uintptr_t buffer0Identity = 0;
    uintptr_t surfaceArrayIdentity = 0;
    uintptr_t record0RtvIdentity = 0;
    uintptr_t record0SrvIdentity = 0;
    uintptr_t selectedRtvIdentity = 0;
    uintptr_t deviceIdentity = 0;
    uintptr_t immediateContextIdentity = 0;
    uintptr_t eyeResourceIdentities[2]{};
    uint32_t specializationCount = 0;
    uint32_t selectedSpecialization = 0;
    bool teardownRequested = false;
};

struct ReachDisplaySurfaceProof
{
    ReachDisplayContinuity continuity{};
    ReachPreflightToken preflight{};
    uintptr_t immediateContextIdentity = 0;
    uintptr_t eyeResourceIdentities[2]{};
    ReachCopyShape source{};
    ReachCopyShape eyes[2]{};
    uint32_t readyEyeMask = 0;
    bool engineSwapchainMatchesPresent = false;
    bool selectedRtvMatchesRecord0 = false;
    bool swapchainContract = false;
    bool sameDevice = false;
    bool immediateContext = false;
};

inline bool ReachDisplayContinuityValid(
    const ReachDisplayContinuity& continuity) noexcept
{
    return ReachModuleEpochValid(continuity.epoch) &&
        continuity.resourceRevision != 0 &&
        continuity.lifecycleSerial != 0 &&
        continuity.lifecycleSerial !=
            std::numeric_limits<uint64_t>::max() &&
        continuity.swapchainIdentity != 0 &&
        continuity.buffer0Identity != 0 &&
        continuity.surfaceArrayIdentity != 0 &&
        continuity.record0RtvIdentity != 0 &&
        continuity.record0SrvIdentity != 0 &&
        continuity.selectedRtvIdentity ==
            continuity.record0RtvIdentity &&
        continuity.deviceIdentity != 0 &&
        continuity.immediateContextIdentity != 0 &&
        continuity.eyeResourceIdentities[0] != 0 &&
        continuity.eyeResourceIdentities[1] != 0 &&
        continuity.eyeResourceIdentities[0] !=
            continuity.eyeResourceIdentities[1] &&
        continuity.eyeResourceIdentities[0] !=
            continuity.buffer0Identity &&
        continuity.eyeResourceIdentities[1] !=
            continuity.buffer0Identity &&
        continuity.specializationCount == kReachDisplaySurfaceCount &&
        continuity.selectedSpecialization == 0 &&
        !continuity.teardownRequested;
}

inline bool ReachSameDisplayContinuity(
    const ReachDisplayContinuity& left,
    const ReachDisplayContinuity& right) noexcept
{
    return ReachDisplayContinuityValid(left) &&
        ReachDisplayContinuityValid(right) &&
        ReachSameModuleEpoch(left.epoch, right.epoch) &&
        left.resourceRevision == right.resourceRevision &&
        left.lifecycleSerial == right.lifecycleSerial &&
        left.swapchainIdentity == right.swapchainIdentity &&
        left.buffer0Identity == right.buffer0Identity &&
        left.surfaceArrayIdentity == right.surfaceArrayIdentity &&
        left.record0RtvIdentity == right.record0RtvIdentity &&
        left.record0SrvIdentity == right.record0SrvIdentity &&
        left.selectedRtvIdentity == right.selectedRtvIdentity &&
        left.deviceIdentity == right.deviceIdentity &&
        left.immediateContextIdentity ==
            right.immediateContextIdentity &&
        left.eyeResourceIdentities[0] ==
            right.eyeResourceIdentities[0] &&
        left.eyeResourceIdentities[1] ==
            right.eyeResourceIdentities[1] &&
        left.specializationCount == right.specializationCount &&
        left.selectedSpecialization == right.selectedSpecialization;
}

inline bool ReachDisplaySurfaceProofComplete(
    const ReachDisplaySurfaceProof& proof) noexcept
{
    return proof.preflight.Complete() &&
        IsPreflightCurrent(proof.preflight) &&
        ReachSameModuleEpoch(
            proof.preflight.Epoch(), proof.continuity.epoch) &&
        ReachDisplayContinuityValid(proof.continuity) &&
        proof.immediateContextIdentity ==
            proof.continuity.immediateContextIdentity &&
        proof.eyeResourceIdentities[0] ==
            proof.continuity.eyeResourceIdentities[0] &&
        proof.eyeResourceIdentities[1] ==
            proof.continuity.eyeResourceIdentities[1] &&
        ReachDisplayCopyShapeValid(proof.source) &&
        ReachSameCopyShape(proof.source, proof.eyes[0]) &&
        ReachSameCopyShape(proof.source, proof.eyes[1]) &&
        proof.readyEyeMask == 0x3u &&
        proof.engineSwapchainMatchesPresent &&
        proof.selectedRtvMatchesRecord0 &&
        proof.swapchainContract &&
        proof.sameDevice &&
        proof.immediateContext;
}

// Owns the live display-resource revision. A copied readiness token becomes
// unusable immediately when the swapchain, buffer, engine views, ResizeBuffers,
// title epoch, or device/context proof is invalidated.
class ReachDirectCopyGate
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
        m_lastResourceRevision = 0;
        m_continuity = {};
        m_preflight = {};
        m_ready = false;
        return true;
    }

    bool Publish(const ReachDisplaySurfaceProof& proof) noexcept
    {
        // A refresh attempt replaces the old capability atomically from the
        // policy's perspective: even a failed replacement revokes prior
        // readiness and all retained live identities.
        m_ready = false;
        m_continuity = {};
        m_preflight = {};
        if (!ReachSameModuleEpoch(proof.continuity.epoch, m_epoch) ||
            !ReachDisplaySurfaceProofComplete(proof) ||
            proof.continuity.resourceRevision <=
                m_lastResourceRevision ||
            m_nonceCounter == std::numeric_limits<uint64_t>::max())
        {
            return false;
        }
        ++m_nonceCounter;
        m_lastResourceRevision = proof.continuity.resourceRevision;
        m_continuity = proof.continuity;
        m_preflight = proof.preflight;
        m_nonce = m_nonceCounter;
        m_ready = true;
        return Ready();
    }

    ReachDirectCopyToken Prepare(
        const ReachPreparedFrameToken& preparedFrame,
        const ReachDisplayContinuity& live) const noexcept
    {
        return Ready() && preparedFrame.Ready() &&
                ReachSameModuleEpoch(preparedFrame.Epoch(), m_epoch) &&
                ReachSameDisplayContinuity(live, m_continuity)
            ? ReachDirectCopyToken(
                m_epoch, preparedFrame.Serial(),
                m_continuity.resourceRevision, m_nonce)
            : ReachDirectCopyToken{};
    }

    bool IsCurrent(
        const ReachDirectCopyToken& token,
        const ReachDisplayContinuity& live) const noexcept
    {
        return Ready() && token.Ready() &&
            ReachSameModuleEpoch(token.Epoch(), m_epoch) &&
            token.ResourceRevision() ==
                m_continuity.resourceRevision &&
            token.Nonce() == m_nonce &&
            ReachSameDisplayContinuity(live, m_continuity);
    }

    bool Invalidate(const ReachModuleEpoch& epoch) noexcept
    {
        if (!ReachSameModuleEpoch(epoch, m_epoch))
            return false;
        m_ready = false;
        m_continuity = {};
        m_preflight = {};
        return true;
    }

    bool Teardown(const ReachModuleEpoch& epoch) noexcept
    {
        if (!ReachSameModuleEpoch(epoch, m_epoch))
            return false;
        m_ready = false;
        m_continuity = {};
        m_preflight = {};
        m_epoch = {};
        m_lastResourceRevision = 0;
        return true;
    }

    bool Ready() const noexcept
    {
        return m_ready && IsPreflightCurrent(m_preflight);
    }
    ReachDisplayContinuity Continuity() const noexcept
    {
        return m_continuity;
    }
    uint64_t LastResourceRevision() const noexcept
    {
        return m_lastResourceRevision;
    }

private:
    ReachModuleEpoch m_epoch{};
    uint32_t m_highestGeneration = 0;
    uint64_t m_lastResourceRevision = 0;
    uint64_t m_nonceCounter = 0;
    uint64_t m_nonce = 0;
    ReachDisplayContinuity m_continuity{};
    ReachPreflightToken m_preflight{};
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

inline bool ReachMotionBlurSlotsMatchPinnedImage(
    uintptr_t moduleBase, size_t moduleSize,
    uintptr_t scaleSlot, uintptr_t maxSlot) noexcept
{
    if (moduleSize != kReachRetailImageSize || scaleSlot == maxSlot)
        return false;
    uintptr_t expectedScale = 0;
    uintptr_t expectedMax = 0;
    return ReachAddressFromRva(
               moduleBase, moduleSize,
               kReachMotionBlurScaleValueRva, expectedScale) &&
        ReachAddressFromRva(
               moduleBase, moduleSize,
               kReachMotionBlurMaxValueRva, expectedMax) &&
        scaleSlot == expectedScale && maxSlot == expectedMax;
}

inline bool ReachMotionBlurScaleUsable(float scale) noexcept
{
    return std::isfinite(scale) &&
        scale > kReachMotionBlurMinimumUsableScale;
}

inline bool ReachMotionBlurSuppressionValuesValid(
    float scale, float maximum) noexcept
{
    return ReachMotionBlurScaleUsable(scale) &&
        std::isfinite(maximum) && maximum >= 0.0f;
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
    if (!input.preflight.Complete() ||
        !IsPreflightCurrent(input.preflight) ||
        !input.freshCamera.Stable() ||
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
        input.cameraStackDepthBefore < -1 ||
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
    ReachDisplayContinuity displayContinuity{};
    bool teardownRequested = false;
};

inline bool ReachInnerScopeMatches(
    const ReachRenderOwnerGate& owner,
    const ReachRenderOwnerToken& token,
    const ReachDirectCopyGate& directCopyGate,
    const ReachInnerRenderInput& input) noexcept
{
    if (!owner.IsCurrent(token) ||
        !ReachModuleEpochValid(token.Epoch()) || !token.Workspace() ||
        !token.PlayerView() || token.CameraStackDepthBefore() < -1 ||
        token.CameraStackDepthBefore() >= 3 ||
        !input.preparedFrame.Ready() || !input.directCopy.Ready() ||
        !ReachSameModuleEpoch(
            input.preparedFrame.Epoch(), token.Epoch()) ||
        !ReachSameModuleEpoch(input.directCopy.Epoch(), token.Epoch()) ||
        input.preparedFrame.Serial() != token.PreparedFrameSerial() ||
        input.directCopy.PreparedFrameSerial() !=
            token.PreparedFrameSerial() ||
        !directCopyGate.IsCurrent(
            input.directCopy, input.displayContinuity) ||
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
    const ReachDirectCopyGate& directCopyGate,
    const ReachInnerRenderInput& input) noexcept
{
    return runtimeHooksPermitted && preflight.Complete() &&
            IsPreflightCurrent(preflight) &&
            owner.IsCurrent(token) &&
            ReachSameModuleEpoch(preflight.Epoch(), token.Epoch()) &&
            ReachInnerScopeMatches(
                owner, token, directCopyGate, input)
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
