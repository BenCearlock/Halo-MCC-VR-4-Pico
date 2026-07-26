#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdlib>
#include <cstdint>
#include <intrin.h>
#include <MinHook.h>
#include "sigscan.h"
#include "d3d11_hook.h"
#include "game.h"
#include "vr.h"
#include "../common/config.h"
#include "../common/desktop_fit_logic.h"
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
typedef BOOL(WINAPI* GetCursorPosFn)(LPPOINT);
static CreateSwapChainForHwndFn g_origCreateSwapChainForHwnd = nullptr;
static CreateSwapChainFn g_origCreateSwapChain = nullptr;
static GetClientRectFn g_origGetClientRect = nullptr;
static GetCursorPosFn g_origGetCursorPos = nullptr;
static UINT g_forcedRenderW = 0;
static UINT g_forcedRenderH = 0;
static bool g_forcedMainSwapchain = false; // only force the game's own (first) swapchain
static HWND g_gameHwnd = nullptr;          // captured at swapchain creation
static bool g_fitActive = false;           // set once at startup: fit on AND its hooks installed
// Return address immediately after MCC's uniquely signature-resolved
// GetCursorPos -> ScreenToClient coordinate call. Every other GetCursorPos
// consumer must retain real screen coordinates (notably WindowFromPoint).
static uintptr_t g_mccMenuCursorReturn = 0;
static volatile LONG g_cursorRemapLogs = 0;
static volatile LONG g_cursorOutsideLogs = 0;
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

// Feed MCC's own resize code the full render size (so it keeps drawing full),
// but only on the game's WM_SIZE call stack and only for the game window. DXGI's
// present-time query (render thread, flag clear) still gets the real small size.
static BOOL WINAPI GetClientRectHook(HWND hwnd, LPRECT rc)
{
    const BOOL ok = g_origGetClientRect(hwnd, rc);
    if (ok && rc && g_lieClientToGame && hwnd == g_gameHwnd &&
        g_forcedRenderW && g_forcedRenderH)
    {
        rc->left = 0;
        rc->top = 0;
        rc->right = (LONG)g_forcedRenderW;
        rc->bottom = (LONG)g_forcedRenderH;
    }
    return ok;
}

// Resolve the one MCC cursor read that immediately feeds ScreenToClient and two
// stored float coordinates. The retail executable has other GetCursorPos
// consumers:
// one feeds WindowFromPoint (window ownership) and another performs a separate
// active-window/DPI conversion. Rewriting those process-wide was the a440654
// failure: synthetic render-space points can be off-screen, so WindowFromPoint
// can stop recognizing MCC and corrupt native window/input routing.
//
// The AOB contains both imported calls and is unique in the pinned retail
// executable. Its RIP-relative IAT displacements and branch displacement are
// wildcarded. Zero/multiple matches or unexpected import targets fail open: the
// fitted window is not activated.
static bool ResolveMccMenuCursorCaller(
    void* getCursorPosApi, void* screenToClientApi)
{
    static constexpr char kCursorToClientPattern[] =
        "4C 89 7C 24 38 48 8D 4C 24 38 "
        "FF 15 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? "
        "48 8D 54 24 38 49 8B 8C 24 C0 00 00 00 "
        "FF 15 ?? ?? ?? ?? 85 C0";
    static constexpr size_t kGetCursorCallOffset = 10;
    static constexpr size_t kGetCursorReturnOffset = 16;
    static constexpr size_t kScreenToClientCallOffset = 37;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!executable)
        return false;
    const uintptr_t base = reinterpret_cast<uintptr_t>(executable);
    const IMAGE_DOS_HEADER* dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return false;
    const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        base + static_cast<uintptr_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.SizeOfImage == 0)
    {
        return false;
    }
    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    const uintptr_t imageEnd = base + imageSize;

    const uintptr_t match = sig::Find(base, imageSize, kCursorToClientPattern);
    if (!match)
    {
        LOG("fit: MCC menu cursor signature missing; desktop fit remains inactive");
        return false;
    }
    const uintptr_t next = match + 1;
    if (next < imageEnd &&
        sig::Find(next, static_cast<size_t>(imageEnd - next),
                  kCursorToClientPattern))
    {
        LOG("fit: MCC menu cursor signature is ambiguous; desktop fit remains inactive");
        return false;
    }

    auto importedCallTarget = [base, imageEnd](uintptr_t call) -> void* {
        const BYTE* code = reinterpret_cast<const BYTE*>(call);
        if (code[0] != 0xFF || code[1] != 0x15)
            return nullptr;
        const uintptr_t slot = sig::RipTarget(call + 2, call + 6);
        if (slot < base || slot > imageEnd - sizeof(void*))
            return nullptr;
        return *reinterpret_cast<void* const*>(slot);
    };

    if (importedCallTarget(match + kGetCursorCallOffset) != getCursorPosApi ||
        importedCallTarget(match + kScreenToClientCallOffset) !=
            screenToClientApi)
    {
        LOG("fit: MCC menu cursor signature import identity failed; desktop fit "
            "remains inactive");
        return false;
    }

    g_mccMenuCursorReturn = match + kGetCursorReturnOffset;
    LOG("fit: MCC menu cursor input resolved by unique signature at executable+0x%llX",
        static_cast<unsigned long long>(match - base));
    return true;
}

