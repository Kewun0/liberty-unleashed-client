#pragma once

IMGUI_API bool ImGui_ImplRenderWare_Init();
IMGUI_API void ImGui_ImplRenderWare_RenderDrawData(ImDrawData* draw_data);
IMGUI_API bool ImGui_ImplRenderWare_CreateDeviceObjects();
IMGUI_API void ImGui_ImplRenderWare_ShutDown();
IMGUI_API void ImGui_ImplRenderWare_NewFrame();