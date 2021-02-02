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
#define FUNC_CWeapon__DoBulletImpact 0x55F950
#define FUNC_CAutomobile__ProcessControl 0x531470
#define _pad(x,y) BYTE x[y]
#define ADDR_KEYSTATES 0x6F0360
#define  _SL_STATIC
#define NUDE void _declspec(naked) 



#include "plugin.h"
#include "CMenuManager.h"
#include "extensions/ScriptCommands.h"
#include "CWorld.h"
#include "ePedPieceTypes.h"
#include "CCamera.h"
#include "CHud.h"
#include "CTxdStore.h"
#include "imgui/imgui.h"
#include "imgui/directx8/imgui_impl_dx8.h"
#include "imgui/directx8/imgui_impl_win32.h"

#include <stdio.h>
#include <thread>
#include <enet/enet.h>
#include <io.h>
#include <stdlib.h>
#include <chrono>
#include <windowsx.h>
#include <map> 
#include <d3d8.h>

#pragma warning(disable: 4018)
#pragma warning(disable: 4244)
#pragma warning(disable: 4996)

#pragma comment(lib,"enet.lib")
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"d3d8.lib")
#pragma comment(lib,"winmm.lib") 


BOOL					bWindowedMode = false;

IDirect3DDevice8* pD3DDevice;

