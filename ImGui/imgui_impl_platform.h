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
#include "Export.h"			// IMGUI_IMPL_API
#include "imgui.h"			// IMGUI_IMPL_API
#include <vector>
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/RenderDelegate.h"

namespace platform
{
	namespace crossplatform
	{
		class Texture;
		class RenderPlatform;
		struct GraphicsDeviceContext;
		class DisplaySurfaceManagerInterface;
		class RenderDelegaterInterface;
	}
}

//! A wrapper class for drawing textures including mip/slice info.
struct ImGui_ImplPlatform_TextureView
{
	platform::crossplatform::Texture *texture=nullptr;
	float mip=0.0f;
	int slice=0;
	//! If set, this delegate will be called to render into the texture.
	platform::crossplatform::RenderDelegate renderDelegate;
	//! If texture is null, get a scratch texture of the appropriate width/height.
	int width=0,height=0;
};

//! Initialize the Platform implementation. If hosted is set to true, this will also fill in ImGuiPlatformIO, e.g. if imgui is hosted
//! in a game engine etc instead of needing Win32 behaviour etc.
IMGUI_IMPL_API bool     ImGui_ImplPlatform_Init(platform::crossplatform::RenderPlatform* r,bool hosted=false);
IMGUI_IMPL_API void     ImGui_ImplPlatform_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplPlatform_NewFrame(bool in3d=false, int ui_pixel_width=400, int ui_pixel_height=300,const float *menupos=nullptr, float az=0.0f, float tilt=0.0f,float width_m=2.0f);
IMGUI_IMPL_API void		ImGui_ImplPlatform_Win32_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplPlatform_RenderDrawData(platform::crossplatform::GraphicsDeviceContext& deviceContext, ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API void     ImGui_ImplPlatform_InvalidateDeviceObjects();
IMGUI_IMPL_API bool     ImGui_ImplPlatform_CreateDeviceObjects();

IMGUI_IMPL_API void		ImGui_ImplPlatform_RecompileShaders();

//! Set the mouse position and screen size, per-frame.
IMGUI_IMPL_API void		ImGui_ImplPlatform_SetMousePos(int x, int y, int W, int H);
IMGUI_IMPL_API void		ImGui_ImplPlatform_SetMouseDown(int button, bool value);

//! translate the screen mouse position into 3D
IMGUI_IMPL_API void		ImGui_ImplPlatform_Update3DMousePos();
//! translate 3D controller positions into pseudo-mouse positions and clicks.
IMGUI_IMPL_API void		ImGui_ImplPlatform_Update3DTouchPos(const std::vector<vec4>& position_press);
IMGUI_IMPL_API void		ImGui_ImplPlatform_Get3DTouchClientPos( std::vector<vec3>& client_press);

IMGUI_IMPL_API void		ImGui_ImplPlatform_DebugInfo();
// Forward Declarations
IMGUI_IMPL_API void		ImGui_ImplPlatform_InitPlatformInterface();
IMGUI_IMPL_API void		ImGui_ImplPlatform_ShutdownPlatformInterface();


IMGUI_IMPL_API void		ImGui_ImplPlatform_SetDisplaySurfaceManagerAndPlatformRenderer(platform::crossplatform::DisplaySurfaceManagerInterface *d,
																		platform::crossplatform::RenderDelegaterInterface *p);
IMGUI_IMPL_API ImDrawData *ImGui_ImplPlatform_GetDrawData(int view_id);

//! Draw the specified texture
IMGUI_IMPL_API void ImGui_ImplPlatform_DrawTexture(platform::crossplatform::Texture* texture, float mip, int slice, int width, int height);
//! Draw the specified texture with a delegate to draw into it first.
IMGUI_IMPL_API void ImGui_ImplPlatform_DrawTexture(platform::crossplatform::RenderDelegate d, const char* textureName, float mip, int slice, int width, int height);
