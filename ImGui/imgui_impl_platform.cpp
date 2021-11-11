#include "imgui.h"
#include "imgui_impl_platform.h"

#include <stdio.h>
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Buffer.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Macros.h"

using namespace simul;
using namespace crossplatform;

struct ImGuiCB
{
	static const int bindingIndex=0;
	mat4	ProjectionMatrix;
};


// Platform data
struct ImGui_ImplPlatform_Data
{
	RenderPlatform*			renderPlatform = nullptr;
	Buffer*					pVB = nullptr;
	Buffer*					pIB = nullptr;
	Effect*					effect = nullptr;
	EffectPass*				effectPass=nullptr;
	Layout*					pInputLayout = nullptr;
	ConstantBuffer<ImGuiCB>	constantBuffer;
	Texture*				pFontTextureView = nullptr;
	RenderState*			pRasterizerState = nullptr;
	RenderState*			pBlendState = nullptr;
	RenderState*			pDepthStencilState = nullptr;
	int						VertexBufferSize=0;
	int						IndexBufferSize=0;
	float					width_m=2.0f;
	// pos/orientation of 3D UI:
	vec3 centre;
	vec3 normal;

	mat4 imgui_to_world;
	mat4 local_to_world;
	mat4 imgui_to_local;
	mat4 world_to_imgui;
	mat4 invViewProj;
	vec3 view_pos;
	bool textureUploaded=false;

	int2 mouse,screen;
	bool is3d=false;

	ImGui_ImplPlatform_Data()	   {  VertexBufferSize = 5000; IndexBufferSize = 10000; }

	void Release()
	{
	}
};

struct VERTEX_CONSTANT_BUFFER
{
	float   mvp[4][4];
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplPlatform_Data* ImGui_ImplPlatform_GetBackendData()
{
	return ImGui::GetCurrentContext() ? (ImGui_ImplPlatform_Data*)ImGui::GetIO().BackendRendererUserData : NULL;
}

// Functions
static void ImGui_ImplPlatform_SetupRenderState(ImDrawData* draw_data, GraphicsDeviceContext &deviceContext)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if (!bd->textureUploaded)
	{
		unsigned char* pixels;
		int width, height;
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		bd->pFontTextureView->setTexels(deviceContext, pixels, 0, width * height);
	}

	// Setup viewport
	Viewport vp;
	memset(&vp, 0, sizeof(Viewport));
	vp.w = (int)draw_data->DisplaySize.x;
	vp.h = (int)draw_data->DisplaySize.y;
	vp.x = vp.y = 0;
	if(!bd->is3d)
		bd->renderPlatform->SetViewports(deviceContext,1, &vp);

	// Setup shader and vertex buffers
	unsigned int stride = sizeof(ImDrawVert);
	unsigned int offset = 0;
	bd->renderPlatform->SetVertexBuffers(deviceContext, 0, 1, &bd->pVB, bd->pInputLayout);
	bd->renderPlatform->SetIndexBuffer(deviceContext, bd->pIB);
	bd->renderPlatform->SetTopology(deviceContext, crossplatform::Topology::TRIANGLELIST);
}

