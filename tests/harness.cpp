// Logic tests for premiere-pro-theme.wh.cpp, built with the Windhawk API
// stubbed out (-DWH_EDITING) and the setting and hook calls redirected to the
// doubles below. tests/run.ps1 builds and runs them.

#include <windhawk_api.h>

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>
#include <vector>

struct FakeInt {
    const wchar_t* name;
    int value;
};

static FakeInt g_fakeInts[] = {
    {L"strength", 100},      {L"ceiling", 28},  {L"dvauiHook", 1},
    {L"brushHook", 1},       {L"monitorBand", 1},
    {L"nativeDarkMode", 1},
    {L"menuHook", 1},        {L"gdiHook", 1},
    {L"uxpPanels", 1},       {L"highlight", 1},
};

static int FakeGetIntSetting(PCWSTR name, ...) {
    for (const FakeInt& s : g_fakeInts) {
        if (wcscmp(s.name, name) == 0) {
            return s.value;
        }
    }
    return 0;
}

static void SetFakeInt(PCWSTR name, int value) {
    for (FakeInt& s : g_fakeInts) {
        if (wcscmp(s.name, name) == 0) {
            s.value = value;
        }
    }
}

static const wchar_t* g_fakePalette = L"onyx";

struct FakeString {
    const wchar_t* name;
    const wchar_t* value;
};

// One entry per field of the customTheme group, under its full setting name.
static FakeString g_fakeTheme[] = {
    {L"customTheme.base", L""},         {L"customTheme.panel", L""},
    {L"customTheme.surface", L""},      {L"customTheme.raised", L""},
    {L"customTheme.border", L""},       {L"customTheme.text", L""},
    {L"customTheme.accent", L""},       {L"customTheme.disabledText", L""},
    {L"customTheme.highlight", L""},    {L"customTheme.monitor", L""},
};

static PCWSTR FakeGetStringSetting(PCWSTR name, ...) {
    if (wcscmp(name, L"palette") == 0) {
        return g_fakePalette;
    }

    for (const FakeString& field : g_fakeTheme) {
        if (wcscmp(field.name, name) == 0) {
            return field.value;
        }
    }

    return L"";
}

// By the bare key, without the "customTheme." the group's name adds.
static void SetFakeTheme(const wchar_t* key, const wchar_t* value) {
    for (FakeString& field : g_fakeTheme) {
        if (wcscmp(field.name + wcslen(L"customTheme."), key) == 0) {
            field.value = value;
            return;
        }
    }
}

static void ClearFakeTheme() {
    for (FakeString& field : g_fakeTheme) {
        field.value = L"";
    }
}

static void FakeFreeStringSetting(PCWSTR) {}

static int g_hookCalls = 0;

static BOOL FakeSetFunctionHook(void*, void*, void**) {
    g_hookCalls++;
    return TRUE;
}

// Wh_Log calls that report a missing function, and the last one's label.
static int g_absentLogs = 0;
static const wchar_t* g_lastAbsent = nullptr;
static int g_themeLogs = 0;  // lines about something wrong with a field
static int g_shareLogs = 0;  // the theme written out for sharing
static wchar_t g_sharedLine[2048];
static const wchar_t* g_lastShared = nullptr;

static void FakeLog(PCWSTR format, ...) {
    // "custom theme: ..." is something wrong with a field; the line that
    // starts "custom theme, to share" is the theme itself, written out.
    if (wcsncmp(format, L"custom theme: ", 14) == 0) {
        g_themeLogs++;
        return;
    }

    if (wcsncmp(format, L"custom theme, to share", 22) == 0) {
        // The colors are what matters here, so the line is expanded.
        va_list shared;
        va_start(shared, format);
        vswprintf(g_sharedLine, ARRAYSIZE(g_sharedLine), format, shared);
        va_end(shared);

        g_shareLogs++;
        g_lastShared = g_sharedLine;
        return;
    }

    if (wcscmp(format, L"absent in this version: %s") != 0) {
        return;
    }

    va_list args;
    va_start(args, format);
    g_lastAbsent = va_arg(args, const wchar_t*);
    va_end(args);
    g_absentLogs++;
}

// A dvaui stand-in that exports exactly the names in g_fakeExports.
static const HMODULE kFakeDvaui = reinterpret_cast<HMODULE>(0x10000);
static std::vector<const char*> g_fakeExports;
static FARPROC(WINAPI* const RealGetProcAddress)(HMODULE, LPCSTR) = GetProcAddress;

static INT_PTR WINAPI FakeExport() {
    return 0;
}

static FARPROC WINAPI FakeGetProcAddress(HMODULE module, LPCSTR name) {
    if (module != kFakeDvaui) {
        return RealGetProcAddress(module, name);
    }

    for (const char* exported : g_fakeExports) {
        if (std::strcmp(exported, name) == 0) {
            return FakeExport;
        }
    }

    return nullptr;
}

#define Wh_GetIntSetting FakeGetIntSetting
#define Wh_GetStringSetting FakeGetStringSetting
#define Wh_FreeStringSetting FakeFreeStringSetting
#define Wh_SetFunctionHook FakeSetFunctionHook

#include <windhawk_utils.h>

#define Wh_Log FakeLog
#define GetProcAddress FakeGetProcAddress

#include "../premiere-pro-theme.wh.cpp"

static int g_failures = 0;

#define CHECK(cond)                                             \
    do {                                                        \
        if (!(cond)) {                                          \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond); \
            g_failures++;                                       \
        }                                                       \
    } while (0)

static DvaColorRGBA Gray(int level) {
    float v = level / 255.0f;
    return {v, v, v, 1.0f};
}

static void Reload() {
    LoadSettings();
    InterlockedIncrement(&g_generation);
    RecomputeColorTable(false);
}

// Called from a frame below the caller's, as the real checks always are.
__attribute__((noinline)) static bool InScopeFromBelow() {
    volatile char pad[64];
    pad[0] = 0;
    return InContentScope() && pad[0] == 0;
}

__attribute__((noinline)) static bool PaintFromBelow(const DvaColorRGBA* in,
                                                    DvaColorRGBA* out) {
    volatile char pad[64];
    pad[0] = 0;
    return ConvertForPaint(in, out) && pad[0] == 0;
}

// A ContentScope whose destructor never runs, as after a foreign unwind.
__attribute__((noinline)) static void AbandonScope(int levels) {
    volatile char pad[512];
    pad[0] = 0;

    if (levels > 0) {
        AbandonScope(levels - 1);
        pad[1] = 0;
        return;
    }

    alignas(ContentScope) unsigned char storage[sizeof(ContentScope)];
    new (storage) ContentScope(__builtin_frame_address(0));
}

static bool g_drawSawScope = false;

static void FakeNodeDraw(const void*, void*, bool, const void*) {
    g_drawSawScope = InContentScope();
}

static void SeedClass(uintptr_t vtable, bool content) {
    size_t start = FibonacciIndex<kNodeClassSlots>(vtable >> 3);

    for (size_t probe = 0; probe < 32; probe++) {
        size_t i = (start + probe) & (kNodeClassSlots - 1);

        if (!g_nodeClasses[i]) {
            g_nodeClasses[i] = static_cast<LONG64>(vtable | 2 | (content ? 1 : 0));
            return;
        }
    }
}

static void TestFibonacciIndex() {
    const uint64_t values[] = {0, 1, 0x7FFE0000ull, 0x123456789ABCDEF0ull,
                               0xFFFFFFFFFFFFFFFFull};

    for (uint64_t v : values) {
        CHECK(FibonacciIndex<1024>(v) < 1024);
        CHECK(FibonacciIndex<2048>(v) < 2048);
        CHECK(FibonacciIndex<8192>(v) < 8192);
    }

    // Consecutive 16-byte-aligned keys, as slot addresses are, spread out.
    bool used[1024] = {};
    size_t distinct = 0;

    for (uint64_t i = 0; i < 1024; i++) {
        size_t index = FibonacciIndex<1024>((0x7FF600000000ull + i * 16) >> 4);

        if (!used[index]) {
            used[index] = true;
            distinct++;
        }
    }

    CHECK(distinct > 900);
}

static void TestColorTable() {
    LoadSettings();
    CHECK(AllocateSlots());

    static DvaColorRGBA panel = Gray(0x1D);
    static DvaColorRGBA header = Gray(0x26);

    const DvaColorRGBA* p = ConvertColorRef(&panel);
    CHECK(p != &panel);
    CHECK(IsConvertedSlot(p));
    CHECK(!SameColor(*p, panel));

    DvaColorRGBA out{};
    CHECK(!PaintFromBelow(p, &out));  // already converted

    // An address inside a slot that is not its dst is left alone.
    auto slotStart = reinterpret_cast<const char*>(p) - offsetof(ColorSlot, dst);
    CHECK(IsConvertedSlot(slotStart));

    // "Premiere interface" off at runtime, "Direct fills" still on.
    SetFakeInt(L"dvauiHook", 0);
    Reload();
    CHECK(SameColor(*p, panel));
    CHECK(!IsConvertedSlot(p));
    CHECK(PaintFromBelow(p, &out));
    CHECK(!SameColor(out, panel));

    SetFakeInt(L"dvauiHook", 1);
    Reload();
    CHECK(IsConvertedSlot(p));

    // A write computed under older settings leaves a newer slot alone.
    DvaColorRGBA current = *p;
    DvaColorRGBA bogus = Gray(0x80);
    LONG generation = g_generation;

    const DvaColorRGBA* again = StoreColor(reinterpret_cast<uintptr_t>(&panel),
                                           panel, bogus, generation - 1);
    CHECK(again == p);
    CHECK(SameColor(*p, current));

    // A slot filled under older settings is brought up to date by its filler.
    DvaColorRGBA expected{};
    CHECK(ConvertDvaColor(header, &expected));

    const DvaColorRGBA* q = StoreColor(reinterpret_cast<uintptr_t>(&header),
                                       header, bogus, generation - 1);
    CHECK(q != nullptr);
    CHECK(q && SameColor(*q, expected));

    // Unload hands every slot its original colour back.
    RecomputeColorTable(true);
    CHECK(SameColor(*p, panel));
    CHECK(q && SameColor(*q, header));
}

static void TestContentScope() {
    CHECK(!InScopeFromBelow());

    {
        ContentScope outer(__builtin_frame_address(0));
        CHECK(InScopeFromBelow());

        {
            ContentScope inner(__builtin_frame_address(0));
            CHECK(g_contentDepth == 2);
            CHECK(InScopeFromBelow());
        }

        CHECK(InScopeFromBelow());
    }

    CHECK(!InScopeFromBelow());
    CHECK(g_contentDepth == 0);

    // Inside a scope the brush layer leaves raw colours alone.
    static DvaColorRGBA raw = Gray(0x30);
    DvaColorRGBA out{};
    CHECK(PaintFromBelow(&raw, &out));

    {
        ContentScope scope(__builtin_frame_address(0));
        CHECK(!PaintFromBelow(&raw, &out));
    }

    // An abandoned scope expires at the first check made from above it.
    AbandonScope(4);
    CHECK(g_contentDepth == 1);
    CHECK(!InScopeFromBelow());
    CHECK(g_contentDepth == 0);

    // A new scope opened above an abandoned one replaces it.
    AbandonScope(4);

    {
        ContentScope scope(__builtin_frame_address(0));
        CHECK(g_contentDepth == 1);
        CHECK(InScopeFromBelow());
    }

    CHECK(g_contentDepth == 0);
    CHECK(!InScopeFromBelow());
}

