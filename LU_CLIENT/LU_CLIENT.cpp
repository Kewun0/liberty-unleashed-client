#define _WINSOCKAPI_ 
#define CINTERFACE

#include "plugin.h"
#include "CMenuManager.h"
#include "extensions/ScriptCommands.h"
#include "CWorld.h"

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

int64 last_sync_packet = 0;
 
bool IsConnectedToServer = false;


using namespace plugin;

int init = 0;

FILE* file;

void SendPacket(ENetPeer* peer, const char* data)
{
    ENetPacket* packet = enet_packet_create(data, strlen(data) + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
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


int CLIENT_ID = -1;
void ParseData(char* data)
{
    // Will store the data type (e.g. 1, 2, etc)
    int data_type;

    // Will store the id of the client that is sending the data
    int id;

    // Get first two numbers from the data (data_type and id) and but them in their respective variables
    sscanf(data, "%d|%d", &data_type, &id);

    // Switch between the different data_types
    switch (data_type)
    {
    case 1: // data is a message
        if (id != CLIENT_ID)
        {
            // Get message and Post it using the ClientData at id's username and the parsed msg.
            char msg[80];
            sscanf(data, "%*d|%*d|%[^|]", &msg);
           // chatScreen.PostMessage(client_map[id]->GetUsername().c_str(), msg);
        }
        break;
    case 2: // data is a username
        if (id != CLIENT_ID)
        {
            // Create a new ClientData with username and add it to map at id.
            char username[80];
            sscanf(data, "%*d|%*d|%[^|]", &username);

            client_map[id] = new Clients(id);
            client_map[id]->SetUsername(username);
        }
        break;
    case 3: // data is our ID.
        CLIENT_ID = id; // Set our id to the received id.
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

    printf("Welcome to Liberty Unleashed 0.1\n");
    printf("Connecting to server %s:%s...\n",ip,port);

    if (enet_host_service(client, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
    {
        printf("Connection Successful. Loading server info...\n");

        IsConnectedToServer = true;
        char str_data[80] = "2|";
        strcat(str_data, nickname);
        SendPacket(peer, str_data);
    }
    else
    {
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
            switch (*szCmdLine)
            {
            case 'h':
            case 'H':
                szCmdLine++;
                SetStringFromCommandLine(szCmdLine, ip);
                break;
            case 'p':
            case 'P':
                szCmdLine++;
                SetStringFromCommandLine(szCmdLine, port);
                break;
            case 'n':
            case 'N':
                szCmdLine++;
                SetStringFromCommandLine(szCmdLine, nickname);
                break;
            }
        }
        szCmdLine++;
    }
}


class LU_CLIENT
{
public:
    LU_CLIENT()
    {
        InitSettings();

        AllocConsole();
        freopen("CONOUT$", "w", stdout);

        sprintf(nickname, "Kewun");

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