// Render function
void ImGui_ImplPlatform_RenderDrawData(GraphicsDeviceContext &deviceContext,ImDrawData* draw_data)
{
	// Avoid rendering when minimized
	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
		return;

	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	RenderPlatform* renderPlatform = bd->renderPlatform;

	// Create and grow vertex/index buffers if needed
	if (!bd->pVB || bd->VertexBufferSize < draw_data->TotalVtxCount)
	{
		if (bd->pVB)
		 {
			delete bd->pVB;
			bd->pVB = NULL;
		}
		bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
		bd->pVB=renderPlatform->CreateBuffer();
		bd->pVB->EnsureVertexBuffer(renderPlatform, bd->VertexBufferSize, bd->pInputLayout, nullptr, true);
	}
	if (!bd->pIB || bd->IndexBufferSize < draw_data->TotalIdxCount)
	{
		if (bd->pIB)
		{
			SAFE_DELETE(bd->pIB);
		}
		bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
		bd->pIB=renderPlatform->CreateBuffer();
		bd->pIB->EnsureIndexBuffer(renderPlatform, bd->IndexBufferSize, sizeof(ImDrawIdx),nullptr,true);
	}

	// Upload vertex/index data into a single contiguous GPU buffer
	ImDrawVert* vtx_dst= (ImDrawVert*)bd->pVB->Map(deviceContext);
	if (!vtx_dst)
		return;
	ImDrawIdx* idx_dst= (ImDrawIdx*)bd->pIB->Map(deviceContext);
	if (!idx_dst)
	{
		bd->pVB->Unmap(deviceContext);
		return;
	}
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	bd->pVB->Unmap(deviceContext);
	bd->pIB->Unmap(deviceContext);
	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
	{
		float L = draw_data->DisplayPos.x;
		float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T = draw_data->DisplayPos.y;
		float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		float mvp[4][4] =
		{
			{ 2.0f/(R-L),   0.0f,		   0.0f,	   0.0f },
			{ 0.0f,			2.0f/(T-B),		0.0f,	   0.0f },
			{ 0.0f,			 0.0f,			0.5f,	   0.0f },
			{ (R+L)/(L-R),	(T+B)/(B-T),	0.5f,	   1.0f },
		};
		if(bd->is3d)
		{
			// The intention here is first to transform the pixel-space xy positions into metres in object space,
			// then we can multiply by viewProj.
			float height_m = float(B - T) / float(R - L) * bd->width_m;
			float X = bd->width_m / (R - L);
			// 400*width_m/400 = width_m
			// -width_m/2=width_m/2
			float Y = height_m / (B - T);
			static float tilt =  3.1415926536f /4.0f;
			float cos_t = cos(tilt);
			float sin_t = sin(tilt);
			static float x=0,y=1.f,h = 2.f;
			//bd->centre={x,y,h};
			bd->normal={0,0,1.0f};
			bd->local_to_world =
			{
				 1.0f,	0,		0.0f,	bd->centre.x,
				 0,		cos_t,	-sin_t,	bd->centre.y,
				 0.0f,	sin_t,	cos_t,	bd->centre.z,
				 0.0f,	0.0f,	0.0f,	1.0f
			};
			bd->normal = bd->local_to_world*bd->normal;
			bd->imgui_to_local=
			{
				 X,		0,		0,		-bd->width_m / 2.0f,
				 0,		-Y,		0,		height_m / 2.0f,
				 0,		0,		1.0f,	0,
				 0,		0,		0,		1.0f
			};
			bd->imgui_to_world =
			{
				 X,		0,			0.0f,	bd->centre.x - bd->width_m / 2.0f,
				 0,		Y* cos_t,	0.0f,	bd->centre.y - height_m / 2.0f,
				 0.0f,	Y* sin_t,	1.0f,	bd->centre.z,
				 0.0f,	0.0f,		0.0f,	1.0f
			};
			bd->imgui_to_world = mul(bd->local_to_world,bd->imgui_to_local);
			// store the inverse for mouse clicks:
			simul::math::Matrix4x4 *m=(simul::math::Matrix4x4 *)&bd->imgui_to_world;
			m->Inverse(*((simul::math::Matrix4x4 *)&bd->world_to_imgui));
			bd->view_pos=deviceContext.viewStruct.cam_pos;
			bd->invViewProj=deviceContext.viewStruct.invViewProj;
			mat4 *viewProj= (mat4 * )(&deviceContext.viewStruct.viewProj);
			mat4 modelViewProj=mul(*viewProj, bd->imgui_to_world);
			modelViewProj.transpose();
			memcpy(&bd->constantBuffer.ProjectionMatrix, modelViewProj, sizeof(mat4));
		}
		else
		{
			mat4* ProjectionMatrix = &bd->constantBuffer.ProjectionMatrix;
			memcpy(ProjectionMatrix, mvp, sizeof(mat4));
		}
	}
	bd->renderPlatform->SetConstantBuffer(deviceContext, &bd->constantBuffer);

	crossplatform::Viewport vp=bd->renderPlatform->GetViewport(deviceContext, 1);
	// Setup desired DX state
	ImGui_ImplPlatform_SetupRenderState(draw_data, deviceContext);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_idx_offset = 0;
	int global_vtx_offset = 0;
	ImVec2 clip_off = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					ImGui_ImplPlatform_SetupRenderState(draw_data, deviceContext);
				else
					pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
				ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
				if (clip_max.x < clip_min.x || clip_max.y < clip_min.y)
					continue;

				// TODO: Apply scissor/clipping rectangle
			   // const D3D11_RECT r = { (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
				//renderPlatform->RSSetScissorRects(1, &r);

				// Bind texture, Draw
				Texture* texture_srv = (Texture*)pcmd->GetTexID();
				renderPlatform->SetTexture(deviceContext,bd->effect->GetShaderResource("texture0"),texture_srv);
				renderPlatform->ApplyPass(deviceContext, bd->effectPass);
				bd->pInputLayout->Apply(deviceContext);
				renderPlatform->DrawIndexed(deviceContext,pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
				bd->pInputLayout->Unapply(deviceContext);
				renderPlatform->UnapplyPass(deviceContext );
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
	if(!bd->is3d)
		bd->renderPlatform->SetViewports(deviceContext, 1,&vp);
}

static void ImGui_ImplPlatform_CreateFontsTexture()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	bd->pFontTextureView=bd->renderPlatform->CreateTexture("imgui_font");
	bd->pFontTextureView->ensureTexture2DSizeAndFormat(bd->renderPlatform,width,height,1,PixelFormat::RGBA_8_UNORM);
	bd->textureUploaded=false;
	// Store our identifier
	io.Fonts->SetTexID((ImTextureID)bd->pFontTextureView);
}

