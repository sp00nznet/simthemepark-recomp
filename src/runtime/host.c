/*
 * Sim Theme Park (Gold Edition 2.0) - static recompilation host.
 *
 * A 32-bit host on pcrecomp's runtime/native32 (the native bridge, callbacks,
 * machine lock) and its shared module loader (modules32.c). The game is one
 * executable, tp.exe, mapped at its own base 0x00400000 and lifted whole; its
 * DLLs (QMixer, the wea* online modules, usp11, dplayerx) are third-party and
 * load natively, the way kiss-recomp loads Miles.
 *
 * Linked at /BASE:0x60000000 (CMakeLists.txt) so 0x00400000..0x00BC6000 is
 * free. docs/bringup.md has what each option is for.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native32.h"
#include "recomp_trace.h"

#define EXE_BASE 0x00400000u
#define ARG(n) MEM32(g_esp + 4 + 4 * (n))

static native32_module_t g_mods[] = { { "tp.exe", EXE_BASE } };
static char g_game_dir[MAX_PATH];
static DWORD g_watchdog_s;
static void record_close(void);

static void shim_MessageBoxA(void) {
    fprintf(stderr, "[messagebox] %s: %s\n", (char*)(uintptr_t)ARG(2), (char*)(uintptr_t)ARG(1));
    g_eax = IDOK;
    g_esp += 4 + 4 * 4;
}

static void shim_MessageBoxW(void) {
    fprintf(stderr, "[messagebox] %ls: %ls\n", (wchar_t*)(uintptr_t)ARG(2), (wchar_t*)(uintptr_t)ARG(1));
    recomp_dump_trace("messagebox");
    g_eax = IDOK;
    g_esp += 4 + 4 * 4;
}

static native32_shim_t g_shims[] = {
    { "MessageBoxA", shim_MessageBoxA },
    { "MessageBoxW", shim_MessageBoxW },
};

/* ---- diagnostics ---------------------------------------------------------- */

void recomp_not_lifted(uint32_t va) {
    fprintf(stderr, "\n[not-lifted] sub_%08X  (called from 0x%08X)\n", va, g_cur_func);
    recomp_dump_trace("not-lifted");
    native32_dump_icalls(8);
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 2);
}

recomp_func_t recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }

static LONG CALLBACK crash(EXCEPTION_POINTERS* ep) {
    static volatile LONG once;
    EXCEPTION_RECORD* r = ep->ExceptionRecord;
    HMODULE hm;
    if ((r->ExceptionCode & 0xF0000000u) != 0xC0000000u) return EXCEPTION_CONTINUE_SEARCH;
    /* A fault inside a Windows DLL is its own business first (ddraw probes
     * with __try); only the host (lifted code) and code outside any module. */
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)r->ExceptionAddress, &hm) && hm != GetModuleHandleA(NULL))
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedExchange(&once, 1)) { Sleep(5000); TerminateProcess(GetCurrentProcess(), 3); }
    fprintf(stderr, "\n=== fault 0x%08lX at %p, thread %lu ===\n", r->ExceptionCode, r->ExceptionAddress,
            GetCurrentThreadId());
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2)
        fprintf(stderr, "  %s of 0x%08X\n", r->ExceptionInformation[0] == 0 ? "read" :
                r->ExceptionInformation[0] == 1 ? "write" : "execute", (uint32_t)r->ExceptionInformation[1]);
    fprintf(stderr, "  in lifted sub_%08X, last native call %s\n", g_cur_func, g_cur_import);
    fprintf(stderr, "  eax=%08X ecx=%08X edx=%08X ebx=%08X esp=%08X ebp=%08X esi=%08X edi=%08X\n",
            g_eax, g_ecx, g_edx, g_ebx, g_esp, g_ebp, g_esi, g_edi);
    native32_dump_icalls(12);
    recomp_trace_flush();
    TerminateProcess(GetCurrentProcess(), 3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI watchdog(LPVOID unused) {
    (void)unused;
    Sleep(g_watchdog_s * 1000);
    fprintf(stderr, "\n[watchdog] %lu s: in sub_%08X, last native call %s, %u indirect calls\n",
            g_watchdog_s, g_cur_func, g_cur_import, g_icall_count);
    native32_dump_icalls(8);
    recomp_trace_flush();
    fflush(stderr);
    record_close();
    TerminateProcess(GetCurrentProcess(), 4);
    return 0;
}

/* ---- --shot S FILE and --record FILE.mp4: the frames the game presents ---
 * Taken from the surface the game blits to its window (native32_on_present):
 * on the private desktop of --headless a GDI copy of the window is black. */
static DWORD g_shot_s[16];
static char g_shot_file[16][MAX_PATH];
static int g_shots;
static volatile LONG g_shot_due = -1;
static char g_record[MAX_PATH];
static FILE* g_rec;
static int g_rec_w, g_rec_h, g_rec_bits;

static BOOL CALLBACK find_window(HWND w, LPARAM out) {
    DWORD pid;
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(w)) { *(HWND*)out = w; return FALSE; }
    return TRUE;
}