static void TestNodeDraw() {
    alignas(16) static const uintptr_t vtables[4] = {};
    uintptr_t content = reinterpret_cast<uintptr_t>(&vtables[0]);
    uintptr_t plain = reinterpret_cast<uintptr_t>(&vtables[2]);

    SeedClass(content, true);
    SeedClass(plain, false);

    const uintptr_t contentNode[1] = {content};
    const uintptr_t plainNode[1] = {plain};

    UiDrawSelf_Original = FakeNodeDraw;

    NodeDraw_Hook<&UiDrawSelf_Original>(contentNode, nullptr, false, nullptr);
    CHECK(g_drawSawScope);
    CHECK(g_contentDepth == 0);

    NodeDraw_Hook<&UiDrawSelf_Original>(plainNode, nullptr, false, nullptr);
    CHECK(!g_drawSawScope);

    // With neither setting a scope affects on, the lookup is skipped.
    SetFakeInt(L"brushHook", 0);
    SetFakeInt(L"gdiHook", 0);
    LoadSettings();

    NodeDraw_Hook<&UiDrawSelf_Original>(contentNode, nullptr, false, nullptr);
    CHECK(!g_drawSawScope);

    SetFakeInt(L"brushHook", 1);
    SetFakeInt(L"gdiHook", 1);
    LoadSettings();

    /*
        A class decision keys on a vtable address, and that address belongs to
        the module the vtable is in. Mapping or unmapping anything throws the
        whole table away, so an address the loader hands to something else
        cannot answer for what used to be there.
    */
    SeedClass(content, true);
    NodeDraw_Hook<&UiDrawSelf_Original>(contentNode, nullptr, false, nullptr);
    CHECK(g_drawSawScope);

    const uintptr_t base = 0x520000000000;
    AddModuleRange(base, base + 0x1000);  // bumps the module generation

    // The seeded answer is gone, and the fake vtable resolves to nothing.
    NodeDraw_Hook<&UiDrawSelf_Original>(contentNode, nullptr, false, nullptr);
    CHECK(!g_drawSawScope);

    DropModuleRange(base);
}

static void TestSafeMode() {
    SetFakeInt(L"dvauiHook", 0);
    SetFakeInt(L"brushHook", 0);
    SetFakeInt(L"gdiHook", 0);
    LoadSettings();

    CHECK(!WantsPremiereHooks());

    int before = g_hookCalls;
    CHECK(!HookLoadedModules());
    CHECK(g_hookCalls == before);

    SetFakeInt(L"gdiHook", 1);
    LoadSettings();
    CHECK(WantsPremiereHooks());

    SetFakeInt(L"dvauiHook", 1);
    SetFakeInt(L"brushHook", 1);
    LoadSettings();
}

static void TestDarkModeAttribute() {
    g_buildNumber = 18363;
    CHECK(ImmersiveDarkModeAttribute() == 19);

    g_buildNumber = 18985;
    CHECK(ImmersiveDarkModeAttribute() == 20);

    g_buildNumber = 22631;
    CHECK(ImmersiveDarkModeAttribute() == 20);
}

static void TestThemedWindows() {
    HWND both = reinterpret_cast<HWND>(0x1230);
    HWND classOnly = reinterpret_cast<HWND>(0x4560);

    RememberThemedWindow(both, kThemedClass | kThemedFrame);
    RememberThemedWindow(classOnly, kThemedClass);

    ForgetThemedClass(both);
    ForgetThemedClass(classOnly);

    CHECK(g_themedWindows.count(both) == 1);
    CHECK(g_themedWindows[both] == kThemedFrame);
    CHECK(g_themedWindows.count(classOnly) == 0);

    // The mod's own apply records the window again after its call.
    RememberThemedWindow(classOnly, kThemedClass);
    CHECK(g_themedWindows.count(classOnly) == 1);

    g_themedWindows.clear();
}

static void TestMenuThemes() {
    HTHEME menu = reinterpret_cast<HTHEME>(0x100);
    HTHEME other = reinterpret_cast<HTHEME>(0x200);

    ForgetMenuTheme(other);  // nothing seen yet
    RememberMenuTheme(menu);

    ForgetMenuTheme(other);
    CHECK(IsMenuTheme(menu));

    ForgetMenuTheme(menu);
    CHECK(!IsMenuTheme(menu));

    // TrackMenuTheme registers a "Menu" theme and passes any class through.
    HTHEME tracked = reinterpret_cast<HTHEME>(0x300);
    CHECK(TrackMenuTheme(tracked, L"ScrollBar") == tracked);
    CHECK(!IsMenuTheme(tracked));
    CHECK(TrackMenuTheme(tracked, L"Menu") == tracked);
    CHECK(IsMenuTheme(tracked));

    // uxtheme recycles that value for another class: it must be evicted, so
    // PaintMenuPart never paints that control in menu colors.
    CHECK(TrackMenuTheme(tracked, L"ScrollBar") == tracked);
    CHECK(!IsMenuTheme(tracked));

    // A null handle is passed straight through, before any Remember/Forget.
    CHECK(TrackMenuTheme(nullptr, L"Menu") == nullptr);
}

static void TestGdiProduced() {
    LoadSettings();

    const Settings& s = CurrentSettings();
    COLORREF raw = RGB(0x30, 0x30, 0x30);
    COLORREF once = ConvertGdiColor(s, raw);
    CHECK(once != raw);
    CHECK(ConvertGdiColor(s, once) == once);  // not converted a second time

    // A colour a theme function produced, turned into a COLORREF.
    static DvaColorRGBA divider = Gray(0x3A);
    const DvaColorRGBA* themed = ConvertColorRef(&divider);
    CHECK(themed != &divider);

    COLORREF asGdi = RGB(static_cast<int>(themed->r * 255.0f + 0.5f),
                         static_cast<int>(themed->g * 255.0f + 0.5f),
                         static_cast<int>(themed->b * 255.0f + 0.5f));
    CHECK(ConvertGdiColor(s, asGdi) == asGdi);
}

static void TestKnownModules() {
    LONG before = g_moduleRangeCount;
    NoteKnownModules();
    CHECK(g_moduleRangeCount > before);
    CHECK(IsAdobeUICaller(reinterpret_cast<void*>(&TestKnownModules)));
    CHECK(!IsAdobeUICaller(reinterpret_cast<void*>(0x10)));  // the cached hit is exact

    LONG after = g_moduleRangeCount;
    NoteKnownModules();
    CHECK(g_moduleRangeCount == after);  // deduplicated
}

/*
    A dva module that unloads stops being recognized, and the per-thread cache
    of the last hit goes with it — otherwise whatever the loader maps at that
    base next would paint in the palette.
*/
static void TestModuleRangeUnload() {
    const uintptr_t base = 0x500000000000;
    void* inside = reinterpret_cast<void*>(base + 0x100);

    AddModuleRange(base, base + 0x1000);
    CHECK(IsAdobeUICaller(inside));
    CHECK(IsAdobeUICaller(inside));  // again, now off the cached range

    DropModuleRange(base);
    CHECK(!IsAdobeUICaller(inside));

    // Mapped again at the same base: the entry is revived, not duplicated.
    LONG count = g_moduleRangeCount;
    AddModuleRange(base, base + 0x2000);
    CHECK(g_moduleRangeCount == count);
    CHECK(IsAdobeUICaller(reinterpret_cast<void*>(base + 0x1800)));

    DropModuleRange(base);
    DropModuleRange(base);  // already gone
    CHECK(!IsAdobeUICaller(inside));

    // The same, through the loader notification the mod actually listens to.
    wchar_t dva[] = L"dvaunittest.dll";
    LdrUnicodeString name{};
    name.buffer = dva;
    name.length = static_cast<USHORT>(wcslen(dva) * sizeof(wchar_t));
    name.maximumLength = name.length;

    LdrDllLoadedData data{};
    data.baseDllName = &name;
    data.dllBase = reinterpret_cast<PVOID>(base);
    data.sizeOfImage = 0x1000;

    OnDllNotification(kLdrDllLoaded, &data, nullptr);
    CHECK(IsAdobeUICaller(inside));

    OnDllNotification(kLdrDllUnloaded, &data, nullptr);
    CHECK(!IsAdobeUICaller(inside));

    // A module that is not Adobe's is ignored, whichever way it goes.
    wchar_t other[] = L"vendor.dll";
    name.buffer = other;
    name.length = static_cast<USHORT>(wcslen(other) * sizeof(wchar_t));
    name.maximumLength = name.length;

    OnDllNotification(kLdrDllLoaded, &data, nullptr);
    CHECK(!IsAdobeUICaller(inside));
}

/*
    DisplaySurface is tracked apart from the Adobe UI set, and its range is
    cleared on unload — the monitor band reads it from every D3D12 command
    list in the process, so a stale range would scope that bookkeeping to
    whatever is mapped there next.
*/
static void TestDisplaySurfaceRange() {
    const uintptr_t base = 0x510000000000;
    void* inside = reinterpret_cast<void*>(base + 0x100);

    CHECK(!IsDisplaySurfaceCall(inside));

    SetDisplaySurfaceRange(base, base + 0x60000);
    CHECK(IsDisplaySurfaceCall(inside));
    CHECK(!IsDisplaySurfaceCall(reinterpret_cast<void*>(base + 0x60000)));

    CHECK(!IsDisplaySurfaceCall(reinterpret_cast<void*>(base - 1)));

    SetDisplaySurfaceRange(0, 0);
    CHECK(!IsDisplaySurfaceCall(inside));

    // The same, through the loader notification.
    wchar_t name[] = L"DisplaySurface.dll";
    LdrUnicodeString unicodeName{};
    unicodeName.buffer = name;
    unicodeName.length = static_cast<USHORT>(wcslen(name) * sizeof(wchar_t));
    unicodeName.maximumLength = unicodeName.length;

    LdrDllLoadedData data{};
    data.baseDllName = &unicodeName;
    data.dllBase = reinterpret_cast<PVOID>(base);
    data.sizeOfImage = 0x60000;

    OnDllNotification(kLdrDllLoaded, &data, nullptr);
    CHECK(IsDisplaySurfaceCall(inside));

    // It is not Adobe UI: the GDI layer must not recolor what it paints.
    CHECK(!IsAdobeUICaller(inside));

    OnDllNotification(kLdrDllUnloaded, &data, nullptr);
    CHECK(!IsDisplaySurfaceCall(inside));
}

// The float4 a monitor draw carries, as the raw words the caller passed.
static void SetRootColor(MonitorCommandState* state, float r, float g, float b,
                         float a) {
    const float rgba[4] = {r, g, b, a};

    for (int i = 0; i < 4; i++) {
        std::memcpy(&state->root1Color[i], &rgba[i], sizeof(UINT));
    }

    state->hasRoot1Color = true;
}

static float RootColorChannel(const MonitorCommandState* state, int i) {
    float value = 0.0f;
    std::memcpy(&value, &state->root1Color[i], sizeof(value));
    return value;
}

// What the mod wrote back, and how many times.
static int g_rootWrites = 0;
static UINT g_rootWritten[4];
static UINT g_rootWrittenIndex = 0;
static UINT g_rootWrittenCount = 0;

static void STDMETHODCALLTYPE FakeSetRootConstants(ID3D12GraphicsCommandList*,
                                                   UINT rootParameterIndex,
                                                   UINT num32BitValuesToSet,
                                                   const void* srcData,
                                                   UINT destOffset) {
    g_rootWrites++;
    g_rootWrittenIndex = rootParameterIndex;
    g_rootWrittenCount = num32BitValuesToSet;
    (void)destOffset;

    auto values = static_cast<const UINT*>(srcData);

    for (UINT i = 0; i < num32BitValuesToSet && i < 4; i++) {
        g_rootWritten[i] = values[i];
    }
}

// The rest of the layer's trampolines, which MonitorBandHooksReady wants set.
static void STDMETHODCALLTYPE FakeSetViewports(ID3D12GraphicsCommandList*, UINT,
                                               const D3D12_VIEWPORT*) {}

static void STDMETHODCALLTYPE FakeSetScissors(ID3D12GraphicsCommandList*, UINT,
                                              const D3D12_RECT*) {}