// The selected MCC input-record read starts with an absolute screen point, then
// converts it to client coordinates and stores the result as two floats. Scale
// only that caller for the fitted/full-render geometry. All other callers,
// including MCC's WindowFromPoint ownership check and the mod's ImGui backend,
// continue to receive the real physical cursor.
static BOOL WINAPI GetCursorPosHook(LPPOINT p)
{
    const BOOL ok = g_origGetCursorPos(p);
    if (!ok || !p || !g_fitActive || !g_gameHwnd || !g_forcedRenderW || !g_forcedRenderH)
        return ok;
    if (reinterpret_cast<uintptr_t>(_ReturnAddress()) != g_mccMenuCursorReturn)
        return ok;
    RECT rc{};
    if (!g_origGetClientRect(g_gameHwnd, &rc))
        return ok;
    const int cw = rc.right - rc.left;
    const int ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0)
        return ok;
    POINT origin{0, 0};
    if (!ClientToScreen(g_gameHwnd, &origin))
        return ok;
    if (p->x < origin.x || p->x >= origin.x + cw ||
        p->y < origin.y || p->y >= origin.y + ch)
    {
        const LONG logIndex = InterlockedIncrement(&g_cursorOutsideLogs);
        if (logIndex <= 3)
        {
            LOG("fit: MCC menu cursor is outside fitted client: screen=(%ld,%ld) "
                "clientOrigin=(%ld,%ld) client=%dx%d",
                p->x, p->y, origin.x, origin.y, cw, ch);
        }
        return ok;
    }

    int mappedX = 0;
    int mappedY = 0;
    if (!MapDesktopFitPoint(
            p->x - origin.x, p->y - origin.y, cw, ch,
            static_cast<int>(g_forcedRenderW),
            static_cast<int>(g_forcedRenderH), mappedX, mappedY))
    {
        return ok;
    }
    const LONG rx = origin.x + mappedX;
    const LONG ry = origin.y + mappedY;
    if (cw != static_cast<int>(g_forcedRenderW) ||
        ch != static_cast<int>(g_forcedRenderH))
    {
        const LONG logIndex = InterlockedIncrement(&g_cursorRemapLogs);
        if (logIndex <= 3)
        {
            LOG("fit: menu cursor remap real (%ld,%ld) -> (%ld,%ld) "
                "[client %dx%d render %ux%u]",
                p->x, p->y, rx, ry, cw, ch,
                g_forcedRenderW, g_forcedRenderH);
        }
    }
    p->x = rx;
    p->y = ry;
    return ok;
}

static HRESULT STDMETHODCALLTYPE CreateSwapChainForHwndHook(IDXGIFactory2* self, IUnknown* device,
    HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreen, IDXGIOutput* restrictOut,
    IDXGISwapChain1** ppSwapChain)
{
    if (g_fitActive && pDesc && g_forcedRenderW && g_forcedRenderH &&
        !g_forcedMainSwapchain)
    {
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
        else
        {
            g_forcedMainSwapchain = true;
            g_gameHwnd = hwnd;
        }
        return hr;
    }
    return g_origCreateSwapChainForHwnd(self, device, hwnd, pDesc, pFullscreen,
                                        restrictOut, ppSwapChain);
}

static HRESULT STDMETHODCALLTYPE CreateSwapChainHook(IDXGIFactory* self, IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
{
    if (g_fitActive && pDesc && g_forcedRenderW && g_forcedRenderH &&
        !g_forcedMainSwapchain)
    {
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
        else
        {
            g_forcedMainSwapchain = true;
            g_gameHwnd = pDesc->OutputWindow;
        }
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
    // With the fit inactive this passes the size through unchanged, even when
    // config supplied a forced size but a required display/input proof failed.
    UINT fw = width, fh = height;
    if (g_fitActive && g_forcedRenderW && g_forcedRenderH)
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
        bool cursorHookOk = false;

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
            const bool hwndHookOk =
                MH_CreateHook(facVtbl[15], (void*)&CreateSwapChainForHwndHook,
                              (void**)&g_origCreateSwapChainForHwnd) == MH_OK;
            const bool legacyHookOk =
                MH_CreateHook(facVtbl[10], (void*)&CreateSwapChainHook,
                              (void**)&g_origCreateSwapChain) == MH_OK;
            if (!hwndHookOk)
                LOG("warning: CreateSwapChainForHwnd hook failed; desktop fit inactive");
            if (!legacyHookOk)
                LOG("warning: CreateSwapChain hook failed; legacy desktop fit inactive");
            // MCC currently uses the legacy path, but both entry points are
            // required so a runtime/window recreation cannot cross into an
            // unforced swapchain after the visible window has been shrunk.
            forceHookOk = hwndHookOk && legacyHookOk;
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

            // Remap only the signature-resolved cursor-to-client coordinate
            // read. This input transform is required: shrinking without it
            // produces a visible fit whose native menu cannot be operated.
            void* pGetCursorPos = reinterpret_cast<void*>(
                GetProcAddress(user32, "GetCursorPos"));
            void* pScreenToClient = reinterpret_cast<void*>(
                GetProcAddress(user32, "ScreenToClient"));
            if (pGetCursorPos && pScreenToClient &&
                ResolveMccMenuCursorCaller(pGetCursorPos, pScreenToClient))
            {
                if (MH_CreateHook(pGetCursorPos, (void*)&GetCursorPosHook,
                                  (void**)&g_origGetCursorPos) == MH_OK)
                {
                    cursorHookOk = true;
                }
                else
                {
                    LOG("warning: scoped GetCursorPos hook failed; desktop fit "
                        "remains inactive");
                }
            }
        }

        // The desktop fit engages only when the two display levers and the
        // signature-scoped cursor-coordinate transform are all in place. A
        // missing input lever leaves stock window geometry instead of reproducing a
        // fitted but inoperable menu.
        g_fitActive = forceHookOk && clientRectHookOk && cursorHookOk;
        if (!g_fitActive)
            LOG("fit_desktop_window ON but a required display/input hook is missing; "
                "leaving the desktop window full-size");
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
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
    {
        g_fitActive = false;
        return false;
    }
    return true;
}
