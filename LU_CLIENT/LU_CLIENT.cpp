#define _WINSOCKAPI_ 
#define CINTERFACE
#define	KEY_INCAR_TURRETLR			0
#define	KEY_INCAR_TURRETUD			1
#define	KEY_INCAR_RADIO				2
#define	KEY_INCAR_LOOKL				3
#define	KEY_INCAR_HANDBRAKE			4
#define	KEY_INCAR_LOOKR				5
#define	KEY_INCAR_TURNL				8
#define	KEY_INCAR_TURNR				9
#define	KEY_INCAR_CAMERA			11
#define	KEY_INCAR_BACKWARD			12
#define	KEY_INCAR_EXITVEHICLE		13
#define	KEY_INCAR_FORWARD			14
#define	KEY_INCAR_FIRE				15
#define	KEY_INCAR_HORN				16
#define	KEY_ONFOOT_TURNLR			0
#define	KEY_ONFOOT_ACTION			2
#define	KEY_ONFOOT_NEXTWEAPON		3
#define	KEY_ONFOOT_TARGET			4
#define	KEY_ONFOOT_PREVWEAPON		5
#define	KEY_ONFOOT_FORWARD			6
#define	KEY_ONFOOT_BACKWARD			7
#define	KEY_ONFOOT_LEFT				8
#define	KEY_ONFOOT_RIGHT			9
#define	KEY_ONFOOT_JUMP				12
#define	KEY_ONFOOT_ENTERVEHICLE		13
#define	KEY_ONFOOT_SPRINT			14
#define	KEY_ONFOOT_FIRE				15
#define	KEY_ONFOOT_CROUCH			16
#define	KEY_ONFOOT_LOOKBEHIND		17
#define FUNC_CPlayerPed__ProcessControl 0x4EFD90
#define FUNC_CAutomobile__ProcessControl 0x531470
#define _pad(x,y) BYTE x[y]
#define ADDR_KEYSTATES 0x6F0360
#define  _SL_STATIC
#define NUDE void _declspec(naked) 
#define MULT_X	0.00052083333f	// 1/1920
#define MULT_Y	0.00092592592f 	// 1/1080

#include "plugin.h"
#include "CMenuManager.h"
#include "extensions/ScriptCommands.h"
#include "CWorld.h"
#include "ePedPieceTypes.h"
#include "CCamera.h"
#include "CStreaming.h"
#include "CStreamingInfo.h"
#include "CGame.h"
#include "CHud.h"
#include "CTxdStore.h"
#include "imgui/imgui.h"
#include "imgui/directx8/imgui_impl_win32.h"
#include "imgui_impl_rw.h"
#include "raknet/MessageIdentifiers.h"
#include "raknet/rakpeerinterface.h"
#include "raknet/raknetstatistics.h"
#include "raknet/raknettypes.h"
#include "raknet/BitStream.h"
#include "raknet/RakSleep.h"
#include "raknet/PacketLogger.h"

#include <stdio.h>
#include <thread>
#include <detours.h>
#include <Psapi.h>
#include <io.h>
#include <stdlib.h>
#include <chrono> 
#include <tlhelp32.h>
#include <windowsx.h>
#include <map>  
#include <ole2.h>
#include <olectl.h>
#include <time.h>

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winmm.lib") 
#pragma comment(lib,"psapi.lib") 

extern unsigned char SCMData;

BOOL					bWindowedMode = false;
BOOL                    bChatEnabled = true;
BOOL                    bImguiHooked = false;


RakNet::RakPeerInterface* client;

extern LRESULT ImGui_ImplRW_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT __stdcall HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

WNDPROC OldWndProc = nullptr;
HWND tWindow = nullptr;

char nickname[64];
char ip[64];
char port[16];
char wTitle[64];
char wRand[9];

std::string window_title = "Liberty Unleashed: Reborn";

DWORD myPlayer = 0;
DWORD FHCore = 0;
DWORD dwStackFrame = 0;
DWORD dwCurPlayerActor = 0;
DWORD BarOldStateBlock = 0;
DWORD BarNewStateBlock = 0;

int64 last_sync_packet = 0;

bool IsConnectedToServer = false;

BYTE byteCurPlayer = 0;
BYTE* pbyteCurrentPlayer = (BYTE*)0x95CD61;
BYTE	     byteSavedCameraMode = 0;
BYTE* pbyteCameraMode = (BYTE*)0x5F03D8;

const FARPROC ProcessOneCommand = (FARPROC)0x439500;

CPed* (__cdecl* original_FindPlayerPed)(void);

WORD CPlayerPed_GetKeys();

char(__thiscall* original_CPed__InflictDamage)(CPed*, CEntity*, eWeaponType, float, ePedPieceTypes, UCHAR);

int(__thiscall* original_CPlayerPed__ProcessControl)(CPlayerPed*);
int(__thiscall* original_CAutomobile__ProcessControl)(CAutomobile*);

using namespace plugin;


bool   wndHookInited = false;
bool Initialized = false;
WNDPROC			orig_wndproc;
HWND			orig_wnd;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int init = 0;
int CLIENT_ID = -1;
int has_done = 0;
int m_gameStarted = 0;
int paused = 0;
int mouse = 0;
int startTime = 0;
int currentTime = 0;
int delay = 100;
int D3DInited = 0;

FILE* file;

void _stdcall SwitchContext(DWORD dwPedPtr, bool bPrePost);

typedef struct _GTA_CONTROLSET
{
    DWORD dwFrontPad;
    WORD wKeys1[19];
    DWORD dwFrontPad2;
    WORD wKeys2[19];
    _pad(__pad0, 0xC0);
} GTA_CONTROLSET;

typedef struct _CAMERA_AIM
{
    CVector LookFront;
    CVector Source;
    CVector SourceBeforeLookBehind;
    CVector LookUp;

} CAMERA_AIM;

CAMERA_AIM remotePlayerLookFrontX[50];
CAMERA_AIM		localPlayerLookFrontX;

typedef struct _MATRIX4X4 {
    CVector vLookRight;
    float  pad_r;
    CVector vLookUp;
    float  pad_u;
    CVector vLookAt;
    float  pad_a;
    CVector vPos;
    float  pad_p;
} MATRIX4X4, * PMATRIX4X4;

