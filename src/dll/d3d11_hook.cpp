#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdlib>
#include <climits>
#include <cmath>
#include <intrin.h>
#include <MinHook.h>

#pragma intrinsic(_ReturnAddress)
#include "d3d11_hook.h"
#include "game.h"
#include "vr.h"
#include "../common/config.h"
#include "../common/log.h"

// We can't hook "the game's swapchain" directly because it doesn't exist yet
// when we're injected. Instead we create a throwaway D3D11 device + swapchain
// of our own, read the addresses of Present/ResizeBuffers out of its vtable
// (all swapchains in the process share the same implementation), hook those,
// and throw the dummy away.

typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* Present1Fn)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffersFn)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef void(STDMETHODCALLTYPE* OMSetRenderTargetsFn)(ID3D11DeviceContext*, UINT,
    ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
#if HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP
typedef void(STDMETHODCALLTYPE* CopyResourceFn)(ID3D11DeviceContext*,
    ID3D11Resource*, ID3D11Resource*);
#endif

static PresentFn g_origPresent = nullptr;
static Present1Fn g_origPresent1 = nullptr;
static ResizeBuffersFn g_origResizeBuffers = nullptr;
static OMSetRenderTargetsFn g_origOMSetRenderTargets = nullptr;
#if HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP
static CopyResourceFn g_origCopyResource = nullptr;
#endif

// --- Desktop-window fit (config.fit_desktop_window) -----------------------
// resolution_scale sizes the render the headset captures. On a monitor smaller
// than that render, MCC's window overflows the screen and its menu buttons fall
// off the edge. With the fit on we keep MCC drawing the FULL render (so the
// headset picture and the gun alignment never change) while shrinking only the
// visible window to fit the monitor (menu.cpp); the GPU downscales the full
// backbuffer into the small window on present (flip-model + DXGI_SCALING_STRETCH)
// for free -- no second pass. The hazard is the reverse of the earlier attempt:
// if MCC learns the window shrank it draws a small frame into the corner of the
// big backbuffer (the black-border crop). So we force the backbuffer full at
// creation and keep MCC believing its client is still full-size through its own
// resize handling -- WITHOUT lying to DXGI's present-time client query, which
// must still see the true small window to downscale correctly. Everything below
// is gated on g_config.fit_desktop_window; with it off, none of it is installed.
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForHwndFn)(IDXGIFactory2*, IUnknown*, HWND,
    const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*,
    IDXGISwapChain1**);
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainFn)(IDXGIFactory*, IUnknown*,
    DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
typedef BOOL(WINAPI* GetClientRectFn)(HWND, LPRECT);
static CreateSwapChainForHwndFn g_origCreateSwapChainForHwnd = nullptr;
static CreateSwapChainFn g_origCreateSwapChain = nullptr;
static GetClientRectFn g_origGetClientRect = nullptr;
static UINT g_forcedRenderW = 0;
static UINT g_forcedRenderH = 0;
static bool g_forcedMainSwapchain = false; // only force the game's own (first) swapchain
static HWND g_gameHwnd = nullptr;          // captured at swapchain creation
static bool g_fitActive = false;           // set once at startup: fit on AND its hooks installed
// Set on the game's UI thread ONLY while it synchronously processes a WM_SIZE we
// rewrote to the full render size, so GetClientRectHook feeds MCC's own resize
// code the full size on exactly that call stack -- never on the render thread's
// present-time query, which must keep getting the true small client size.
static thread_local bool g_lieClientToGame = false;

void D3D_GetForcedRenderSize(unsigned& width, unsigned& height)
{
    width = g_forcedRenderW;
    height = g_forcedRenderH;
}

bool D3D_FitActive()
{
    return g_fitActive;
}

void D3D_SetForcedClientLie(bool on)
{
    g_lieClientToGame = on;
}

static void InitForcedRenderSize()
{
    wchar_t buf[32];
    if (GetEnvironmentVariableW(L"HALO3XR_RENDER_W", buf, 32) > 0)
        g_forcedRenderW = (UINT)_wtoi(buf);
    if (GetEnvironmentVariableW(L"HALO3XR_RENDER_H", buf, 32) > 0)
        g_forcedRenderH = (UINT)_wtoi(buf);
    if (g_forcedRenderW == 0 || g_forcedRenderH == 0)
    {
        // DLL run without the current launcher: replicate the launcher's
        // ScaleEven so the two can never disagree. Config is loaded before this
        // (dllmain InitThread order).
        const float s = g_config.resolution_scale;
        auto even = [](int base, float sc) -> UINT {
            int v = (int)((float)base * sc + 0.5f);
            if (v & 1) ++v;
            return (UINT)v;
        };
        g_forcedRenderW = even(kNativeRenderWidth, s);
        g_forcedRenderH = even(kNativeRenderHeight, s);
    }
    LOG("fit_desktop_window ON: forcing MCC backbuffer to %ux%u (full headset "
        "render); the desktop window is shrunk to fit the monitor separately",
        g_forcedRenderW, g_forcedRenderH);
}

// Game-executable image range, used to scope the client-rect lie and cursor
// remap to game-side callers (never DXGI/DWM or our own overlay). Populated by
// InitExeRange() at hook install; the accessors are defined with the cursor
// hooks below.
static const BYTE* g_exeBase = nullptr;
static const BYTE* g_exeEnd = nullptr;
static inline bool CallerInExe(const void* ret); // defined with the cursor hooks

// Feed the game the full render size for the game window whenever the caller is
// game-side: MCC's resize code (so it keeps drawing full) AND its menu hit-test
// (which clamps the cursor against the client size -- if that returns the true
// small window while our cursor is scaled up to render space, only client/render
// of the menu is reachable, i.e. the top-left ~31% dead-zone). DXGI/DWM query
// the client at present time from system DLLs (not the game EXE), so they still
// get the real small size and keep downscaling the full frame into the window.
static BOOL WINAPI GetClientRectHook(HWND hwnd, LPRECT rc)
{
    const void* caller = _ReturnAddress();
    const BOOL ok = g_origGetClientRect(hwnd, rc);
    if (ok && rc && hwnd == g_gameHwnd && g_forcedRenderW && g_forcedRenderH &&
        (g_lieClientToGame || CallerInExe(caller)))
    {
        rc->left = 0;
        rc->top = 0;
        rc->right = (LONG)g_forcedRenderW;
        rc->bottom = (LONG)g_forcedRenderH;
        static int s_log = 0;
        static const void* s_lastCaller = nullptr;
        if (!g_lieClientToGame && s_log < 16 && caller != s_lastCaller &&
            g_exeBase)
        {
            ++s_log;
            s_lastCaller = caller;
            LOG("fit: GetClientRect game-side +0x%llX -> forced %ux%u",
                (unsigned long long)((const BYTE*)caller - g_exeBase),
                g_forcedRenderW, g_forcedRenderH);
        }
    }
    return ok;
}

// --- Fitted-menu cursor coordinate remap -----------------------------------
// With the fit on, MCC DRAWS the full render (e.g. 3204x2310) but the visible
// window is shrunk to the monitor and the GPU downscales it on present. MCC
// lays out and hit-tests its native shell / pause menu in that full render
// space, yet the OS cursor that drives EVERY selection -- the mouse, and the
// gamepad/keyboard "virtual cursor" MCC's console-style shell moves for you --
// is confined to the small physical window. So only the top-left window-sized
// slice of the menu is reachable: the pointer only responds top-left, and
// keyboard/controller focus "moves but stops short" at the window edge. That is
// the exact reported symptom.
//
// Fix: make MCC's OS-cursor coordinate space match the fitted window in BOTH
// directions, and touch only calls that come from the game executable.
//   * GetCursorPos (MCC reads the cursor): scale the physical, window-confined
//     point UP into full render space, so the hit-test lands on the widget
//     directly under the visibly-downscaled cursor.
//   * SetCursorPos (MCC moves the cursor for gamepad/keyboard nav): scale its
//     render-space target back DOWN so the OS cursor stays inside the window
//     and the round-trip through GetCursorPos is exact.
//   * WindowFromPoint MUST keep seeing the TRUE physical point. MCC calls it
//     right after GetCursorPos to confirm the cursor is over its window; if it
//     saw our scaled-up point (which lands outside the small window) it decides
//     the cursor left and drops the input. That silent detail is what broke the
//     earlier broad GetCursorPos rewrite. We undo the remap for exactly the
//     value we last handed out, so no game address needs to be hardcoded.
typedef BOOL(WINAPI* GetCursorPosFn)(LPPOINT);
typedef BOOL(WINAPI* SetCursorPosFn)(int, int);
typedef HWND(WINAPI* WindowFromPointFn)(POINT);
typedef BOOL(WINAPI* ClipCursorFn)(const RECT*);
static GetCursorPosFn g_origGetCursorPos = nullptr;
static SetCursorPosFn g_origSetCursorPos = nullptr;
static WindowFromPointFn g_origWindowFromPoint = nullptr;
static ClipCursorFn g_origClipCursor = nullptr;

// The shell/pause cursor consumers live in the game executable (RE-notes /
// RESOLUTION-FSR-INVESTIGATION static analysis). We only remap calls whose
// return address is inside that image, so our own ImGui overlay and any system
// DLL are never touched. (g_exeBase/g_exeEnd are declared above GetClientRectHook.)
static void InitExeRange()
{
    HMODULE h = GetModuleHandleW(nullptr);
    if (!h)
        return;
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(h);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const BYTE*>(h) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return;
    g_exeBase = reinterpret_cast<const BYTE*>(h);
    g_exeEnd = g_exeBase + nt->OptionalHeader.SizeOfImage;
}
static inline bool CallerInExe(const void* ret)
{
    return g_exeBase && ret >= static_cast<const void*>(g_exeBase) &&
           ret < static_cast<const void*>(g_exeEnd);
}

// True (unlied) fitted-window client origin (in screen space) and size. Uses
// the ORIGINAL GetClientRect so the WM_SIZE full-size lie can never leak in.
static bool FitClientMetrics(POINT& originScreen, LONG& clientW, LONG& clientH)
{
    if (!g_gameHwnd || !g_origGetClientRect)
        return false;
    RECT rc{};
    if (!g_origGetClientRect(g_gameHwnd, &rc))
        return false;
    clientW = rc.right - rc.left;
    clientH = rc.bottom - rc.top;
    if (clientW <= 0 || clientH <= 0)
        return false;
    originScreen.x = 0;
    originScreen.y = 0;
    return ClientToScreen(g_gameHwnd, &originScreen) != FALSE;
}

static BOOL WINAPI GetCursorPosHook(LPPOINT p)
{
    const void* caller = _ReturnAddress();
    const BOOL ok = g_origGetCursorPos(p);
    if (!ok || !p || !g_fitActive || !g_forcedRenderW || !g_forcedRenderH ||
        !CallerInExe(caller))
        return ok;
    POINT origin{};
    LONG cw = 0, ch = 0;
    if (!FitClientMetrics(origin, cw, ch))
        return ok;
    // Only remap while the cursor is actually over the fitted window.
    if (p->x < origin.x || p->y < origin.y ||
        p->x >= origin.x + cw || p->y >= origin.y + ch)
        return ok;
    const POINT phys = *p;
    POINT mapped;
    mapped.x = origin.x +
               (LONG)llround((double)(phys.x - origin.x) * g_forcedRenderW / cw);
    mapped.y = origin.y +
               (LONG)llround((double)(phys.y - origin.y) * g_forcedRenderH / ch);
    *p = mapped;
    // Log only when the physical cursor actually MOVES (and only once the
    // window is shrunk), so the budget records real navigation instead of 40
    // copies of one idle frame.
    static int s_log = 0;
    static POINT s_lastLogged{LONG_MIN, LONG_MIN};
    if (cw < (LONG)g_forcedRenderW && s_log < 60 &&
        (phys.x != s_lastLogged.x || phys.y != s_lastLogged.y))
    {
        ++s_log;
        s_lastLogged = phys;
        LOG("fit: menu cursor read +0x%llX phys(%ld,%ld) -> render(%ld,%ld) "
            "client %ldx%ld",
            (unsigned long long)((const BYTE*)caller - g_exeBase),
            phys.x, phys.y, mapped.x, mapped.y, cw, ch);
    }
    return ok;
}

static BOOL WINAPI SetCursorPosHook(int X, int Y)
{
    const void* caller = _ReturnAddress();
    if (!g_fitActive || !g_forcedRenderW || !g_forcedRenderH ||
        !CallerInExe(caller))
        return g_origSetCursorPos(X, Y);
    POINT origin{};
    LONG cw = 0, ch = 0;
    if (!FitClientMetrics(origin, cw, ch))
        return g_origSetCursorPos(X, Y);
    // MCC targets the cursor in its own (full render) client space. Convert
    // points that fall inside that render rectangle back into the small window
    // so the OS cursor lands where MCC intends; leave anything else untouched.
    const LONG relX = X - origin.x;
    const LONG relY = Y - origin.y;
    if (relX < 0 || relY < 0 ||
        relX > (LONG)g_forcedRenderW || relY > (LONG)g_forcedRenderH)
        return g_origSetCursorPos(X, Y);
    const int px = origin.x + (int)llround((double)relX * cw / g_forcedRenderW);
    const int py = origin.y + (int)llround((double)relY * ch / g_forcedRenderH);
    static int s_log = 0;
    static POINT s_lastLogged{LONG_MIN, LONG_MIN};
    if (cw < (LONG)g_forcedRenderW && s_log < 60 &&
        (X != s_lastLogged.x || Y != s_lastLogged.y))
    {
        ++s_log;
        s_lastLogged.x = X;
        s_lastLogged.y = Y;
        LOG("fit: menu cursor move +0x%llX render(%d,%d) -> phys(%d,%d)",
            (unsigned long long)((const BYTE*)caller - g_exeBase), X, Y, px, py);
    }
    return g_origSetCursorPos(px, py);
}

// If MCC confines the cursor (ClipCursor) to a rectangle expressed in its
// believed full-render space, the OS clips the physical cursor to that (often
// off-screen) rectangle and you can't move the pointer to the lower menu items
// at all. Fold any render-space clip rect back into the real window client so
// the cursor stays free across the whole fitted menu. Fail-open: on anything
// unexpected we pass the request through untouched.
static BOOL WINAPI ClipCursorHook(const RECT* rc)
{
    if (!g_fitActive || !g_forcedRenderW || !g_forcedRenderH || !rc)
        return g_origClipCursor(rc);
    POINT origin{};
    LONG cw = 0, ch = 0;
    if (!FitClientMetrics(origin, cw, ch))
        return g_origClipCursor(rc);
    RECT mapped = *rc;
    auto foldX = [&](LONG v) {
        LONG r = v - origin.x;
        if (r < 0) r = 0;
        if (r > (LONG)g_forcedRenderW) r = (LONG)g_forcedRenderW;
        return origin.x + (LONG)llround((double)r * cw / g_forcedRenderW);
    };
    auto foldY = [&](LONG v) {
        LONG r = v - origin.y;
        if (r < 0) r = 0;
        if (r > (LONG)g_forcedRenderH) r = (LONG)g_forcedRenderH;
        return origin.y + (LONG)llround((double)r * ch / g_forcedRenderH);
    };
    mapped.left = foldX(rc->left);
    mapped.right = foldX(rc->right);
    mapped.top = foldY(rc->top);
    mapped.bottom = foldY(rc->bottom);
    static int s_log = 0;
    if (cw < (LONG)g_forcedRenderW && s_log < 12)
    {
        ++s_log;
        LOG("fit: menu cursor clip (%ld,%ld,%ld,%ld) -> (%ld,%ld,%ld,%ld)",
            rc->left, rc->top, rc->right, rc->bottom,
            mapped.left, mapped.top, mapped.right, mapped.bottom);
    }
    return g_origClipCursor(&mapped);
}

static HWND WINAPI WindowFromPointHook(POINT pt)
{
    // MCC hit-tests its menu in full render space, so it asks WindowFromPoint
    // about points spread across the *believed* 3204x2310 client -- most of
    // which fall OFF the real, shrunk window (and often off the monitor). Those
    // resolve to "not my window" and the menu item dies; only points that happen
    // to still land on the small top-left slice work. Map ANY point inside the
    // render rectangle back down into the real window client before the OS
    // answers, so every menu item resolves to the game window like it should.
    if (g_fitActive && g_forcedRenderW && g_forcedRenderH)
    {
        POINT origin{};
        LONG cw = 0, ch = 0;
        if (FitClientMetrics(origin, cw, ch))
        {
            const LONG rx = pt.x - origin.x;
            const LONG ry = pt.y - origin.y;
            if (rx >= 0 && ry >= 0 && rx <= (LONG)g_forcedRenderW &&
                ry <= (LONG)g_forcedRenderH)
            {
                pt.x = origin.x +
                       (LONG)llround((double)rx * cw / g_forcedRenderW);
                pt.y = origin.y +
                       (LONG)llround((double)ry * ch / g_forcedRenderH);
            }
        }
    }
    return g_origWindowFromPoint(pt);
}

static HRESULT STDMETHODCALLTYPE CreateSwapChainForHwndHook(IDXGIFactory2* self, IUnknown* device,
    HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreen, IDXGIOutput* restrictOut,
    IDXGISwapChain1** ppSwapChain)
{
    if (pDesc && g_forcedRenderW && g_forcedRenderH && !g_forcedMainSwapchain)
    {
        g_forcedMainSwapchain = true;
        g_gameHwnd = hwnd;
        DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
        LOG("fit: CreateSwapChainForHwnd MCC requested %ux%u scaling=%d hwnd=%p "
            "-> forcing backbuffer %ux%u STRETCH",
            pDesc->Width, pDesc->Height, (int)pDesc->Scaling, (void*)hwnd,
            g_forcedRenderW, g_forcedRenderH);
        desc.Width = g_forcedRenderW;
        desc.Height = g_forcedRenderH;
        desc.Scaling = DXGI_SCALING_STRETCH;
        const HRESULT hr = g_origCreateSwapChainForHwnd(self, device, hwnd, &desc,
                                                        pFullscreen, restrictOut, ppSwapChain);
        if (FAILED(hr))
            LOG("fit: forced CreateSwapChainForHwnd FAILED (hr=0x%08X); the fit did "
                "NOT apply on this machine", (unsigned)hr);
        return hr;
    }
    return g_origCreateSwapChainForHwnd(self, device, hwnd, pDesc, pFullscreen,
                                        restrictOut, ppSwapChain);
}

static HRESULT STDMETHODCALLTYPE CreateSwapChainHook(IDXGIFactory* self, IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
{
    if (pDesc && g_forcedRenderW && g_forcedRenderH && !g_forcedMainSwapchain)
    {
        g_forcedMainSwapchain = true;
        g_gameHwnd = pDesc->OutputWindow;
        DXGI_SWAP_CHAIN_DESC desc = *pDesc;
        LOG("fit: CreateSwapChain(legacy) MCC requested %ux%u -> forcing %ux%u",
            pDesc->BufferDesc.Width, pDesc->BufferDesc.Height,
            g_forcedRenderW, g_forcedRenderH);
        desc.BufferDesc.Width = g_forcedRenderW;
        desc.BufferDesc.Height = g_forcedRenderH;
        const HRESULT hr = g_origCreateSwapChain(self, device, &desc, ppSwapChain);
        if (FAILED(hr))
            LOG("fit: forced CreateSwapChain FAILED (hr=0x%08X); the fit did NOT "
                "apply on this machine", (unsigned)hr);
        return hr;
    }
    return g_origCreateSwapChain(self, device, pDesc, ppSwapChain);
}

// Broad probe paths deliberately NOT installed here; each was retired after
// theory. Re-adding any of them costs frame time for information we already
// have:
//   UpdateSubresource/Map/Unmap - constant census sagged fps ~25%; the
//     exact-match matrix matcher scored zero hits in a full session.
//   CopySubresourceRegion and frame-wide CopyResource learning - learned the
//     scene snapshot pairs; per-eye substitution of both sides left the ghost
//     unchanged. The learning also allocated a full-resolution shadow texture
//     per eye per pair (~25 MB each) and re-copied them every eye pass.
// CopyResource itself is now intercepted only for ODST's exact native-CHUD
// phase scope. It performs one retained-pointer comparison and may substitute
// only the source with the already-owned eye cache; it does no discovery.
//   PSSetShaderResources - cross-pass history discovery promoted 0 targets in
//     two sessions.
//   OMSetRenderTargetsAndUnorderedAccessViews - frame-level RTV discovery
//     promoted 0 targets.
//   PSSetShader/VSSetShader/Draw* - the CHUD steal-and-requad classifier
//     (2026-07-18): removed the native HUD from both eyes, never displayed
//     its hand quad, and its calibration retry loop cost ~30 fps.
// OMSetRenderTargets makes the two eye renders land in separate textures.

static void STDMETHODCALLTYPE OMSetRenderTargetsHook(ID3D11DeviceContext* context, UINT count,
    ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv)
{
    ID3D11RenderTargetView* redirected[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    if (count <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT &&
        VR_RedirectRenderTargets(context, count, rtvs, redirected))
    {
        g_origOMSetRenderTargets(context, count, redirected, dsv);
        return;
    }
    g_origOMSetRenderTargets(context, count, rtvs, dsv);
}

#if HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP
static void STDMETHODCALLTYPE CopyResourceHook(ID3D11DeviceContext* context,
    ID3D11Resource* destination, ID3D11Resource* source)
{
    g_origCopyResource(context, destination,
                       VR_RedirectNativeHudCopySource(source));
}
#endif

// Log-only: record what MCC's swapchain actually is, plus how its backbuffer
// compares to the visible window and the monitor. On a monitor smaller than the
// requested render this shows whether MCC drew the full frame (backbuffer size)
// or a small one, the swap effect + scaling (whether an oversized backbuffer
// STRETCHes or CLIPs on present), and the real window client vs the monitor.
// That is exactly what tells us whether the fit is working. Fires once, then
// only when the backbuffer size changes, so it never spams the hot path.
static void LogSwapchainConfigOnce(IDXGISwapChain* sc)
{
    static UINT s_lastW = 0, s_lastH = 0;
    DXGI_SWAP_CHAIN_DESC d{};
    if (FAILED(sc->GetDesc(&d)))
        return;
    if (d.BufferDesc.Width == s_lastW && d.BufferDesc.Height == s_lastH)
        return;
    s_lastW = d.BufferDesc.Width;
    s_lastH = d.BufferDesc.Height;

    const char* swapEffect = "UNKNOWN";
    switch (d.SwapEffect)
    {
        case DXGI_SWAP_EFFECT_DISCARD: swapEffect = "DISCARD"; break;
        case DXGI_SWAP_EFFECT_SEQUENTIAL: swapEffect = "SEQUENTIAL"; break;
        case DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL: swapEffect = "FLIP_SEQUENTIAL"; break;
        case DXGI_SWAP_EFFECT_FLIP_DISCARD: swapEffect = "FLIP_DISCARD"; break;
        default: break;
    }

    const char* scaling = "n/a";
    IDXGISwapChain1* sc1 = nullptr;
    if (SUCCEEDED(sc->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&sc1)))
    {
        DXGI_SWAP_CHAIN_DESC1 d1{};
        if (SUCCEEDED(sc1->GetDesc1(&d1)))
        {
            switch (d1.Scaling)
            {
                case DXGI_SCALING_STRETCH: scaling = "STRETCH"; break;
                case DXGI_SCALING_NONE: scaling = "NONE"; break;
                case DXGI_SCALING_ASPECT_RATIO_STRETCH: scaling = "ASPECT_RATIO_STRETCH"; break;
                default: break;
            }
        }
        sc1->Release();
    }

    int clientW = 0, clientH = 0;
    HWND hwnd = d.OutputWindow;
    RECT client{};
    if (hwnd && GetClientRect(hwnd, &client))
    {
        clientW = client.right - client.left;
        clientH = client.bottom - client.top;
    }
    int monW = 0, monH = 0, workW = 0, workH = 0;
    if (hwnd)
    {
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi{sizeof(mi)};
        if (GetMonitorInfo(mon, &mi))
        {
            monW = mi.rcMonitor.right - mi.rcMonitor.left;
            monH = mi.rcMonitor.bottom - mi.rcMonitor.top;
            workW = mi.rcWork.right - mi.rcWork.left;
            workH = mi.rcWork.bottom - mi.rcWork.top;
        }
    }

    LOG("swapchain: backbuffer %ux%u fmt=%d swapEffect=%s scaling=%s bufferCount=%u "
        "windowed=%d | window client %dx%d | monitor %dx%d work %dx%d | fit=%d",
        d.BufferDesc.Width, d.BufferDesc.Height, (int)d.BufferDesc.Format,
        swapEffect, scaling, d.BufferCount, d.Windowed ? 1 : 0,
        clientW, clientH, monW, monH, workW, workH,
        g_config.fit_desktop_window ? 1 : 0);
}

// Present1 can forward to Present internally; this depth counter makes sure
// we only run the VR frame once per game frame.
static thread_local int g_presentDepth = 0;

static HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* sc, UINT syncInterval, UINT flags)
{
    const bool topLevel = (g_presentDepth++ == 0);
    const bool runVrFrame = topLevel && !(flags & DXGI_PRESENT_TEST);
    if (runVrFrame)
    {
        LogSwapchainConfigOnce(sc);
        VR_BeforePresent(sc);
    }
    HRESULT hr = g_origPresent(sc, syncInterval, flags);
    if (runVrFrame)
        VR_AfterPresent(sc);
    g_presentDepth--;
    return hr;
}

static HRESULT STDMETHODCALLTYPE Present1Hook(IDXGISwapChain1* sc, UINT syncInterval, UINT flags,
                                              const DXGI_PRESENT_PARAMETERS* params)
{
    const bool topLevel = (g_presentDepth++ == 0);
    const bool runVrFrame = topLevel && !(flags & DXGI_PRESENT_TEST);
    if (runVrFrame)
    {
        LogSwapchainConfigOnce(sc);
        VR_BeforePresent(sc);
    }
    HRESULT hr = g_origPresent1(sc, syncInterval, flags, params);
    if (runVrFrame)
        VR_AfterPresent(sc);
    g_presentDepth--;
    return hr;
}

static HRESULT STDMETHODCALLTYPE ResizeBuffersHook(IDXGISwapChain* sc, UINT bufferCount, UINT width,
                                                   UINT height, DXGI_FORMAT format, UINT flags)
{
    // With the fit on, keep the backbuffer pinned to the full launched render
    // size so a later resize (e.g. triggered when we shrink the visible window to
    // fit the monitor) can't clamp the surface the headset captures back down.
    // With the fit off, g_forcedRenderW/H are 0 and this passes the size through
    // unchanged -- exactly the previous behavior.
    UINT fw = width, fh = height;
    if (g_forcedRenderW && g_forcedRenderH)
    {
        fw = g_forcedRenderW;
        fh = g_forcedRenderH;
    }
    LOG("game resized its swapchain to %ux%u (using %ux%u)", width, height, fw, fh);
    VR_OnResizeBuffers(sc); // we must drop any references to the old backbuffer first
    const HRESULT result =
        g_origResizeBuffers(sc, bufferCount, fw, fh, format, flags);
    VR_AfterResizeBuffers(sc);
    return result;
}

bool InstallD3D11Hooks()
{
    // Config is loaded before this runs. Only when the desktop-window fit is on
    // do we compute the forced render size and install the swapchain-creation /
    // GetClientRect hooks below; with it off, none of that exists and the render
    // path is byte-for-byte the previous behavior.
    const bool fit = g_config.fit_desktop_window;
    if (fit)
        InitForcedRenderSize();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"halo3xr_dummy_window";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED,
                                0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd)
    {
        LOG("dummy window creation failed (%lu)", GetLastError());
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 64;
    sd.BufferDesc.Height = 64;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &ctx);
    if (FAILED(hr))
    {
        LOG("dummy D3D11 device creation failed (hr=0x%08X)", (unsigned)hr);
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** vtbl = *(void***)sc;
    void** contextVtbl = *(void***)ctx;
    bool ok = MH_CreateHook(vtbl[8], (void*)&PresentHook, (void**)&g_origPresent) == MH_OK &&
              MH_CreateHook(vtbl[13], (void*)&ResizeBuffersHook, (void**)&g_origResizeBuffers) == MH_OK &&
              MH_CreateHook(contextVtbl[33], (void*)&OMSetRenderTargetsHook,
                            (void**)&g_origOMSetRenderTargets) == MH_OK;
#if HALOMCCVR_EXPERIMENTAL_ODST_BRINGUP
    ok = ok && MH_CreateHook(contextVtbl[47], (void*)&CopyResourceHook,
                             (void**)&g_origCopyResource) == MH_OK;
#endif

    IDXGISwapChain1* sc1 = nullptr;
    if (SUCCEEDED(sc->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&sc1)))
    {
        void** vtbl1 = *(void***)sc1;
        if (MH_CreateHook(vtbl1[22], (void*)&Present1Hook, (void**)&g_origPresent1) != MH_OK)
            LOG("warning: Present1 hook failed; flip-model swapchains may not be captured");
        sc1->Release();
    }

    if (fit)
    {
        bool forceHookOk = false;
        bool clientRectHookOk = false;

        // Hook the DXGI factory's swapchain-creation entry points so we can force
        // MCC's backbuffer to the full launched render size. All factories in the
        // process share the same vtable implementation, so hooking the dummy
        // device's factory hooks MCC's real creation call -- exactly how the
        // Present/ResizeBuffers vtables above are taken.
        // CreateSwapChain=vtbl[10], CreateSwapChainForHwnd=vtbl[15] (IDXGIFactory2).
        IDXGIDevice* dxgiDev = nullptr;
        IDXGIAdapter* adapter = nullptr;
        IDXGIFactory2* factory2 = nullptr;
        if (SUCCEEDED(dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev)) &&
            SUCCEEDED(dxgiDev->GetAdapter(&adapter)) &&
            SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory2)))
        {
            void** facVtbl = *(void***)factory2;
            if (MH_CreateHook(facVtbl[15], (void*)&CreateSwapChainForHwndHook,
                              (void**)&g_origCreateSwapChainForHwnd) == MH_OK)
                forceHookOk = true;
            else
                LOG("warning: CreateSwapChainForHwnd hook failed; desktop fit inactive");
            if (MH_CreateHook(facVtbl[10], (void*)&CreateSwapChainHook,
                              (void**)&g_origCreateSwapChain) != MH_OK)
                LOG("warning: CreateSwapChain hook failed; legacy desktop fit inactive");
        }
        else
        {
            LOG("warning: could not reach the DXGI factory; desktop fit inactive");
        }
        if (factory2) factory2->Release();
        if (adapter) adapter->Release();
        if (dxgiDev) dxgiDev->Release();

        // Hook user32!GetClientRect so MCC's own resize code, while it handles the
        // WM_SIZE we rewrite, sees the full render size and keeps drawing full.
        // The hook only lies on that thread-local call stack (see the header note),
        // so DXGI's present-time query still downscales the full frame into the
        // real, smaller window.
        if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
        {
            if (void* pGetClientRect = (void*)GetProcAddress(user32, "GetClientRect"))
            {
                if (MH_CreateHook(pGetClientRect, (void*)&GetClientRectHook,
                                  (void**)&g_origGetClientRect) == MH_OK)
                    clientRectHookOk = true;
                else
                    LOG("warning: GetClientRect hook failed; the fit may crop on "
                        "resize-polling titles");
            }

            // Cursor coordinate remap so MCC's native shell / pause menu is
            // navigable in the fitted window (see the block above the hooks).
            // These are supplementary: if any fails, the display fit still works,
            // the menu is just no more navigable than before -- so they do NOT
            // gate g_fitActive.
            InitExeRange();
            void* pGetCursorPos = (void*)GetProcAddress(user32, "GetCursorPos");
            void* pSetCursorPos = (void*)GetProcAddress(user32, "SetCursorPos");
            void* pWindowFromPoint = (void*)GetProcAddress(user32, "WindowFromPoint");
            const bool cursorHooksOk =
                g_exeBase &&
                pGetCursorPos &&
                MH_CreateHook(pGetCursorPos, (void*)&GetCursorPosHook,
                              (void**)&g_origGetCursorPos) == MH_OK &&
                pSetCursorPos &&
                MH_CreateHook(pSetCursorPos, (void*)&SetCursorPosHook,
                              (void**)&g_origSetCursorPos) == MH_OK &&
                pWindowFromPoint &&
                MH_CreateHook(pWindowFromPoint, (void*)&WindowFromPointHook,
                              (void**)&g_origWindowFromPoint) == MH_OK;
            if (!cursorHooksOk)
                LOG("warning: fitted-menu cursor remap hooks failed; the native "
                    "shell/pause menu may not be fully navigable in the fitted window");

            // ClipCursor correction is independent of the remap trio; warn-only.
            if (void* pClipCursor = (void*)GetProcAddress(user32, "ClipCursor"))
            {
                if (MH_CreateHook(pClipCursor, (void*)&ClipCursorHook,
                                  (void**)&g_origClipCursor) != MH_OK)
                    LOG("warning: ClipCursor hook failed; a fitted-menu cursor "
                        "clip could still confine the pointer to the wrong rect");
            }
        }

        // The desktop fit only engages if BOTH levers that keep MCC drawing full
        // are in place. If either failed, we leave the window at full size (the
        // previous overflow behavior) rather than risk shrinking it into a crop.
        g_fitActive = forceHookOk && clientRectHookOk;
        if (!g_fitActive)
            LOG("fit_desktop_window ON but a required hook is missing; leaving the "
                "desktop window full-size (no shrink) to avoid a cropped render");
    }

    sc->Release();
    ctx->Release();
    dev->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (!ok)
    {
        LOG("MinHook could not hook the required D3D render path");
        return false;
    }
    return MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
}
