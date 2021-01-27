#define _WINSOCKAPI_ 
#define CINTERFACE

#include "plugin.h"
#include "CMenuManager.h"
#include <stdio.h>
#include <enet/enet.h>
#include <io.h>
#include <stdlib.h>
#include "extensions/ScriptCommands.h"
#include "CWorld.h"

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

bool IsConnectedToServer = false;


using namespace plugin;
int init = 0;
FILE* file;

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

    enet_address_set_host(&address, "127.0.0.1");
    address.port = 7777;

    peer = enet_host_connect(client, &address, 1, 0);
    if (peer == NULL)
    {
        MessageBox(NULL, "No available peers for initiating an ENet connection!", "Fatal Error", NULL);
        SetForegroundWindow(HWND_DESKTOP);
        ExitProcess(1);
    }

    printf("Welcome to Liberty Unleashed 0.1\n");
    printf("Connecting to server 127.0.0.1:7777...\n");

    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
    {
        printf("Connection Successful. Loading server info...\n");
        IsConnectedToServer = true;
    }
    else
    {
        printf("You failed to connect to the server. \n");
        enet_peer_reset(peer);
        IsConnectedToServer = false;
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


class LU_CLIENT
{
public:
    LU_CLIENT()
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);

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

            patch::Nop(0x48C975, 5); // Disable Replays

            char loadsc4[9] = "mainsc1";

            if (fopen("txd\\lu.txd", "r") != NULL)
            {
                sprintf(loadsc4, "lu");
            }

            memcpy((char*)0x5F55E0, loadsc4, sizeof(loadsc4));
        };

        Events::menuDrawingEvent += [] { if (init == 0) {
            plugin::Call<0x48AB40>(); init = 1; patch::Nop(0x4872B0, 5); patch::Nop(0x487998
                , 5);
        }};

        Events::initScriptsEvent += []
        {
            
        };

        Events::processScriptsEvent += []
        {
            if (FindPlayerPed())
            {
                FindPlayerPed()->m_nPedType = 1;
            }
            *(INT*)0x8F4374 = 60;
            *(BYTE*)0x5F2E60 = 1;
        };

        Events::initGameEvent += []
        {
            IsConnectedToServer = false;

            CWorld::Players[0].m_bInfiniteSprint = true;

            CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)LUThread, NULL, 0, nullptr));
        };
        
    }
} lU_CLIENT;
