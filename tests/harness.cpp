// Logic tests for premiere-pro-theme.wh.cpp, built with the Windhawk API
// stubbed out (-DWH_EDITING) and the setting and hook calls redirected to the
// doubles below. tests/run.ps1 builds and runs them.

#include <windhawk_api.h>

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
    {L"brushHook", 1},       {L"nativeDarkMode", 1},
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
static const wchar_t* g_fakeTheme = L"";

static PCWSTR FakeGetStringSetting(PCWSTR name, ...) {
    if (wcscmp(name, L"palette") == 0) {
        return g_fakePalette;
    }
    if (wcscmp(name, L"customTheme") == 0) {
        return g_fakeTheme;
    }
    return L"";
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
static int g_themeLogs = 0;  // lines about the custom theme

static void FakeLog(PCWSTR format, ...) {
    if (wcsncmp(format, L"custom theme", 12) == 0) {
        g_themeLogs++;
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

    COLORREF raw = RGB(0x30, 0x30, 0x30);
    COLORREF once = ConvertGdiColor(raw);
    CHECK(once != raw);
    CHECK(ConvertGdiColor(once) == once);  // not converted a second time

    // A colour a theme function produced, turned into a COLORREF.
    static DvaColorRGBA divider = Gray(0x3A);
    const DvaColorRGBA* themed = ConvertColorRef(&divider);
    CHECK(themed != &divider);

    COLORREF asGdi = RGB(static_cast<int>(themed->r * 255.0f + 0.5f),
                         static_cast<int>(themed->g * 255.0f + 0.5f),
                         static_cast<int>(themed->b * 255.0f + 0.5f));
    CHECK(ConvertGdiColor(asGdi) == asGdi);
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
    g_fakeTheme = L"{\"base\":\"#050505\",\"panel\":\"#101010\",\"surface\":\"#0E0E0E\","
                  L"\"raised\":\"#161616\",\"border\":\"#242424\",\"text\":\"#404040\"}";
    LoadSettings();
    CHECK(CurrentSettings().palette.text == RGB(0x40, 0x40, 0x40));
    CHECK(CurrentSettings().palette.dimText == RGB(0x28, 0x28, 0x28));
    CHECK(!CurrentSettings().highlight);  // no highlight in the theme: the blue stays

    g_fakePalette = L"onyx";
    g_fakeTheme = L"";
    LoadSettings();
}

static Palette ThemeFrom(const wchar_t* json, int* logs) {
    g_fakePalette = L"custom";
    g_fakeTheme = json;
    g_themeLogs = 0;
    LoadSettings();
    *logs = g_themeLogs;
    return CurrentSettings().palette;
}

// The default the settings block ships, read from the mod itself.
static std::wstring DefaultTheme() {
    std::string path = __FILE__;
    path = path.substr(0, path.find_last_of("/\\") + 1) + "../premiere-pro-theme.wh.cpp";
    std::FILE* f = std::fopen(path.c_str(), "rb");
    std::string s;

    if (f) {
        char buffer[4096];
        size_t n;
        while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) {
            s.append(buffer, n);
        }
        std::fclose(f);
    }

    const std::string marker = "- customTheme: '";
    size_t at = s.find(marker);

    if (at == std::string::npos) {
        return {};
    }

    at += marker.size();
    std::string json = s.substr(at, s.find('\'', at) - at);
    return std::wstring(json.begin(), json.end());
}

static void TestCustomTheme() {
    const Palette onyx = kPalettes[0].colors;
    int logs = 0;

    // The shipped default is Onyx, exactly...
    std::wstring shipped = DefaultTheme();
    CHECK(!shipped.empty());
    Palette p = ThemeFrom(shipped.c_str(), &logs);
    CHECK(std::memcmp(&p, &onyx, sizeof(Palette)) == 0);
    CHECK(logs == 1);  // its name, with no author to add

    // ...and lists every key the reader knows, so it is the template to edit.
    std::vector<ThemeMember> members;
    size_t errorAt = 0;
    CHECK(ReadThemeMembers(shipped.c_str(), &members, &errorAt));

    for (PCWSTR key : {L"name", L"author", L"base", L"panel", L"surface", L"raised",
                       L"border", L"text", L"disabledText", L"accent", L"highlight"}) {
        CHECK(FindThemeMember(members, key) != nullptr);
    }

    // A full theme, with what a Discord paste brings along around it.
    p = ThemeFrom(
        L"here it is:\n```json\n{\n  \"name\": \"Midnight \\u2728\", \"author\": \"someone\",\n"
        L"  \"base\": \"#05060A\", \"panel\": \"#0A0C14\", \"surface\": \"#10131F\",\n"
        L"  \"raised\": \"#181C2C\", \"border\": \"#2A3048\", \"text\": \"#E6E9F5\",\n"
        L"  \"accent\": \"#3A4270\", \"disabledText\": \"#6B7090\", \"highlight\": \"4F7BFF\",\n"
        L"  \"version\": 2, \"extra\": {\"nested\": [1, -2.5e3, true, null, {\"a\": \"}\"}]}\n"
        L"}\n```\nenjoy :}",
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
    CHECK(CurrentSettings().highlight);
    CHECK(logs == 1);  // only the name line

    // Optional members left out, and keys in another case.
    p = ThemeFrom(L"{\"Base\":\"#000000\",\"PANEL\":\"#101010\",\"surface\":\"#202020\","
                  L"\"raised\":\"#303030\",\"border\":\"#404040\",\"text\":\"#F0F0F0\"}",
                  &logs);
    CHECK(p.ramp[0] == RGB(0, 0, 0));
    CHECK(p.ramp[1] == RGB(0x10, 0x10, 0x10));
    CHECK(p.accent == RGB(0x40, 0x40, 0x40));      // the border
    CHECK(p.dimText == RGB(0x80, 0x80, 0x80));     // halfway from text to panel
    CHECK(p.highlight == CLR_INVALID);
    CHECK(!CurrentSettings().highlight);
    CHECK(logs == 0);

    // A required color missing or unreadable falls back to Onyx's, one at a time.
    p = ThemeFrom(L"{\"base\":\"#000000\",\"panel\":\"red\",\"surface\":7,"
                  L"\"raised\":\"#303030\",\"border\":\"#404040\",\"text\":\"#F0F0F0\"}",
                  &logs);
    CHECK(p.ramp[0] == RGB(0, 0, 0));
    CHECK(p.ramp[1] == onyx.ramp[1]);
    CHECK(p.ramp[2] == onyx.ramp[2]);
    CHECK(p.ramp[3] == RGB(0x30, 0x30, 0x30));
    CHECK(logs == 2);

    // An optional one present but unreadable is logged and left out.
    p = ThemeFrom(L"{\"base\":\"#000000\",\"panel\":\"#101010\",\"surface\":\"#202020\","
                  L"\"raised\":\"#303030\",\"border\":\"#404040\",\"text\":\"#F0F0F0\","
                  L"\"accent\":\"#12345\"}",
                  &logs);
    CHECK(p.accent == RGB(0x40, 0x40, 0x40));
    CHECK(logs == 1);

    // Left empty or null on purpose: the defaults, without a word.
    p = ThemeFrom(L"{\"base\":\"#000000\",\"panel\":\"#101010\",\"surface\":\"#202020\","
                  L"\"raised\":\"#303030\",\"border\":\"#404040\",\"text\":\"#F0F0F0\","
                  L"\"accent\":\"\",\"disabledText\":null,\"highlight\":\"\","
                  L"\"name\":\"\",\"author\":\"\"}",
                  &logs);
    CHECK(p.accent == RGB(0x40, 0x40, 0x40));
    CHECK(p.dimText == RGB(0x80, 0x80, 0x80));
    CHECK(p.highlight == CLR_INVALID);
    CHECK(!CurrentSettings().highlight);
    CHECK(logs == 0);

    // A required color left empty is still missing.
    p = ThemeFrom(L"{\"base\":\"\",\"panel\":\"#101010\",\"surface\":\"#202020\","
                  L"\"raised\":\"#303030\",\"border\":\"#404040\",\"text\":\"#F0F0F0\"}",
                  &logs);
    CHECK(p.ramp[0] == onyx.ramp[0]);
    CHECK(logs == 1);

    // An empty object parses; every required color is reported.
    p = ThemeFrom(L"{}", &logs);
    CHECK(p.ramp[0] == onyx.ramp[0]);
    CHECK(logs == 6);

    // Anything that is not valid JSON is rejected whole: Onyx, and one line.
    const wchar_t* broken[] = {
        L"",
        L"no braces at all",
        L"{\"base\":\"#000000\",}",                 // trailing comma
        L"{\"base\":\"#000000\"",                   // unterminated object
        L"{\"base\":\"#0000",                       // unterminated string
        L"{base:\"#000000\"}",                      // unquoted key
        L"{\"base\" \"#000000\"}",                  // no colon
        L"{\"base\":\"\\x41\"}",                    // bad escape
        L"{\"base\":\"\\u12G4\"}",                  // bad \u
        L"{\u201Cbase\u201D:\u201C#000000\u201D}",  // curly quotes
        L"{\"a\":tru}",
        L"{\"a\":-}",
        L"{\"a\":1.}",
        L"{\"a\":[1 2]}",
        L"{\"a\":\"line\nbreak\"}",                 // raw control character
    };

    for (const wchar_t* text : broken) {
        p = ThemeFrom(text, &logs);
        CHECK(std::memcmp(&p, &onyx, sizeof(Palette)) == 0);
        CHECK(logs == 1);
    }

    // Nesting past the bound, and a paste past the length limit.
    std::wstring deep = L"{\"a\":";
    deep += std::wstring(200, L'[') + std::wstring(200, L']') + L"}";
    p = ThemeFrom(deep.c_str(), &logs);
    CHECK(std::memcmp(&p, &onyx, sizeof(Palette)) == 0);
    CHECK(logs == 1);

    std::wstring shallow = L"{\"a\":" + std::wstring(8, L'[') + std::wstring(8, L']') +
                           L",\"base\":\"#010203\"}";
    p = ThemeFrom(shallow.c_str(), &logs);
    CHECK(p.ramp[0] == RGB(1, 2, 3));

    std::wstring huge = L"{\"name\":\"" + std::wstring(20000, L'x') + L"\"}";
    p = ThemeFrom(huge.c_str(), &logs);
    CHECK(std::memcmp(&p, &onyx, sizeof(Palette)) == 0);
    CHECK(logs == 1);

    g_fakePalette = L"onyx";
    g_fakeTheme = L"";
    LoadSettings();
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

    CHECK(ShouldConvertGdi(RGB(0x30, 0x30, 0x30), adobe));
    CHECK(!ShouldConvertGdi(RGB(0xE0, 0xE0, 0xE0), adobe));    // too light
    CHECK(!ShouldConvertGdi(RGB(0x30, 0x10, 0x10), adobe));    // saturated
    CHECK(!ShouldConvertGdi(RGB(0x30, 0x30, 0x30), nullptr));  // not Adobe

    SetFakeInt(L"gdiHook", 0);
    LoadSettings();
    CHECK(!ShouldConvertGdi(RGB(0x30, 0x30, 0x30), adobe));

    SetFakeInt(L"gdiHook", 1);
    LoadSettings();
}

// The GDI path now goes through ConvertDvaColor; it must give what it gave.
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

        CHECK(ConvertGdiColor(RGB(level, level, level)) == old);
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
    CHECK(IsBundledStylesheet(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                              L"\\UXP\\plugins\\com.adobe.dva.text\\static\\css\\main.css",
                              &relative));
    CHECK(relative && wcscmp(relative, L"com.adobe.dva.text\\static\\css\\main.css") == 0);

    CHECK(IsBundledStylesheet(
        L"c:/program files/adobe/adobe premiere pro 2026/uxp/plugins/p/main.CSS", nullptr));
    CHECK(IsBundledStylesheet(L"\\\\?\\C:\\Program Files\\Adobe\\Adobe Premiere Pro "
                              L"2026\\UXP\\plugins\\p\\a.css",
                              nullptr));

    CHECK(!IsBundledStylesheet(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                               L"\\UXP\\plugins\\p\\main.js",
                               nullptr));
    CHECK(!IsBundledStylesheet(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                               L"\\UXP\\plugins2\\p\\a.css",
                               nullptr));
    CHECK(!IsBundledStylesheet(L"C:\\Users\\me\\AppData\\Roaming\\Adobe\\UXP\\plugins"
                               L"\\p\\a.css",
                               nullptr));
    CHECK(!IsBundledStylesheet(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
                               L"\\UXP\\plugins\\p\\..\\..\\..\\x.css",
                               nullptr));  // climbs back out
    CHECK(!IsBundledStylesheet(nullptr, nullptr));
    CHECK(!IsBundledStylesheet(L"a.css", nullptr));

    g_uxpPluginsDirLength = 0;
    CHECK(!IsBundledStylesheet(L"C:\\Program Files\\Adobe\\Adobe Premiere Pro 2026"
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

static void TestMessageOnlyWindows() {
    CreateWindowExW_Original = CreateWindowExW;
    g_themedWindows.clear();

    HWND helper = CreateWindowExW_Hook(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                       nullptr, nullptr, nullptr);
    HWND popup = CreateWindowExW_Hook(0, L"STATIC", L"", WS_POPUP, 0, 0, 10, 10,
                                      nullptr, nullptr, nullptr, nullptr);

    CHECK(helper && g_themedWindows.count(helper) == 0);  // never shows
    CHECK(popup && g_themedWindows.count(popup) == 1);

    DestroyWindow(helper);
    DestroyWindow(popup);
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
    TestGdiOrder();
    TestGdiMatchesOldFormula();
    TestColorHookCounting();
    TestMenuTextOptions();
    TestMenuBarGate();
    TestMenuBarTheme();
    TestCustomDimText();
    TestCustomTheme();
    TestHighlight();
    TestStylesheetRewrite();
    TestBundledStylesheetPath();
    TestStylesheetRedirect();
    TestProducedIsRecentOnly();
    TestExplorerThemeClasses();
    TestOpenThemeDataEx();
    TestMessageOnlyWindows();
    TestSlotSaturation();

    if (g_failures) {
        std::printf("%d FAILED\n", g_failures);
        return 1;
    }

    std::printf("all passed\n");
    return 0;
}
