#pragma once

#include "../common/reach_render_logic.h"

// Compiled scaffolding only. The title registry, lifecycle publisher, MinHook
// installer, camera mutator, and VR capture path do not call into a Reach
// detour in this candidate.
bool ReachRenderCandidate_Compiled() noexcept;
bool ReachRenderCandidate_RuntimeHooksEnabled() noexcept;
ReachRenderAction ReachRenderCandidate_SelectAction(
    const ReachPreflightToken& preflight,
    const ReachRenderOwnerGate& owner,
    const ReachRenderOwnerToken& token,
    const ReachInnerRenderInput& input) noexcept;
