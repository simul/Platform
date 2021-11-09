// dear imgui: Renderer Backend for DirectX11
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'ID3D11ShaderResourceView*' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bit indices.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this. 
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#pragma once
#include "Export.h"      // IMGUI_IMPL_API
#include "imgui.h"      // IMGUI_IMPL_API

namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct GraphicsDeviceContext;
	}
}
IMGUI_IMPL_API bool     ImGui_ImplPlatform_Init(simul::crossplatform::RenderPlatform* r);
IMGUI_IMPL_API void     ImGui_ImplPlatform_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplPlatform_NewFrame(bool in3d=false, int ui_pixel_width=400, int ui_pixel_height=300,const float *menupos=nullptr,float width_m=2.0f);
IMGUI_IMPL_API void     ImGui_ImplPlatform_RenderDrawData(simul::crossplatform::GraphicsDeviceContext& deviceContext, ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API void     ImGui_ImplPlatform_InvalidateDeviceObjects();
IMGUI_IMPL_API bool     ImGui_ImplPlatform_CreateDeviceObjects();

IMGUI_IMPL_API void		ImGui_ImplPlatform_RecompileShaders();

//! translate the screen mouse position into 3D
IMGUI_IMPL_API void		ImGui_ImplPlatform_Update3DMousePos();
//! Set the mouse position and screen size, per-frame.
IMGUI_IMPL_API void		ImGui_ImplPlatform_SetMousePos(int x, int y, int W, int H);

IMGUI_IMPL_API void ImGui_ImplPlatform_DebugInfo();