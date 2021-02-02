//----------------------------------------------------------
//
// VC:MP Multiplayer Modification For GTA:VC
// Copyright 2004-2005 SA:MP team
//
// File Author(s): jenksta
// License: See LICENSE in root directory
//
//----------------------------------------------------------
#define CINTERFACE
#include <detours.h>
#include <d3d8.h>
#include <stdio.h>
#include "IDirectInputDevice8Hook.h" 
#include "d3d8hook.h"
#include "IDirect3D8Hook.h"
#include "IDirectInput8Hook.h" 


typedef IDirect3D8 * (WINAPI * Direct3DCreate8_t)(UINT SDKVersion);
typedef HRESULT	(WINAPI * DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

Direct3DCreate8_t  m_pfnDirect3DCreate8 = NULL;
DirectInput8Create_t m_pfnDirectInput8Create = NULL;

IDirect3D8 * WINAPI Direct3DCreate8(UINT SDKVersion)
{printf("Direct3DCreate8 %i\n",SDKVersion);
	// Create our device
	IDirect3D8 * pDevice = m_pfnDirect3DCreate8(SDKVersion);

	// Create our device hook
	IDirect3D8Hook * pDeviceHook = new IDirect3D8Hook(pDevice);

	
	// Return the device hook pointer
	printf("NEW\n");
	return pDeviceHook;
}

HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
	printf("MAKING FUCKING INPUT\n");
	HRESULT hr = m_pfnDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
	IDirectInput8 * pDInput = (IDirectInput8 *)*ppvOut;
	*ppvOut = new IDirectInput8Hook(pDInput);
	printf("HALT\n");
	return hr;
}

void InstallD3D8Hook()
{
	printf("Installing d3d8 hook\n");
	if(!m_pfnDirect3DCreate8)
	{ 
		m_pfnDirect3DCreate8 = (Direct3DCreate8_t)DetourFunction(DetourFindFunction((PCHAR)"d3d8.dll", (PCHAR)"Direct3DCreate8"), (PBYTE)Direct3DCreate8);
	}

	if(!m_pfnDirectInput8Create)
	{
		m_pfnDirectInput8Create = (DirectInput8Create_t)DetourFunction(DetourFindFunction((PCHAR)"dinput8.dll", (PCHAR)"DirectInput8Create"), (PBYTE)DirectInput8Create);
	}
	printf("Shit\n");
}

void UninstallD3D8Hook()
{
	if(m_pfnDirect3DCreate8)
	{
		DetourRemove((PBYTE)m_pfnDirect3DCreate8, (PBYTE)Direct3DCreate8);
	}

	if(m_pfnDirectInput8Create)
	{
		DetourRemove((PBYTE)m_pfnDirectInput8Create, (PBYTE)DirectInput8Create);
	}
}