static HRESULT STDMETHODCALLTYPE FakeReset(ID3D12GraphicsCommandList*,
                                           ID3D12CommandAllocator*,
                                           ID3D12PipelineState*) {
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE FakeClose(ID3D12GraphicsCommandList*) {
    return S_OK;
}

static float WrittenChannel(int i) {
    float value = 0.0f;
    std::memcpy(&value, &g_rootWritten[i], sizeof(value));
    return value;
}

/*
    Which colors are the band's.

    The gray is not a fixed value to compare against. Stock it is #1D1D1D, but
    the band's color comes from the theme before DisplaySurface paints with it,
    so with "Premiere interface" on it arrives converted — and under a palette
    with a hue in the ramp, that conversion is not neutral.

    That is what this round fixed. The test used to require neutrality, which
    ten of the fifteen palettes could never satisfy; Miku, the one palette that
    ships a band color of its own, was among them. The case below pins it: what
    #1D1D1D becomes under Miku is #0E2128, and it has to be recognized.

    Not black is the part that carries the weight. Everything the monitor shows
    through the picture is black: the empty sequence frame over a gap in the
    timeline, and the backing a clip with an alpha channel is composited onto.
    Both were lost to earlier rules, and both are what this keeps.
*/
static void TestMonitorBandColor() {
    MonitorCommandState state{};

    g_fakePalette = L"onyx";
    ClearFakeTheme();
    SetFakeInt(L"strength", 100);
    SetFakeInt(L"ceiling", 28);
    LoadSettings();

    const Settings& onyx = CurrentSettings();

    CHECK(!IsMonitorBandColor(onyx, state));  // nothing set yet

    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
    CHECK(IsMonitorBandColor(onyx, state));

    SetRootColor(&state, 0x0E / 255.0f, 0x0E / 255.0f, 0x0E / 255.0f, 1.0f);
    CHECK(IsMonitorBandColor(onyx, state));

    // Black is the backing, never the band.
    SetRootColor(&state, 0.0f, 0.0f, 0.0f, 1.0f);
    CHECK(!IsMonitorBandColor(onyx, state));

    // Light is content.
    SetRootColor(&state, 0.5f, 0.5f, 0.5f, 1.0f);
    CHECK(!IsMonitorBandColor(onyx, state));

    // A hue Onyx could never have produced is somebody else's draw.
    SetRootColor(&state, 0x20 / 255.0f, 0x0C / 255.0f, 0x0C / 255.0f, 1.0f);
    CHECK(!IsMonitorBandColor(onyx, state));

    // And anything the picture shows through.
    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 0.5f);
    CHECK(!IsMonitorBandColor(onyx, state));

    /*
        Every palette has to recognize its own conversion of the stock gray,
        whatever hue that carries, and still keep black and a foreign hue out.
    */
    for (const NamedPalette& named : kPalettes) {
        g_fakePalette = named.id;
        LoadSettings();

        const Settings& s = CurrentSettings();
        float stock = 0x1D / 255.0f;
        DvaColorRGBA in{stock, stock, stock, 1.0f};
        DvaColorRGBA converted{};

        CHECK(ConvertDvaColorWith(s, in, &converted));

        SetRootColor(&state, converted.r, converted.g, converted.b, 1.0f);
        CHECK(IsMonitorBandColor(s, state));

        // The untouched gray too: "Premiere interface" can be off.
        SetRootColor(&state, stock, stock, stock, 1.0f);
        CHECK(IsMonitorBandColor(s, state));

        SetRootColor(&state, 0.0f, 0.0f, 0.0f, 1.0f);
        CHECK(!IsMonitorBandColor(s, state));

        // A saturated color at the same brightness is not a ramp tone.
        float gray = (converted.r + converted.g + converted.b) / 3.0f;
        SetRootColor(&state, ClampFloat(gray * 3.0f, 0.0f, 1.0f), 0.0f, 0.0f, 1.0f);
        CHECK(!IsMonitorBandColor(s, state));
    }

    // Miku's, spelled out: #1D1D1D becomes #0E2128, which is far from neutral.
    g_fakePalette = L"miku";
    LoadSettings();

    SetRootColor(&state, 0x0E / 255.0f, 0x21 / 255.0f, 0x28 / 255.0f, 1.0f);
    CHECK(IsMonitorBandColor(CurrentSettings(), state));

    g_fakePalette = L"onyx";
    LoadSettings();
}

/*
    Only a draw that covers the whole render target, which the band does and a
    thumbnail or a scope does not.
*/
static void TestMonitorFullSurface() {
    MonitorCommandState state{};

    CHECK(!IsFullMonitorState(state));

    state.hasViewport = true;
    state.viewport = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};
    CHECK(!IsFullMonitorState(state));  // no scissor yet

    state.hasScissor = true;
    state.scissor = {0, 0, 1280, 720};
    CHECK(IsFullMonitorState(state));

    // A sub-rectangle of the surface is somebody else's draw.
    state.scissor = {0, 0, 640, 720};
    CHECK(!IsFullMonitorState(state));

    state.scissor = {0, 0, 1280, 720};
    state.viewport.TopLeftX = 8.0f;
    CHECK(!IsFullMonitorState(state));

    // Too small to be a monitor.
    state.viewport = {0.0f, 0.0f, 320.0f, 200.0f, 0.0f, 1.0f};
    state.scissor = {0, 0, 320, 200};
    CHECK(!IsFullMonitorState(state));
}

/*
    The band changes color where its color is set, not where it is drawn.

    The quads cover the sides of the picture and the clear shows between them,
    which is the same black the picture is composited onto. So the clear is
    left alone and the quads' own color is replaced on its way to the shader —
    the only way the band can take the theme while the backing stays black.
*/
static void TestMonitorBandRecolor() {
    auto list = reinterpret_cast<ID3D12GraphicsCommandList*>(0x1000);

    g_fakePalette = L"onyx";
    SetFakeInt(L"monitorBand", 1);
    SetFakeInt(L"strength", 100);
    LoadSettings();

    MonitorSetGraphicsRoot32BitConstants_Original = FakeSetRootConstants;
    MonitorRSSetViewports_Original = FakeSetViewports;
    MonitorRSSetScissorRects_Original = FakeSetScissors;
    MonitorReset_Original = FakeReset;
    MonitorClose_Original = FakeClose;

    MonitorCommandState state{};
    state.hasViewport = true;
    state.viewport = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};
    state.hasScissor = true;
    state.scissor = {0, 0, 1280, 720};

    const Palette& onyx = CurrentSettings().palette;
    auto channel = [](float v) {
        return ClampInt(static_cast<int>(v * 255.0f + 0.5f), 0, 255);
    };

    // The band takes the theme, written back to root parameter 1 in full.
    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
    g_rootWrites = 0;
    CHECK(RecolorBandConstants(list, &state));
    CHECK(g_rootWrites == 1);
    CHECK(g_rootWrittenIndex == 1);
    CHECK(g_rootWrittenCount == 4);
    CHECK(channel(WrittenChannel(0)) == GetRValue(onyx.ramp[1]));
    CHECK(channel(WrittenChannel(1)) == GetGValue(onyx.ramp[1]));
    CHECK(channel(WrittenChannel(2)) == GetBValue(onyx.ramp[1]));
    CHECK(WrittenChannel(3) == 1.0f);  // the draw's own alpha, untouched

    // The state now holds what the shader will see, and is not judged again.
    CHECK(channel(RootColorChannel(&state, 0)) == GetRValue(onyx.ramp[1]));
    CHECK(!state.hasRoot1Color);
    g_rootWrites = 0;
    CHECK(!RecolorBandConstants(list, &state));
    CHECK(g_rootWrites == 0);

    /*
        Black is left exactly as it is. This is the backing behind the picture
        — the empty frame over a gap, and whatever shows through a clip with an
        alpha channel — and touching it is what made the preview disappear.
    */
    SetRootColor(&state, 0.0f, 0.0f, 0.0f, 1.0f);
    g_rootWrites = 0;
    CHECK(!RecolorBandConstants(list, &state));
    CHECK(g_rootWrites == 0);
    CHECK(RootColorChannel(&state, 0) == 0.0f);

    // A theme that names a band color uses it instead of the panel.
    SetFakeTheme(L"base", L"#050505");
    SetFakeTheme(L"panel", L"#090909");
    SetFakeTheme(L"surface", L"#0E0E0E");
    SetFakeTheme(L"raised", L"#161616");
    SetFakeTheme(L"border", L"#242424");
    SetFakeTheme(L"text", L"#E6E6E6");
    SetFakeTheme(L"monitor", L"#200040");
    g_fakePalette = L"custom";
    LoadSettings();

    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
    CHECK(RecolorBandConstants(list, &state));
    CHECK(channel(WrittenChannel(0)) == 0x20);
    CHECK(channel(WrittenChannel(1)) == 0x00);
    CHECK(channel(WrittenChannel(2)) == 0x40);

    g_fakePalette = L"onyx";
    ClearFakeTheme();

    // Half strength lands halfway between what Premiere carries and the theme.
    SetFakeInt(L"strength", 50);
    LoadSettings();
    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
    CHECK(RecolorBandConstants(list, &state));
    CHECK(channel(WrittenChannel(0)) == (0x1D + GetRValue(onyx.ramp[1]) + 1) / 2);

    // At zero, and with the layer off, nothing is touched at all.
    SetFakeInt(L"strength", 0);
    LoadSettings();
    CHECK(!MonitorBandActive(CurrentSettings()));

    SetFakeInt(L"strength", 100);
    SetFakeInt(L"monitorBand", 0);
    LoadSettings();
    CHECK(!MonitorBandActive(CurrentSettings()));

    // Direct fills is a different layer now, and does not govern this one.
    SetFakeInt(L"monitorBand", 1);
    SetFakeInt(L"brushHook", 0);
    LoadSettings();
    CHECK(MonitorBandActive(CurrentSettings()));

    SetFakeInt(L"brushHook", 1);
    LoadSettings();

    // A surface that is not a whole monitor is left alone whatever its color.
    state.scissor = {0, 0, 640, 720};
    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
    g_rootWrites = 0;
    CHECK(!RecolorBandConstants(list, &state));
    CHECK(g_rootWrites == 0);

    // And so is anything at all when the write cannot be made.
    state.scissor = {0, 0, 1280, 720};
    MonitorSetGraphicsRoot32BitConstants_Original = nullptr;
    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
    CHECK(!RecolorBandConstants(list, &state));

    /*
        Or when only some of the layer went in. The two hooks that bound a
        recording are the ones a partial install drops quietly, and without
        them the recolor would be running on state nothing invalidates.
    */
    MonitorSetGraphicsRoot32BitConstants_Original = FakeSetRootConstants;

    for (void** missing : {reinterpret_cast<void**>(&MonitorReset_Original),
                           reinterpret_cast<void**>(&MonitorClose_Original),
                           reinterpret_cast<void**>(&MonitorRSSetViewports_Original),
                           reinterpret_cast<void**>(&MonitorRSSetScissorRects_Original)}) {
        void* saved = *missing;
        *missing = nullptr;

        SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
        g_rootWrites = 0;
        CHECK(!RecolorBandConstants(list, &state));
        CHECK(g_rootWrites == 0);

        *missing = saved;
    }

    // Whole again, and back to recoloring.
    CHECK(MonitorBandHooksReady());
    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
    CHECK(RecolorBandConstants(list, &state));

    ForgetMonitorState(list);
    LoadSettings();
}

/*
    Only the exact float4 is taken for a color.

    A larger block bound at root parameter 1 — a transform and a color, say —
    would otherwise have its first four words rewritten, and a color assembled
    from several smaller writes could mix words belonging to different draws.
*/
static void TestMonitorConstantShape() {
    auto list = reinterpret_cast<ID3D12GraphicsCommandList*>(0x2000);
    const float band[4] = {0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f};

    g_fakePalette = L"onyx";
    ClearFakeTheme();
    SetFakeInt(L"monitorBand", 1);
    SetFakeInt(L"strength", 100);
    LoadSettings();

    MonitorSetGraphicsRoot32BitConstants_Original = FakeSetRootConstants;
    SetDisplaySurfaceRange(0x400000, 0x500000);
    auto fromDisplaySurface = reinterpret_cast<void*>(0x410000);

    auto record = [&](UINT parameter, UINT count, const void* data, UINT offset) {
        g_rootWrites = 0;
        MonitorSetGraphicsRoot32BitConstants_Original(list, parameter, count,
                                                      data, offset);
        // What the hook body does, with the return address supplied.
        if (parameter != 1 || !data || offset != 0 || count != 4) {
            return false;
        }
        MonitorCommandState* state =
            MonitorStateForColor(fromDisplaySurface, list);
        CHECK(state != nullptr);
        auto values = reinterpret_cast<const UINT*>(data);
        for (int i = 0; i < 4; i++) {
            state->root1Color[i] = values[i];
        }
        state->hasRoot1Color = true;
        return true;
    };

    // Four words at offset zero is the shape; anything else is not.
    CHECK(record(1, 4, band, 0));
    CHECK(!record(1, 8, band, 0));   // a larger block
    CHECK(!record(1, 4, band, 4));   // a color further into the block
    CHECK(!record(1, 1, band, 3));   // one word of four
    CHECK(!record(2, 4, band, 0));   // another root parameter

    ForgetMonitorState(list);
    SetDisplaySurfaceRange(0, 0);
    LoadSettings();
}