HRESULT   __stdcall nReset(LPDIRECT3DDEVICE8 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
typedef HRESULT(APIENTRY* Reset_t) (LPDIRECT3DDEVICE8 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
Reset_t     pReset;


HRESULT   __stdcall  nPresent(LPDIRECT3DDEVICE8 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
typedef HRESULT(APIENTRY* Present_t)(LPDIRECT3DDEVICE8 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
Present_t   pPresent;

extern LRESULT ImGui_ImplDX8_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT __stdcall HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

WNDPROC OldWndProc = nullptr;
HWND tWindow = nullptr;

ENetHost* client;
ENetAddress address; 
ENetEvent event;
ENetPeer* peer;
 
char nickname[64]; 
char ip[64]; 
char port[16];

DWORD myPlayer = 0;
DWORD FHCore = 0;
DWORD dwStackFrame = 0;
DWORD dwCurPlayerActor = 0;
DWORD BarOldStateBlock = 0;
DWORD BarNewStateBlock = 0;

D3DMATRIX matView;

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
int(__thiscall* original_CWeapon__DoBulletImpact)(CWeapon* This, CEntity*, CEntity*, CVector*, CVector*, CColPoint*, CVector2D);
 

void onD3DRender(LPDIRECT3DDEVICE8 pDevice);

using namespace plugin;

int init = 0;
int CLIENT_ID = -1;
int m_gameStarted = 0;
int paused = 0;
int mouse = 0;

FILE* file;

void _stdcall SwitchContext(DWORD dwPedPtr, bool bPrePost);
void InstallD3D8Hook();
void UninstallD3D8Hook();

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

class CPlayer
{
public:

    int m_playerID = 0;
    int m_playerPedID = -1;
    int m_CurrentAction = 0;
    int m_PrevAction = 0;

    time_t m_LastSyncTime = 0;

    bool m_bInVehicle = false;

    int m_VehicleID = -1;

    CVehicle* m_pVehicle;

    float m_fHealth = 0;
    float m_fArmour = 0;
    std::string m_playerName;

    SHORT m_Keys;

    CPlayerPed* m_pPed = nullptr;

    CPlayer(int ID)
    {
        m_playerID = ID;
    }

    ~CPlayer()
    {
        if (m_pPed) CWorld::Remove(m_pPed);
        m_playerID = -1;
        m_pPed = nullptr;
        m_playerName = "";
    }

    void CreatePlayer(float x, float y, float z)
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
            ped->Teleport(FindPlayerPed()->GetPosition());
        }
        m_playerPedID = iPlayerNumber;
        m_pPed = ped;
    }
};
CPlayer* Players[128];

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

int __fastcall CWeapon__DoBulletImpact_Hook(CWeapon* This, DWORD _EDX, CEntity* source, CEntity* target, CVector* start, CVector* end, CColPoint* colpoint, CVector2D ahead)
{
    return original_CWeapon__DoBulletImpact(This, source, target, start, end, colpoint, ahead);
}

void Initialize()
{
    tWindow = FindWindow(0, "GTA3");
    if (tWindow) {
        OldWndProc = (WNDPROC)SetWindowLongPtr(tWindow, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
    }

}

void Set()
{
    SetWindowLongPtr(tWindow, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
}

void Restore()
{
    SetWindowLongPtr(tWindow, GWLP_WNDPROC, (LONG_PTR)OldWndProc);
}
void windowThread()
{
    while (FindWindow(NULL, "GTA3") == 0) { //if couldnt find the window? 
        Sleep(100);
    }
    while (reinterpret_cast<IDirect3DDevice8*>(RwD3D8GetCurrentD3DDevice()) == NULL) {  //if couldnt find the device.
        Sleep(100);
    }

    tWindow = FindWindow(NULL, "GTA3");
}

void SendPacket(ENetPeer* peer, const char* data)
{
    ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(peer, 0, packet);
}

struct ChatBox
{
    char                  InputBuf[256];
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

        Commands.push_back("HELP");
        Commands.push_back("HISTORY");
        Commands.push_back("CLEAR");
        Commands.push_back("CLASSIFY");

        AutoScroll = true;
        ScrollToBottom = false;
        AddLog("Welcome to Liberty Unleashed 0.1");
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
    }

    void    Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoBackground))
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
        SendPacket(peer, Format("9| %s",command_line).c_str());

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
            IsConnectedToServer = false;
            enet_peer_disconnect(peer, 0);
            Sleep(1000);
            exit(-1);
        }

        ScrollToBottom = true;
    }
    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
    {
        ChatBox * console = (ChatBox*)data->UserData;
        return console->TextEditCallback(data);
    }

    int TextEditCallback(ImGuiInputTextCallbackData* data)
    {
        switch (data->EventFlag)
        {
        case ImGuiInputTextFlags_CallbackCompletion:
        {
            const char* word_end = data->Buf + data->CursorPos;
            const char* word_start = word_end;
            while (word_start > data->Buf)
            {
                const char c = word_start[-1];
                if (c == ' ' || c == '\t' || c == ',' || c == ';')
                    break;
                word_start--;
            }
            ImVector<const char*> candidates;
            for (int i = 0; i < Commands.Size; i++)
                if (Strnicmp(Commands[i], word_start, (int)(word_end - word_start)) == 0)
                    candidates.push_back(Commands[i]);

            if (candidates.Size == 0)
            {
                AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
            }
            else if (candidates.Size == 1)
            {
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0]);
                data->InsertChars(data->CursorPos, " ");
            }
            else
            {
                int match_len = (int)(word_end - word_start);
                for (;;)
                {
                    int c = 0;
                    bool all_candidates_matches = true;
                    for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                        if (i == 0)
                            c = toupper(candidates[i][match_len]);
                        else if (c == 0 || c != toupper(candidates[i][match_len]))
                            all_candidates_matches = false;
                    if (!all_candidates_matches)
                        break;
                    match_len++;
                }

                if (match_len > 0)
                {
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                    data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
                }
                AddLog("Possible matches:\n");
                for (int i = 0; i < candidates.Size; i++)
                    AddLog("- %s\n", candidates[i]);
            }

            break;
        }
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

    io.MouseDrawCursor = false; //to view mouse
    if (1==1)
    {
        //ImGUI implementation windproc
        ImGui_ImplDX8_WndProcHandler(hWnd, uMsg, wParam, lParam);
        //Calls and return the default window procedure.
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return FALSE;
}

