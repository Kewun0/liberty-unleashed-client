// ImGui Win32 + DirectX9 binding
// In this binding, ImTextureID is used to store a 'LPDIRECT3DTEXTURE9' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui
#define CINTERFACE
#include "../imgui.h"

#include "imgui_impl_dx8.h"

// DirectX
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <d3d8.h>


// Data
static HWND                     g_hWnd = 0;
static INT64                    g_Time = 0;
static INT64                    g_TicksPerSecond = 0;
static LPDIRECT3DDEVICE8        g_pd3dDevice = NULL;
static LPDIRECT3DVERTEXBUFFER8  g_pVB = NULL;
static LPDIRECT3DINDEXBUFFER8   g_pIB = NULL;
static LPDIRECT3DTEXTURE8       g_FontTexture = NULL;
static int                      g_VertexBufferSize = 5000, g_IndexBufferSize = 10000;

struct CUSTOMVERTEX
{
	float    pos[3];
	DWORD col;
	float    uv[2];
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

// Structs for updating and restoring device states by RAII

struct ScopedRS
{
	D3DRENDERSTATETYPE renderStateType;
	DWORD previousValue;

	ScopedRS(D3DRENDERSTATETYPE inRenderStateType, DWORD value)
		: renderStateType(inRenderStateType)
	{
	
		IDirect3DDevice8_GetRenderState(g_pd3dDevice, renderStateType, &previousValue);
		IDirect3DDevice8_SetRenderState(g_pd3dDevice, renderStateType, value);
	}

	~ScopedRS()
	{
		IDirect3DDevice8_SetRenderState(g_pd3dDevice,renderStateType, previousValue);
	}
};

struct ScopedTSS
{
	DWORD textureStage;
	D3DTEXTURESTAGESTATETYPE textureStageStateType;
	DWORD previousValue;

	ScopedTSS(DWORD inTextureStage, D3DTEXTURESTAGESTATETYPE inTextureStageStateType, DWORD value)
		: textureStage(inTextureStage)
		, textureStageStateType(inTextureStageStateType)
	{
		IDirect3DDevice8_GetTextureStageState(g_pd3dDevice,textureStage, textureStageStateType, &previousValue);
		IDirect3DDevice8_SetTextureStageState(g_pd3dDevice,textureStage, textureStageStateType, value);
	}

	~ScopedTSS()
	{
		IDirect3DDevice8_SetTextureStageState(g_pd3dDevice,textureStage, textureStageStateType, previousValue);
	}
};

struct ScopedTex
{
	DWORD textureStage;
	IDirect3DBaseTexture8* previousTexture;

	ScopedTex(DWORD inTextureStage, IDirect3DBaseTexture8* texture)
		: textureStage(inTextureStage)
	{
		IDirect3DDevice8_GetTexture(g_pd3dDevice,textureStage, &previousTexture);
		IDirect3DDevice8_SetTexture(g_pd3dDevice,textureStage, texture);
	}

	ScopedTex(DWORD inTextureStage)
		: textureStage(inTextureStage)
	{
		IDirect3DDevice8_GetTexture(g_pd3dDevice,textureStage, &previousTexture);
	}

	~ScopedTex()
	{
		IDirect3DDevice8_SetTexture(g_pd3dDevice,textureStage, previousTexture);
	}
};

struct ScopedTransform
{
	D3DTRANSFORMSTATETYPE transformStateType;
	D3DMATRIX previousTransformMatrix;

	ScopedTransform(D3DTRANSFORMSTATETYPE inTransformType, const D3DMATRIX& transformMatrix)
		: transformStateType(inTransformType)
	{
		IDirect3DDevice8_GetTransform(g_pd3dDevice,transformStateType, &previousTransformMatrix);
		IDirect3DDevice8_SetTransform(g_pd3dDevice, transformStateType, &transformMatrix);
	}

	ScopedTransform(D3DTRANSFORMSTATETYPE inTransformType)
	{
		IDirect3DDevice8_GetTransform(g_pd3dDevice, transformStateType, &previousTransformMatrix);
	}