bool	ImGui_ImplPlatform_CreateDeviceObjects()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if (!bd->renderPlatform)
		return false;
	if (bd->pFontTextureView)
		ImGui_ImplPlatform_InvalidateDeviceObjects();

	crossplatform::LayoutDesc local_layout[] =
	{
		{ "POSITION", 0, crossplatform::RG_32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, pos), false, 0 },
		{ "TEXCOORD", 0, crossplatform::RG_32_FLOAT,   0, (UINT)IM_OFFSETOF(ImDrawVert, uv),  false, 0 },
		{ "COLOR"   , 0, crossplatform::RGBA_8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), false, 0 },
	};
	bd->pInputLayout=bd->renderPlatform->CreateLayout(3,local_layout,true);

	bd->effect= bd->renderPlatform->CreateEffect("imgui");
	bd->effectPass=bd->effect->GetTechniqueByIndex(0)->GetPass(0);
	ImGui_ImplPlatform_CreateFontsTexture();
	bd->constantBuffer.RestoreDeviceObjects(bd->renderPlatform);
	return true;
}

void	ImGui_ImplPlatform_InvalidateDeviceObjects()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if (!bd)
		return;
	if (!bd->renderPlatform)
		return;

	// (bd->pFontSampler)		   { bd->pFontSampler->Release(); bd->pFontSampler = NULL; }
	if (bd->pFontTextureView)	
	{
		bd->pFontTextureView->InvalidateDeviceObjects();
		ImGui::GetIO().Fonts->SetTexID(NULL);
		// We copied data->pFontTextureView to io.Fonts->TexID so let's clear that as well.
	}
	if (bd->pIB)					
		bd->pIB->InvalidateDeviceObjects();
	if (bd->pVB)
		bd->pVB->InvalidateDeviceObjects();
	if (bd->pBlendState)
		bd->pBlendState->InvalidateDeviceObjects();
	if (bd->pDepthStencilState)	 
		bd->pDepthStencilState->InvalidateDeviceObjects();
	if (bd->pRasterizerState)	   
		bd->pRasterizerState->InvalidateDeviceObjects();
	bd->constantBuffer.InvalidateDeviceObjects();
	if (bd->pInputLayout)
		bd->pInputLayout->InvalidateDeviceObjects();
	if (bd->effect)
		bd->effect->InvalidateDeviceObjects();
}
void	ImGui_ImplPlatform_RecompileShaders()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if (!bd)
		return;
	delete bd->effect;
	bd->effect =nullptr;
	if(!bd->renderPlatform)
		return;
	bd->effect = bd->renderPlatform->CreateEffect("imgui");
	bd->effectPass = bd->effect->GetTechniqueByIndex(0)->GetPass(0);
}