typedef struct _CAMERA_TYPE
{
    _pad(__pad0, 0x190); // 000-190
    BYTE byteDriveByLeft; // 190-191
    BYTE byteDriveByRight; // 191-192
    _pad(__pad1, 0x15E); // 192-2F0
    CAMERA_AIM aim;      // 2F0-320
    _pad(__pad2, 0x41C); // 320-73C
    CVector vecPosition;  // 73C-748
    CVector vecRotation;  // 748-754
    _pad(__pad3, 0x114); // 754-868
    BYTE byteInFreeMode; // 868-869
    _pad(__pad4, 0xEF);  // 869-958
} CAMERA_TYPE;

GTA_CONTROLSET* pGcsInternalKeys = (GTA_CONTROLSET*)ADDR_KEYSTATES;

struct
{
    GTA_CONTROLSET gcsControlState;
    BYTE byteDriveByLeft;
    BYTE byteDriveByRight;
} SavedKeys;


CAMERA_AIM caLocalPlayerAim;
CAMERA_AIM caRemotePlayerAim[50];
GTA_CONTROLSET gcsRemotePlayerKeys[50];

CPed* FindLocalPlayer() { return CWorld::Players[0].m_pPed; }

int iPlayerNumber = 1;



BYTE FindPlayerNumFromPedPtr(DWORD dwPedPtr)
{
    CPlayerPed* ped = (CPlayerPed*)dwPedPtr;

    for (BYTE i = 0; i < 50; i++)
    {
        if (CWorld::Players[i].m_pPed)
        {
            if (CWorld::Players[i].m_pPed == ped)
            {
                return i;
            }
        }
    }
    return 0;
}

std::string insert_newlines(const std::string& in, const size_t every_n)
{
    std::string out;
    out.reserve(in.size() + in.size() / every_n);
    for (std::string::size_type i = 0; i < in.size(); i++) {
        if (!(i % every_n) && i) {
            out.push_back('\n');
        }
        out.push_back(in[i]);
    }
    return out;
}

bool saveBitmap(LPCSTR filename, HBITMAP bmp, HPALETTE pal)
{
    bool result = false;
    PICTDESC pd;

    pd.cbSizeofstruct = sizeof(PICTDESC);
    pd.picType = PICTYPE_BITMAP;
    pd.bmp.hbitmap = bmp;
    pd.bmp.hpal = pal;

    LPPICTURE picture;
    HRESULT res = OleCreatePictureIndirect(&pd, IID_IPicture, false,
        reinterpret_cast<void**>(&picture));

    if (!SUCCEEDED(res))
        return false;

    LPSTREAM stream;
    res = CreateStreamOnHGlobal(0, true, &stream);

    if (!SUCCEEDED(res))
    {
        // IUnknown_Release_Proxy(picture);

        picture->lpVtbl->Release(picture);

        return false;
    }

    LONG bytes_streamed;
    res = picture->lpVtbl->SaveAsFile(picture, stream, true, &bytes_streamed);

    HANDLE file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (!SUCCEEDED(res) || !file)
    {
        stream->lpVtbl->Release(stream);
        picture->lpVtbl->Release(picture);
        return false;
    }

    HGLOBAL mem = 0;
    GetHGlobalFromStream(stream, &mem);
    LPVOID data = GlobalLock(mem);

    DWORD bytes_written;

    result = !!WriteFile(file, data, bytes_streamed, &bytes_written, 0);
    result &= (bytes_written == static_cast<DWORD>(bytes_streamed));

    GlobalUnlock(mem);
    CloseHandle(file);

    stream->lpVtbl->Release(stream);
    picture->lpVtbl->Release(picture);

    return result;
}

bool screenCapturePart(int x, int y, int w, int h, LPCSTR fname) {
    HDC hdcSource = GetDC(NULL);
    HDC hdcMemory = CreateCompatibleDC(hdcSource);

    int capX = GetDeviceCaps(hdcSource, HORZRES);
    int capY = GetDeviceCaps(hdcSource, VERTRES);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcSource, w, h);
    HBITMAP hBitmapOld = (HBITMAP)SelectObject(hdcMemory, hBitmap);

    BitBlt(hdcMemory, 0, 0, w, h, hdcSource, x, y, SRCCOPY);
    hBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmapOld);

    DeleteDC(hdcSource);
    DeleteDC(hdcMemory);

    HPALETTE hpal = NULL;
    if (saveBitmap(fname, hBitmap, hpal)) return true;
    return false;
}

float ImGui_ProgressBar(const char* optionalPrefixText, float value, const float minValue, const float maxValue, const char* format, const ImVec2& sizeOfBarWithoutTextInPixels, const ImVec4& colorLeft, const ImVec4& colorRight, const ImVec4& colorBorder) {
    if (value < minValue) value = minValue;
    else if (value > maxValue) value = maxValue;
    const float valueFraction = (maxValue == minValue) ? 1.0f : ((value - minValue) / (maxValue - minValue));
    const bool needsPercConversion = strstr(format, "%%") != NULL;

    ImVec2 size = sizeOfBarWithoutTextInPixels;
    if (size.x <= 0) size.x = ImGui::GetWindowWidth() * 0.25f;
    if (size.y <= 0) size.y = ImGui::GetTextLineHeightWithSpacing(); // or without

    const ImFontAtlas* fontAtlas = ImGui::GetIO().Fonts;

    if (optionalPrefixText && strlen(optionalPrefixText) > 0) {
        ImGui::AlignFirstTextHeightToWidgets();
        ImGui::Text(optionalPrefixText);
        ImGui::SameLine();
    }

    if (valueFraction > 0) {
        ImGui::Image(fontAtlas->TexID, ImVec2(size.x * valueFraction, size.y), fontAtlas->TexUvWhitePixel, fontAtlas->TexUvWhitePixel, colorLeft, colorBorder);
    }
    if (valueFraction < 1) {
        if (valueFraction > 0) ImGui::SameLine(0, 0);
        ImGui::Image(fontAtlas->TexID, ImVec2(size.x * (1.f - valueFraction), size.y), fontAtlas->TexUvWhitePixel, fontAtlas->TexUvWhitePixel, colorRight, colorBorder);
    }
    ImGui::SameLine();

    ImGui::Text(format, needsPercConversion ? (valueFraction * 100.f + 0.0001f) : value);
    return valueFraction;

}

