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
#define NUDE void _declspec(naked) 



#include "plugin.h"
#include "CMenuManager.h"
#include "extensions/ScriptCommands.h"
#include "CWorld.h"
#include "ePedPieceTypes.h"
#include "CCamera.h"
#include "CHud.h"

#include <stdio.h>
#include <thread>
#include <enet/enet.h>
#include <io.h>
#include <stdlib.h>
#include <chrono>
#include <map>

#pragma comment(lib,"enet.lib")
#pragma warning(disable: 4018)
#pragma warning(disable: 4244)
#pragma warning(disable: 4996)

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winmm.lib") 


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


using namespace plugin;

int init = 0;
int CLIENT_ID = -1;

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

int __fastcall CWeapon__DoBulletImpact_Hook(CWeapon* This, DWORD _EDX, CEntity* source, CEntity* target, CVector* start, CVector* end, CColPoint* colpoint, CVector2D ahead)
{
    return original_CWeapon__DoBulletImpact(This, source, target, start, end, colpoint, ahead);
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

void SendPacket(ENetPeer* peer, const char* data)
{
    ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
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

void ParseData(char* data)
{
    int data_type;
    int id;
    sscanf(data, "%d|%d", &data_type, &id);
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

            client_map[id] = new Clients(id);
            client_map[id]->SetUsername(username);
        }
        break;
    case 3: 
        CLIENT_ID = id;
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
                    char sync_packet[6];
                    sprintf(sync_packet, "8|%i", (int)FindPlayerPed()->m_fHealth);
                    SendPacket(peer, sync_packet);
                    Sleep(400);
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


    CHud::SetHelpMessage(stws(Format(" Connecting to %s:%s...", ip, port)), false);

    printf("Welcome to Liberty Unleashed 0.1\n");
    printf("Connecting to server %s:%s...\n",ip,port);

    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
    {
        printf("Connection Successful. Loading server info...\n");
        CHud::SetHelpMessage(stws(Format(" Connection successful. Loading game", ip, port)), false);
        IsConnectedToServer = true;
        char str_data[80] = "2|";
        strcat(str_data, nickname);
        SendPacket(peer, str_data);
    }
    else
    {
        CHud::SetHelpMessage(stws(Format(" You failed to connect to the server", ip, port)), false);
        printf("You failed to connect to the server. \n");
        enet_peer_reset(peer);
        IsConnectedToServer = false;
    }

    while (enet_host_service(client, &event, 5000) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
            last_sync_packet = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            printf("A packet of length %u containing %s was received from %x:%u on channel %u.\n",
                event.packet->dataLength,
                event.packet->data,
                event.peer->address.host,
                event.peer->address.port,
                event.channelID);
            break;
        }
    }

    while (1 != 2)
    {
        Sleep(1);
    }
    return 0;
}

#include "CCamera.h"

void CCamera_SetCamPositionForFixedMode(CVector const& vecFixedModeSource, CVector const& vecFixedModeUpOffSet) {
    plugin::CallMethod<0x46BA72, CCamera*, CVector const&, CVector const&>(&TheCamera, vecFixedModeSource, vecFixedModeUpOffSet);
}

void LostConnection()
{
    IsConnectedToServer = false;
    enet_peer_disconnect(peer, 0);
    printf("You have lost connection to the server\n");
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
        }
        szCmdLine++;
    }
}

const char* my_fun(const char* par)
{
    return par;
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

        AllocConsole();
        freopen("CONOUT$", "w", stdout);

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

        Events::initRwEvent += []
        {
            srand(time(NULL));

         //   Hook((void*)0x52BFB0, (void*)my_fun, 5); // fix missing text

            GameKeyStatesInit();
            InstallMethodHook(0x5FA308, (DWORD)CPlayerPed_ProcessControl_Hook);
            patch::Nop(0x48C975, 5); // Disable Replays

            char loadsc4[9] = "mainsc1";

            if (fopen("txd\\lu.txd", "r") != NULL)
            {
                sprintf(loadsc4, "lu");
            }

            char missing[3] = "%s";

            memcpy((char*)0x600200, missing, sizeof(missing)); // fix missing text
            memcpy((char*)0x5F55E0, loadsc4, sizeof(loadsc4)); // custom load scr
        };

        Events::menuDrawingEvent += [] { if (init == 0) {
            plugin::Call<0x48AB40>(); init = 1; patch::Nop(0x4872B0, 5); patch::Nop(0x487998
                , 5);
        }};

        Events::processScriptsEvent += []
        {
            if (FindPlayerPed())
            {
                FindPlayerPed()->m_nPedType = 1;
            }

            *(INT*)0x8F4374 = 60;
            *(BYTE*)0x5F2E60 = 1;

            ProcessSync();
        };

        Events::initGameEvent += []
        {
            IsConnectedToServer = false;

            CWorld::Players[0].m_bInfiniteSprint = true;

            CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)LUThread, NULL, 0, nullptr));
            CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)SyncThread, NULL, 0, nullptr));
        };
        
    }
} lU_CLIENT;
