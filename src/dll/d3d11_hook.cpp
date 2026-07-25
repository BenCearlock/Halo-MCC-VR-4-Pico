#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdlib>
#include <MinHook.h>
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

// --- Desktop/headset resolution decouple ---------------------------------
// The launcher passes -ResX/-ResY sized from resolution_scale; that is BOTH the
// desktop window size AND the render surface the headset captures, because they
// are the same swapchain. On a monitor smaller than that surface, some machines
// clamp the backbuffer down to the panel while the engine still renders the full
// frame -> the headset gets a cropped picture and the gun goes out of alignment
// (Halo 3/ODST force projection from the headset FOV and assume the whole frame
// is present). Other machines honor the oversized backbuffer and look correct.
// We remove that hardware lottery: force MCC's backbuffer to the exact launched
// render size on EVERY machine (so the headset always captures the complete
// frame, guns aligned), and shrink only the visible window to fit the monitor
// (menu.cpp). swapeffect=4 (flip-model) makes an oversized backbuffer legal;
// DXGI_SCALING_STRETCH downscales it into the smaller window on present.
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForHwndFn)(IDXGIFactory2*, IUnknown*, HWND,
    const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*,
    IDXGISwapChain1**);
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainFn)(IDXGIFactory*, IUnknown*,
    DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
static CreateSwapChainForHwndFn g_origCreateSwapChainForHwnd = nullptr;
static CreateSwapChainFn g_origCreateSwapChain = nullptr;
static UINT g_forcedRenderW = 0;
static UINT g_forcedRenderH = 0;
static bool g_forcedMainSwapchain = false; // only the game's own (first) swapchain

static void InitForcedRenderSize()
{
    wchar_t buf[32];
    if (GetEnvironmentVariableW(L"HALO3XR_RENDER_W", buf, 32) > 0)
        g_forcedRenderW = (UINT)_wtoi(buf);
    if (GetEnvironmentVariableW(L"HALO3XR_RENDER_H", buf, 32) > 0)
        g_forcedRenderH = (UINT)_wtoi(buf);
    if (g_forcedRenderW == 0 || g_forcedRenderH == 0)
    {
        // Fallback (DLL run without the current launcher): replicate the
        // launcher's ScaleEven so the two can never disagree. Config is loaded
        // before this runs (dllmain InitThread order).
        const float s = g_config.resolution_scale;
        auto even = [](int base, float sc) -> UINT {
            int v = (int)((float)base * sc + 0.5f);
            if (v & 1) ++v;
            return (UINT)v;
        };
        g_forcedRenderW = even(kNativeRenderWidth, s);
        g_forcedRenderH = even(kNativeRenderHeight, s);
    }
    LOG("STAGE1 decouple: forcing MCC backbuffer to %ux%u on this machine "
        "(headset render size; desktop window fits the monitor separately)",
        g_forcedRenderW, g_forcedRenderH);
}

static HRESULT STDMETHODCALLTYPE CreateSwapChainForHwndHook(IDXGIFactory2* self, IUnknown* device,
    HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreen, IDXGIOutput* restrictOut,
    IDXGISwapChain1** ppSwapChain)
{
    if (pDesc && g_forcedRenderW && g_forcedRenderH && !g_forcedMainSwapchain)
    {
        g_forcedMainSwapchain = true;
        DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
        LOG("STAGE1 CreateSwapChainForHwnd: MCC requested %ux%u scaling=%d hwnd=%p "
            "-> forcing backbuffer %ux%u STRETCH (decouple)",
            pDesc->Width, pDesc->Height, (int)pDesc->Scaling, (void*)hwnd,
            g_forcedRenderW, g_forcedRenderH);
        desc.Width = g_forcedRenderW;
        desc.Height = g_forcedRenderH;
        desc.Scaling = DXGI_SCALING_STRETCH;
        const HRESULT hr = g_origCreateSwapChainForHwnd(self, device, hwnd, &desc,
                                                        pFullscreen, restrictOut, ppSwapChain);
        // No fallback: the backbuffer is forced to full size unconditionally. If a
        // driver ever rejected it, fail LOUDLY here rather than silently reverting
        // to the clamped surface (which is the bug). We return that result as-is.
        if (FAILED(hr))
            LOG("STAGE1 forced CreateSwapChainForHwnd FAILED (hr=0x%08X) - decouple "
                "did NOT apply on this machine", (unsigned)hr);
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
        DXGI_SWAP_CHAIN_DESC desc = *pDesc;
        LOG("STAGE1 CreateSwapChain(legacy): MCC requested %ux%u -> forcing %ux%u (decouple)",
            pDesc->BufferDesc.Width, pDesc->BufferDesc.Height,
            g_forcedRenderW, g_forcedRenderH);
        desc.BufferDesc.Width = g_forcedRenderW;
        desc.BufferDesc.Height = g_forcedRenderH;
        const HRESULT hr = g_origCreateSwapChain(self, device, &desc, ppSwapChain);
        // No fallback (see CreateSwapChainForHwndHook): force unconditionally,
        // fail loudly rather than silently reverting to the clamped surface.
        if (FAILED(hr))
            LOG("STAGE1 forced CreateSwapChain FAILED (hr=0x%08X) - decouple did NOT "
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

// STAGE 0 (log-only, no behavior change): record what MCC's own swapchain
// actually is on this machine, plus how its backbuffer compares to the visible
// window and the monitor. On a monitor smaller than the requested -ResX/-ResY
// this reveals whether MCC clamps the backbuffer (and to what), and whether a
// backbuffer larger than the window would STRETCH or CLIP on present (SwapEffect
// + Scaling). That single fact decides the Stage 1 decouple mechanism. Fires
// once, then only when the backbuffer size changes, so it never spams the hot
// path.
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

    const int expectedW = (int)(kNativeRenderWidth * g_config.resolution_scale + 0.5f);
    const int expectedH = (int)(kNativeRenderHeight * g_config.resolution_scale + 0.5f);

    LOG("STAGE0 swapchain: backbuffer %ux%u fmt=%d swapEffect=%s scaling=%s "
        "bufferCount=%u flags=0x%X windowed=%d | expected render %dx%d "
        "(scale %.2f) | window client %dx%d | monitor %dx%d work %dx%d",
        d.BufferDesc.Width, d.BufferDesc.Height, (int)d.BufferDesc.Format,
        swapEffect, scaling, d.BufferCount, d.Flags, d.Windowed ? 1 : 0,
        expectedW, expectedH, g_config.resolution_scale,
        clientW, clientH, monW, monH, workW, workH);
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
    // Keep the backbuffer pinned to the full launched render size so a later
    // resize (e.g. triggered when we shrink the visible window to fit the
    // monitor) can't clamp the surface the headset captures back down.
    UINT fw = width, fh = height;
    if (g_forcedRenderW && g_forcedRenderH)
    {
        fw = g_forcedRenderW;
        fh = g_forcedRenderH;
    }
    LOG("game resized its swapchain to %ux%u (pinned to %ux%u)", width, height, fw, fh);
    VR_OnResizeBuffers(sc); // we must drop any references to the old backbuffer first
    const HRESULT result =
        g_origResizeBuffers(sc, bufferCount, fw, fh, format, flags);
    VR_AfterResizeBuffers(sc);
    return result;
}

bool InstallD3D11Hooks()
{
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

    // Hook the DXGI factory's swapchain-creation entry points so we can force
    // MCC's backbuffer to the full launched render size (the decouple). All
    // factories in the process share the same vtable implementation, so hooking
    // the dummy device's factory hooks MCC's real creation call too - exactly
    // how the Present/ResizeBuffers vtables above are taken. Non-fatal: if this
    // fails the mod still runs, just without the oversized-backbuffer force.
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
                          (void**)&g_origCreateSwapChainForHwnd) != MH_OK)
            LOG("warning: CreateSwapChainForHwnd hook failed; backbuffer force inactive");
        if (MH_CreateHook(facVtbl[10], (void*)&CreateSwapChainHook,
                          (void**)&g_origCreateSwapChain) != MH_OK)
            LOG("warning: CreateSwapChain hook failed; legacy backbuffer force inactive");
    }
    else
    {
        LOG("warning: could not reach the DXGI factory; backbuffer force inactive");
    }
    if (factory2) factory2->Release();
    if (adapter) adapter->Release();
    if (dxgiDev) dxgiDev->Release();

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