/*
    The band is recolored by whichever of the three pieces arrives last.

    It takes a viewport, a scissor and a color to decide, and only the order
    DisplaySurface records in says which completes the set. All three hooks
    try, so a build that set the color before the surface would still be
    recolored instead of silently stopping.
*/
static void TestMonitorRecolorOrder() {
    auto list = reinterpret_cast<ID3D12GraphicsCommandList*>(0x5000);

    g_fakePalette = L"onyx";
    ClearFakeTheme();
    SetFakeInt(L"monitorBand", 1);
    SetFakeInt(L"strength", 100);
    LoadSettings();

    MonitorSetGraphicsRoot32BitConstants_Original = FakeSetRootConstants;
    MonitorRSSetViewports_Original = FakeSetViewports;
    MonitorRSSetScissorRects_Original = FakeSetScissors;
    MonitorReset_Original = FakeReset;
    MonitorClose_Original = FakeClose;

    MonitorCommandState state{};

    // The color first, which is the order the layer used to depend on not
    // happening: nothing to recolor against yet.
    SetRootColor(&state, 0x1D / 255.0f, 0x1D / 255.0f, 0x1D / 255.0f, 1.0f);
    g_rootWrites = 0;
    CHECK(!RecolorBandConstants(list, &state));

    // Then the viewport: still only half a surface.
    state.hasViewport = true;
    state.viewport = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};
    CHECK(!RecolorBandConstants(list, &state));
    CHECK(g_rootWrites == 0);

    // The scissor completes it, and that is where the band changes.
    state.hasScissor = true;
    state.scissor = {0, 0, 1280, 720};
    CHECK(RecolorBandConstants(list, &state));
    CHECK(g_rootWrites == 1);

    ForgetMonitorState(list);
    LoadSettings();
}

/*
    A slot describes one recording, bounded at both ends.

    Reset begins one and clears the list's viewport, scissor and root
    constants; Close ends it. State kept past either would describe work that
    is over — and a released list's address can be handed to a new one, whose
    owner would then inherit a full-surface viewport it never set and have its
    own dark gray taken for the band.
*/
static void TestMonitorReset() {
    auto list = reinterpret_cast<ID3D12GraphicsCommandList*>(0x3000);

    auto record = [&] {
        MonitorCommandState* s = MonitorStateFor(list);
        CHECK(s != nullptr);
        s->hasViewport = true;
        s->viewport = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};
        s->hasScissor = true;
        s->scissor = {0, 0, 1280, 720};
        CHECK(IsFullMonitorState(*s));
        return s;
    };

    MonitorRSSetViewports_Original = FakeSetViewports;
    MonitorRSSetScissorRects_Original = FakeSetScissors;
    MonitorReset_Original = FakeReset;
    MonitorClose_Original = FakeClose;

    MonitorCommandState* state = record();
    CHECK(KnownMonitorState(list) == state);

    // Both ends of a recording drop it.
    MonitorReset_Hook(list, nullptr, nullptr);
    CHECK(KnownMonitorState(list) == nullptr);

    record();
    MonitorClose_Hook(list);
    CHECK(KnownMonitorState(list) == nullptr);

    record();
    ForgetMonitorState(list);
    CHECK(KnownMonitorState(list) == nullptr);

    // The same address again is a new recording, with nothing carried over.
    MonitorCommandState* fresh = MonitorStateFor(list);
    CHECK(fresh != nullptr);
    CHECK(!fresh->hasViewport);
    CHECK(!fresh->hasScissor);
    CHECK(!fresh->hasRoot1Color);
    CHECK(!IsFullMonitorState(*fresh));

    ForgetMonitorState(list);

    /*
        And the slots are fixed, so more lists than there are slots costs the
        oldest one rather than an allocation.
    */
    for (size_t i = 0; i < kMaxMonitorStates + 4; i++) {
        auto many = reinterpret_cast<ID3D12GraphicsCommandList*>(0x4000 + i * 16);
        CHECK(MonitorStateFor(many) != nullptr);
    }

    // The most recent kMaxMonitorStates are the ones still held.
    for (size_t i = 4; i < kMaxMonitorStates + 4; i++) {
        auto many = reinterpret_cast<ID3D12GraphicsCommandList*>(0x4000 + i * 16);
        CHECK(KnownMonitorState(many) != nullptr);
        ForgetMonitorState(many);
    }
}

static HookCount InstallColorHooksFrom(std::vector<const char*> exports) {
    g_fakeExports = std::move(exports);
    g_absentLogs = 0;
    g_lastAbsent = nullptr;

    HookCount count;
    InstallColorHooksImpl(kFakeDvaui, count,
                          std::make_index_sequence<kColorSymbolCount>{});
    g_fakeExports.clear();
    return count;
}

static void TestColorHookCounting() {
    std::vector<const char*> unrenamed, newer, older;
    const ColorSymbol* background = nullptr;

    for (const ColorSymbol& sym : kColorSymbols) {
        if (sym.before2026) {
            newer.push_back(sym.mangled);
            older.push_back(sym.before2026);
        } else {
            unrenamed.push_back(sym.mangled);
        }

        if (wcscmp(sym.label, L"GetApplicationBackgroundColor") == 0) {
            background = &sym;
        }
    }

    CHECK(kColorSymbolCount == 26);
    CHECK(older.size() == 14);
    CHECK(background && background->before2026);

    if (!background) {
        return;
    }

    auto plus = [&](const std::vector<const char*>& names) {
        std::vector<const char*> all = unrenamed;
        all.insert(all.end(), names.begin(), names.end());
        return all;
    };

    // 2026 exports the newer names: all found, and the older ones not reported.
    HookCount count = InstallColorHooksFrom(plus(newer));
    CHECK(count.installed == 26);
    CHECK(count.missing == 0);
    CHECK(g_absentLogs == 0);

    // Earlier versions export the older names.
    count = InstallColorHooksFrom(plus(older));
    CHECK(count.installed == 26);
    CHECK(count.missing == 0);
    CHECK(g_absentLogs == 0);

    // A function under neither name is counted and reported once, by its label.
    std::vector<const char*> partial;

    for (const char* name : older) {
        if (name != background->before2026) {
            partial.push_back(name);
        }
    }

    count = InstallColorHooksFrom(plus(partial));
    CHECK(count.installed == 25);
    CHECK(count.missing == 1);
    CHECK(g_absentLogs == 1);
    CHECK(g_lastAbsent && wcscmp(g_lastAbsent, L"GetApplicationBackgroundColor") == 0);

    // A module that exports none of them.
    count = InstallColorHooksFrom({});
    CHECK(count.installed == 0);
    CHECK(count.missing == 26);
    CHECK(g_absentLogs == 26);

    // UIFramework keeps a count of its own.
    HookCount uif;
    InstallUifHooks(GetModuleHandleW(nullptr), uif);
    CHECK(uif.installed == 0);
    CHECK(uif.missing == 4);
}

static const void* g_seenOptions = nullptr;

static HRESULT WINAPI FakeDrawThemeTextEx(HTHEME, HDC, int, int, LPCWSTR, int,
                                          DWORD, RECT*, const void* options) {
    g_seenOptions = options;
    return S_OK;
}

static void TestMenuTextOptions() {
    HTHEME menu = reinterpret_cast<HTHEME>(0x300);
    RememberMenuTheme(menu);
    DrawThemeTextEx_Original = FakeDrawThemeTextEx;

    RECT rect{};

    struct {
        DWORD dwSize;
        BYTE rest[256];
    } larger{};
    larger.dwSize = sizeof(larger);

    DrawThemeTextEx_Hook(menu, nullptr, kMenuPopupItem, 1, L"x", 1, 0, &rect,
                         &larger);
    CHECK(g_seenOptions == &larger);  // forwarded as it came

    ThemeDttOpts known{};
    known.dwSize = sizeof(known);

    DrawThemeTextEx_Hook(menu, nullptr, kMenuPopupItem, 1, L"x", 1, 0, &rect,
                         &known);
    CHECK(g_seenOptions != &known);  // the mod's copy, colour swapped

    ForgetMenuTheme(menu);
    DrawThemeTextEx_Original = nullptr;
}

static void TestMenuBarGate() {
    HWND frame = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPEDWINDOW, 0, 0,
                                 100, 100, nullptr, CreateMenu(), nullptr,
                                 nullptr);
    HWND child = CreateWindowExW(0, L"STATIC", L"", WS_CHILD, 0, 0, 10, 10, frame,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(42)),
                                 nullptr, nullptr);
    CHECK(frame != nullptr);
    CHECK(child != nullptr);

    CHECK(NeedsMenuBarWork(frame, WM_NCPAINT));
    CHECK(!NeedsMenuBarWork(child, WM_NCPAINT));  // GetMenu would say 42
    CHECK(!NeedsMenuBarWork(frame, WM_PAINT));

    DestroyWindow(frame);
}

static void TestCustomDimText() {
    g_fakePalette = L"custom";
    SetFakeTheme(L"base", L"#050505");
    SetFakeTheme(L"panel", L"#101010");
    SetFakeTheme(L"surface", L"#0E0E0E");
    SetFakeTheme(L"raised", L"#161616");
    SetFakeTheme(L"border", L"#242424");
    SetFakeTheme(L"text", L"#404040");
    LoadSettings();
    CHECK(CurrentSettings().palette.text == RGB(0x40, 0x40, 0x40));
    CHECK(CurrentSettings().palette.dimText == RGB(0x28, 0x28, 0x28));
    CHECK(!CurrentSettings().highlight);  // no highlight in the theme: the blue stays

    g_fakePalette = L"onyx";
    ClearFakeTheme();
    LoadSettings();
}

struct ThemeField {
    const wchar_t* key;
    const wchar_t* value;
};

// Every field not listed is left empty, as the settings ship it.
static Palette ThemeFrom(std::initializer_list<ThemeField> fields, int* logs) {
    g_fakePalette = L"custom";
    ClearFakeTheme();

    for (const ThemeField& field : fields) {
        SetFakeTheme(field.key, field.value);
    }

    g_themeLogs = 0;
    LoadSettings();
    *logs = g_themeLogs;
    return CurrentSettings().palette;
}

// The mod's own source, with the CRLFs dropped so lines can be matched.
static std::string ModSource() {
    std::string path = __FILE__;
    path = path.substr(0, path.find_last_of("/\\") + 1) + "../premiere-pro-theme.wh.cpp";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    std::string s;

    if (f) {
        char buffer[4096];
        size_t n;

        while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) {
            for (size_t i = 0; i < n; i++) {
                if (buffer[i] != '\r') {
                    s.push_back(buffer[i]);
                }
            }
        }

        std::fclose(f);
    }

    return s;
}

struct ShippedField {
    std::wstring key;
    std::wstring value;
};

/*
    The defaults the settings block ships for the customTheme group, so the
    block and the reader cannot drift apart: every "  - <key>: <value>" line
    under it, with the $name and $description lines that follow skipped.
*/
static std::vector<ShippedField> ShippedTheme() {
    std::string s = ModSource();
    std::vector<ShippedField> fields;
    size_t at = s.find("\n- customTheme:\n");

    if (at == std::string::npos) {
        return fields;
    }

    size_t i = s.find('\n', at + 1) + 1;

    while (i < s.size() && s.compare(i, 4, "  - ") == 0) {
        size_t eol = s.find('\n', i);
        size_t colon = s.find(':', i + 4);
        std::string key = s.substr(i + 4, colon - (i + 4));
        std::string value;
        size_t quote = s.find('"', colon);

        if (quote != std::string::npos && quote < eol) {
            size_t close = s.find('"', quote + 1);
            value = s.substr(quote + 1, close - quote - 1);
        }

        fields.push_back({std::wstring(key.begin(), key.end()),
                          std::wstring(value.begin(), value.end())});

        i = eol + 1;

        while (i < s.size() && s.compare(i, 4, "    ") == 0) {
            i = s.find('\n', i) + 1;
        }
    }

    return fields;
}