struct ChatBox
{
    char                  InputBuf[128];
    ImVector<char*>       Items;
    ImVector<const char*> Commands;
    ImVector<char*>       History;
    int                   HistoryPos;
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;

    ChatBox()
    {
        ClearLog();
        memset(InputBuf, 0, sizeof(InputBuf));
        HistoryPos = -1;
        AutoScroll = true;
        ScrollToBottom = false;
        AddLog("Welcome to Liberty Unleashed Reborn");
    }
    ~ChatBox()
    {
        ClearLog();
        for (int i = 0; i < History.Size; i++)
            free(History[i]);
    }

    static int   Stricmp(const char* s1, const char* s2) { int d; while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; } return d; }
    static int   Strnicmp(const char* s1, const char* s2, int n) { int d = 0; while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; n--; } return d; }
    static char* Strdup(const char* s) { IM_ASSERT(s); size_t len = strlen(s) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
    static void  Strtrim(char* s) { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

    void    ClearLog()
    {
        for (int i = 0; i < Items.Size; i++)
            free(Items[i]);
        Items.clear();
    }

    void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
        buf[IM_ARRAYSIZE(buf) - 1] = 0;
        va_end(args);
        Items.push_back(Strdup(buf));
        if (Items.Size >= 35)
        {
            Items.erase(Items.begin());
        }
    }

    void    Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(520, 500), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Close Console"))
                *p_open = false;
            ImGui::EndPopup();
        }

        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::Selectable("Clear")) ClearLog();
            ImGui::EndPopup();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

        for (int i = 0; i < Items.Size; i++)
        {
            const char* item = Items[i];
            if (!Filter.PassFilter(item))
                continue;
            ImVec4 color;
            bool has_color = false;
            if (strstr(item, "[error]")) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
            else if (strncmp(item, "# ", 2) == 0) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
            if (has_color)
                ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item);
            if (has_color)
                ImGui::PopStyleColor();
        }

        if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
        ScrollToBottom = false;

        ImGui::PopStyleVar();
        ImGui::EndChild();

        bool reclaim_focus = false;
        ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;

        if (mouse == 1)
        {
            if (ImGui::InputText("  ", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
            {
                char* s = InputBuf;
                Strtrim(s);
                if (s[0])
                    ExecCommand(s);
                strcpy(s, "");
                reclaim_focus = true;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SetKeyboardFocusHere(-1);
        }

        ImGui::End();
    }

    void    ExecCommand(const char* command_line)
    {


        if (mouse == 1) { mouse = 0; }

        HistoryPos = -1;
        for (int i = History.Size - 1; i >= 0; i--)
            if (Stricmp(History[i], command_line) == 0)
            {
                free(History[i]);
                History.erase(History.begin() + i);
                break;
            }
        History.push_back(Strdup(command_line));

        if (Stricmp(command_line, "/q") == 0 || Stricmp(command_line, "/quit") == 0)
        {

            if (IsConnectedToServer) {
                client->Shutdown(250);
                RakNet::RakPeerInterface::DestroyInstance(client);
                Sleep(250);
                IsConnectedToServer = false;
            }
            exit(-1);
        }
        char msg[255];
        sprintf(msg, "MESS%s", command_line);

        if (IsConnectedToServer) client->Send(msg, strlen(msg) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

        ScrollToBottom = true;
    }
    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
    {
        ChatBox* console = (ChatBox*)data->UserData;
        return console->TextEditCallback(data);
    }

    int TextEditCallback(ImGuiInputTextCallbackData* data)
    {
        switch (data->EventFlag)
        {
        case ImGuiInputTextFlags_CallbackHistory:
        {
            const int prev_history_pos = HistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (HistoryPos == -1)
                    HistoryPos = History.Size - 1;
                else if (HistoryPos > 0)
                    HistoryPos--;
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (HistoryPos != -1)
                    if (++HistoryPos >= History.Size)
                        HistoryPos = -1;
            }
            if (prev_history_pos != HistoryPos)
            {
                const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, history_str);
            }
        }
        }
        return 0;
    }
};
static ChatBox p_ChatBox;

LRESULT __stdcall HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = false;
    if (1 == 1)
    {
        ImGui_ImplRW_WndProcHandler(hWnd, uMsg, wParam, lParam);
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return FALSE;
}

void InstallMethodHook(DWORD dwInstallAddress, DWORD dwHookFunction)
{
    DWORD dwVP, dwVP2;
    VirtualProtect((LPVOID)dwInstallAddress, 4, PAGE_EXECUTE_READWRITE, &dwVP);
    *(PDWORD)dwInstallAddress = (DWORD)dwHookFunction;
    VirtualProtect((LPVOID)dwInstallAddress, 4, dwVP, &dwVP2);
}

char __fastcall CPlayerPed__ProcessControl_Hook(CPlayerPed* This, DWORD _EDX)
{
    return original_CPlayerPed__ProcessControl(This);
}

bool Hook(void* toHook, void* ourFunct, int len)
{
    if (len < 5) {
        return false;
    }
    DWORD curProtection;
    VirtualProtect(toHook, len, PAGE_EXECUTE_READWRITE, &curProtection);
    memset(toHook, 0x90, len);
    DWORD relativeAddress = ((DWORD)ourFunct - (DWORD)toHook) - 5;
    *(BYTE*)toHook = 0xE9;
    *(DWORD*)((DWORD)toHook + 1) = relativeAddress;
    DWORD temp;
    VirtualProtect(toHook, len, curProtection, &temp);
    return true;
}

CPed* FindPlayerPed_Hook(void)
{
    return CWorld::Players[CWorld::PlayerInFocus].m_pPed;
}