	~ScopedTransform()
	{
		IDirect3DDevice8_SetTransform(g_pd3dDevice, transformStateType, &previousTransformMatrix);
	}
};

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// If text or lines are blurry when integrating ImGui in your engine:
// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
void ImGui_ImplDX8_RenderDrawLists(ImDrawData* draw_data)
{
	// Avoid rendering when minimized
	ImGuiIO& io = ImGui::GetIO();
	if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f)
		return;

	// Create and grow buffers if needed
	if (!g_pVB || g_VertexBufferSize < draw_data->TotalVtxCount)
	{
		
		if (g_pVB) { IDirect3DVertexBuffer8_Release(g_pVB); g_pVB = NULL; }
		g_VertexBufferSize = draw_data->TotalVtxCount + 5000;
		if (IDirect3DDevice8_CreateVertexBuffer(g_pd3dDevice,g_VertexBufferSize * sizeof(CUSTOMVERTEX), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB) < 0)
			return;
	}
	if (!g_pIB || g_IndexBufferSize < draw_data->TotalIdxCount)
	{
		
		if (g_pIB) { IDirect3DIndexBuffer8_Release(g_pIB); g_pIB = NULL; }
		g_IndexBufferSize = draw_data->TotalIdxCount + 10000;
		if (IDirect3DDevice8_CreateIndexBuffer(g_pd3dDevice,g_IndexBufferSize * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &g_pIB) < 0)
			return;
	}

	// Copy and convert all vertices into a single contiguous buffer
	CUSTOMVERTEX* vtx_dst;
	ImDrawIdx* idx_dst;
	if (IDirect3DVertexBuffer8_Lock(g_pVB,0, (UINT)(draw_data->TotalVtxCount * sizeof(CUSTOMVERTEX)), (BYTE**)&vtx_dst, D3DLOCK_DISCARD) < 0)
		return;
	if (IDirect3DIndexBuffer8_Lock(g_pIB,0, (UINT)(draw_data->TotalIdxCount * sizeof(ImDrawIdx)), (BYTE**)&idx_dst, D3DLOCK_DISCARD) < 0)
		return;

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		const ImDrawVert* vtx_src = cmd_list->VtxBuffer.Data;
		for (int i = 0; i < cmd_list->VtxBuffer.Size; i++)
		{
			vtx_dst->pos[0] = vtx_src->pos.x;
			vtx_dst->pos[1] = vtx_src->pos.y;
			vtx_dst->pos[2] = 0.0f;
			vtx_dst->col = (vtx_src->col & 0xFF00FF00) | ((vtx_src->col & 0xFF0000) >> 16) | ((vtx_src->col & 0xFF) << 16);    
			vtx_dst->uv[0] = vtx_src->uv.x;
			vtx_dst->uv[1] = vtx_src->uv.y;
			vtx_dst++;
			vtx_src++;
		}
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	IDirect3DVertexBuffer8_Unlock(g_pVB);
	IDirect3DIndexBuffer8_Unlock(g_pIB);
	IDirect3DDevice8_SetStreamSource(g_pd3dDevice,0, g_pVB, sizeof(CUSTOMVERTEX));
	IDirect3DDevice8_SetVertexShader(g_pd3dDevice,D3DFVF_CUSTOMVERTEX); 
														
	D3DVIEWPORT8 vp;
	vp.X = vp.Y = 0;
	vp.Width = (DWORD)io.DisplaySize.x;
	vp.Height = (DWORD)io.DisplaySize.y;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	IDirect3DDevice8_SetViewport(g_pd3dDevice,&vp);
	IDirect3DDevice8_SetPixelShader(g_pd3dDevice,NULL);

	ScopedRS scopedRenderStates[] =
	{
		ScopedRS(D3DRS_CULLMODE, D3DCULL_NONE),
		ScopedRS(D3DRS_LIGHTING, false),
		ScopedRS(D3DRS_ZENABLE, false),
		ScopedRS(D3DRS_ALPHABLENDENABLE, true),
		ScopedRS(D3DRS_ALPHATESTENABLE, false),
		ScopedRS(D3DRS_BLENDOP, D3DBLENDOP_ADD),
		ScopedRS(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA),
		ScopedRS(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA),
	};