// A top-level true/false default in the settings block.
static bool ShippedFlag(const char* name) {
    std::string s = ModSource();
    std::string marker = std::string("\n- ") + name + ": ";
    size_t at = s.find(marker);
    CHECK(at != std::string::npos);
    return s.compare(at + marker.size(), 4, "true") == 0;
}

// The WCAG contrast ratio, on the luminance the mod already computes.
static float ContrastRatio(COLORREF a, COLORREF b) {
    float la = Luminance(a);
    float lb = Luminance(b);
    float hi = la > lb ? la : lb;
    float lo = la > lb ? lb : la;
    return (hi + 0.05f) / (lo + 0.05f);
}

static float RampBrightness(COLORREF c) {
    return (GetRValue(c) + GetGValue(c) + GetBValue(c)) / 3.0f / 255.0f;
}

/*
    What the readme promises about every built-in palette, held to.

    The accent is the background of a hovered menu item with the palette's own
    text on it, so it is the one that has bitten: read at full saturation, a
    bright accent against light text is a menu nobody can read. Blossom's,
    Ember's, Amethyst's and Miku's were all darkened for this.
*/
static void TestPaletteRules() {
    for (const NamedPalette& named : kPalettes) {
        const Palette& p = named.colors;

        // The five steps rise, deepest to lightest — the ramp is interpolated
        // between them, so an inversion would fold two tones together.
        for (int i = 1; i < 5; i++) {
            CHECK(RampBrightness(p.ramp[i]) > RampBrightness(p.ramp[i - 1]));
        }

        // Text has to carry against the panel it sits on.
        CHECK(ContrastRatio(p.text, p.ramp[1]) >= 7.0f);

        // And the accent has to carry that same text.
        CHECK(ContrastRatio(p.text, p.accent) >= 4.5f);

        // Disabled text is dimmer than text, but still legible.
        CHECK(Luminance(p.dimText) < Luminance(p.text));
        CHECK(ContrastRatio(p.dimText, p.ramp[1]) >= 3.0f);

        /*
            A highlight is a hue, not a tone: the shade each of Premiere's
            blues becomes is computed at that blue's own luminance, so white
            keeps its footing on a blue button whatever hue replaces it.
        */
        if (p.highlight != CLR_INVALID) {
            COLORREF track = RGB(0x00, 0x5C, 0xC8);  // the track-targeting blue
            COLORREF shade = ShadeWithLuminance(p.highlight, Luminance(track));
            CHECK(ContrastRatio(RGB(0xFF, 0xFF, 0xFF), shade) >= 4.5f);
        }
    }
}

/*
    Every palette is offered, and every option is a palette. The list lives in
    the settings block and the colors live in the code, so nothing but this
    keeps a palette from being added to one and not the other.
*/
static void TestPaletteOptions() {
    std::string s = ModSource();
    size_t at = s.find("\n  $options:\n");
    CHECK(at != std::string::npos);

    std::vector<std::string> options;
    size_t i = s.find('\n', at + 1) + 1;

    while (i < s.size() && s.compare(i, 4, "  - ") == 0) {
        size_t colon = s.find(':', i + 4);
        options.push_back(s.substr(i + 4, colon - (i + 4)));
        i = s.find('\n', i) + 1;
    }

    // The built-ins, plus Custom.
    CHECK(options.size() == ARRAYSIZE(kPalettes) + 1);
    CHECK(options.back() == "custom");

    for (const NamedPalette& named : kPalettes) {
        std::wstring id(named.id);
        std::string narrow(id.begin(), id.end());
        bool offered = false;

        for (const std::string& option : options) {
            offered = offered || option == narrow;
        }

        CHECK(offered);
    }

    // And each option that is not Custom names a palette that exists.
    for (const std::string& option : options) {
        if (option == "custom") {
            continue;
        }

        std::wstring wide(option.begin(), option.end());
        bool known = false;

        for (const NamedPalette& named : kPalettes) {
            known = known || wide == named.id;
        }

        CHECK(known);
    }
}

/*
    Turning the layer on has to reach both ways in.

    With "Monitor band" off at load the D3D12 export is not hooked, and the
    probe declines until DisplaySurface is mapped. If a settings change only
    retried the probe, turning the switch on while Premiere was still starting
    would leave the layer off for the whole session with nothing in the log.
*/
static void TestSettingsChangedRetriesBothWaysIn() {
    std::string s = ModSource();
    size_t at = s.find("void Wh_ModSettingsChanged() {");
    CHECK(at != std::string::npos);

    std::string body = s.substr(at, s.find("\n}\n", at) - at);

    CHECK(body.find("HookLoadedModules()") != std::string::npos);
    CHECK(body.find("HookD3D12CreateDevice()") != std::string::npos);
    CHECK(body.find("InstallMonitorBandFromProbe()") != std::string::npos);

    /*
        And all three hooks that record a piece of the decision have to try the
        recolor, or the band depends on the order DisplaySurface records in.
        The logic itself is TestMonitorRecolorOrder; this is only that every
        hook reaches it.
    */
    for (const char* hook : {"MonitorRSSetViewports_Hook",
                             "MonitorRSSetScissorRects_Hook",
                             "MonitorSetGraphicsRoot32BitConstants_Hook"}) {
        size_t body_at = s.find(std::string(hook) + "(ID3D12GraphicsCommandList");
        CHECK(body_at != std::string::npos);

        // The definition, not the forward declaration above it.
        size_t open_brace = s.find('{', body_at);
        size_t close = s.find("\n}\n", body_at);
        CHECK(open_brace != std::string::npos);
        CHECK(close != std::string::npos);

        std::string text = s.substr(open_brace, close - open_brace);
        CHECK(text.find("RecolorBandConstants(commandList, state)") !=
              std::string::npos);
    }
}

/*
    And every palette has a column in the readme's tables.

    The tables and the prose around them are kept by hand, so nothing but this
    stops a palette from shipping with its colors undocumented — which is what
    nearly happened when Miku was added. The column header is what is looked
    for, not the name on its own: "Premiere" is the name of the application on
    almost every line.
*/
static void TestPaletteReadme() {
    std::string s = ModSource();
    size_t begin = s.find("// ==WindhawkModReadme==");
    size_t end = s.find("// ==/WindhawkModReadme==");

    CHECK(begin != std::string::npos);
    CHECK(end != std::string::npos);
    CHECK(begin < end);

    std::string readme = s.substr(begin, end - begin);

    for (const NamedPalette& named : kPalettes) {
        std::wstring id(named.id);
        std::string column = "| " + std::string(id.begin(), id.end());
        column[2] = static_cast<char>(std::toupper(column[2]));

        CHECK(readme.find(column) != std::string::npos);
    }
}

static void TestShippedDefaults() {
    /*
        The UXP layer is the one whose effect only a restart undoes, so it has
        to be the one the user turns on.
    */
    CHECK(!ShippedFlag("uxpPanels"));

    // Every other layer is on.
    for (const char* on : {"dvauiHook", "brushHook", "monitorBand",
                           "nativeDarkMode", "menuHook", "gdiHook",
                           "highlight"}) {
        CHECK(ShippedFlag(on));
    }
}

static void TestCustomTheme() {
    const Palette onyx = kPalettes[0].colors;
    int logs = 0;

    // The block ships exactly the fields the reader knows...
    std::vector<ShippedField> shipped = ShippedTheme();
    CHECK(shipped.size() == 10);

    for (const wchar_t* key : {L"base", L"panel", L"surface", L"raised", L"border",
                               L"text", L"accent", L"disabledText", L"highlight",
                               L"monitor"}) {
        bool found = false;

        for (const ShippedField& field : shipped) {
            found = found || field.key == key;
        }

        CHECK(found);
    }

    // ...and those defaults are Onyx, exactly, without a word in the log.
    g_fakePalette = L"custom";
    ClearFakeTheme();

    for (const ShippedField& field : shipped) {
        SetFakeTheme(field.key.c_str(), field.value.c_str());
    }

    g_themeLogs = 0;
    LoadSettings();
    Palette p = CurrentSettings().palette;
    CHECK(std::memcmp(&p, &onyx, sizeof(Palette)) == 0);
    CHECK(g_themeLogs == 0);

    // A full theme, highlight included; the leading # is optional.
    p = ThemeFrom({{L"base", L"#05060A"},
                   {L"panel", L"#0A0C14"},
                   {L"surface", L"#10131F"},
                   {L"raised", L"#181C2C"},
                   {L"border", L"#2A3048"},
                   {L"text", L"#E6E9F5"},
                   {L"accent", L"#3A4270"},
                   {L"disabledText", L"#6B7090"},
                   {L"highlight", L"4F7BFF"},
                   {L"monitor", L"#020409"}},
                  &logs);
    CHECK(p.ramp[0] == RGB(0x05, 0x06, 0x0A));
    CHECK(p.ramp[1] == RGB(0x0A, 0x0C, 0x14));
    CHECK(p.ramp[2] == RGB(0x10, 0x13, 0x1F));
    CHECK(p.ramp[3] == RGB(0x18, 0x1C, 0x2C));
    CHECK(p.ramp[4] == RGB(0x2A, 0x30, 0x48));
    CHECK(p.text == RGB(0xE6, 0xE9, 0xF5));
    CHECK(p.accent == RGB(0x3A, 0x42, 0x70));
    CHECK(p.dimText == RGB(0x6B, 0x70, 0x90));
    CHECK(p.highlight == RGB(0x4F, 0x7B, 0xFF));
    CHECK(p.monitor == RGB(0x02, 0x04, 0x09));
    CHECK(CurrentSettings().highlight);
    CHECK(logs == 0);

    // The three optional fields left empty: their defaults, without a word.
    p = ThemeFrom({{L"base", L"#000000"},
                   {L"panel", L"#101010"},
                   {L"surface", L"#202020"},
                   {L"raised", L"#303030"},
                   {L"border", L"#404040"},
                   {L"text", L"#F0F0F0"}},
                  &logs);
    CHECK(p.accent == RGB(0x40, 0x40, 0x40));   // the border
    CHECK(p.dimText == RGB(0x80, 0x80, 0x80));  // halfway from text to panel
    CHECK(p.highlight == CLR_INVALID);
    CHECK(!CurrentSettings().highlight);
    CHECK(logs == 0);

    // A required color left empty or unreadable falls back to Onyx's, one at a time.
    p = ThemeFrom({{L"base", L"#000000"},
                   {L"panel", L"red"},
                   {L"surface", L""},
                   {L"raised", L"#303030"},
                   {L"border", L"#404040"},
                   {L"text", L"#F0F0F0"}},
                  &logs);
    CHECK(p.ramp[0] == RGB(0, 0, 0));
    CHECK(p.ramp[1] == onyx.ramp[1]);
    CHECK(p.ramp[2] == onyx.ramp[2]);
    CHECK(p.ramp[3] == RGB(0x30, 0x30, 0x30));
    CHECK(logs == 2);

    // An optional one filled in but unreadable is logged and left out.
    p = ThemeFrom({{L"base", L"#000000"},
                   {L"panel", L"#101010"},
                   {L"surface", L"#202020"},
                   {L"raised", L"#303030"},
                   {L"border", L"#404040"},
                   {L"text", L"#F0F0F0"},
                   {L"accent", L"#12345"}},
                  &logs);
    CHECK(p.accent == RGB(0x40, 0x40, 0x40));
    CHECK(logs == 1);

    /*
        Every field empty: Onyx, one line per required color, and an accent
        that follows the border rather than Onyx's own.
    */
    p = ThemeFrom({}, &logs);
    CHECK(p.ramp[0] == onyx.ramp[0]);
    CHECK(p.ramp[4] == onyx.ramp[4]);
    CHECK(p.text == onyx.text);
    CHECK(p.dimText == onyx.dimText);
    CHECK(p.accent == onyx.ramp[4]);
    CHECK(p.highlight == CLR_INVALID);
    CHECK(p.monitor == CLR_INVALID);  // the panel tone, as Premiere does
    CHECK(logs == 6);

    g_fakePalette = L"onyx";
    ClearFakeTheme();
    LoadSettings();
}