DWORD        dwFunc = 0;
NUDE CPlayerPed_ProcessControl_Hook()
{
    _asm
    {
        mov dwCurPlayerActor, ecx
        pushad
    }

    SwitchContext(dwCurPlayerActor, true);

    dwFunc = FUNC_CPlayerPed__ProcessControl;
    _asm
    {
        popad
        call dwFunc
        pushad
    }

    SwitchContext(dwCurPlayerActor, false);

    _asm
    {
        popad
        ret
    }
}

void InstallHook(DWORD dwInstallAddress, DWORD dwHookFunction, DWORD dwHookStorage, BYTE* pbyteJmpCode, int iJmpCodeSize)
{
    DWORD dwVP, dwVP2;
    VirtualProtect((PVOID)dwHookStorage, 4, PAGE_EXECUTE_READWRITE, &dwVP);
    *(PDWORD)dwHookStorage = (DWORD)dwHookFunction;
    VirtualProtect((PVOID)dwHookStorage, 4, dwVP, &dwVP2);
    VirtualProtect((PVOID)dwInstallAddress, iJmpCodeSize, PAGE_EXECUTE_READWRITE, &dwVP);
    memcpy((PVOID)dwInstallAddress, pbyteJmpCode, iJmpCodeSize);
    VirtualProtect((PVOID)dwInstallAddress, iJmpCodeSize, dwVP, &dwVP2);
}

void GameKeyStatesInit()
{
    memset(&SavedKeys, 0, sizeof(SavedKeys));
    memset(&gcsRemotePlayerKeys, 0, sizeof(gcsRemotePlayerKeys));
}

void GameStoreLocalPlayerKeys()
{
    memcpy(&SavedKeys.gcsControlState, pGcsInternalKeys, sizeof(GTA_CONTROLSET));
}
void GameSetLocalPlayerKeys()
{
    memcpy(pGcsInternalKeys, &SavedKeys.gcsControlState, sizeof(GTA_CONTROLSET));
}

void GameStoreRemotePlayerKeys(int iPlayer, GTA_CONTROLSET* pGcsKeyStates)
{
    memcpy(&gcsRemotePlayerKeys[iPlayer], pGcsKeyStates, sizeof(GTA_CONTROLSET));
}
void GameSetRemotePlayerKeys(int iPlayer)
{
    memcpy(pGcsInternalKeys, &gcsRemotePlayerKeys[iPlayer], sizeof(GTA_CONTROLSET));
}
GTA_CONTROLSET* GameGetInternalKeys()
{
    return pGcsInternalKeys;
}

GTA_CONTROLSET* GameGetLocalPlayerKeys()
{
    return &SavedKeys.gcsControlState;
}

GTA_CONTROLSET* GameGetPlayerKeys(int iPlayer)
{
    return &gcsRemotePlayerKeys[iPlayer];
}

void GameResetPlayerKeys(int iPlayer)
{
    memset(&gcsRemotePlayerKeys[iPlayer], 0, sizeof(GTA_CONTROLSET));
}

void GameResetLocalKeys()
{
    memset(pGcsInternalKeys, 0, sizeof(GTA_CONTROLSET));
}

void _stdcall SwitchContext(DWORD dwPedPtr, bool bPrePost)
{
    if (dwPedPtr)
    {
        if (FHCore == 0)
        {
            myPlayer = dwPedPtr;
            FHCore = 1;
        }

        byteCurPlayer = FindPlayerNumFromPedPtr(dwPedPtr);

        if (dwPedPtr != (DWORD)CWorld::Players[0].m_pPed)
        {
            if (bPrePost)
            {
                GameStoreLocalPlayerKeys();
                GameSetRemotePlayerKeys(byteCurPlayer);
                *(CAMERA_AIM*)&TheCamera.m_asCams[TheCamera.m_nActiveCam].m_vecFront = remotePlayerLookFrontX[byteCurPlayer];

                *pbyteCurrentPlayer = byteCurPlayer;
            }
            else
            {
                *(CAMERA_AIM*)&TheCamera.m_asCams[TheCamera.m_nActiveCam].m_vecFront = localPlayerLookFrontX;
                *pbyteCurrentPlayer = 0;
                GameSetLocalPlayerKeys();
            }
        }
    }
}

WORD CPlayerPed_GetKeys()
{
    WORD wKeys = 0;
    GTA_CONTROLSET* pInternalKeys = GameGetInternalKeys();
    if (pInternalKeys->wKeys1[KEY_ONFOOT_FORWARD]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_ONFOOT_BACKWARD]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_ONFOOT_LEFT]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_ONFOOT_RIGHT]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_ONFOOT_JUMP]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_ONFOOT_SPRINT]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_ONFOOT_FIRE]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_ONFOOT_CROUCH]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_INCAR_TURRETUD] == 0x80) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_INCAR_TURRETUD] == 0xFF80) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_INCAR_LOOKL]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_INCAR_LOOKR]) wKeys |= 1;
    wKeys <<= 1;
    if (pInternalKeys->wKeys1[KEY_INCAR_HANDBRAKE]) wKeys |= 1;
    return wKeys;
}
void CPlayerPed_SetKeys(int iPlayerID, WORD wKeys)
{
    GTA_CONTROLSET* pPlayerKeys = GameGetPlayerKeys(iPlayerID);
    memcpy(pPlayerKeys->wKeys2, pPlayerKeys->wKeys1, (sizeof(WORD) * 19));
    pPlayerKeys->wKeys1[KEY_INCAR_HANDBRAKE] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 1
    pPlayerKeys->wKeys1[KEY_INCAR_LOOKR] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 2
    pPlayerKeys->wKeys1[KEY_INCAR_LOOKL] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 3
    pPlayerKeys->wKeys1[KEY_INCAR_TURRETUD] = (wKeys & 1) ? 0xFF80 : 0x00;
    wKeys >>= 1; // 4
    pPlayerKeys->wKeys1[KEY_INCAR_TURRETUD] = (wKeys & 1) ? 0x80 : 0x00;
    wKeys >>= 1; // 5
    pPlayerKeys->wKeys1[KEY_ONFOOT_CROUCH] = (wKeys & 0) ? 0xFF : 0x00; // stolen from vc, probably will require a change later
    wKeys >>= 1; // 6
    pPlayerKeys->wKeys1[KEY_ONFOOT_FIRE] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 7
    pPlayerKeys->wKeys1[KEY_ONFOOT_SPRINT] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 8
    pPlayerKeys->wKeys1[KEY_ONFOOT_JUMP] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 9
    pPlayerKeys->wKeys1[KEY_ONFOOT_RIGHT] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 10
    pPlayerKeys->wKeys1[KEY_ONFOOT_LEFT] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 11
    pPlayerKeys->wKeys1[KEY_ONFOOT_BACKWARD] = (wKeys & 1) ? 0xFF : 0x00;
    wKeys >>= 1; // 12
    pPlayerKeys->wKeys1[KEY_ONFOOT_FORWARD] = (wKeys & 1) ? 0xFF : 0x00;
    GameStoreRemotePlayerKeys(iPlayerID, pPlayerKeys);
}