bool CRC32(const char* filename, char* checksumhex)
{
    unsigned long checksum;
   // if (CFastCRC32::Calculate(&checksum, filename) != 0)
     //   return false;
   // SL_FCRC_ConvertToHex32(checksumhex, checksum, 0);
    return true;
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

#include <detours.h>

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

void CRender_DrawProgressBar(CRect size, int value, CRGBA color, float max)
{
    CRGBA darkbg = color;
    if (color.r < 65)darkbg.r = 0;
    else darkbg.r = color.r - 65;
    if (color.g < 65)darkbg.g = 0;
    else darkbg.g = color.g - 65;
    if (color.b < 65)darkbg.b = 0;
    else darkbg.b = color.b - 65;
    CSprite2d::DrawRect(CRect(size.left, size.top, size.right, size.bottom), CRGBA(0, 0, 0, 255));
    CSprite2d::DrawRect(CRect(size.left + 1, size.top + 1, size.right - 1, size.bottom - 1), darkbg);
    int len = (size.right - size.left);
    CSprite2d::DrawRect(CRect(size.left + 1, size.top + 1, size.left + ceil((len / max) * value) - 1, size.bottom - 1), color);
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

void InstallHook(DWORD dwInstallAddress, DWORD dwHookFunction, DWORD dwHookStorage,BYTE* pbyteJmpCode, int iJmpCodeSize)
{
    DWORD dwVP, dwVP2;
    VirtualProtect((PVOID)dwHookStorage, 4, PAGE_EXECUTE_READWRITE, &dwVP);
    *(PDWORD)dwHookStorage = (DWORD)dwHookFunction;
    VirtualProtect((PVOID)dwHookStorage, 4, dwVP, &dwVP2);
    VirtualProtect((PVOID)dwInstallAddress, iJmpCodeSize, PAGE_EXECUTE_READWRITE, &dwVP);
    memcpy((PVOID)dwInstallAddress, pbyteJmpCode, iJmpCodeSize);
    VirtualProtect((PVOID)dwInstallAddress, iJmpCodeSize, dwVP, &dwVP2);
}

void ErasePEHeader(HINSTANCE hModule)
{
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery((LPCVOID)hModule, &mbi, sizeof(mbi));
    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect);
    ZeroMemory((PVOID)hModule, 4096);
    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, NULL);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)mbi.BaseAddress, mbi.RegionSize);
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
    pPlayerKeys->wKeys1[KEY_ONFOOT_CROUCH] = (wKeys & 0) ? 0xFF : 0x00;
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
private:
    int m_id;
    std::string m_username;

public:
    Clients(int id) : m_id(id) {}

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

void DecryptPacket(char* data, int client_id)
{
    std::string str(data);
    std::string buf;
    std::stringstream ss(str);

    std::vector<std::string> tokens;

    while (ss >> buf)
    {
        tokens.push_back(buf);
    }

    if (tokens[0] == "8|") // PACKET TYPE: PLAYER MOVEMENT SYNC
    {

    }
    if (data[0] == '9' && data[1] == '|')
    {
        char* token = strtok(data, "|");
        std::string packet;
        while (token != NULL)
        {
            packet = token;
            token = strtok(NULL, "|");
        } p_ChatBox.AddLog("%s", packet.c_str());
    }
}
void ParseData(int plrid, char* data)
{
    int data_type;
    int id;
    sscanf(data, "%d|%d", &data_type, &id);
    if ( data_type != 8 ) printf("Received Packet with data type %i\n", data_type);
    switch (data_type)
    {
    case 1: 
        if (id != CLIENT_ID)
        {
            char msg[80];
            sscanf(data, "%*d|%*d|%[^|]", &msg);
        }
        break;
    case 2: 
        if (id != CLIENT_ID)
        {
            
            char username[80];
            sscanf(data, "%*d|%*d|%[^|]", &username);
            p_ChatBox.AddLog("%s has joined the server", username);
            client_map[id] = new Clients(id);
            client_map[id]->SetUsername(username);
        }
        break;
    case 3: 
        CLIENT_ID = id;
        break;
    case 4:

        p_ChatBox.AddLog("%s has left the sercer", client_map[id]->GetUsername().c_str());
  

        break;
    case 9:
        DecryptPacket(data, id);
        break;
    
    }
}