/*
    A theme is shared as the settings text Windhawk itself reads, so the mod
    writes its own out: the lines that are the theme, and nothing else, in the
    names and the order the settings use.
*/
static void TestThemeForSharing() {
    int logs = 0;

    g_shareLogs = 0;
    g_lastShared = nullptr;

    Palette p = ThemeFrom({{L"base", L"#05060A"},
                           {L"panel", L"#0A0C14"},
                           {L"surface", L"#10131F"},
                           {L"raised", L"#181C2C"},
                           {L"border", L"#2A3048"},
                           {L"text", L"#E6E9F5"},
                           {L"accent", L"#3A4270"},
                           {L"disabledText", L"#6B7090"},
                           {L"highlight", L"#4F7BFF"},
                           {L"monitor", L"#020409"}},
                          &logs);
    CHECK(logs == 0);
    CHECK(g_shareLogs == 1);
    CHECK(g_lastShared != nullptr);

    /*
        YAML's flow form, which is what the Settings tab's text mode reads: the
        palette, then the group as a block under it. Every field is named the
        way the settings name it, and the colors are quoted, because a bare #
        opens a comment in YAML.
    */
    for (const wchar_t* key :
         {L"{palette: custom, customTheme: {", L"base: '#05060A'",
          L"panel: '#0A0C14'", L"surface: '#10131F'", L"raised: '#181C2C'",
          L"border: '#2A3048'", L"text: '#E6E9F5'", L"accent: '#3A4270'",
          L"disabledText: '#6B7090'", L"highlight: '#4F7BFF'",
          L"monitor: '#020409'}}"}) {
        CHECK(wcsstr(g_lastShared, key) != nullptr);
    }

    // The colors come back as the settings spell them, uppercase and hashed.
    wchar_t hex[8];

    FormatHexColor(RGB(0x05, 0x06, 0x0A), hex);
    CHECK(wcscmp(hex, L"#05060A") == 0);

    FormatHexColor(RGB(0xE6, 0xE9, 0xF5), hex);
    CHECK(wcscmp(hex, L"#E6E9F5") == 0);

    // A color the theme leaves to its default is written as an empty value,
    // which is what the settings hold for it.
    FormatHexColor(CLR_INVALID, hex);
    CHECK(hex[0] == L'\0');

    /*
        The resolved theme is what goes out, not the raw fields: an accent left
        empty travels as the border it became, so the theme lands the same way
        on someone else's Windhawk.
    */
    g_shareLogs = 0;
    p = ThemeFrom({{L"base", L"#000000"},
                   {L"panel", L"#101010"},
                   {L"surface", L"#202020"},
                   {L"raised", L"#303030"},
                   {L"border", L"#404040"},
                   {L"text", L"#F0F0F0"}},
                  &logs);
    CHECK(g_shareLogs == 1);
    CHECK(p.accent == RGB(0x40, 0x40, 0x40));
    CHECK(wcsstr(g_lastShared, L"accent: '#404040'") != nullptr);
    CHECK(wcsstr(g_lastShared, L"highlight: ''") != nullptr);
    CHECK(wcsstr(g_lastShared, L"monitor: ''}}") != nullptr);

    // A built-in palette is not a theme anyone needs the text of.
    g_shareLogs = 0;
    g_fakePalette = L"onyx";
    ClearFakeTheme();
    LoadSettings();
    CHECK(g_shareLogs == 0);
}

static const DvaColorRGBA* g_seenColor = nullptr;
static DvaColorRGBA g_seenValue{};

static void* FakeEraseCtor(void* self, void*, const DvaColorRGBA* color) {
    g_seenColor = color;
    g_seenValue = *color;
    return self;
}

static void FakeDispatch(void*, const DvaColorRGBA* color, void*, bool) {
    g_seenColor = color;
    g_seenValue = *color;
}

static void TestEraseHooks() {
    LoadSettings();
    EraseBackgroundCtor_Original = FakeEraseCtor;
    DispatchDrawFromRoot_Original = FakeDispatch;

    static DvaColorRGBA raw = Gray(0x2A);
    int self = 0;

    // Converted into a temporary, not into a table slot.
    CHECK(EraseBackgroundCtor_Hook(&self, nullptr, &raw) == &self);
    CHECK(g_seenColor != &raw);
    CHECK(!IsOurSlot(g_seenColor));
    CHECK(!SameColor(g_seenValue, raw));

    DispatchDrawFromRoot_Hook(nullptr, &raw, nullptr, false);
    CHECK(g_seenColor != &raw);
    CHECK(!IsOurSlot(g_seenColor));
    CHECK(!SameColor(g_seenValue, raw));

    // "Direct fills" off: forwarded as it came.
    SetFakeInt(L"brushHook", 0);
    LoadSettings();
    DispatchDrawFromRoot_Hook(nullptr, &raw, nullptr, false);
    CHECK(g_seenColor == &raw);

    SetFakeInt(L"brushHook", 1);
    LoadSettings();
}

static int g_themeOpens = 0;
static int g_themeCloses = 0;
static UINT g_themeDpi = 0;
static bool g_darkMenuExists = true;

static HTHEME WINAPI FakeOpenThemeForDpi(HWND, LPCWSTR themeClass, UINT dpi) {
    g_themeDpi = dpi;

    if (g_darkMenuExists && wcscmp(themeClass, L"DarkMode::Menu") == 0) {
        g_themeOpens++;
        return reinterpret_cast<HTHEME>(0x500);
    }

    if (wcscmp(themeClass, L"Menu") == 0) {
        g_themeOpens++;
        return reinterpret_cast<HTHEME>(0x600);
    }

    return nullptr;
}

static HRESULT WINAPI FakeCloseTheme(HTHEME) {
    g_themeCloses++;
    return S_OK;
}

static UINT WINAPI FakeDpiForWindow(HWND) {
    return 144;
}

static void TestMenuBarTheme() {
    OpenThemeDataForDpi_Original = FakeOpenThemeForDpi;
    CloseThemeData_Original = FakeCloseTheme;
    g_GetDpiForWindow = FakeDpiForWindow;

    {
        MenuBarTheme theme(nullptr);
        CHECK(theme.get() == reinterpret_cast<HTHEME>(0x500));
        CHECK(g_themeDpi == 144);
    }

    CHECK(g_themeOpens == 1);
    CHECK(g_themeCloses == 1);  // closed as soon as the item is drawn

    g_darkMenuExists = false;

    {
        MenuBarTheme theme(nullptr);
        CHECK(theme.get() == reinterpret_cast<HTHEME>(0x600));
    }

    CHECK(g_themeCloses == 2);

    OpenThemeDataForDpi_Original = nullptr;
    CloseThemeData_Original = nullptr;
    g_GetDpiForWindow = nullptr;
}

static void TestGdiOrder() {
    LoadSettings();

    void* adobe = reinterpret_cast<void*>(&TestKnownModules);  // the executable

    CHECK(ShouldConvertGdi(CurrentSettings(), RGB(0x30, 0x30, 0x30), adobe));

    // Too light, saturated, and not from an Adobe module.
    CHECK(!ShouldConvertGdi(CurrentSettings(), RGB(0xE0, 0xE0, 0xE0), adobe));
    CHECK(!ShouldConvertGdi(CurrentSettings(), RGB(0x30, 0x10, 0x10), adobe));
    CHECK(!ShouldConvertGdi(CurrentSettings(), RGB(0x30, 0x30, 0x30), nullptr));

    SetFakeInt(L"gdiHook", 0);
    LoadSettings();
    CHECK(!ShouldConvertGdi(CurrentSettings(), RGB(0x30, 0x30, 0x30), adobe));

    SetFakeInt(L"gdiHook", 1);
    LoadSettings();
}

// The GDI path now goes through ConvertDvaColorWith; it must give what it gave.
static void TestGdiMatchesOldFormula() {
    LoadSettings();

    for (int level : {0x30, 0x3A, 0x47}) {
        float v = level / 255.0f;
        COLORREF target = PickTarget(CurrentSettings(), v);

        auto channel = [&](int targetChannel) {
            float blended = BlendWith(CurrentSettings().strength, v, targetChannel / 255.0f);
            return ClampInt(static_cast<int>(blended * 255.0f + 0.5f), 0, 255);
        };

        COLORREF old = RGB(channel(GetRValue(target)), channel(GetGValue(target)),
                           channel(GetBValue(target)));

        CHECK(ConvertGdiColor(CurrentSettings(), RGB(level, level, level)) == old);
    }
}

// ---------------------------------------------------------------------------
// Palette highlight and UXP stylesheets
// ---------------------------------------------------------------------------

static std::string Hex(COLORREF c, bool upper) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), upper ? "#%02X%02X%02X" : "#%02x%02x%02x",
                  GetRValue(c), GetGValue(c), GetBValue(c));
    return buffer;
}

static std::string Triplet(COLORREF c, size_t width) {
    std::string s = std::to_string(GetRValue(c)) + "," + std::to_string(GetGValue(c)) +
                    "," + std::to_string(GetBValue(c));
    s.resize(width, ' ');
    return s;
}

// What the interface layer makes of an 8-bit color.
static COLORREF Convert8(COLORREF c) {
    DvaColorRGBA out{};
    return ConvertDvaColor(GdiToDva(c), &out) ? DvaToGdi(out) : c;
}

static void TestHighlight() {
    const DvaColorRGBA track = GdiToDva(RGB(0x00, 0x5C, 0xC8));
    const DvaColorRGBA pure = {0.0f, 0.0f, 1.0f, 1.0f};  // blue, but not Spectrum's
    DvaColorRGBA out{};

    // Every listed blue is recognized, and each only once.
    for (size_t i = 0; i < kInterfaceBlueCount; i++) {
        CHECK(InterfaceBlueIndex(GdiToDva(kInterfaceBlues[i])) == static_cast<int>(i));
    }

    // The palettes before 1.1 leave Premiere's blue alone.
    g_fakePalette = L"onyx";
    LoadSettings();
    CHECK(!CurrentSettings().highlight);
    CHECK(!ConvertDvaColor(track, &out));

    // The track-targeting shades the readme gives.
    const struct {
        const wchar_t* palette;
        COLORREF shade;
    } kTrack[] = {
        {L"contrast", RGB(0x60, 0x60, 0x60)}, {L"violet", RGB(0x77, 0x37, 0xDC)},
        {L"blossom", RGB(0x93, 0x48, 0x69)},  {L"ember", RGB(0x9F, 0x46, 0x08)},
        {L"amethyst", RGB(0x75, 0x37, 0xDF)}, {L"crimson", RGB(0xBB, 0x22, 0x22)},
        {L"threshold", RGB(0xBC, 0x20, 0x20)},
    };

    for (const auto& t : kTrack) {
        g_fakePalette = t.palette;
        LoadSettings();
        CHECK(CurrentSettings().highlight);
        CHECK(ConvertDvaColor(track, &out));

        if (DvaToGdi(out) != t.shade) {
            std::printf("  %ls: track shade is %s\n", t.palette,
                        Hex(DvaToGdi(out), true).c_str());
        }

        CHECK(DvaToGdi(out) == t.shade);
        CHECK(!ConvertDvaColor(pure, &out));

        // Each blue keeps its luminance, so text on it keeps its contrast.
        for (size_t i = 0; i < kInterfaceBlueCount; i++) {
            float want = Luminance(kInterfaceBlues[i]);
            float got = Luminance(CurrentSettings().highlightShades[i]);
            CHECK(std::fabs(got - want) < 0.01f);
        }
    }

    // A gray made from a blue is produced once, and not darkened as a gray.
    g_fakePalette = L"contrast";
    LoadSettings();

    static DvaColorRGBA deep = GdiToDva(RGB(0x00, 0x26, 0x51));
    DvaColorRGBA painted{};
    DvaColorRGBA again{};
    CHECK(PaintFromBelow(&deep, &painted));
    CHECK(IsNeutral(painted.r, painted.g, painted.b, 0.01f));
    CHECK(!PaintFromBelow(&painted, &again));

    // A blue a theme function hands out takes the highlight in its slot, and
    // gets its blue back on a palette without one.
    g_fakePalette = L"threshold";
    Reload();

    static DvaColorRGBA focus = GdiToDva(RGB(0x40, 0x96, 0xF3));
    const DvaColorRGBA* p = ConvertColorRef(&focus);
    CHECK(p != &focus);
    CHECK(IsConvertedSlot(p));

    g_fakePalette = L"onyx";
    Reload();
    CHECK(SameColor(*p, focus));

    // The switch keeps the blue on any palette.
    g_fakePalette = L"threshold";
    SetFakeInt(L"highlight", 0);
    LoadSettings();
    CHECK(!CurrentSettings().highlight);
    CHECK(!ConvertDvaColor(track, &out));

    SetFakeInt(L"highlight", 1);
    g_fakePalette = L"onyx";
    Reload();
}