class Clients
{
public:

    int m_id;
    bool m_bExists;
    CPlayerPed* m_pPlayer;
    std::string m_username;
    int m_playerPedID = -1;
    int m_CurrentAction = 0;
    int m_PrevAction = 0;
    float m_fHealth = 0;
    float m_fArmour = 0;

    SHORT m_Keys;

    Clients(int id) : m_id(id) {}

    void CreatePlayer(float x, float y, float z)
    {
        if (!m_pPlayer)
        {
            iPlayerNumber++;
            CPlayerPed::SetupPlayerPed(iPlayerNumber);
            auto ped = CWorld::Players[iPlayerNumber].m_pPed;
            if (ped)
            {
                ped->m_nFlags.bBulletProof = true;
                ped->m_nFlags.bCollisionProof = true;
                ped->m_nFlags.bFireProof = true;
                ped->m_nFlags.bExplosionProof = true;
                ped->m_nFlags.bMeleeProof = true;
                ped->m_nPedStatus = 2;
            }
            m_playerPedID = iPlayerNumber;
            m_pPlayer = ped;
        }
    }
    void SetUsername(std::string username) { m_username = username; }

    int GetID() { return m_id; }
    std::string GetUsername() { return m_username; }
};
std::map<int, Clients*> client_map;

class Timer {
    bool clear = false;

public:
    template<typename Function>
    void setTimeout(Function function, int delay);

    template<typename Function>
    void setInterval(Function function, int interval);

    void stop();
};

void Timer::stop() {
    this->clear = true;
}
template<typename Function>
void Timer::setTimeout(Function function, int delay) {
    this->clear = false;
    std::thread t([=]() {

        if (this->clear) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        if (this->clear) return;
        function();
        });
    t.detach();
}
template<typename Function>
void Timer::setInterval(Function function, int interval) {
    this->clear = false;
    std::thread t([=]() {
        while (true) {
            if (this->clear) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            if (this->clear) return;
            function();
        }
        });
    t.detach();
}

std::string GetLUID()
{
    DWORD HddNumber = 0;
    if (GetVolumeInformation(NULL, NULL, NULL, &HddNumber, NULL, NULL, NULL, NULL))
    {
        std::stringstream sstream;
        sstream << std::hex << HddNumber;
        std::string result = sstream.str();
        return result;
    }
    else
        exit(-1);
}

LRESULT CALLBACK wnd_proc(HWND wnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
    ImGuiIO& io = ImGui::GetIO();
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;


    switch (umsg)
    {
    case WM_DESTROY:
    case WM_CLOSE:
        ExitProcess(-1);
        break;

    case WM_MOUSEMOVE:
        POINT ul, lr;
        RECT rect;
        GetClientRect(wnd, &rect);

        ul.x = rect.left;
        ul.y = rect.top;
        lr.x = rect.right;
        lr.y = rect.bottom;

        MapWindowPoints(wnd, nullptr, &ul, 1);
        MapWindowPoints(wnd, nullptr, &lr, 1);

        rect.left = ul.x;
        rect.top = ul.y;
        rect.right = lr.x;
        rect.bottom = lr.y;

        if (GetActiveWindow() == FindWindow(0, wTitle))
            ClipCursor(&rect);
        break;
    case WM_KEYUP:
        if (wparam < 256)

            io.KeysDown[wparam] = 0;

        break;
    case WM_MOUSEHOVER:
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (wparam == VK_F5) bChatEnabled = !bChatEnabled;
        if (wparam == VK_RETURN) io.KeysDown[ImGuiKey_Enter] = 1;
        if (wparam < 256)
            io.KeysDown[wparam] = 1;
    }
    }
    if (ImGui_ImplWin32_WndProcHandler(wnd, umsg, wparam, lparam) && mouse == 1) return 0;

    return CallWindowProc(orig_wndproc, wnd, umsg, wparam, lparam);
}

void RenderChatbox();

bool bCompare(const BYTE* pData, const BYTE* bMask, const char* szMask)
{
    for (; *szMask; ++szMask, ++pData, ++bMask)
        if (*szMask == 'x' && *pData != *bMask)   return 0;
    return (*szMask) == NULL;
}

MODULEINFO GetModuleInfo(char* szModule)
{
    MODULEINFO modinfo = { 0 };
    HMODULE hModule = GetModuleHandle(szModule);
    if (hModule == 0)
        return modinfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof(MODULEINFO));
    return modinfo;
}

DWORD FindPattern(DWORD dwAddress, DWORD dwLen, BYTE* bMask, char* szMask)
{
    for (DWORD i = 0; i < dwLen; i++)
        if (bCompare((BYTE*)(dwAddress + i), bMask, szMask))  return (DWORD)(dwAddress + i);
    return 0;
}

DWORD GetModuleSize(LPSTR lpModuleName, DWORD dwProcessId)
{
    MODULEENTRY32 lpModuleEntry = { 0 };

    HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwProcessId);

    if (!hSnapShot)
        return NULL;

    lpModuleEntry.dwSize = sizeof(lpModuleEntry);

    BOOL bModule = Module32First(hSnapShot, &lpModuleEntry);

    while (bModule)
    {
        if (!strcmp(lpModuleEntry.szModule, lpModuleName))
        {
            CloseHandle(hSnapShot);

            return (DWORD)lpModuleEntry.modBaseAddr;
        }

        bModule = Module32Next(hSnapShot, &lpModuleEntry);
    }

    CloseHandle(hSnapShot);

    return NULL;
}