static HWND game_window(void) {
    HWND w = NULL;
    EnumWindows(find_window, (LPARAM)&w);
    return w;
}

static void write_bmp(const char* file, int w, int h, const uint8_t* bgr, DWORD stride) {
    BITMAPINFOHEADER bh = { sizeof bh, w, h, 1, 24, BI_RGB };
    BITMAPFILEHEADER fh = { 0x4D42, sizeof fh + sizeof bh + stride * h, 0, 0, sizeof fh + sizeof bh };
    FILE* f = fopen(file, "wb");
    if (!f) return;
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&bh, sizeof bh, 1, f);
    fwrite(bgr, stride * h, 1, f);
    fclose(f);
    printf("[shot] %dx%d -> %s\n", w, h, file);
}

static void on_present(void* surf, int version) {
    LONG due = g_shot_due;
    if (due < 0 && !g_record[0]) return;
    typedef HRESULT (WINAPI *lock_t)(void*, RECT*, void*, DWORD, HANDLE);
    typedef HRESULT (WINAPI *unlock_t)(void*, void*);
    void** vt = *(void***)surf;
    uint8_t d[0x7C] = { 0 };                    /* DDSURFACEDESC (0x6C) or DDSURFACEDESC2 (0x7C) */
    *(DWORD*)d = version == 1 ? 0x6C : 0x7C;
    if (((lock_t)vt[25])(surf, NULL, d, 1 /* DDLOCK_WAIT */, NULL)) return;
    int w = *(int*)(d + 0x0C), h = *(int*)(d + 0x08), pitch = *(int*)(d + 0x10);
    int bits = (int)*(uint32_t*)(d + 0x54);
    uint32_t rmask = *(uint32_t*)(d + 0x58);
    const uint8_t* src = *(const uint8_t**)(d + 0x24);
    if (due >= 0) {
        DWORD stride = (w * 3 + 3) & ~3u;
        uint8_t* out = (uint8_t*)calloc(stride, h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                uint8_t* o = out + (h - 1 - y) * stride + x * 3;
                if (bits == 16) {
                    uint16_t p = ((const uint16_t*)(src + y * pitch))[x];
                    int r5 = rmask == 0x7C00 ? (p >> 10) & 31 : p >> 11;
                    int g = rmask == 0x7C00 ? ((p >> 5) & 31) << 3 : ((p >> 5) & 63) << 2;
                    o[0] = (uint8_t)((p & 31) << 3); o[1] = (uint8_t)g; o[2] = (uint8_t)(r5 << 3);
                } else if (bits == 32) {
                    memcpy(o, src + y * pitch + x * 4, 3);
                }
            }
        write_bmp(g_shot_file[due], w, h, out, stride);
        free(out);
        InterlockedExchange(&g_shot_due, -1);
    }
    if (g_record[0] && (bits == 16 || bits == 32)) {
        if (!g_rec) {
            char cmd[MAX_PATH + 256];
            _snprintf(cmd, sizeof cmd - 1, "ffmpeg -hide_banner -loglevel error -y -f rawvideo -pix_fmt %s -s %dx%d "
                      "-use_wallclock_as_timestamps 1 -i - -vf format=yuv420p -fps_mode vfr \"%s\"",
                      bits == 32 ? "bgr0" : rmask == 0x7C00 ? "rgb555le" : "rgb565le", w, h, g_record);
            cmd[sizeof cmd - 1] = 0;
            g_rec = _popen(cmd, "wb");
            g_rec_w = w; g_rec_h = h; g_rec_bits = bits;
            if (!g_rec) { fprintf(stderr, "[record] cannot start ffmpeg\n"); g_record[0] = 0; }
        }
        if (g_rec && w == g_rec_w && h == g_rec_h && bits == g_rec_bits)
            for (int y = 0; y < h; y++) fwrite(src + y * pitch, (size_t)(w * bits / 8), 1, g_rec);
    }
    ((unlock_t)vt[32])(surf, NULL);
}