	ScopedTSS scopedTextureStageStates[] =
	{
		ScopedTSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE),
		ScopedTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE),
		ScopedTSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE),
		ScopedTSS(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE),
		ScopedTSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE),
		ScopedTSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE),
		ScopedTSS(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR),
		ScopedTSS(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR),
	};

	ScopedTransform scopedTransforms[] =
	{
		ScopedTransform(D3DTS_WORLD),
		ScopedTransform(D3DTS_VIEW),
		ScopedTransform(D3DTS_PROJECTION),
	};
	D3DMATRIX last_world, last_view, last_projection;
	IDirect3DDevice8_GetTransform(g_pd3dDevice,D3DTS_WORLD, &last_world);
	IDirect3DDevice8_GetTransform(g_pd3dDevice, D3DTS_VIEW, &last_view);
	IDirect3DDevice8_GetTransform(g_pd3dDevice, D3DTS_PROJECTION, &last_projection);
	
	ScopedTex scopedTexture(0);

	// Setup orthographic projection matrix
	// Being agnostic of whether <d3DX8.h> or <DirectXMath.h> can be used, we aren't relying on D3DXMatrixIdentity()/D3DXMatrixOrthoOffCenterLH() or DirectX::XMMatrixIdentity()/DirectX::XMMatrixOrthographicOffCenterLH()
	{
		const float L = 0.5f, R = io.DisplaySize.x + 0.5f, T = 0.5f, B = io.DisplaySize.y + 0.5f;
		D3DMATRIX mat_identity = { { 1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.0f, 1.0f } };
		D3DMATRIX mat_projection =
		{
			2.0f / (R - L),   0.0f,         0.0f,  0.0f,
			0.0f,         2.0f / (T - B),   0.0f,  0.0f,
			0.0f,         0.0f,         0.5f,  0.0f,
			(L + R) / (L - R),  (T + B) / (B - T),  0.5f,  1.0f,
		};
		IDirect3DDevice8_SetTransform(g_pd3dDevice,D3DTS_WORLD, &mat_identity);
		IDirect3DDevice8_SetTransform(g_pd3dDevice,D3DTS_VIEW, &mat_identity);
		IDirect3DDevice8_SetTransform(g_pd3dDevice,D3DTS_PROJECTION, &mat_projection);
	}

	// Render command lists
	int vtx_offset = 0;
	int idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				const RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
				IDirect3DDevice8_SetTexture(g_pd3dDevice,0, (IDirect3DBaseTexture8*)pcmd->TextureId);
				
				IDirect3DDevice8_SetIndices(g_pd3dDevice,g_pIB, vtx_offset);
				IDirect3DDevice8_DrawIndexedPrimitive(g_pd3dDevice,D3DPT_TRIANGLELIST, 0, (UINT)cmd_list->VtxBuffer.Size, idx_offset, pcmd->ElemCount / 3);
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += cmd_list->VtxBuffer.Size;
	}
	IDirect3DDevice8_SetTransform(g_pd3dDevice, D3DTS_WORLD, &last_world);
	IDirect3DDevice8_SetTransform(g_pd3dDevice, D3DTS_VIEW, &last_view);
	IDirect3DDevice8_SetTransform(g_pd3dDevice, D3DTS_PROJECTION, &last_projection);
}

IMGUI_API LRESULT ImGui_ImplDX8_WndProcHandler(HWND, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (msg)
	{
	case WM_LBUTTONDOWN:
		io.MouseDown[0] = true;
		return true;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false;
		return true;
	case WM_RBUTTONDOWN:
		io.MouseDown[1] = true;
		return true;
	case WM_RBUTTONUP:
		io.MouseDown[1] = false;
		return true;
	case WM_MBUTTONDOWN: 
		io.MouseDown[2] = true;
		return true;
	case WM_MBUTTONUP:
		io.MouseDown[2] = false;
		return true;
	case WM_MOUSEWHEEL:
		io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
		return true;
	case WM_MOUSEMOVE:
		io.MousePos.x = (signed short)(lParam);
		io.MousePos.y = (signed short)(lParam >> 16);
		return true;
	case WM_KEYDOWN:
		if (wParam < 256)
			io.KeysDown[wParam] = 1;
		return true;
	case WM_KEYUP:
		if (wParam < 256)
			io.KeysDown[wParam] = 0;
		return true;
	case WM_CHAR:
		// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
		if (wParam > 0 && wParam < 0x10000)
			io.AddInputCharacter((unsigned short)wParam);
		return true;
	}
	return 0;
}