char* TrampHook(char* src, char* dst, unsigned int len)
{
    if (len < 5) return 0;
    char* gateway = (char*)VirtualAlloc(0, len + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(gateway, src, len);
    uintptr_t gateJmpAddy = (uintptr_t)(src - gateway - 5);
    *(gateway + len) = (char)0xE9;
    *(uintptr_t*)(gateway + len + 1) = gateJmpAddy;
    if (Hook(src, dst, len))
    {
        return gateway;
    }
    else return nullptr;
}

static HWND window;

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
    DWORD wndProcId;
    GetWindowThreadProcessId(handle, &wndProcId);

    if (GetCurrentProcessId() != wndProcId)
        return TRUE; // skip to next window

    window = handle;
    return FALSE;
}

HWND GetProcessWindow()
{
    window = NULL;
    EnumWindows(EnumWindowsCallback, NULL);
    return window;
}

void CCamera_SetCamPositionForFixedMode(CVector const& vecFixedModeSource, CVector const& vecFixedModeUpOffSet) {
    plugin::CallMethod<0x46BA72, CCamera*, CVector const&, CVector const&>(&TheCamera, vecFixedModeSource, vecFixedModeUpOffSet);
}

void SetStringFromCommandLine(char* szCmdLine, char* szString)
{
    while (*szCmdLine == ' ')
    {
        szCmdLine++;
    }

    while (*szCmdLine && *szCmdLine != ' ' && *szCmdLine != '-' && *szCmdLine != '/')
    {
        *szString = *szCmdLine;
        szString++; szCmdLine++;
    }

    *szString = '\0';
}

int debug = 0;

BOOL IsWindowMode = TRUE;
WINDOWPLACEMENT wpc;
LONG HWNDStyle = 0;
LONG HWNDStyleEx = 0;

void FullScreenSwitch()
{
    HWND HWNDWindow = FindWindow(NULL, wTitle);

    SetWindowLong(HWNDWindow, GWL_STYLE, HWNDStyle);
    SetWindowLong(HWNDWindow, GWL_EXSTYLE, HWNDStyleEx);
    ShowWindow(HWNDWindow, SW_SHOWNORMAL);
    SetWindowPlacement(HWNDWindow, &wpc);

}

void InitSettings()
{
    char* szCmdLine = GetCommandLine();

    while (*szCmdLine)
    {
        if (*szCmdLine == '-' || *szCmdLine == '/')
        {
            szCmdLine++;
            if (*szCmdLine == 'h')
            {
                szCmdLine++;
                SetStringFromCommandLine(szCmdLine, ip);
            }
            if (*szCmdLine == 'p')
            {
                szCmdLine++;
                SetStringFromCommandLine(szCmdLine, port);
            }
            if (*szCmdLine == 'n')
            {
                szCmdLine++;
                SetStringFromCommandLine(szCmdLine, nickname);
                printf("%s", nickname);
            }
            if (*szCmdLine == 'd')
            {
                szCmdLine++;
                debug = 1;
            }
        }
        szCmdLine++;
    }
}

unsigned int getfilesize(const char* path)
{
    struct stat ss;
    stat(path, &ss);
    return (unsigned int)ss.st_size;
}

bool is_dll_loaded(LPCSTR moduleName)
{
    return GetModuleHandle(moduleName);
}

void IsPaused()
{
    if (*(INT*)0x95CD7C == 1) paused = 1;
    else paused = 0;
}

void RenderChatbox()
{
    if (m_gameStarted == 1)
    {
        ImGui_ImplRenderWare_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(512, 256));

        p_ChatBox.Draw("Chatbox", NULL);

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplRenderWare_RenderDrawData(ImGui::GetDrawData());
    }
}

void CreatePlaya()
{
    int player;
    Command<0x053>(0, 811.875, -939.93, 35.75, &player);
}

DWORD jmpAddy = 0x48C339;

__declspec(naked) void CreatePlayer()
{
    __asm
    {
        call CreatePlaya
        jmp jmpAddy
    }
}