/* ponytail: a 200 ms grace for the frame in flight, not a lock. */
static void record_close(void) {
    if (!g_rec) return;
    g_record[0] = 0;
    Sleep(200);
    _pclose(g_rec);
    g_rec = NULL;
}

static DWORD WINAPI shot_thread(LPVOID unused) {
    (void)unused;
    DWORD t0 = GetTickCount();
    for (int i = 0; i < g_shots; i++) {
        DWORD at = t0 + g_shot_s[i] * 1000, now = GetTickCount();
        if (at > now) Sleep(at - now);
        InterlockedExchange(&g_shot_due, i);
        for (int k = 0; k < 150 && g_shot_due >= 0; k++) Sleep(20);
        if (InterlockedExchange(&g_shot_due, -1) >= 0) printf("[shot] no frame presented in 3 s\n");
    }
    return 0;
}

/* ---- --key VK@S and --click X,Y@S: input posted to the game window --------
 * Posted, not SendInput: under --headless the window is on a desktop of its
 * own, and SendInput would type into the user's session. */
static struct { int x, y; WORD vk; DWORD at; } g_input[64];
static int g_ninput;

static DWORD WINAPI input_thread(LPVOID unused) {
    (void)unused;
    DWORD t0 = GetTickCount();
    for (int i = 0; i < g_ninput; i++) {
        DWORD at = t0 + g_input[i].at, now = GetTickCount();
        if (at > now) Sleep(at - now);
        HWND w = game_window();
        if (!w) { printf("[input] no window\n"); continue; }
        /* Both ways in: the game's menus and park read DirectInput (native32's
         * synthetic devices under --headless), its text boxes WM_CHAR. The
         * mouse is relative, so a click first pins it to the top-left corner
         * and then moves it by X,Y; the game's cursor follows its deltas. */
        if (g_input[i].vk) {
            UINT sc = MapVirtualKeyA(g_input[i].vk, 0);
            LPARAM lp = 1 | ((LPARAM)sc << 16);
            native32_input_key((int)sc, 1);
            PostMessageA(w, WM_KEYDOWN, g_input[i].vk, lp);
            Sleep(150);
            native32_input_key((int)sc, 0);
            PostMessageA(w, WM_KEYUP, g_input[i].vk, lp | 0xC0000000);
            printf("[key] 0x%02X at %lu ms\n", g_input[i].vk, GetTickCount() - t0);
        } else {
            native32_input_mouse(-4000, -4000, 0);
            Sleep(150);
            native32_input_mouse(g_input[i].x, g_input[i].y, 0);
            LPARAM xy = MAKELPARAM(g_input[i].x, g_input[i].y);
            PostMessageA(w, WM_MOUSEMOVE, 0, xy);
            Sleep(250);
            native32_input_button(0, 1);
            PostMessageA(w, WM_LBUTTONDOWN, MK_LBUTTON, xy);
            Sleep(200);
            native32_input_button(0, 0);
            PostMessageA(w, WM_LBUTTONUP, 0, xy);
            printf("[click] %d,%d at %lu ms\n", g_input[i].x, g_input[i].y, GetTickCount() - t0);
        }
    }
    return 0;
}