bool	ImGui_ImplPlatform_Init(simul::crossplatform::RenderPlatform* r)
{
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");

	// Setup backend capabilities flags
	ImGui_ImplPlatform_Data* bd = IM_NEW(ImGui_ImplPlatform_Data)();
	io.BackendRendererUserData = (void*)bd;
	io.BackendRendererName = "imgui_impl_dx11";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

	bd->renderPlatform = r;

	return true;
}

void ImGui_ImplPlatform_Shutdown()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if(!bd)
		return;
	IM_ASSERT(bd != NULL && "No renderer backend to shutdown, or already shutdown?");
	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererName = NULL;
	io.BackendRendererUserData = NULL;

	ImGui_ImplPlatform_InvalidateDeviceObjects();
	IM_DELETE(bd);
	bd=nullptr;
}

void ImGui_ImplPlatform_NewFrame(bool in3d,int ui_pixel_width,int ui_pixel_height,const float *pos,float width_m)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	IM_ASSERT(bd != NULL && "Did you call ImGui_ImplPlatform_Init()?");
	
	bd->is3d=in3d;
	bd->width_m=width_m;
	if (!bd->pFontTextureView)
		ImGui_ImplPlatform_CreateDeviceObjects();

	// In 3d this takes the place of the CPU platform's NewFrame
	if(in3d)
	{
		ImGuiIO& io = ImGui::GetIO();
		// Setup display size (every frame to accommodate for window resizing)
		io.DisplaySize = ImVec2((float)ui_pixel_width, (float)ui_pixel_height);
		if(pos)
			bd->centre=pos;
    // Override what win32 did with this
		ImGui_ImplPlatform_Update3DMousePos();
	}
}


void ImGui_ImplPlatform_SetMousePos(int x, int y, int W, int H)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if(!bd)
		return;
	bd->mouse={x,y};
	bd->screen={W,H};
}



void ImGui_ImplPlatform_Update3DMousePos()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if(!bd->is3d)
		return;
	if(!bd->screen.x||!bd->screen.y)
		return;
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(bd->hWnd != 0);

	const ImVec2 mouse_pos_prev = io.MousePos;

	// Set Dear ImGui mouse position from OS position

	vec3 diff			= bd->centre - bd->view_pos;
	// from the Windows mouse pos obtain a direction.
	vec4 view_dir		= { (2.0f*bd->mouse.x - bd->screen.x) / float(bd->screen.x),(bd->screen.y - 2.0f * bd->mouse.y) / float(bd->screen.y),1.0f,1.0f};
	// Transform this into a worldspace direction.
	vec3 view_w			= (view_dir* bd->invViewProj).xyz();
	view_w				= normalize(view_w);
	// Take the vector in this direction from view_pos, Intersect it with the UI surface.
	float dist			= dot(diff, bd->normal)/dot(view_w,bd->normal);
	vec3 intersection_w	= bd->view_pos+view_w*dist;

	// Transform the intersection point into object-space then into UI-space.
	vec3 client_pos		= (bd->world_to_imgui*vec4(intersection_w,1.0f)).xyz();
	// Finally, set this as the mouse pos.
	io.MousePos			= ImVec2(client_pos.x,client_pos.y);
}

void ImGui_ImplPlatform_DebugInfo()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("mouse: %d %d gui %1.0f %1.0f", bd->mouse.x,bd->mouse.y, io.MousePos.x, io.MousePos.y);
}