unsigned char GetPacketIdentifier(RakNet::Packet* p)
{
    if (p == 0)
        return 255;

    if ((unsigned char)p->data[0] == ID_TIMESTAMP)
    {
        RakAssert(p->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
        return (unsigned char)p->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
    }
    else
        return (unsigned char)p->data[0];
}

void ProcessPacket(unsigned char* data)
{
    if (data[0] == 'F' && data[1] == 'P' && data[2] == 'S')
    {
        unsigned char* _fps = data + 3;
        int limit = atoi((const char*)_fps);
        *(INT*)0x8F4374 = limit;
    }
    if (data[0] == 'M' && data[1] == 'E' && data[2] == 'S' && data[3] == 'S')
    {
        unsigned char* _msg = data + 4;

        const char* chat_message = (const char*)(char*)_msg;

        if (strlen((char*)_msg) >= 50) {
            p_ChatBox.AddLog(insert_newlines((char*)_msg, 50).c_str());
        }
        else p_ChatBox.AddLog((char*)_msg);
    }
}

void onFunctionPacket(RakNet::Packet* p)
{
    RakNet::RakString rs;
    RakNet::BitStream bsIn(p->data, p->length, false);
    bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
    bsIn.Read(rs);
    char data[16];
    sprintf(data, "%s", rs.C_String());
    char* pch;
    pch = strtok(data, " ");
    int function_id = atoi(pch);

    if (function_id == 0)
    {
        pch = strtok(NULL, " ");
        float x = atof(pch);
        pch = strtok(NULL, " ");
        float y = atof(pch);
        pch = strtok(NULL, " ");
        float z = atof(pch);

        if (FindPlayerPed()) FindPlayerPed()->SetPosition(x, y, z);
    }
}

void onGiveWeaponPacket(RakNet::Packet* p)
{
    RakNet::RakString rs;
    RakNet::BitStream bsIn(p->data, p->length, false);
    bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
    bsIn.Read(rs);
    char data[16];
    sprintf(data, "%s", rs.C_String());
    char* pch;
    pch = strtok(data, " ");
    int wep = atoi(pch);
    pch = strtok(NULL, " ");
    int ammo = atoi(pch);
    
    if (FindPlayerPed()) FindPlayerPed()->GiveWeapon((eWeaponType)wep, ammo);
}

void SendOnFootSync()
{
    if (FindPlayerPed() && IsConnectedToServer == 1 && !FindPlayerPed()->m_pVehicle)
    {
        RakNet::BitStream bsOut;
        bsOut.Write((RakNet::MessageID)ID_LUMSG2);
        char package[255];
        sprintf(package, "%f %f %f %f %f %f %i", FindPlayerPed()->GetPosition().x, FindPlayerPed()->GetPosition().y, FindPlayerPed()->GetPosition().z, FindPlayerPed()->GetHeading(), FindPlayerPed()->m_fHealth, FindPlayerPed()->m_fArmour, FindPlayerPed()->m_nWepSlot);
        bsOut.Write(package);
        client->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
    }
}

DWORD WINAPI LUThread(HMODULE hMod)
{
    RakNet::RakNetStatistics* rss;
    client = RakNet::RakPeerInterface::GetInstance();
    RakNet::Packet* p;
    unsigned char packetIdentifier;
    RakNet::SystemAddress clientID = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
    client->AllowConnectionResponseIPMigration(false);
    RakNet::SocketDescriptor socketDescriptor(static_cast<unsigned short>(rand() % 65534), 0);
    
    client->Startup(8, &socketDescriptor, 1);
    client->SetOccasionalPing(true);
    RakNet::ConnectionAttemptResult car = client->Connect(ip, static_cast<unsigned short>(atoi(port)), 0, 0);
    RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
    for (;;)
    {
        Sleep(30);
        SendOnFootSync();
        for (p = client->Receive(); p; client->DeallocatePacket(p), p = client->Receive())
        {
            packetIdentifier = GetPacketIdentifier(p);
            switch (packetIdentifier)
            {
            case ID_DISCONNECTION_NOTIFICATION:
                IsConnectedToServer = false;
                p_ChatBox.AddLog("You were disconnected from the server");
                printf("ID_DISCONNECTION_NOTIFICATION\n");
                break;
            case ID_ALREADY_CONNECTED:
                p_ChatBox.AddLog("You are already connected to the server");
                printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", p->guid.g);
                break;
            case ID_INCOMPATIBLE_PROTOCOL_VERSION:
                printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
                break;
            case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
                printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
                break;
            case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
                printf("ID_REMOTE_CONNECTION_LOST\n");
                break;
            case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
                printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
                break;
            case ID_CONNECTION_BANNED: // Banned from this server
                IsConnectedToServer = false;
                p_ChatBox.AddLog("You are banned from this server");
                break;
            case ID_CONNECTION_ATTEMPT_FAILED:
                IsConnectedToServer = false;
                p_ChatBox.AddLog("You failed to connect to the server");
                break;
            case ID_NO_FREE_INCOMING_CONNECTIONS:
                IsConnectedToServer = false;
                p_ChatBox.AddLog("Could not connect because the server is full.");
                printf("ID_NO_FREE_INCOMING_CONNECTIONS\n");
                break;

            case ID_INVALID_PASSWORD:
                printf("ID_INVALID_PASSWORD\n");
                IsConnectedToServer = false;
                p_ChatBox.AddLog("You have entered an invalid password");
                break;

            case ID_CONNECTION_LOST:
                IsConnectedToServer = false;
                p_ChatBox.AddLog("You have lost connection to the server");
                printf("ID_CONNECTION_LOST\n");
                break;

            case ID_CONNECTION_REQUEST_ACCEPTED:
                IsConnectedToServer = true;
                Command<0x2EB>();
                Command<0x1B4>(0, true);
                p_ChatBox.AddLog("Connection successful. Loading server data");
                char Nick[64];
                sprintf(Nick, "NAME%s", nickname);
                client->Send(Nick, strlen(Nick) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
                char LUID[32];
                sprintf(LUID, "LUID%s", GetLUID().c_str());
                client->Send(LUID, strlen(LUID) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
                break;
            case ID_CONNECTED_PING:

                break;
            case ID_LUMSG1:
                onGiveWeaponPacket(p);
                break;
            case ID_LUMSG2:
                onFunctionPacket(p);
                break;
            case ID_UNCONNECTED_PING:
                printf("Ping from %s\n", p->systemAddress.ToString(true));
                break;
            default:
                printf("[PACKET] %s\n", p->data);
                ProcessPacket(p->data);
                break;
            }
        }
    }
    return 1;
}

void hex_string(char str[], int length)
{
    char hex_characters[] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
    int i;
    for (i = 0; i < length; i++)
    {
        str[i] = hex_characters[rand() % 16];
    }
    str[length] = 0;
}
void DisableCheats(char str[]) {
    __asm
    {
        nop
    }
}
class LU_CLIENT
{
public:
    LU_CLIENT()
    {
        srand(time(NULL));

        for (int i = 0; i <= 256; i++)
        {
            client_map[i] = new Clients(i);
        }

        InitSettings();

        patch::SetChar(0x61187C, 0x54); // Disable Savegames 1
        patch::SetChar(0x6118F4, 0x69); // Disable Savegames 2
        patch::SetInt(0x582C1B, 204265); // Skip Movies 1
        patch::SetInt(0x582A8B, 208145899); // Skip Movies 2
        patch::Nop(0x582C26, 5); // Skip Movies 3
        patch::Nop(0x4F39D6, 5); // Disable Peds 1
        patch::Nop(0x48C9F9, 5); // Disable Peds 2
        patch::Nop(0x4F3AB5, 5); // Disable Peds 3
        patch::Nop(0x48CA03, 5); // Prevent removing far away cars
        patch::Nop(0x48C9FE, 5); // Don't generate roadblocks
        patch::Nop(0x4D443D, 5); // Disable Train entry 1
        patch::Nop(0x4CB597, 5); // Disable Train entry 2
        patch::Nop(0x48C8FF, 5); // Disable Trains
        patch::Nop(0x4888A5, 5); // Enable Sync when paused 1
        patch::Nop(0x488880, 5); // Enable Sync when paused 2
        patch::Nop(0x592165, 5); // Disable Saves
        patch::Nop(0x487998, 5); // Disable Start New Game
        patch::SetInt(0x485267, 8815887); // Disable load game
        patch::Nop(0x453FC9, 5); // Disable CLEO
        patch::Nop(0x590DC0, 5); // Disable CLEO 2
        patch::Nop(0x439400, 5); // Disable CLEO 3
        patch::Nop(0x439440, 5); // Disable CLEO 4
        patch::Nop(0x48C26B, 5); // Don't init scripts
        patch::Nop(0x48C32F, 5); // Don't process
        patch::Nop(0x48C975, 5); // Disable Replays
        patch::Nop(0x40B58D, 5); // Disable Island Load screen
        patch::Nop(0x40B59B, 5); // Disable Island Load screen 2
        patch::Nop(0x582E6C, 5); // Disable Island Load screen 3
        patch::Nop(0x4882CA, 5); // Unblock resolution
        *(INT*)0x8F4374 = 9999; // Infinite FPS (speed up loading)
        Hook((void*)0x48C334, CreatePlayer, 5);
        patch::Nop(0x492492, 5); // Disable Cheats Start
        patch::Nop(0x4924AF, 5); //
        patch::Nop(0x4924CC, 5); //
        patch::Nop(0x4924E9, 5); //
        patch::Nop(0x492506, 5); //
        patch::Nop(0x492523, 5); //
        patch::Nop(0x492540, 5); //
        patch::Nop(0x49255D, 5); //
        patch::Nop(0x49257A, 5); //
        patch::Nop(0x492597, 5); //
        patch::Nop(0x4925B4, 5); //
        patch::Nop(0x4925D1, 5); //
        patch::Nop(0x4925EE, 5); //
        patch::Nop(0x49260B, 5); //
        patch::Nop(0x4926B9, 5); //
        patch::Nop(0x4926D6, 5); //
        patch::Nop(0x4926F3, 5); //
        patch::Nop(0x49260B, 5); // Disable Cheats End

        if (debug == 1)
        {
            AllocConsole();
            freopen("CONOUT$", "w", stdout);
        }

        p_ChatBox.AddLog("Connecting to %s:%s...", ip, port);

        Events::initRwEvent += []
        {
            hex_string(wRand, 8);
            sprintf(wTitle, "Liberty Unleashed: Reborn [%s]",wRand);
            SetWindowTextA(FindWindowA(NULL, "GTA3"), wTitle);
            srand(time(NULL));
            GameKeyStatesInit();
            InstallMethodHook(0x5FA308, (DWORD)CPlayerPed_ProcessControl_Hook);

            char loadsc4[9] = "mainsc1";

            if (fopen("txd\\lu.txd", "r") != NULL)
            {
                sprintf(loadsc4, "lu");
            }

            char missing[3] = "%s";

            memcpy((char*)0x600200, missing, sizeof(missing)); // fix missing text
            memcpy((char*)0x5F55E0, loadsc4, sizeof(loadsc4)); // custom load scr

            CGame::InitialiseOnceAfterRW();
            patch::SetInt(0x8F5AEE, 0);
            patch::SetInt(0x8F5838, 9);
            plugin::Call<0x48E7E0>();
        };

        Events::shutdownRwEvent += []
        {
            if (bImguiHooked) ImGui_ImplRenderWare_ShutDown();
        };

        Events::drawingEvent += []
        {
            if (bImguiHooked && GetActiveWindow() == FindWindowA(NULL, wTitle)) RenderChatbox();
        };

        Events::processScriptsEvent += []
        {
            if (IsDebuggerPresent()) exit(-1);
            if (KeyPressed(VK_ESCAPE)) paused = 1;
            if (KeyPressed('T')) { if (mouse == 0 && bChatEnabled) { mouse = 1; } }
            if (KeyPressed(VK_F12))
            {
                char filename[256];
                sprintf(filename, "screen%i.bmp", rand() % 1000);
                screenCapturePart(1, 1, RsGlobal.maximumWidth, RsGlobal.maximumHeight, filename);
                p_ChatBox.AddLog("Screenshot saved as %s", filename);
                Sleep(100);
            }

            if (FindPlayerPed())
            {
                FindPlayerPed()->m_nPedType = 1;
            }

            *(BYTE*)0x5F2E60 = 1; // Framelimiter Patch
            *(BYTE*)0x5F2E5C = 0; // Disable V-Sync

            if (m_gameStarted == 0)
            {
                *(INT*)0x8F4374 = 30;
                HWND wnd = FindWindow(0, wTitle);
                if (wnd && !bImguiHooked && GetActiveWindow() == wnd)
                {
                    ImGui::CreateContext();
                    ImGuiIO& io = ImGui::GetIO();
                    ImGui_ImplRenderWare_Init();
                    bImguiHooked = true;
                    if (orig_wndproc == NULL || wnd != orig_wnd)
                    {
                        orig_wndproc = (WNDPROC)(UINT_PTR)SetWindowLong(wnd, GWL_WNDPROC, (LONG)(UINT_PTR)wnd_proc);
                        orig_wnd = wnd;
                        ImmAssociateContext(wnd, 0);
                    }
                    RECT rect;
                    GetWindowRect(wnd, &rect);
                    wndHookInited = true;
                    Initialized = true;
                    bImguiHooked = true;
                }
                Command<0x3F7>(0);
                Command<0x15F>(750.0, 750.0, 250.0, 0.0, 0.1);
                Command<0x160>(685.25, 600.0, 230.0, 2);
                Command<0x1B4>(0, false);

                m_gameStarted = 1;
                IsConnectedToServer = false;

                CWorld::Players[0].m_bInfiniteSprint = true;

                CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)LUThread, NULL, 0, nullptr));
            }
        };
    }
} lU_CLIENT;