static void TestStylesheetRewrite() {
    g_fakePalette = L"onyx";
    LoadSettings();

    char css[] =
        "body{background-color:#1e1e1e;color:#FFF}a{color:#2680eb}"
        ":root{--spectrum-global-color-gray-100-rgb:29,29,29;--x-rgb:8,8,8}"
        ".o{background:rgba(41, 41, 41,.498)}.l{color:#ebebeb99}"
        ".d{color:#4b4b4b}.u{fill:#2C2C2C}.v{color:rgb(var(--x-rgb))}"
        "#abcdefg{}.w{color:#1e1e1eff}";
    const std::string original = css;

    CHECK(RecolorStylesheet(css, original.size()) == 6);

    const std::string s = css;
    CHECK(s.size() == original.size());  // every color at its own length

    const std::string panel = Hex(Convert8(RGB(0x1E, 0x1E, 0x1E)), false);
    CHECK(s.find("background-color:" + panel + ";") != std::string::npos);
    CHECK(s.find("color:#FFF}") != std::string::npos);          // too light
    CHECK(s.find("a{color:#2680eb}") != std::string::npos);     // Onyx keeps the blue
    CHECK(s.find(".l{color:#ebebeb99}") != std::string::npos);  // too light
    CHECK(s.find(".d{color:#4b4b4b}") != std::string::npos);    // above the ceiling
    CHECK(s.find("rgb(var(--x-rgb))") != std::string::npos);    // a variable
    CHECK(s.find("#abcdefg{}") != std::string::npos);           // a name, not a color
    CHECK(s.find(".w{color:" + panel + "ff}") != std::string::npos);  // alpha kept
    CHECK(s.find(".u{fill:" + Hex(Convert8(RGB(0x2C, 0x2C, 0x2C)), true) + "}") !=
          std::string::npos);

    // Triplets, padded to the length they had.
    CHECK(s.find("gray-100-rgb:" + Triplet(Convert8(RGB(29, 29, 29)), 8) + ";") !=
          std::string::npos);
    CHECK(s.find("--x-rgb:" + Triplet(Convert8(RGB(8, 8, 8)), 5) + "}") !=
          std::string::npos);
    CHECK(s.find("rgba(" + Triplet(Convert8(RGB(41, 41, 41)), 10) + ",.498)") !=
          std::string::npos);

    // With a highlight, Spectrum's blue changes hue as well.
    g_fakePalette = L"threshold";
    LoadSettings();

    char link[] = "a{color:#2680eb}";
    CHECK(RecolorStylesheet(link, sizeof(link) - 1) == 1);
    CHECK(std::string(link) ==
          "a{color:" + Hex(Convert8(RGB(0x26, 0x80, 0xEB)), false) + "}");

    // A color whose digits would not fit keeps its own.
    g_fakePalette = L"premiere";
    LoadSettings();

    char tight[] = "--x-rgb:8,8,8;";
    CHECK(Convert8(RGB(8, 8, 8)) != RGB(8, 8, 8));
    CHECK(RecolorStylesheet(tight, sizeof(tight) - 1) == 0);
    CHECK(std::string(tight) == "--x-rgb:8,8,8;");

    g_fakePalette = L"onyx";
    LoadSettings();
}

