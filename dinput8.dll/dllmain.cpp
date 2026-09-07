#include "pch.h"
#include <unknwn.h>
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <string>

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

const uintptr_t ADDR_CAMERA_CONTROLLER_PB = 0x006D50B8;
const uintptr_t ADDR_DAT_0064EF98_PB = 0x0064EF98;

const uintptr_t ADDR_LEVEL_MODE = 0x006d99CC;
const uintptr_t ADDR_SCENE_ID = 0x006d9924;

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

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Player {
    uintptr_t playerBase;
    uintptr_t jumpFlag;
    uintptr_t vtable;
    float health;
    Vector3 position;
    float gravity;
    uint8_t characterType;
};

struct Level {
    uint16_t sceneId;
    uint32_t mode;
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

    uintptr_t ResolveJumpPointer() {
        uintptr_t basePtr = 0;
        if (!SafeRead(g_addrPlayerBase, &basePtr)) return 0;

        uint32_t offsets[3] = { 0x10, 0x1E4, 0xFBC };
        uintptr_t addr = basePtr;
        for (int i = 0; i < 2; i++) {
            if (!SafeRead(addr + offsets[i], &addr) || addr == 0) return 0;
        }

        return addr + offsets[2];
    }

    // can still read garbage but wont crash on an access violation
    // writes the player struct to out if possible
    // returns true if all reads were successful, otherwise returns false.
    bool Read(Player& out) {
        out.playerBase = ResolveBase();
        if (out.playerBase == 0) return false;
        out.jumpFlag = ResolveJumpPointer();

        bool safe = true;
        safe &= SafeRead(out.playerBase + 0x30, &out.position.x);
        safe &= SafeRead(out.playerBase + 0x34, &out.position.y);
        safe &= SafeRead(out.playerBase + 0x38, &out.position.z);
        safe &= SafeRead(out.playerBase + 0x84, &out.gravity);

        return safe;
    }


}

namespace LevelMemory {
    bool Read(Level& out) {
        bool safe = true;
        safe &= SafeRead(ADDR_LEVEL_MODE, &out.mode);
        safe &= SafeRead(ADDR_SCENE_ID, &out.sceneId);

        return safe;
    }
}

// idek if this class makes sense
// since we have the memory namespace it would make more sense to have this be
// the container and make any writes from the given namespace.
// might rework later.
class GameState {
public:
    Player displayPlayer;
    Level displayLevel;

    void Refresh() {
        PlayerMemory::Read(displayPlayer);
        LevelMemory::Read(displayLevel);
    }

    bool WriteGravity(float newGrav) {
        if (displayPlayer.playerBase == 0) return false;
        if (!SafeWrite(displayPlayer.playerBase + 0x84, newGrav)) return false;
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

    bool WriteY(float newY) {
        if (displayPlayer.playerBase == false) return false;
        return SafeWrite(displayPlayer.playerBase + 0x34, newY);
    }

    // 0 and 1 are the only valid modes so This forces it to be that
    bool WriteLevelMode(uint32_t mode) {
        bool success = true;
        return SafeWrite(ADDR_LEVEL_MODE, mode);
    }

    bool WriteSceneId(uint16_t sceneId) {
        if (sceneId > 27) return false;
        return SafeWrite(ADDR_SCENE_ID, sceneId);
    }

    bool InfiniteJump() {
        uint8_t flag = 0x03;
        return SafeWrite(displayPlayer.jumpFlag, flag);
    }
};

const char* ResolveModeName(uint32_t mode) {
    if ((mode & 0x1) == 0x1) {
        return "Adventure";
    }
    else {
        return "Story";
    }
}

// toggles
volatile bool g_gravityDisabled = false;
volatile bool g_yFrozen = false;
volatile float g_heightMod = 1.0f;
volatile float g_ySaveFrozen = 0.0f;
volatile float g_savedY = 0.0f;
volatile float g_savedX = 0.0f;
volatile float g_savedZ = 0.0f;
volatile bool g_hasSavedY = false;
volatile bool g_gotoBoxesDirty = false;
volatile bool g_updatingGotoBoxes = false;
volatile bool g_hasInfiniteJumps = false;

GameState g_gameState;
static float g_originalGravity = 1.0f;
static bool  g_gravityCaptured = false;

static bool g_yCaptured = false;

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
    ID_APPLY_HEIGHT,
    ID_CHECK_FREEZE_Y,
    ID_BUTTON_LEVEL_MODE,
    ID_STATIC_LEVEL_MODE,
    ID_STATIC_SCENE_ID,
    ID_EDIT_GOTO_X,
    ID_EDIT_GOTO_Y,
    ID_EDIT_GOTO_Z,
    ID_BUTTON_GOTO,
    ID_BUTTON_INFINITE_JUMP
};