bool    ImGui_ImplDX8_Init(void* hwnd, IDirect3DDevice8* device)
{
	g_hWnd = (HWND)hwnd;
	g_pd3dDevice = device;

	if (!QueryPerformanceFrequency((LARGE_INTEGER *)&g_TicksPerSecond))
		return false;
	if (!QueryPerformanceCounter((LARGE_INTEGER *)&g_Time))
		return false;

	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;                       // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array that we will update during the application lifetime.
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';

	io.RenderDrawListsFn = ImGui_ImplDX8_RenderDrawLists;   // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
	io.ImeWindowHandle = g_hWnd;

	return true;
}

void ImGui_ImplDX8_Shutdown()
{
	ImGui_ImplDX8_InvalidateDeviceObjects();
	//ImGui::Shutdown();
	g_pd3dDevice = NULL;
	g_hWnd = 0;
}
#include <stdio.h>
static bool ImGui_ImplDX8_CreateFontsTexture()
{
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height, bytes_per_pixel;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);
	
	printf("Texture shit %i, %i \n", width, height);

	g_FontTexture = NULL;
	if (IDirect3DDevice8_CreateTexture(g_pd3dDevice,width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_FontTexture) < 0)
		return false;
	D3DLOCKED_RECT tex_locked_rect;
	
	if (IDirect3DTexture8_LockRect(g_FontTexture,0, &tex_locked_rect, NULL, 0) != D3D_OK)
		return false;
	for (int y = 0; y < height; y++)
		memcpy((unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y, pixels + (width * bytes_per_pixel) * y, (width * bytes_per_pixel));
	IDirect3DTexture8_UnlockRect(g_FontTexture,0);

	// Store our identifier
	io.Fonts->TexID = (void *)g_FontTexture;

	return true;
}

bool ImGui_ImplDX8_CreateDeviceObjects()
{
	if (!g_pd3dDevice)
		return false;
	if (!ImGui_ImplDX8_CreateFontsTexture())
		return false;
	return true;
}

void ImGui_ImplDX8_InvalidateDeviceObjects()
{
	if (!g_pd3dDevice)
		return;
	if (g_pVB)
	{
		IDirect3DVertexBuffer8_Release(g_pVB);
		g_pVB = NULL;
	}
	if (g_pIB)
	{
		IDirect3DIndexBuffer8_Release(g_pIB);
		g_pIB = NULL;
	}

	// At this point note that we set ImGui::GetIO().Fonts->TexID to be == g_FontTexture, so clear both.
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(g_FontTexture == io.Fonts->TexID);
	if (g_FontTexture)
		IDirect3DTexture8_Release(g_FontTexture);
	g_FontTexture = NULL;
	io.Fonts->TexID = NULL;
}

void ImGui_ImplDX8_NewFrame()
{
	
	if (!g_FontTexture)
		ImGui_ImplDX8_CreateDeviceObjects();
	
	ImGuiIO& io = ImGui::GetIO();

	// Setup display size (every frame to accommodate for window resizing)
	RECT rect;
	GetClientRect(g_hWnd, &rect);
	io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

	// Setup time step
	INT64 current_time;
	QueryPerformanceCounter((LARGE_INTEGER *)&current_time);
	io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
	g_Time = current_time;

	// Read keyboard modifiers inputs
	io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
	io.KeySuper = false;
	// io.KeysDown : filled by WM_KEYDOWN/WM_KEYUP events
	// io.MousePos : filled by WM_MOUSEMOVE events
	// io.MouseDown : filled by WM_*BUTTON* events
	// io.MouseWheel : filled by WM_MOUSEWHEEL events

	// Hide OS mouse cursor if ImGui is drawing it
	if (io.MouseDrawCursor)
		SetCursor(NULL);

	// Start the frame
	ImGui::NewFrame();
}