DWORD WINAPI SyncThread(HMODULE hModule)
{
    while (1 != 2)
    {
        if (IsConnectedToServer)
        {
            if (FindPlayerPed())
            {
                if (FindPlayerPed()->m_fHealth != 0)
                {
                    char sync_packet[80];
                    sprintf(sync_packet, "8| %i %f %f %f",(int)FindPlayerPed()->m_fHealth, FindPlayerPed()->GetPosition().x, FindPlayerPed()->GetPosition().y, FindPlayerPed()->GetPosition().z);
                    SendPacket(peer, sync_packet);
                    Sleep(10);
                }
            }
        }
    }
    return 0;
}

wchar_t* stws(std::string my_shit)
{
    std::wstring widestr;
    for (int i = 0; i < my_shit.length(); ++i)
        widestr += wchar_t(my_shit[i]);
    const wchar_t* your_result = widestr.c_str();
    return (wchar_t*)your_result;
}

DWORD WINAPI LUThread(HMODULE hModule)
{
    patch::SetInt(0x95CD7C, 0);
    if (paused == 1)
    {
        plugin::Call<0x488920>();
        paused = 0;
    }
    if (enet_initialize() != 0)
    {
        MessageBox(NULL, "An error has occured while initializing ENet!", "Fatal Error", NULL);
        SetForegroundWindow(HWND_DESKTOP);
        ExitProcess(1);
    }
    client = enet_host_create(NULL, 1, 1, 0, 0);
    if (client == NULL)
    {
        MessageBox(NULL, "An error occurred while trying to create an ENet client host!", "Fatal Error", NULL);
        SetForegroundWindow(HWND_DESKTOP);
        ExitProcess(1);
    }

    int xport;
    sscanf(port, "%d", &xport);
    enet_address_set_host(&address, ip);
    address.port = xport;

    peer = enet_host_connect(client, &address, 1, 0);
    if (peer == NULL)
    {
        MessageBox(NULL, "No available peers for initiating an ENet connection!", "Fatal Error", NULL);
        SetForegroundWindow(HWND_DESKTOP);
        ExitProcess(1);
    }

    printf("Welcome to Liberty Unleashed 0.1\n");

    p_ChatBox.AddLog("Connecting to server %s:%s...",ip,port);

    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
    {
        p_ChatBox.AddLog("Connection Successful. Loading game...");
        IsConnectedToServer = true;
        char str_data[80] = "2|";
        strcat(str_data, nickname);
        SendPacket(peer, str_data);
    }
    else
    {
        
        p_ChatBox.AddLog("You failed to connect to the server.");
        enet_peer_reset(peer);
        IsConnectedToServer = false;
    }

    while (enet_host_service(client, &event, 2000) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
            last_sync_packet = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();


           /* printf("A packet of length %u containing %s was received from %x:%u on channel %u.\n",
                event.packet->dataLength,
                event.packet->data,
                event.peer->address.host,
                event.peer->address.port,
                event.channelID);*/
            char data[256];
            sprintf(data, "%s", event.packet->data);
            ParseData(static_cast<Clients*>(event.peer->data)->GetID(), data);

            enet_packet_destroy(event.packet);
            break;
        }
    }

    while (1 != 2)
    {
       
    }
    return 0;
}

#include "CCamera.h"

int has_done = 0;

HRESULT __stdcall nReset(LPDIRECT3DDEVICE8 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    _asm PUSHAD;

    printf("Hook Reset\n");

    _asm POPAD;

    return pReset(pDevice, pPresentationParameters);
}