HWND g_hStaticPlayerY = NULL;
HWND g_hStaticCameraInfo = NULL;
HWND g_hCheckGravity = NULL;
HWND g_hCheckFreezeY = NULL;
HWND g_hStaticHeightMod = NULL;
HWND g_hStaticLevelMode = NULL;
HWND g_hStaticSceneId = NULL;
HWND g_hEditGotoX = NULL;
HWND g_hEditGotoY = NULL;
HWND g_hEditGotoZ = NULL;

void GUISavePosition() {
            g_gameState.Refresh(); // make sure we're saving current, not stale, position
            g_savedX = g_gameState.displayPlayer.position.x;
            g_savedY = g_gameState.displayPlayer.position.y;
            g_savedZ = g_gameState.displayPlayer.position.z;
            g_hasSavedY = true;
}

void GUILoadPosition() {
    if (g_hasSavedY) {
        Vector3 pos = { g_savedX, g_savedY, g_savedZ };
        g_gameState.WritePosition(pos);
    }
}
void RefreshDebugWindow()
{
    g_gameState.Refresh();

    if (g_gameState.displayPlayer.playerBase == 0) {
        SetWindowTextA(g_hStaticPlayerY, "Player Pos: (not found)");
        return;
    }

    char buf[128];
    sprintf_s(buf, "Player Pos X/Y/Z: %.2f %.2f %.2f  Grav=%.2f",
        g_gameState.displayPlayer.position.x,
        g_gameState.displayPlayer.position.y,
        g_gameState.displayPlayer.position.z,
        g_gameState.displayPlayer.gravity);

    SetWindowTextA(g_hStaticPlayerY, buf);

    sprintf_s(buf, "Game Mode: %s", ResolveModeName(g_gameState.displayLevel.mode));

    SetWindowTextA(g_hStaticLevelMode, buf);

    sprintf_s(buf, "Scene ID: %d", g_gameState.displayLevel.sceneId);
    SetWindowTextA(g_hStaticSceneId, buf);

    if (!g_gotoBoxesDirty) {
        g_updatingGotoBoxes = true; // suppress EN_CHANGE while we write

        char bx[32], by[32], bz[32];
        sprintf_s(bx, "%.2f", g_gameState.displayPlayer.position.x);
        sprintf_s(by, "%.2f", g_gameState.displayPlayer.position.y);
        sprintf_s(bz, "%.2f", g_gameState.displayPlayer.position.z);
        SetWindowTextA(g_hEditGotoX, bx);
        SetWindowTextA(g_hEditGotoY, by);
        SetWindowTextA(g_hEditGotoZ, bz);

        g_updatingGotoBoxes = false;
    }

    // reflect actual toggle state in case something external changed it
    SendMessageA(g_hCheckGravity, BM_SETCHECK, g_gravityDisabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_hCheckFreezeY, BM_SETCHECK, g_yFrozen ? BST_CHECKED : BST_UNCHECKED, 0);
}

