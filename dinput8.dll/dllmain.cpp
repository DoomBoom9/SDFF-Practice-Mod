#include "pch.h"
#include <unknwn.h>
#include <windows.h>
#include <cstdio>
#include <cstdint>

// dinput8 proxy wrapper
HMODULE g_hOriginalDll = NULL;
FARPROC pDirectInput8Create = NULL;

void LoadOriginalDll()
{
    if (g_hOriginalDll) return;

    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    strcat_s(systemPath, "\\dinput8.dll");

    g_hOriginalDll = LoadLibraryA(systemPath);
    if (g_hOriginalDll) {
        pDirectInput8Create = GetProcAddress(g_hOriginalDll, "DirectInput8Create");
    }
}


extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
    if (!pDirectInput8Create) {
        LoadOriginalDll();
    }

    typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    return ((DirectInput8Create_t)pDirectInput8Create)(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

uintptr_t g_delta = 0;

const uintptr_t PREFERRED_BASE = 0x00400000;

const uintptr_t PTR_DEBUG_STRUCT_PB = 0x006E0550;
const uintptr_t ADDR_OpenCharacterDataDebugWindow_PB = 0x00432BDE; // FUN_00432bde
const uintptr_t ADDR_HOOK_TARGET_PB = 0x0042576F; // DrawDebugOverlays entry

const uintptr_t ADDR_SCENEID = 0x006d9924;

const uintptr_t ADDR_CAMERA_CONTROLLER_PB = 0x006D50B8;
const uintptr_t ADDR_DAT_0064EF98_PB = 0x0064EF98;

const DWORD OFFSET_3D_RENDER_FLAGS = 0x00;
const DWORD OFFSET_COLLISION_FLAGS = 0x04;
const DWORD OFFSET_DRAW_LEGEND = 0x34;
const DWORD OFFSET_COLLISION_MODE = 0x6C;

uintptr_t g_ptrDebugStruct;
uintptr_t g_addrDebugWindow;
uintptr_t g_addrHookTarget;

uintptr_t g_addrCameraController;
uintptr_t g_addrPlayerBase;

uintptr_t GetDebugStructBase()
{
    if (g_ptrDebugStruct == 0) return 0;
    if (IsBadReadPtr((void*)g_ptrDebugStruct, sizeof(uintptr_t))) return 0;

    uintptr_t base = *(uintptr_t*)g_ptrDebugStruct;
    if (base == 0 || IsBadReadPtr((void*)base, sizeof(uintptr_t))) return 0;

    return base;
}

void WriteInt(DWORD offset, int value)
{
    uintptr_t base = GetDebugStructBase();
    if (base != 0) *(int*)(base + offset) = value;
}

void WriteUINT(DWORD offset, UINT value)
{
    uintptr_t base = GetDebugStructBase();
    if (base != 0) *(UINT*)(base + offset) = value;
}


// this class is strictly a container that reads and stores playerData
// to be rendered to the UI, It does not write to game mem.
// any operation that writes should be done by the GameState class.
struct Player {
    uintptr_t playerBase;
    uintptr_t vtable;
    float health;
    Vector3 position;
    float gravity;
    uint8_t characterType;
};

struct Vector3 {
    float x;
    float y;
    float z;
};

template <typename T>
bool SafeRead(uintptr_t addr, T* outValue)
{
    __try {
        if (addr == 0) return false;
        *outValue = *(T*)addr;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool SafeWrite(uintptr_t addr, T value)
{
    __try {
        if (addr == 0) return false;
        *(T*)addr = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

namespace PlayerMemory {
    uintptr_t ResolveBase() {
        uintptr_t basePtr = 0;
        if (!SafeRead(g_addrPlayerBase, &basePtr)) return 0;

        uint32_t offsets[4] = { 0x10, 0x740, 0x130, 0x4 };
        uintptr_t addr = basePtr;
        for (int i = 0; i < 4; i++) {
            if (!SafeRead(addr + offsets[i], &addr) || addr == 0) return 0;
        }
        return addr;
    }

    // can still read garbage but wont crash on an access violation
    // writes the player struct to out if possible
    // returns true if all reads were successful, otherwise returns false.
    bool Read(Player& out) {
        out.playerBase = ResolveBase();
        if (out.playerBase == 0) return false;

        bool safe = true;
        safe &= SafeRead(out.playerBase + 0x30, &out.position.x);
        safe &= SafeRead(out.playerBase + 0x34, &out.position.y);
        safe &= SafeRead(out.playerBase + 0x38, &out.position.z);
        safe &= SafeRead(out.playerBase + 0x84, &out.gravity);

        return safe;
    }


}

class GameState {
public:
    Player displayPlayer;

    void Refresh() {
        PlayerMemory::Read(displayPlayer);
    }

    bool WriteGravity(float newGrav) {
        if (displayPlayer.playerBase == 0) return false;
        if (!SafeWrite(displayPlayer.playerBase + 0x88, newGrav)) return false;
    }

    bool WritePosition(const Vector3& newPos) {
        if (displayPlayer.playerBase == 0) return false;
        bool success = true;
        success &= SafeWrite(displayPlayer.playerBase + 0x30, newPos.x);
        success &= SafeWrite(displayPlayer.playerBase + 0x34, newPos.y);
        success &= SafeWrite(displayPlayer.playerBase + 0x38, newPos.z);
        if (success) displayPlayer.position = newPos;

        return success;
    }

template <typename T>
bool SafeRead(uintptr_t addr, T* outValue)
{
    __try {
        if (addr == 0) return false;
        *outValue = *(T*)addr;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool SafeWrite(uintptr_t addr, T value)
{
    __try {
        if (addr == 0) return false;
        *(T*)addr = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// toggles
volatile bool g_gravityDisabled = false;
volatile float g_heightMod = 1.0f;
volatile float g_savedY = 0.0f;
volatile float g_savedX = 0.0f;
volatile float g_savedZ = 0.0f;
volatile bool g_hasSavedY = false;

HMODULE g_hMyModule = NULL;
int g_sceneId = -1;

// dbg window
enum ControlId : int {
    ID_STATIC_PLAYER_Y = 1001,
    ID_STATIC_CAMERA_INFO,
    ID_CHECK_GRAVITY,
    ID_BUTTON_SAVE_Y,
    ID_BUTTON_LOAD_Y,
    ID_BUTTON_HEIGHT_UP,
    ID_BUTTON_HEIGHT_DOWN,
    ID_STATIC_HEIGHT_MOD,
    ID_APPLY_HEIGHT
};

HWND g_hStaticPlayerY = NULL;
HWND g_hStaticCameraInfo = NULL;
HWND g_hCheckGravity = NULL;
HWND g_hStaticHeightMod = NULL;

void RefreshDebugWindow()
{
 
}

LRESULT CALLBACK DebugWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        return 0;
    }

    case WM_TIMER:
        RefreshDebugWindow();
        return 0;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int notify = HIWORD(wParam);

        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

DWORD WINAPI DebugWindowThread(LPVOID lpParam)
{
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DebugWndProc;
    wc.hInstance = g_hMyModule;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "CustomDebugToolWindowClass";
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(
        WS_EX_TOPMOST, "CustomDebugToolWindowClass", "Debug Tool",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 420,
        NULL, NULL, g_hMyModule, NULL);

    if (!hwnd) {
        OutputDebugStringA("[Debug] Failed to create debug window\n");
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}

DWORD WINAPI GameplayThread(LPVOID lpParam)
{
    while (true)
    {

        Sleep(50);
    }
    return 0;
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    Sleep(200);

    uintptr_t actualBase = (uintptr_t)GetModuleHandleA(NULL);
    g_delta = actualBase - PREFERRED_BASE;

    g_addrCameraController = ADDR_CAMERA_CONTROLLER_PB + g_delta;
    g_addrPlayerBase = ADDR_DAT_0064EF98_PB + g_delta;
    g_ptrDebugStruct = PTR_DEBUG_STRUCT_PB + g_delta;

    char buf[128];
    sprintf_s(buf, "[Debug] Module base=0x%p delta=0x%p\n", (void*)actualBase, (void*)g_delta);
    OutputDebugStringA(buf);

    CreateThread(NULL, 0, DebugWindowThread, NULL, 0, NULL);
    CreateThread(NULL, 0, GameplayThread, NULL, 0, NULL);
    return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_hMyModule = hModule;
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (g_hOriginalDll) FreeLibrary(g_hOriginalDll);
    }
    return TRUE;
}