static int add_input(const char* spec, int click) {
    const char* at = strchr(spec, '@');
    if (!at || g_ninput >= 64) return 0;
    if (click) {
        if (sscanf(spec, "%d,%d", &g_input[g_ninput].x, &g_input[g_ninput].y) != 2) return 0;
    } else {
        g_input[g_ninput].vk = (WORD)strtoul(spec, NULL, 0);
    }
    g_input[g_ninput++].at = (DWORD)(atof(at + 1) * 1000);
    return 1;
}

static int host_main(int argc, char** argv) {
    const char* game = "game";
    int run = 0;
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Silent test runs: --mute, and --headless unless PCRECOMP_MUTE says
     * otherwise (native32_init mutes the audio session; PCRECOMP_MUTE=0
     * restores it). Set before the relaunches, which inherit it. */
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--mute") ||
            (!strcmp(argv[i], "--headless") && !GetEnvironmentVariableA("PCRECOMP_MUTE", NULL, 0)))
            SetEnvironmentVariableA("PCRECOMP_MUTE", "1");
    for (int i = 1; i < argc; i++) {
        int rc;
        if (!strcmp(argv[i], "--headless") && (rc = native32_headless()) >= 0) return rc;
    }
    /* The renderer wants a 16-bit display mode (docs/bringup.md). */
    {
        int want16 = 0, rc;
        for (int i = 1; i < argc; i++) want16 |= !strcmp(argv[i], "--16bit");
        for (int i = 1; i < argc; i++)
            if (want16 && !strcmp(argv[i], "--run") && (rc = native32_require_color_depth(16)) >= 0) return rc;
    }
    for (int i = 1; i < argc; i++) {
        int n = recomp_trace_arg(argc, argv, i);
        if (n) { i += n - 1; continue; }
        if (!strcmp(argv[i], "--run")) run = 1;
        else if (!strcmp(argv[i], "--headless") || !strcmp(argv[i], "--16bit") || !strcmp(argv[i], "--mute")) ;
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--watchdog") && i + 1 < argc) g_watchdog_s = strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc && g_shots < 16) {
            g_shot_s[g_shots] = strtoul(argv[++i], NULL, 0);
            GetFullPathNameA(argv[++i], MAX_PATH, g_shot_file[g_shots++], NULL);
        }
        else if (!strcmp(argv[i], "--key") && i + 1 < argc) { if (!add_input(argv[++i], 0)) return 1; }
        else if (!strcmp(argv[i], "--click") && i + 1 < argc) { if (!add_input(argv[++i], 1)) return 1; }
        else if (!strcmp(argv[i], "--record") && i + 1 < argc) GetFullPathNameA(argv[++i], MAX_PATH, g_record, NULL);
        else if (!strcmp(argv[i], "--native-trace")) native32_trace_native = 1;
        else if (!strcmp(argv[i], "--callbacks")) native32_trace_callbacks = 1;
        else {
            printf("usage: simthemepark [--run] [--headless] [--game DIR] [--watchdog S]\n"
                   "                    [--shot S FILE.bmp] [--record FILE.mp4] [--key VK@S] [--click X,Y@S]\n"
                   "                    [--native-trace] [--callbacks]\n");
            recomp_trace_help();
            return argv[i][1] == 'h' || argv[i][2] == 'h' ? 0 : 1;
        }
    }
    GetFullPathNameA(game, MAX_PATH, g_game_dir, NULL);
    {
        char exe[MAX_PATH];
        _snprintf(exe, sizeof exe - 1, "%s\\tp.exe", g_game_dir);
        exe[MAX_PATH - 1] = 0;
        native32_hold_image(exe, EXE_BASE);
    }
    SetDllDirectoryA(g_game_dir);     /* QMixer.dll and the wea* DLLs */

    native32_init();
    native32_allow_heap_code();
    {   /* What the installer would have put in HKLM (docs/bringup.md): the
         * game's Reg* calls under its key are served from this file.
         * Language is a LANGID (0x409 US English; the game's default 0x809). */
        char reg[MAX_PATH];
        GetFullPathNameA("work\\registry.reg", MAX_PATH, reg, NULL);
        if (GetFileAttributesA(reg) == INVALID_FILE_ATTRIBUTES) {
            FILE* f = fopen(reg, "w");
            if (f) {
                fprintf(f, "REGEDIT4\n\n[HKEY_LOCAL_MACHINE\\Software\\Bullfrog Productions Ltd\\Theme Park World]\n"
                           "\"Language\"=dword:00000409\n");
                fclose(f);
            }
        }
        native32_registry(reg);
    }
    {   /* dialog.tcf: the options the original launcher saved (docs/bringup.md).
         * Without it the game starts full screen, which a private desktop
         * (--headless) cannot do. Version 2, the engine flags with bit 0
         * (full screen) clear, start-up flags 0x4002 (0x4000: the front end,
         * not a saved park), theme, guard words, texture cache. */
        char tcf[MAX_PATH];
        _snprintf(tcf, sizeof tcf - 1, "%s\\dialog.tcf", g_game_dir);
        tcf[MAX_PATH - 1] = 0;
        if (GetFileAttributesA(tcf) == INVALID_FILE_ATTRIBUTES) {
            static const uint32_t dlg[7] = { 2, 0xC0E14, 0x4002, 0, 0, 0, 0 };
            FILE* f = fopen(tcf, "wb");
            if (f) { fwrite(dlg, sizeof dlg, 1, f); fclose(f); printf("  wrote %s (windowed)\n", tcf); }
        }
    }
    native32_on_present(on_present);
    AddVectoredExceptionHandler(0, crash);
    printf("Sim Theme Park recomp host\n  lifted functions in dispatch: %u\n", recomp_dispatch_count);
    if (native32_modules_map(g_game_dir, g_game_dir, g_mods, 1) != 1) {
        fprintf(stderr, "cannot map %s\\tp.exe at 0x%08X\n", g_game_dir, EXE_BASE);
        return 1;
    }
    int missing = native32_modules_bind(g_shims, (int)(sizeof g_shims / sizeof g_shims[0]));
    if (missing) fprintf(stderr, "warning: %d imports unresolved\n", missing);
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(uintptr_t)(EXE_BASE + MEM32(EXE_BASE + 0x3C));
    uint32_t entry = EXE_BASE + nt->OptionalHeader.AddressOfEntryPoint;
    if (!run) {
        printf("\n(dry run: mapped and bound; --run enters 0x%08X)\n", entry);
        return missing ? 1 : 0;
    }
    if (!SetCurrentDirectoryA(g_game_dir)) { fprintf(stderr, "cannot enter %s\n", g_game_dir); return 1; }
    if (g_watchdog_s) CloseHandle(CreateThread(NULL, 0, watchdog, NULL, 0, NULL));
    if (g_shots) CloseHandle(CreateThread(NULL, 0, shot_thread, NULL, 0, NULL));
    if (g_ninput) CloseHandle(CreateThread(NULL, 0, input_thread, NULL, 0, NULL));
    if (!native32_attach_imports(EXE_BASE)) return 1;
    printf("  entering 0x%08X\n\n", entry);
    native32_call_guest(entry, 0, NULL);
    record_close();
    printf("\nentry returned eax=%08X\n", g_eax);
    return (int)g_eax;
}

/* Lifted code runs on the host's C stack and nests deeply, so it gets 16 MB on
 * a thread of its own; the first thread is linked with 256 KB (CMakeLists.txt)
 * so its stack fits below tp.exe (native32_hold_image). */
static int g_argc;
static char** g_argv;
static DWORD WINAPI host_thread(LPVOID unused) { (void)unused; return (DWORD)host_main(g_argc, g_argv); }

int main(int argc, char** argv) {
    DWORD code = 1;
    g_argc = argc;
    g_argv = argv;
    HANDLE t = CreateThread(NULL, 16u << 20, host_thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!t) return 1;
    WaitForSingleObject(t, INFINITE);
    GetExitCodeThread(t, &code);
    return (int)code;
}