static void TestBundledStylesheetPath() {
    SetUxpPluginsDirFrom(
        L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026\\Adobe Premiere Pro.exe");

    LPCWSTR relative = nullptr;
    CHECK(BundledFile::Stylesheet == BundledFileKind(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                              L"\\UXP\\plugins\\com.adobe.dva.text\\static\\css\\main.css",
                              &relative));
    CHECK(relative && wcscmp(relative, L"com.adobe.dva.text\\static\\css\\main.css") == 0);

    CHECK(BundledFile::Stylesheet == BundledFileKind(
        L"c:/program files/adobe/adobe premiere pro 2026/uxp/plugins/p/main.CSS", nullptr));
    CHECK(BundledFile::Stylesheet == BundledFileKind(L"\\\\?\\C:\\Program Files\\Adobe\\Adobe Premiere Pro "
                              L"2026\\UXP\\plugins\\p\\a.css",
                              nullptr));

    // The panel's script is bundled too, and carries the design tokens.
    CHECK(BundledFileKind(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                          L"\\UXP\\plugins\\p\\static\\js\\main.js",
                          nullptr) == BundledFile::Script);
    CHECK(BundledFileKind(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                          L"\\UXP\\plugins\\p\\a.json",
                          nullptr) == BundledFile::None);
    CHECK(BundledFile::None == BundledFileKind(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                               L"\\UXP\\plugins2\\p\\a.css",
                               nullptr));
    CHECK(BundledFile::None == BundledFileKind(L"C:\\Users\\me\\AppData\\Roaming\\Adobe\\UXP\\plugins"
                               L"\\p\\a.css",
                               nullptr));
    CHECK(BundledFile::None == BundledFileKind(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                               L"\\UXP\\plugins\\p\\..\\..\\..\\x.css",
                               nullptr));  // climbs back out
    CHECK(BundledFile::None == BundledFileKind(nullptr, nullptr));
    CHECK(BundledFile::None == BundledFileKind(L"a.css", nullptr));

    g_uxpPluginsDirLength = 0;
    CHECK(BundledFile::None == BundledFileKind(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                               L"\\UXP\\plugins\\p\\a.css",
                               nullptr));
}

static std::string ReadAllFrom(HANDLE file) {
    char buffer[256]{};
    DWORD read = 0;

    if (file == INVALID_HANDLE_VALUE ||
        !ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) {
        return {};
    }

    return std::string(buffer, read);
}

/*
    A border just above the ceiling is still a border.

    Spectrum outlines its inputs #494949 — 28.6%, six tenths of a point past
    the default ceiling — while their fill, #080808, is well under it. So the
    box was themed and its outline was not, which on a search field is the
    whole of the control. Brightness cannot separate the two: disabled text is
    #4B4B4B, eight tenths away. The property separates them, and a stylesheet
    is the one place in the mod that knows it.
*/
static COLORREF ConvertChrome8(COLORREF c) {
    const Settings& s = CurrentSettings();
    float gray = (GetRValue(c) + GetGValue(c) + GetBValue(c)) / 3.0f / 255.0f;
    COLORREF target = PickTarget(s, gray);

    auto channel = [&](BYTE from, BYTE to) {
        float v = BlendWith(s.strength, from / 255.0f, to / 255.0f) * 255.0f + 0.5f;
        return ClampInt(static_cast<int>(v), 0, 255);
    };

    return RGB(channel(GetRValue(c), GetRValue(target)),
               channel(GetGValue(c), GetGValue(target)),
               channel(GetBValue(c), GetBValue(target)));
}

static void TestStylesheetChromeCeiling() {
    g_fakePalette = L"onyx";
    SetFakeInt(L"ceiling", 28);
    LoadSettings();

    const std::string edge = Hex(ConvertChrome8(RGB(0x49, 0x49, 0x49)), false);

    char css[] =
        ".a{background-color:#080808;border-color:#494949;color:#c8c8c8}"
        ".b:hover{border-color:#494949}.c{border-top-color:#494949}"
        ".d{outline-color:#494949}.e{background:#494949}"
        ".f{color:#494949}.g{-webkit-text-fill-color:#494949}"
        ".h{fill:#494949}.i{border-color:#696969}";
    const std::string original = css;

    RecolorStylesheet(css, original.size());

    const std::string s = css;
    CHECK(s.size() == original.size());  // still every color at its own length

    // Chrome, all of it, and all at the palette's edge tone.
    for (const char* at : {".b:hover{border-color:", ".c{border-top-color:",
                           ".d{outline-color:", ".e{background:"}) {
        CHECK(s.find(std::string(at) + edge) != std::string::npos);
    }

    CHECK(s.find("border-color:" + edge + ";color:#c8c8c8") != std::string::npos);

    // Text and icons at the very same value are left exactly as they were.
    CHECK(s.find(".f{color:#494949}") != std::string::npos);
    CHECK(s.find(".g{-webkit-text-fill-color:#494949}") != std::string::npos);
    CHECK(s.find(".h{fill:#494949}") != std::string::npos);

    // And the slack stops short of the next neutral Spectrum uses for chrome.
    CHECK(s.find(".i{border-color:#696969}") != std::string::npos);

    /*
        A lower ceiling takes the border back out of reach, which is right: a
        lower ceiling is the user asking for less of the interface to be
        touched, not for borders to be exempt from that.
    */
    SetFakeInt(L"ceiling", 10);
    LoadSettings();

    char low[] = ".a{border-color:#494949}";
    CHECK(RecolorStylesheet(low, sizeof(low) - 1) == 0);

    SetFakeInt(L"ceiling", 28);
    LoadSettings();
}

/*
    The design tokens in a panel's own script.

    A UXP panel keeps its colors twice, and the copy in the script is the one
    its components actually use: they read it and set it as an inline style,
    which beats every rule a stylesheet can state. Premiere's Text panel sets
    its search field from "background-color":"rgb(37, 37, 37)" there, so
    recoloring main.css never reached it however right that recoloring was.

    Only the token shape is touched, and nothing goes looking for hex colors
    the way the stylesheet pass does — an icon stroke in the same script is
    "#231f20", and it has to stay an icon.
*/
static void TestTokenTableRewrite() {
    g_fakePalette = L"onyx";
    SetFakeInt(L"ceiling", 28);
    SetFakeInt(L"strength", 100);
    LoadSettings();

    const std::string field = Triplet(Convert8(RGB(37, 37, 37)), 10);
    const std::string edge = Triplet(ConvertChrome8(RGB(74, 74, 74)), 10);

    char js[] =
        "textfield:{default:{states:{default:{\"background-color\":\"rgb(37, 37, 37)\","
        "\"border-color\":\"rgb(74, 74, 74)\"},"
        "disabled:{\"text-color\":\"rgb(74, 74, 74)\"}}},"
        "icon:N.createElement(\"path\",{stroke:\"#231f20\"}),"
        "loose:\"rgb(37, 37, 37)\",white:{\"background-color\":\"rgb(255, 255, 255)\"}";
    const std::string original = js;

    size_t n = RecolorTokenTable(js, original.size());
    const std::string s = js;

    CHECK(s.size() == original.size());  // the script still parses the same
    CHECK(n == 2);

    // The search field's own fill, and its border, which needs the chrome slack.
    CHECK(s.find("\"background-color\":\"rgb(" + field + ")\"") != std::string::npos);
    CHECK(s.find("\"border-color\":\"rgb(" + edge + ")\"") != std::string::npos);

    /*
        Text at the very value the border took is left alone: the slack past
        the ceiling is for chrome, and a stylesheet and a token table draw the
        line in the same place.
    */
    CHECK(s.find("\"text-color\":\"rgb(74, 74, 74)\"") != std::string::npos);

    // And so is everything that is not a token: an icon stroke, a loose
    // string, and a color above the ceiling.
    CHECK(s.find("stroke:\"#231f20\"") != std::string::npos);
    CHECK(s.find("loose:\"rgb(37, 37, 37)\"") != std::string::npos);
    CHECK(s.find("\"rgb(255, 255, 255)\"") != std::string::npos);

    // A stylesheet's own pass does not run on a script, and vice versa.
    char both[] = ".a{color:#1e1e1e}\"border-color\":\"rgb(37, 37, 37)\"";
    const std::string beforeBoth = both;
    CHECK(RecolorTokenTable(both, beforeBoth.size()) == 1);
    CHECK(std::string(both).find(".a{color:#1e1e1e}") != std::string::npos);
}

static void TestStylesheetRedirect() {
    g_fakePalette = L"onyx";
    LoadSettings();
    CreateFileW_Original = CreateFileW;
    CreateFile2_Original = CreateFile2;

    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);

    const std::wstring root =
        std::wstring(temp) + L"pptheme-harness-" + std::to_wstring(GetCurrentProcessId());
    const std::wstring dirs[] = {root, root + L"\\UXP", root + L"\\UXP\\plugins",
                                 root + L"\\UXP\\plugins\\p"};

    for (const std::wstring& d : dirs) {
        CreateDirectoryW(d.c_str(), nullptr);
    }

    const std::wstring css = dirs[3] + L"\\main.css";
    const std::string original = "body{background-color:#1e1e1e}";

    HANDLE file = CreateFileW(css.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    WriteFile(file, original.data(), static_cast<DWORD>(original.size()), &written,
              nullptr);
    CloseHandle(file);

    SetUxpPluginsDirFrom((root + L"\\Adobe Premiere Pro.exe").c_str());

    const std::string themed =
        "body{background-color:" + Hex(Convert8(RGB(0x1E, 0x1E, 0x1E)), false) + "}";

    // A plain read gets the recolored copy, at the original's size.
    HANDLE copy = CreateFileW_Hook(css.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(copy != INVALID_HANDLE_VALUE);

    LARGE_INTEGER size{};
    CHECK(GetFileSizeEx(copy, &size) &&
          size.QuadPart == static_cast<LONGLONG>(original.size()));
    CHECK(ReadAllFrom(copy) == themed);

    DWORD wrote = 0;
    CHECK(!WriteFile(copy, "x", 1, &wrote, nullptr));  // only the rights it asked for

    wchar_t copyPath[MAX_PATH + 8]{};
    CHECK(GetFinalPathNameByHandleW(copy, copyPath, ARRAYSIZE(copyPath), 0) > 0);
    CloseHandle(copy);
    CHECK(GetFileAttributesW(copyPath) == INVALID_FILE_ATTRIBUTES);  // gone once closed

    // Overlapped, as an asynchronous reader opens it.
    copy = CreateFileW_Hook(css.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    CHECK(copy != INVALID_HANDLE_VALUE);

    if (copy != INVALID_HANDLE_VALUE) {
        char buffer[256]{};
        OVERLAPPED io{};
        io.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        DWORD read = 0;
        bool started = ReadFile(copy, buffer, sizeof(buffer), nullptr, &io) ||
                       GetLastError() == ERROR_IO_PENDING;
        CHECK(started && GetOverlappedResult(copy, &io, &read, TRUE));
        CHECK(std::string(buffer, read) == themed);

        CloseHandle(io.hEvent);
        CloseHandle(copy);
    }

    // CreateFile2 as well.
    copy = CreateFile2_Hook(css.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
                            nullptr);
    CHECK(ReadAllFrom(copy) == themed);
    CloseHandle(copy);

    // A name already taken, as a leftover or a planted link would be, is
    // stepped around and left untouched.
    const std::wstring taken = std::wstring(temp) + L"premiere-pro-theme-" +
                               std::to_wstring(GetCurrentProcessId()) + L"-" +
                               std::to_wstring(g_stylesheetSerial + 1) + L".css";
    HANDLE squat = CreateFileW(taken.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    WriteFile(squat, "keep", 4, &written, nullptr);
    CloseHandle(squat);

    copy = CreateFileW_Hook(css.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, 0, nullptr);
    CHECK(ReadAllFrom(copy) == themed);
    CloseHandle(copy);

    HANDLE kept = CreateFileW(taken.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    CHECK(ReadAllFrom(kept) == "keep");
    CloseHandle(kept);
    DeleteFileW(taken.c_str());

    // A write opens the file itself.
    copy = CreateFileW_Hook(css.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                            OPEN_EXISTING, 0, nullptr);
    CHECK(ReadAllFrom(copy) == original);
    CloseHandle(copy);

    // So does a read with the setting off.
    SetFakeInt(L"uxpPanels", 0);
    LoadSettings();

    copy = CreateFileW_Hook(css.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, 0, nullptr);
    CHECK(ReadAllFrom(copy) == original);
    CloseHandle(copy);

    SetFakeInt(L"uxpPanels", 1);
    LoadSettings();

    DeleteFileW(css.c_str());

    for (int i = 3; i >= 0; i--) {
        RemoveDirectoryW(dirs[i].c_str());
    }

    g_uxpPluginsDirLength = 0;
    CreateFileW_Original = nullptr;
    CreateFile2_Original = nullptr;
}

/*
    A value the mod produced is also a real Premiere gray: Onyx turns #3F3F3F
    into #1D1D1D, the stock panel gray. Only the paint that produced it may
    skip it, so a later #1D1D1D fill still converts.
*/
static void TestProducedIsRecentOnly() {
    g_fakePalette = L"onyx";
    LoadSettings();
    ForgetRecentProduced();

    static DvaColorRGBA header = Gray(0x3F);
    static DvaColorRGBA panel = Gray(0x1D);
    static DvaColorRGBA light = Gray(0xE0);
    DvaColorRGBA out{};

    CHECK(PaintFromBelow(&header, &out));
    CHECK(DvaToGdi(out) == RGB(0x1D, 0x1D, 0x1D));  // the collision

    // In the same paint #1D1D1D may be that very color copied back: left alone.
    CHECK(!PaintFromBelow(&panel, &out));

    // The next paint starts clean, and the real panel gray converts.
    DispatchDrawFromRoot_Original = FakeDispatch;
    DispatchDrawFromRoot_Hook(nullptr, &light, nullptr, false);
    CHECK(PaintFromBelow(&panel, &out));
    CHECK(DvaToGdi(out) != RGB(0x1D, 0x1D, 0x1D));

    // Without a paint boundary it ages out as the thread produces other colors.
    CHECK(PaintFromBelow(&header, &out));

    static DvaColorRGBA others[kRecentProduced];

    for (int i = 0; i < kRecentProduced; i++) {
        others[i] = Gray(0x28 + i);  // outputs 0x10..0x17, never an input here
        CHECK(PaintFromBelow(&others[i], &out));
    }

    CHECK(PaintFromBelow(&panel, &out));

    // An entry past its lifetime no longer counts, with no paint root at all.
    CHECK(PaintFromBelow(&header, &out));

    for (ULONGLONG& at : g_recentProducedAt) {
        at -= kRecentProducedMs + 1;
    }

    CHECK(PaintFromBelow(&panel, &out));

    ForgetRecentProduced();
}

static HTHEME g_exResult = nullptr;
static LPCWSTR g_exClass = nullptr;

static HTHEME WINAPI FakeOpenThemeDataEx(HWND, LPCWSTR themeClass, DWORD) {
    g_exClass = themeClass;
    return g_exResult;
}

static void TestOpenThemeDataEx() {
    OpenThemeDataEx_Original = FakeOpenThemeDataEx;
    HTHEME value = reinterpret_cast<HTHEME>(0x900);
    g_exResult = value;

    // "Menu" is swapped for the dark class and registered.
    CHECK(OpenThemeDataEx_Hook(nullptr, L"Menu", 0) == value);
    CHECK(g_exClass && wcscmp(g_exClass, L"DarkMode::Menu") == 0);
    CHECK(IsMenuTheme(value));

    // The same value handed back for another class is evicted.
    CHECK(OpenThemeDataEx_Hook(nullptr, L"ScrollBar", 0) == value);
    CHECK(!IsMenuTheme(value));

    OpenThemeDataEx_Original = nullptr;
}

/*
    Which windows the frame work is spent on.

    Only a top-level window with a frame to color. A message-only window never
    shows; a bare popup — which is what a menu, a tooltip and a combo dropdown
    are, and Premiere makes them constantly — has no caption, so setting
    caption colors on it is four round trips to DWM and a locked insert for
    nothing.
*/
static void TestFramedWindowsOnly() {
    CreateWindowExW_Original = CreateWindowExW;
    g_themedWindows.clear();

    HWND helper = CreateWindowExW_Hook(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                       nullptr, nullptr, nullptr);
    HWND popup = CreateWindowExW_Hook(0, L"STATIC", L"", WS_POPUP, 0, 0, 10, 10,
                                      nullptr, nullptr, nullptr, nullptr);
    HWND child = CreateWindowExW_Hook(0, L"STATIC", L"", WS_CHILD, 0, 0, 10, 10,
                                      helper, nullptr, nullptr, nullptr);
    HWND framed = CreateWindowExW_Hook(0, L"STATIC", L"", WS_POPUP | WS_CAPTION, 0, 0,
                                       10, 10, nullptr, nullptr, nullptr, nullptr);
    HWND sizing = CreateWindowExW_Hook(0, L"STATIC", L"", WS_POPUP | WS_THICKFRAME, 0,
                                       0, 10, 10, nullptr, nullptr, nullptr, nullptr);

    CHECK(helper && g_themedWindows.count(helper) == 0);  // never shows
    CHECK(popup && g_themedWindows.count(popup) == 0);    // no frame to color
    CHECK(child && g_themedWindows.count(child) == 0);    // not top level

    CHECK(framed && g_themedWindows.count(framed) == 1);
    CHECK(g_themedWindows[framed] & kThemedFrame);

    CHECK(sizing && g_themedWindows.count(sizing) == 1);
    CHECK(g_themedWindows[sizing] & kThemedFrame);

    for (HWND hwnd : {helper, popup, child, framed, sizing}) {
        DestroyWindow(hwnd);
    }

    g_themedWindows.clear();
    CreateWindowExW_Original = nullptr;
}

static void TestExplorerThemeClasses() {
    HWND bar = CreateWindowExW(0, L"ScrollBar", L"", WS_POPUP, 0, 0, 10, 10, nullptr,
                               nullptr, nullptr, nullptr);
    HWND label = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 10, 10, nullptr,
                                 nullptr, nullptr, nullptr);

    CHECK(bar && WantsExplorerTheme(bar));
    CHECK(label && !WantsExplorerTheme(label));  // everything else: no WM_THEMECHANGED
    CHECK(!WantsExplorerTheme(nullptr));

    DestroyWindow(bar);
    DestroyWindow(label);
}

// Last: it fills the table.
static void TestSlotSaturation() {
    LoadSettings();

    static DvaColorRGBA many[kSlotCount + 16];
    size_t stored = 0;

    for (DvaColorRGBA& c : many) {
        c = Gray(0x1D);
        stored += ConvertColorRef(&c) != &c ? 1 : 0;
    }

    CHECK(stored <= kSlotCount);
    CHECK(stored < ARRAYSIZE(many));  // the rest keep their own color
    CHECK(g_slotsFullLogged);
}

int main() {
    TestFibonacciIndex();
    TestColorTable();
    TestEraseHooks();
    TestContentScope();
    TestNodeDraw();
    TestSafeMode();
    TestDarkModeAttribute();
    TestThemedWindows();
    TestMenuThemes();
    TestGdiProduced();
    TestKnownModules();
    TestModuleRangeUnload();
    TestDisplaySurfaceRange();
    TestMonitorBandColor();
    TestMonitorFullSurface();
    TestMonitorBandRecolor();
    TestMonitorConstantShape();
    TestMonitorReset();
    TestMonitorRecolorOrder();
    TestSettingsChangedRetriesBothWaysIn();
    TestGdiOrder();
    TestGdiMatchesOldFormula();
    TestColorHookCounting();
    TestMenuTextOptions();
    TestMenuBarGate();
    TestMenuBarTheme();
    TestShippedDefaults();
    TestPaletteRules();
    TestPaletteOptions();
    TestPaletteReadme();
    TestCustomDimText();
    TestCustomTheme();
    TestThemeForSharing();
    TestHighlight();
    TestStylesheetRewrite();
    TestStylesheetChromeCeiling();
    TestTokenTableRewrite();
    TestBundledStylesheetPath();
    TestStylesheetRedirect();
    TestProducedIsRecentOnly();
    TestExplorerThemeClasses();
    TestOpenThemeDataEx();
    TestFramedWindowsOnly();
    TestSlotSaturation();

    if (g_failures) {
        std::printf("%d FAILED\n", g_failures);
        return 1;
    }

    std::printf("all passed\n");
    return 0;
}
