#include "reach_render_candidate.h"

#ifndef HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE
#define HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE 0
#endif

static_assert(HALOMCCVR_EXPERIMENTAL_REACH_RENDER_CANDIDATE == 1,
    "reach_render_candidate.cpp must only compile in the selected scaffold preset");
static_assert(kReachMainRenderViewAob.size() == 32);
static_assert(kReachPlayerViewRenderAob.size() == 69);
static_assert(kReachFrustumHelperAob.size() == 25);

bool ReachRenderCandidate_Compiled() noexcept
{
    return true;
}

bool ReachRenderCandidate_RuntimeHooksEnabled() noexcept
{
    // This milestone deliberately has no MinHook installation path. Flipping
    // the compile option cannot make a Reach callback reachable.
    return false;
}

ReachRenderAction ReachRenderCandidate_SelectAction(
    const ReachPreflightToken& preflight,
    const ReachRenderOwnerGate& owner,
    const ReachRenderOwnerToken& token,
    const ReachInnerRenderInput& input) noexcept
{
    return SelectReachRenderAction(
        ReachRenderCandidate_RuntimeHooksEnabled(),
        preflight, owner, token, input);
}