LRESULT CALLBACK DebugWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hStaticPlayerY = CreateWindowA("STATIC", "Player Pos: --",
            WS_CHILD | WS_VISIBLE, 10, 10, 400, 20, hwnd, (HMENU)ID_STATIC_PLAYER_Y, g_hMyModule, NULL);

        g_hCheckGravity = CreateWindowA("BUTTON", "Disable Gravity",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 40, 150, 20, hwnd, (HMENU)ID_CHECK_GRAVITY, g_hMyModule, NULL);

        CreateWindowA("BUTTON", "Toggle Infinite Jumps",
            WS_CHILD | WS_VISIBLE, 140, 40, 200, 25, hwnd, (HMENU)ID_BUTTON_INFINITE_JUMP, g_hMyModule, NULL);

        g_hCheckFreezeY = CreateWindowA("BUTTON", "Freeze Y",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 70, 150, 20, hwnd, (HMENU)ID_CHECK_FREEZE_Y, g_hMyModule, NULL);

        CreateWindowA("BUTTON", "Save Position",
            WS_CHILD | WS_VISIBLE, 10, 100, 120, 25, hwnd, (HMENU)ID_BUTTON_SAVE_Y, g_hMyModule, NULL);

        CreateWindowA("BUTTON", "Load Position",
            WS_CHILD | WS_VISIBLE, 140, 100, 120, 25, hwnd, (HMENU)ID_BUTTON_LOAD_Y, g_hMyModule, NULL);

        CreateWindowA("BUTTON", "Toggle GameMode",
            WS_CHILD | WS_VISIBLE, 10, 130, 150, 25, hwnd, (HMENU)ID_BUTTON_LEVEL_MODE, g_hMyModule, NULL);

        g_hStaticLevelMode = CreateWindowA("STATIC", "Level Mode: --",
            WS_CHILD | WS_VISIBLE, 10, 160, 400, 20, hwnd, (HMENU)ID_STATIC_LEVEL_MODE, g_hMyModule, NULL);

        g_hStaticSceneId = CreateWindowA("STATIC", "Scene ID: --",
            WS_CHILD | WS_VISIBLE, 10, 180, 400, 20, hwnd, (HMENU)ID_STATIC_SCENE_ID, g_hMyModule, NULL);

        CreateWindowA("STATIC", "Goto X/Y/Z:", WS_CHILD | WS_VISIBLE,
            10, 200, 80, 20, hwnd, NULL, g_hMyModule, NULL);

        g_hEditGotoX = CreateWindowA("EDIT", "0.0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            90, 200, 80, 20, hwnd, (HMENU)ID_EDIT_GOTO_X, g_hMyModule, NULL);

        g_hEditGotoY = CreateWindowA("EDIT", "0.0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            175, 200, 80, 20, hwnd, (HMENU)ID_EDIT_GOTO_Y, g_hMyModule, NULL);

        g_hEditGotoZ = CreateWindowA("EDIT", "0.0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            260, 200, 80, 20, hwnd, (HMENU)ID_EDIT_GOTO_Z, g_hMyModule, NULL);

        CreateWindowA("BUTTON", "Goto", WS_CHILD | WS_VISIBLE,
            350, 200, 80, 25, hwnd, (HMENU)ID_BUTTON_GOTO, g_hMyModule, NULL);


        SetTimer(hwnd, 1, 100, NULL);
        return 0;
    }

    case WM_TIMER:
        RefreshDebugWindow();
        return 0;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int notify = HIWORD(wParam);

        if (id == ID_CHECK_GRAVITY && notify == BN_CLICKED) {
            bool checked = (SendMessageA(g_hCheckGravity, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_gravityDisabled = checked;
        }
        else if (id == ID_BUTTON_SAVE_Y && notify == BN_CLICKED) {
            GUISavePosition();
        }
        else if (id == ID_BUTTON_LOAD_Y && notify == BN_CLICKED) {
            GUILoadPosition();
        }
        else if (id == ID_CHECK_FREEZE_Y && notify == BN_CLICKED) {
            bool checked = (SendMessageA(g_hCheckFreezeY, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_yFrozen = checked;
        }
        else if (id == ID_BUTTON_LEVEL_MODE && notify == BN_CLICKED) {
            uint32_t mode = 1;
            if (g_gameState.displayLevel.mode & 0x1) {
                mode = 0;
            }
            g_gameState.WriteLevelMode(mode);
        }
        if ((id == ID_EDIT_GOTO_X || id == ID_EDIT_GOTO_Y || id == ID_EDIT_GOTO_Z) && notify == EN_CHANGE) {
            if (!g_updatingGotoBoxes) {
                g_gotoBoxesDirty = true;
            }
            return 0;
        }

        if (id == ID_BUTTON_GOTO && notify == BN_CLICKED) {
            char bufX[64], bufY[64], bufZ[64];
            GetWindowTextA(g_hEditGotoX, bufX, sizeof(bufX));
            GetWindowTextA(g_hEditGotoY, bufY, sizeof(bufY));
            GetWindowTextA(g_hEditGotoZ, bufZ, sizeof(bufZ));

            char* endX = nullptr, * endY = nullptr, * endZ = nullptr;
            float x = strtof(bufX, &endX);
            float y = strtof(bufY, &endY);
            float z = strtof(bufZ, &endZ);

            bool validX = (endX != bufX);
            bool validY = (endY != bufY);
            bool validZ = (endZ != bufZ);

            if (validX && validY && validZ) {
                Vector3 pos = { x, y, z };
                g_gameState.WritePosition(pos);
            }
            else {
                MessageBoxA(hwnd, "Enter valid numbers for X, Y, and Z.", "Invalid input", MB_OK | MB_ICONWARNING);
            }

            g_gotoBoxesDirty = false;
            return 0;
        }

        if (id == ID_BUTTON_INFINITE_JUMP && notify == BN_CLICKED) {
            g_hasInfiniteJumps = !g_hasInfiniteJumps;
        }

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
        if (g_gameState.displayPlayer.playerBase != 0) {
            float currentGrav = 0.0f;
            if (GetAsyncKeyState(VK_F1) & 0x8000) {
                GUISavePosition();
            }
            if (GetAsyncKeyState(VK_F2) & 0x8000) {
                GUILoadPosition();
            }
            if (SafeRead(g_gameState.displayPlayer.playerBase + 0x84, &currentGrav)) {

                if (g_gravityDisabled) {
                    if (!g_gravityCaptured) {
                        g_gravityCaptured = true;
                    }
                    g_gameState.WriteGravity(0.0f);
                }
                else if (g_gravityCaptured) {
                    g_gameState.WriteGravity(g_originalGravity);
                    g_gravityCaptured = false;
                }
            }

            if (g_yFrozen) {
                if (!g_yCaptured) {
                    g_yCaptured = true;
                    g_ySaveFrozen = g_gameState.displayPlayer.position.y;
                }
                g_gameState.WriteY(g_ySaveFrozen);
            }
            else {
                g_yCaptured = false;
            }
            if (GetAsyncKeyState(VK_F3) & 0x8000) {
                g_hasInfiniteJumps = !g_hasInfiniteJumps;
            }
            if (g_hasInfiniteJumps) {
                g_gameState.InfiniteJump();
            }
        }

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