bool   wndHookInited = false;
bool Initialized = false;
WNDPROC			orig_wndproc;
HWND			orig_wnd;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK wnd_proc(HWND wnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
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

        if (GetActiveWindow() == FindWindow(0,"GTA3"))
            ClipCursor(&rect);
        break;
    case WM_MOUSEHOVER:
        break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {}
    }
    if (ImGui_ImplDX8_WndProcHandler(wnd, umsg, wparam, lparam)&&mouse==1) return 0;

    return CallWindowProc(orig_wndproc, wnd, umsg, wparam, lparam);
}
void RenderChatbox();

HRESULT __stdcall nPresent(LPDIRECT3DDEVICE8 pDevice, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
    _asm PUSHAD;


    if (!wndHookInited)
    {
        printf("Initing windows hook for imgui\n");
        ImGui::CreateContext();
        HWND  wnd = FindWindow(0, "GTA3");
        if (wnd)
        {
            printf("Window found & context created\n");
            if (orig_wndproc == NULL || wnd != orig_wnd)
            {
                orig_wndproc = (WNDPROC)(UINT_PTR)SetWindowLong(wnd, GWL_WNDPROC, (LONG)(UINT_PTR)wnd_proc);
                orig_wnd = wnd;
                ImmAssociateContext(wnd, 0);
            }
            RECT rect;

            printf("associated\n");

            GetWindowRect(wnd, &rect);
            wndHookInited = true;

            ImGui_ImplDX8_Init(orig_wnd, pDevice);
            printf("IMGUI HAS BEEN IMPLEMENTED\n");

            Initialized = true;
        }
        wndHookInited = true;
    }
    else
    {
        RenderChatbox();
    }

    _asm POPAD;

    return  pPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

DWORD_PTR* _FindDevice(DWORD Base, DWORD Len)
{
    unsigned long i = 0, n = 0;

    for (i = 0; i < Len; i++)
    {
        if (*(BYTE*)(Base + i + 0x00) == 0xC7)n++;
        if (*(BYTE*)(Base + i + 0x01) == 0x06)n++;
        if (*(BYTE*)(Base + i + 0x06) == 0x89)n++;
        if (*(BYTE*)(Base + i + 0x07) == 0x86)n++;
        if (*(BYTE*)(Base + i + 0x0C) == 0x89)n++;
        if (*(BYTE*)(Base + i + 0x0D) == 0x86)n++;
        if (n == 6) return (DWORD_PTR*)(Base + i + 2); n = 0;
    }
    return(0);
}

int __fastcall StaticHook(void)
{

    HMODULE hD3D8Dll;
    do
    {
        printf(" Loading d3d8.dll->\n");
        hD3D8Dll = GetModuleHandle("d3d8.dll");
        Sleep(20);
        printf("OK!\n");
    } while (!hD3D8Dll);

    printf("Enable Device [GTA3]");
    DWORD_PTR* VtablePtr = _FindDevice((DWORD)hD3D8Dll, 0x128000);
    printf("OK!\n");

    if (VtablePtr == NULL)
    {
        MessageBox(NULL, "Device Not Found !! Please try again", 0, MB_ICONSTOP);
        ExitProcess(TRUE);
    }
    DWORD_PTR* VTable = 0;
    *(DWORD_PTR*)&VTable = *(DWORD_PTR*)VtablePtr;
    printf("OK!\n");
    printf("%s - Hooking Class->");
   
    pPresent = (Present_t)DetourFunction((PBYTE)VTable[15], (LPBYTE)nPresent);
    pReset = (Reset_t)DetourFunction((PBYTE)VTable[14], (LPBYTE)nReset);
    printf("OK!\n");


    return(0);
}

void CCamera_SetCamPositionForFixedMode(CVector const& vecFixedModeSource, CVector const& vecFixedModeUpOffSet) {
    plugin::CallMethod<0x46BA72, CCamera*, CVector const&, CVector const&>(&TheCamera, vecFixedModeSource, vecFixedModeUpOffSet);
}

void LostConnection()
{
    IsConnectedToServer = false;
    enet_peer_disconnect(peer, 0);
    p_ChatBox.AddLog("You have lost connection to the server");
    
}

void ProcessSync()
{
    unsigned __int64 now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
    int64 test = last_sync_packet - now;

    if (test <= -10000)
    {
        if (IsConnectedToServer)
        {
            LostConnection();
        }
    }
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

void InitSettings()
{
    char* szCmdLine = GetCommandLine();

    while (*szCmdLine)
    {
        if (*szCmdLine == '-' || *szCmdLine == '/')
        {
            szCmdLine++;
            if (*szCmdLine=='h')
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

const char* my_fun(const char* par)
{
    return par;
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
void InitD3DStuff() { printf("Initd3dstuff"); }
void TheSceneEnd() { printf("SceneEnd"); }

void SendKeyEvent(DWORD key, bool state) // state: true == down, false == up.
{

}

BOOL HandleCharacterInput(DWORD dwChar)
{
    return FALSE;
}

WNDPROC hOldProc;

LRESULT APIENTRY NewWndProc(HWND, UINT, WPARAM, LPARAM);

BOOL HandleKeyPress(DWORD vKey)
{
    return FALSE;
}


LRESULT APIENTRY NewWndProc(HWND hwnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam)
{
  //  if (pGUI) pGUI->MsgProc(hwnd, uMsg, wParam, lParam);
    switch (uMsg) {
    case WM_KEYUP:
    {
        SendKeyEvent((DWORD)wParam, false);
        if (HandleKeyPress((DWORD)wParam)) { // 'I' handled it.
            return 0;
        }
    }
    break;
    case WM_KEYDOWN:
    {
        SendKeyEvent((DWORD)wParam, true);
    }
    break;
    case WM_CHAR:
        if (HandleCharacterInput((DWORD)wParam)) { // 'I' handled it.
            return 0;
        }
        break;
    }
    return CallWindowProc(hOldProc, hwnd, uMsg, wParam, lParam);
}

BOOL SubclassGameWindow(HWND hWnd)
{
    printf("Ha wu en de");
    if (hWnd) {
        hOldProc = SubclassWindow(hWnd, NewWndProc);
        return TRUE;
    }
    printf("CRasher");
    return FALSE;
}

bool Anticheat()
{
    if (getfilesize("data\\lu.scm") != 80919)
    {
        return false;
    }
    return true;
}
int D3DInited = 0;

LPDIRECT3DDEVICE8 p_Device;

void onD3DRender(LPDIRECT3DDEVICE8 pDevice)
{
    pDevice = p_Device;
}

void IsPaused()
{
    if (*(INT*)0x95CD7C == 1) paused = 1;
    else paused = 0;
}

CSprite2d mySprite;

void RenderChatbox()
{
    if (m_gameStarted == 1)
    {
        ImGui_ImplDX8_NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(512, 256));

        auto io = ImGui::GetIO();

        if (mouse == 1)
        {

            io.MouseDrawCursor = true;
        }
        else { io.MouseDrawCursor = false; }
        
        p_ChatBox.Draw("Chatbox", NULL);

        if (KeyPressed(VK_TAB) && GetActiveWindow() == FindWindow(0,"GTA3"))
        {
            ImGui::Begin("Scoreboard");
            ImGui_ProgressBar(" ", FindPlayerPed()->m_fHealth, 0, 100, "  ",ImVec2(50,5),ImVec4(0.0f,1.0f,0.0f,1.0f), ImVec4(1.0f, 0.0f, 0.0f, 1.0f), ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::End();
        }

        ImGui::EndFrame();
        ImGui::Render();
    }
}


class LU_CLIENT
{
public:
    LU_CLIENT()
    {

        for (int i = 0; i <= 127; i++)
        {
            Players[i] = new CPlayer(i);
        }

        InitSettings();

        patch::SetInt(0x582A8B, 208145899); // Skip Movies
        patch::Nop(0x582C26, 5); // Skip Movies
        patch::SetChar(0x61187C, 0x54); // Disable Savegames
        patch::Nop(0x4F39D6, 5); // Disable Peds 1
        patch::Nop(0x48C9F9, 5); // Disable Peds 2
        patch::Nop(0x4F3AB5, 5); // Disable Peds 3
        patch::Nop(0x48CA03, 5); // Prevent removing far away cars
        patch::Nop(0x48C9FE, 5); // Don't generate roadblocks
        patch::Nop(0x4D443D, 5); // Disable Train entry
        patch::Nop(0x4CB597, 5); // Disable Train entry 2
        patch::Nop(0x48C8FF, 5); // Disable Trains
        patch::Nop(0x4888A5, 5); // Disable Pause 1
        patch::Nop(0x485168, 5); // Disable Pause 2


        if (debug == 1)
        {
            AllocConsole();
            freopen("CONOUT$", "w", stdout);
        }

        Events::initRwEvent += []
        {

            int txd = CTxdStore::AddTxdSlot("menu");
            CTxdStore::LoadTxd(txd, "models/menu.txd");
            CTxdStore::AddRef(txd);
            CTxdStore::PushCurrentTxd();
            CTxdStore::SetCurrentTxd(txd);

            char mousetxd[] = "mouse";
            char moualp[] = "mouse_alpha";
            mySprite.SetTexture(mousetxd,moualp);

            CTxdStore::PopCurrentTxd();

           

            srand(time(NULL));

            StaticHook();

            GameKeyStatesInit();
            InstallMethodHook(0x5FA308, (DWORD)CPlayerPed_ProcessControl_Hook);
            patch::Nop(0x48C975, 5); // Disable Replays

            char loadsc4[9] = "mainsc1";

            if (fopen("txd\\lu.txd", "r") != NULL)
            {
                sprintf(loadsc4, "lu");
            }

            char missing[3] = "%s";
            char scm[7] = "lu.scm";
            char scm_data[15] = "data\\lu.scm";

            memcpy((char*)0x5EE79C, scm, sizeof(scm)); // scm patch 
            memcpy((char*)0x6108A0, scm_data, sizeof(scm_data)); // scm patch
            memcpy((char*)0x600200, missing, sizeof(missing)); // fix missing text
            memcpy((char*)0x5F55E0, loadsc4, sizeof(loadsc4)); // custom load scr
        };

        Events::menuDrawingEvent += [] { if (init == 0) {
            plugin::Call<0x48AB40>(); init = 1; patch::Nop(0x4872B0, 5); patch::Nop(0x487998
                , 5);
        }};

        Events::processScriptsEvent += []
        {
            if (KeyPressed(VK_ESCAPE)) paused = 1;
            if (KeyPressed('T')) { if (mouse == 0) { mouse = 1; } }
            
            IsPaused();
            if (FindPlayerPed())
            {
                FindPlayerPed()->m_nPedType = 1;
            }

            *(INT*)0x8F4374 = 60;
            *(BYTE*)0x5F2E60 = 1;

            ProcessSync();
        };

        Events::d3dLostEvent += []
        {
            ImGui_ImplDX8_InvalidateDeviceObjects();
        };

        Events::d3dResetEvent += []
        {
            ImGui_ImplDX8_InvalidateDeviceObjects();
        };

        Events::initGameEvent += []
        {
            if (m_gameStarted == 0) m_gameStarted = 1;
            IsConnectedToServer = false;

            CWorld::Players[0].m_bInfiniteSprint = true;

            if (Anticheat())
            {
                CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)LUThread, NULL, 0, nullptr));
                CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)SyncThread, NULL, 0, nullptr));
            }
            else
            {
                p_ChatBox.AddLog("CRC32 CHECK: Failed on lu.scm");
            }
        };
    }
   
} lU_CLIENT;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        
    }
    return TRUE;
}