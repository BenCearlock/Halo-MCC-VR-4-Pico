#include "reach_adapter.h"

#ifndef HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP
#define HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP 0
#endif

static_assert(HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP == 0 ||
              HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP == 1);

namespace
{
    constexpr ReachEvidenceIdentity kReachRetailEvidence = {
        L"haloreach.dll",
        "738DD2D24EA3AEA12E1EE9AA4A61094BF116027D42004C35A19E5048608B0894",
        0x68A0EFE1u,
        0x04EDA000u,
        "2023.07.17.176677.1-QFE1",
    };
}

ReachAdapterStage ReachAdapter_GetStage()
{
#if HALOMCCVR_EXPERIMENTAL_REACH_BRINGUP
    return ReachAdapterStage::ControllerInputOnly;
#else
    return ReachAdapterStage::Disabled;
#endif
}

const ReachEvidenceIdentity& ReachAdapter_GetEvidenceIdentity()
{
    return kReachRetailEvidence;
}

bool ReachAdapter_HookProofComplete(const ReachHookProof& proof)
{
    return proof.retailIdentity &&
        proof.loadedImageMatchCount == 1 &&
        proof.executableRange &&
        proof.abi &&
        proof.callers &&
        proof.dataFlow &&
        proof.hrekSemantics &&
        proof.consumedLayoutFields;
}

bool ReachAdapter_RuntimeHooksPermitted()
{
    // Controller transport is shared XInput policy, not a Reach engine hook.
    // Camera, render, aim, movement, HUD, IK, haptics, and lifecycle hooks stay
    // forbidden in this candidate.
    return false;
}
