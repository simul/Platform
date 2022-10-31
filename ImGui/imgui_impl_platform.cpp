#include "imgui.h"
#include "imgui_impl_platform.h"

#include <stdio.h>
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Buffer.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/CrossPlatform/Macros.h"
#include "Platform/CrossPlatform/GraphicsDeviceInterface.h"

using namespace platform;
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
	EffectPass*				effectPass_testDepth=nullptr;
	EffectPass*				effectPass_noDepth=nullptr;
	EffectPass*				effectPass_placeIn3D_testDepth=nullptr;
	EffectPass*				effectPass_placeIn3D_noDepth=nullptr;
	
	Layout*					pInputLayout = nullptr;
	ConstantBuffer<ImGuiCB>	constantBuffer;
	Texture*				fontTexture = nullptr;
	Texture*				framebufferTexture = nullptr;
	ImGui_ImplPlatform_TextureView fontTextureView;
	ImGui_ImplPlatform_TextureView framebufferTextureView;
	RenderState*			pBlendState = nullptr;
	int						VertexBufferSize=0;
	int						IndexBufferSize=0;
	float					width_m=2.0f;
	// pos/orientation of 3D UI:
	vec3 centre;
	vec3 normal;
	float azimuth = 0.0f;
	float tilt = 3.1415926536f / 4.0f;
	unsigned matrices_ticker=0;
	unsigned used_ticker=0;
	mat4 imgui_to_world;
	mat4 canvas_to_world;
	mat4 local_to_world;
	mat4 world_to_local;
	mat4 imgui_to_local;
	mat4 relative_to_local;
	mat4 world_to_imgui;
	mat4 invViewProj;
	vec3 view_pos;
	// temp:
	float height_m=1.f;
	float X=1.f,Y=1.f;
	bool textureUploaded=false;

	int2 mouse,screen;
	bool is3d=false;
	std::vector<vec3> client_press;
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
		bd->fontTexture->setTexels(deviceContext, pixels, 0, width * height);
		bd->fontTextureView.texture=bd->fontTexture;
		bd->textureUploaded = true;
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
	TextureCreate tc;
	tc.make_rt=true;
	tc.w=int(draw_data->DisplaySize.x);
	tc.l=int(draw_data->DisplaySize.y);
	tc.f=crossplatform::PixelFormat::RGBA_8_UNORM;
	tc.mips=1;
	bd->framebufferTexture->EnsureTexture(deviceContext.renderPlatform,&tc);
	bd->framebufferTextureView.texture=bd->framebufferTexture;
}

// Render function
void ImGui_ImplPlatform_RenderDrawData(GraphicsDeviceContext &deviceContext,ImDrawData* draw_data)
{
	// Avoid rendering when minimized
	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
		return;
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	RenderPlatform* renderPlatform = bd->renderPlatform;
	{
		if(!bd->framebufferTexture)
			bd->framebufferTexture=renderPlatform->CreateTexture("imgui_framebuffer");
	}
	int4 old_scissor=renderPlatform->GetScissor(deviceContext);
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
		bd->pVB->EnsureVertexBuffer(renderPlatform, bd->VertexBufferSize, bd->pInputLayout, nullptr,true);
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
	{
	// Force recreate to find error.
		SAFE_DELETE(bd->pVB);
		return;
	}
	ImDrawIdx* idx_dst= (ImDrawIdx*)bd->pIB->Map(deviceContext);
	if (!idx_dst)
	{
		bd->pVB->Unmap(deviceContext);
		SAFE_DELETE(bd->pIB);
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
	
	float L = draw_data->DisplayPos.x;
	float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float T = draw_data->DisplayPos.y;
	float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	mat4 imgui_projection =
	{
		 2.0f/(R-L),   0.0f,		   0.0f,	   0.0f ,
		 0.0f,			2.0f/(T-B),		0.0f,	   0.0f ,
		 0.0f,			 0.0f,			0.5f,	   0.0f ,
		 (R+L)/(L-R),	(T+B)/(B-T),	0.5f,	   1.0f ,
	};
	mat4 modelViewProj;
	if(bd->is3d)
	{
		if(bd->matrices_ticker!=bd->used_ticker)
		{
			bd->used_ticker=bd->matrices_ticker;
			// The intention here is first to transform the pixel-space xy positions into metres in object space,
			// then we can multiply by viewProj.
			bd->height_m = float(B - T) / float(R - L) * bd->width_m;
			bd->X = bd->width_m / (R - L);
			bd->Y =bd-> height_m / (B - T);
			float cos_t = cos(bd->tilt);
			float sin_t = sin(bd->tilt);
			float cos_a = cos(bd->azimuth);
			float sin_a = sin(bd->azimuth);
			static float x=0,y=1.f,h = 2.f;
			bd->local_to_world =
			{
				 cos_a		,	-sin_a*cos_t	,sin_a*sin_t	,bd->centre.x,
				 sin_a		,	cos_a*cos_t		,-cos_a*sin_t	,bd->centre.y,
				 0			,	sin_t			,cos_t			,bd->centre.z,
				 0.0f		,	0.0f			,0.0f			,1.0f
			};
			bd->normal = bd->local_to_world*vec3(0,0,1.0f);
			bd->imgui_to_local=
			{
				 bd->X,		0,		0,		-bd->width_m / 2.0f,
				 0,		-bd->Y,		0,		bd->height_m / 2.0f,
				 0,		0,		1.0f,	0,
				 0,		0,		0,		1.0f
			};
			mat4 canvas_to_local=
			{
				 bd->width_m/2.f,0					,0		,0,
				 0				,-bd->height_m/2.f	,0		,0,
				 0				,0					,1.0f	,0,
				 0				,0					,0		,1.0f
			};
			bd->relative_to_local =
			{
				 cos_a,		sin_a,			0.0f,	0,
				 -sin_a,	cos_a*cos_t,	sin_t,	0,
				 0.0f,		-sin_t,			cos_t,	0,
				 0.0f,		0.0f,			0.0f,	1.0f
			};
			bd->imgui_to_world = mul(bd->local_to_world,bd->imgui_to_local);
			bd->canvas_to_world=mul(bd->local_to_world,canvas_to_local);
			bd->world_to_local=mat4::unscaled_inverse_transform(bd->local_to_world);
		#if 0
			mat4 check_inverse;
			((platform::math::Matrix4x4 *)&bd->local_to_world)->SimpleInverseOfTransposeTransform(*((platform::math::Matrix4x4 *)&check_inverse));

			vec3 check_centre=(bd->local_to_world*vec4(0,0,0,1.0f)).xyz;
			vec3 check_zero=(bd->world_to_local*vec4(bd->centre,1.0f)).xyz;

			mat4 i_test = mul(bd->local_to_world,check_inverse);
			((platform::math::Matrix4x4 *)&bd->local_to_world)->Inverse(*((platform::math::Matrix4x4 *)&check_inverse));
		
			i_test = mul(bd->local_to_world,check_inverse);
			platform::math::Matrix4x4 *g=(platform::math::Matrix4x4 *)&bd->local_to_world;
			//g->SimpleInverse(*((platform::math::Matrix4x4 *)&bd->world_to_local));
			 i_test = mul(bd->local_to_world,bd->world_to_local);
			#endif

			// store the inverse for mouse clicks:
			platform::math::Matrix4x4 *m=(platform::math::Matrix4x4 *)&bd->imgui_to_world;
			bd->world_to_imgui=mat4::inverse(bd->imgui_to_world);//(*((platform::math::Matrix4x4 *)&bd->world_to_imgui));
			//mat4 i_test = mul(bd->world_to_imgui,bd->imgui_to_world);
		}
		bd->view_pos=deviceContext.viewStruct.cam_pos;
		bd->invViewProj=deviceContext.viewStruct.invViewProj;
		mat4 *viewProj= (mat4 * )(&deviceContext.viewStruct.viewProj);
		modelViewProj=mul(*viewProj, bd->canvas_to_world);
		modelViewProj.transpose();
	}
	bd->constantBuffer.ProjectionMatrix= imgui_projection;
	bd->renderPlatform->SetConstantBuffer(deviceContext, &bd->constantBuffer);

	crossplatform::Viewport vp=bd->renderPlatform->GetViewport(deviceContext, 1);
	// Setup desired DX state
	ImGui_ImplPlatform_SetupRenderState(draw_data, deviceContext);
	auto *tv=deviceContext.GetCurrentTargetsAndViewport();
	bool test_depth=tv&&tv->depthTarget.texture!=nullptr?true:false;
	
	if(bd->is3d)
		bd->framebufferTexture->activateRenderTarget(deviceContext);
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
				int4 clip={(int)clip_min.x, (int)clip_min.y,(int)(clip_max.x-clip_min.x), (int)(clip_max.y-clip_min.y)};
				renderPlatform->SetScissor(deviceContext, clip);

				// Bind texture, Draw
				const ImGui_ImplPlatform_TextureView* texture_srv = (ImGui_ImplPlatform_TextureView*)pcmd->GetTexID();
				renderPlatform->SetTexture(deviceContext,bd->effect->GetShaderResource("texture0"),(Texture*)texture_srv->texture,texture_srv->slice,texture_srv->mip);
				renderPlatform->ApplyPass(deviceContext, bd->effectPass_noDepth);
				bd->pInputLayout->Apply(deviceContext);
				renderPlatform->DrawIndexed(deviceContext,pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset);
				bd->pInputLayout->Unapply(deviceContext);
				renderPlatform->UnapplyPass(deviceContext );
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
	renderPlatform->SetScissor(deviceContext, old_scissor);
	if(bd->is3d)
	{
		bd->framebufferTexture->deactivateRenderTarget(deviceContext);
		bd->constantBuffer.ProjectionMatrix= modelViewProj;
		bd->renderPlatform->SetConstantBuffer(deviceContext, &bd->constantBuffer);
		// now draw it to screen.
		renderPlatform->SetTexture(deviceContext,bd->effect->GetShaderResource("texture0"),bd->framebufferTexture);
		renderPlatform->ApplyPass(deviceContext, test_depth?bd->effectPass_placeIn3D_testDepth:bd->effectPass_placeIn3D_noDepth);
		bd->pInputLayout->Apply(deviceContext);
		renderPlatform->DrawQuad(deviceContext);
		bd->pInputLayout->Unapply(deviceContext);
		renderPlatform->UnapplyPass(deviceContext );
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
	bd->fontTexture=bd->renderPlatform->CreateTexture("imgui_font");
	bd->fontTexture->ensureTexture2DSizeAndFormat(bd->renderPlatform,width,height,1,PixelFormat::RGBA_8_UNORM);
	bd->fontTextureView.texture=bd->fontTexture;
	bd->textureUploaded=false;
	// Store our identifier
	io.Fonts->SetTexID((ImTextureID)&bd->fontTextureView);
}

bool	ImGui_ImplPlatform_CreateDeviceObjects()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if (!bd->renderPlatform)
		return false;
	if (bd->fontTexture)
		ImGui_ImplPlatform_InvalidateDeviceObjects();

	crossplatform::LayoutDesc local_layout[] =
	{
		{ "POSITION", 0, crossplatform::RG_32_FLOAT,	0, (uint32_t)IM_OFFSETOF(ImDrawVert, pos), false, 0 },
		{ "TEXCOORD", 0, crossplatform::RG_32_FLOAT,	0, (uint32_t)IM_OFFSETOF(ImDrawVert, uv),  false, 0 },
		{ "COLOR"   , 0, crossplatform::RGBA_8_UNORM,	0, (uint32_t)IM_OFFSETOF(ImDrawVert, col), false, 0 },
	};
	if (!bd->pInputLayout)
		bd->pInputLayout=bd->renderPlatform->CreateLayout(3,local_layout,true);
#if 1
	if (!bd->effect)
		bd->effect= bd->renderPlatform->CreateEffect("imgui");
	bd->effectPass_testDepth=bd->effect->GetTechniqueByName("layout_in_2d")->GetPass("test_depth");
	bd->effectPass_noDepth=bd->effect->GetTechniqueByName("layout_in_2d")->GetPass("no_depth");
	
	bd->effectPass_placeIn3D_testDepth=bd->effect->GetTechniqueByName("place_in_3d")->GetPass("test_depth");
	bd->effectPass_placeIn3D_noDepth=bd->effect->GetTechniqueByName("place_in_3d")->GetPass("no_depth");
	
	if (!bd->fontTexture)
		ImGui_ImplPlatform_CreateFontsTexture();
	bd->constantBuffer.RestoreDeviceObjects(bd->renderPlatform);
#endif
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
	if (bd->fontTexture)	
	{
		bd->fontTexture->InvalidateDeviceObjects();
		ImGui::GetIO().Fonts->SetTexID(NULL);
		// We copied data->pFontTextureView to io.Fonts->TexID so let's clear that as well.
	}
	SAFE_DELETE(bd->pIB);
	SAFE_DELETE(bd->pVB);
	SAFE_DELETE(bd->pBlendState);

	bd->constantBuffer.InvalidateDeviceObjects();
	SAFE_DELETE(bd->pInputLayout);
	SAFE_DELETE(bd->effect);
	SAFE_DELETE(bd->framebufferTexture);
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
	bd->effectPass_testDepth=bd->effect->GetTechniqueByName("layout_in_2d")->GetPass("test_depth");
	bd->effectPass_noDepth=bd->effect->GetTechniqueByName("layout_in_2d")->GetPass("no_depth");
	
	bd->effectPass_placeIn3D_testDepth=bd->effect->GetTechniqueByName("place_in_3d")->GetPass("test_depth");
	bd->effectPass_placeIn3D_noDepth=bd->effect->GetTechniqueByName("place_in_3d")->GetPass("no_depth");
}

bool	ImGui_ImplPlatform_Init(platform::crossplatform::RenderPlatform* r)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if (bd)
		return true;
	ImGuiIO& io = ImGui::GetIO();
	IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");

	// Setup backend capabilities flags
	bd = IM_NEW(ImGui_ImplPlatform_Data)();
	io.BackendRendererUserData = (void*)bd;
	io.BackendRendererName = "imgui_impl_platform";
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
	ImGui_ImplPlatform_InvalidateDeviceObjects();
	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererName = NULL;
	io.BackendRendererUserData = NULL;
	IM_DELETE(bd);
	bd=nullptr;
}
void ImGui_ImplPlatform_SetFrame(float w_m,float az,float tilt,vec3 c)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	IM_ASSERT(bd != NULL && "Did you call ImGui_ImplPlatform_Init()?");
	if(bd->width_m!=w_m||bd->azimuth!=az||bd->tilt!=tilt||bd->centre!=c)
	{
		bd->width_m=w_m;
		bd->azimuth = az;
		bd->tilt = tilt;
		bd->centre=c;
		bd->matrices_ticker++;
	}
}

void ImGui_ImplPlatform_NewFrame(bool in3d,int ui_pixel_width,int ui_pixel_height,const float *pos, float az, float tilt,float width_m)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	IM_ASSERT(bd != NULL && "Did you call ImGui_ImplPlatform_Init()?");
	
	bd->is3d=in3d;
	static vec3 zero={0,0,0};
	vec3 ctr=zero;
	if(pos)
		ctr=pos;
	ImGui_ImplPlatform_SetFrame(width_m,az,tilt,ctr);
	if (!bd->fontTexture)
		ImGui_ImplPlatform_CreateDeviceObjects();

	// In 3d this takes the place of the CPU platform's NewFrame
	if(in3d)
	{
		ImGuiIO& io = ImGui::GetIO();
		// Setup display size (every frame to accommodate for window resizing)
		io.DisplaySize = ImVec2((float)ui_pixel_width, (float)ui_pixel_height);
	// Override what win32 did with this
		//ImGui_ImplPlatform_Update3DMousePos();
	}
}


void ImGui_ImplPlatform_SetMousePos(int x, int y)//, int W, int H)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if(!bd)
		return;
	bd->mouse={x,y};
	//bd->screen={W,H};
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos			= ImVec2(bd->mouse.x,bd->mouse.y);
}
void ImGui_ImplPlatform_SetMouseDown(int button, bool value)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDown[button]=value;
}


void ImGui_ImplPlatform_Update3DMousePos()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if(!bd->is3d)
		return;
	if(!bd->screen.x||!bd->screen.y)
		return;
	ImGuiIO& io = ImGui::GetIO();

	const ImVec2 mouse_pos_prev = io.MousePos;

	// Set Dear ImGui mouse position from OS position

	vec3 diff			= bd->centre - bd->view_pos;
	// from the Windows mouse pos obtain a direction.
	vec4 view_dir		= { (2.0f*bd->mouse.x - bd->screen.x) / float(bd->screen.x),(bd->screen.y - 2.0f * bd->mouse.y) / float(bd->screen.y),1.0f,1.0f};
	// Transform this into a worldspace direction.
	vec3 view_w			= (view_dir*bd->invViewProj).xyz;
	view_w				= normalize(view_w);
	// Take the vector in this direction from view_pos, Intersect it with the UI surface.
	float dist			= dot(diff, bd->normal)/dot(view_w,bd->normal);
	vec3 intersection_w	= bd->view_pos+view_w*dist;

	// Transform the intersection point into object-space then into UI-space.
	mat4 m				= bd->world_to_imgui;
	vec3 client_pos		= (m*vec4(intersection_w,1.0f)).xyz;
	// Finally, set this as the mouse pos.
	io.MousePos			= ImVec2(client_pos.x,client_pos.y);
}

void ImGui_ImplPlatform_Get3DTouchClientPos( std::vector<vec3>& client_press)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	client_press=bd->client_press;
}

void ImGui_ImplPlatform_Update3DTouchPos(const std::vector<vec4> &position_press)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	if (!bd->is3d)
		return;
	ImGuiIO& io = ImGui::GetIO();

	const ImVec2 mouse_pos_prev = io.MousePos;
	static ImVec2 last_pos = { 0,0 };
	static float control_area_thickness = 0.05f;
	static float min_z =-0.05f;
	static float max_z = 0.0f;
	static float control_surface_thickness = 0.0f;
	static float hysteresis_thickness = 0.02f;
	static float release_thickness = 0.04f;
	float lowest = max_z;
	bool any = false;
	bool all_released=true;
	/*static std::vector<bool> clicked;
	if (clicked.size() != position_press.size())
	{
		clicked.resize(position_press.size());
		for (size_t i = 0; i < position_press.size(); i++)
			clicked[i] = false;
	}*/
			bd->imgui_to_local=
			{
				 bd->X,		0,		0,		-bd->width_m / 2.0f,
				 0,		-bd->Y,		0,		bd->height_m / 2.0f,
				 0,		0,		1.0f,	0,
				 0,		0,		0,		1.0f
			};
	bd->client_press.resize(position_press.size());
	//bd->client_press[position_press.size()]=(bd->world_to_local*vec4(bd->centre,1.0f)).xyz;
	for (size_t i = 0; i < position_press.size(); i++)
	{
		// Set Dear ImGui mouse position from OS position
		vec3 pos = position_press[i].xyz;
		float press = position_press[i].w;
		vec3 rel=pos-bd->centre;
		vec3 local=(bd->relative_to_local*vec4(rel,1.0f)).xyz;
		// resolve position onto the control surface:
		//client_pr=(bd->world_to_local*vec4(pos,1.0f)).xyz;
		vec3 client_pos =(bd->world_to_imgui*vec4(pos,1.0f)).xyz;
		// Record the vertical position:
		bd->client_press[i].z=client_pos.z;
		//if (!clicked[i])
		{
			if (client_pos.z < min_z || client_pos.z > max_z)
				continue;
			if (client_pos.x<0 || client_pos.y<0 || client_pos.x>io.DisplaySize.x || client_pos.y>io.DisplaySize.y)
				continue;
		}
		if (client_pos.z <= hysteresis_thickness)
		{
			any = true;
		}
		if (client_pos.z <= release_thickness)
		{
			all_released = false;
		}
		// Go from unpressed to pressed.
		if(!io.MouseDown[0])
		{
			// record mouseDown position, ONLY on transition.
			bd->client_press[i].x=client_pos.x;
			bd->client_press[i].y=client_pos.y;
			if(client_pos.z <= control_surface_thickness)
			{
				io.MouseDown[0] = true;
			}
		}
		if (client_pos.z < lowest)
		{
			lowest = client_pos.z;
			// Finally, set this as the mouse pos.
			io.MousePos = ImVec2(bd->client_press[i].x, bd->client_press[i].y);
		}
		/*if (clicked[i] && client_pos.z > control_area_thickness)
		{
			io.MousePos = ImVec2(client_pos.x, client_pos.y);
		}*/
	}
	if (io.MouseDown[0] && all_released)
	{
		io.MouseDown[0] = false;
		//io.MousePos= last_pos;
	}
	last_pos = io.MousePos;
}

void ImGui_ImplPlatform_DebugInfo()
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	ImGuiIO& io = ImGui::GetIO();
	ImGui::Text("mouse: %d %d gui %1.0f %1.0f", bd->mouse.x,bd->mouse.y, io.MousePos.x, io.MousePos.y);
}


//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the backend to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------
platform::crossplatform::DisplaySurfaceManagerInterface *displaySurfaceManagerInterface=nullptr;
platform::crossplatform::PlatformRendererInterface *platformRendererInterface=nullptr;
// Helper structure we store in the void* RenderUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGui_ImplPlatform_ViewportData
{
	int viewId=-1;

};

void ImGui_ImplPlatform_SetDisplaySurfaceMaangerAndPlatformRenderer(platform::crossplatform::DisplaySurfaceManagerInterface *d,
																		platform::crossplatform::PlatformRendererInterface *p)
{
	displaySurfaceManagerInterface=d;
	platformRendererInterface=p;
}

static void ImGui_ImplPlatform_CreateWindow(ImGuiViewport* viewport)
{
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	ImGui_ImplPlatform_ViewportData* vd = IM_NEW(ImGui_ImplPlatform_ViewportData)();
	viewport->RendererUserData = vd;
	// PlatformHandleRaw should always be a HWND, whereas PlatformHandle might be a higher-level handle (e.g. GLFWWindow*, SDL_Window*).
	// Some backend will leave PlatformHandleRaw NULL, in which case we assume PlatformHandle will contain the HWND for Win32 and void* for other platfoms.
	// So we will use Platform's cp_hwnd type.
	cp_hwnd hwnd = viewport->PlatformHandleRaw ? (cp_hwnd)viewport->PlatformHandleRaw : (cp_hwnd)viewport->PlatformHandle;
	IM_ASSERT(hwnd != 0);

	// All should be handled from within the displaySurface manager.
	displaySurfaceManagerInterface->AddWindow(hwnd,crossplatform::PixelFormat::RGBA_8_UNORM);
	static int viewId=2;
	displaySurfaceManagerInterface->SetRenderer(hwnd,platformRendererInterface,viewId++);
	vd->viewId=displaySurfaceManagerInterface->GetViewId(hwnd);

}

static void ImGui_ImplPlatform_DestroyWindow(ImGuiViewport* viewport)
{
	cp_hwnd hwnd = viewport->PlatformHandleRaw ? (cp_hwnd)viewport->PlatformHandleRaw : (cp_hwnd)viewport->PlatformHandle;
	IM_ASSERT(hwnd != 0);
	displaySurfaceManagerInterface->RemoveWindow(hwnd);
	// The main viewport (owned by the application) will always have RendererUserData == NULL since we didn't create the data for it.
	if (ImGui_ImplPlatform_ViewportData* vd = (ImGui_ImplPlatform_ViewportData*)viewport->RendererUserData)
	{
		IM_DELETE(vd);
	}
	viewport->RendererUserData = NULL;
}

static void ImGui_ImplPlatform_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
	cp_hwnd hwnd = viewport->PlatformHandleRaw ? (cp_hwnd)viewport->PlatformHandleRaw : (cp_hwnd)viewport->PlatformHandle;
	IM_ASSERT(hwnd != 0);
	displaySurfaceManagerInterface->ResizeSwapChain(hwnd);
	/*
	ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	ImGui_ImplPlatform_ViewportData* vd = (ImGui_ImplPlatform_ViewportData*)viewport->RendererUserData;
	if (vd->RTView)
	{
		vd->RTView->Release();
		vd->RTView = NULL;
	}
	if (vd->SwapChain)
	{
		ID3D11Texture2D* pBackBuffer = NULL;
		vd->SwapChain->ResizeBuffers(0, (UINT)size.x, (UINT)size.y, DXGI_FORMAT_UNKNOWN, 0);
		vd->SwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		if (pBackBuffer == NULL) { fprintf(stderr, "ImGui_ImplPlatform_SetWindowSize() failed creating buffers.\n"); return; }
		bd->pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &vd->RTView);
		pBackBuffer->Release();
	}*/
}

std::map<int,ImDrawData*> drawData;
ImDrawData *ImGui_ImplPlatform_GetDrawData(int view_id)
{
	const auto &i=drawData.find(view_id);
	if(i==drawData.end())
		return nullptr;
	return i->second;
}

static void ImGui_ImplPlatform_RenderWindow(ImGuiViewport* viewport, void*)
{
	cp_hwnd hwnd = viewport->PlatformHandleRaw ? (cp_hwnd)viewport->PlatformHandleRaw : (cp_hwnd)viewport->PlatformHandle;
	IM_ASSERT(hwnd != 0);
	displaySurfaceManagerInterface->Render(hwnd);
	ImGui_ImplPlatform_ViewportData* vd = (ImGui_ImplPlatform_ViewportData*)viewport->RendererUserData;
	drawData[vd->viewId]=viewport->DrawData;
 /*   ImGui_ImplPlatform_Data* bd = ImGui_ImplPlatform_GetBackendData();
	ImGui_ImplPlatform_ViewportData* vd = (ImGui_ImplPlatform_ViewportData*)viewport->RendererUserData;
	ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	bd->pd3dDeviceContext->OMSetRenderTargets(1, &vd->RTView, NULL);
	if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
		bd->pd3dDeviceContext->ClearRenderTargetView(vd->RTView, (float*)&clear_color);
	ImGui_ImplPlatform_RenderDrawData(viewport->DrawData);*/
}

static void ImGui_ImplPlatform_SwapBuffers(ImGuiViewport* viewport, void*)
{
	ImGui_ImplPlatform_ViewportData* vd = (ImGui_ImplPlatform_ViewportData*)viewport->RendererUserData;
   // vd->SwapChain->Present(0, 0); // Present without vsync
}

void ImGui_ImplPlatform_InitPlatformInterface()
{
	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Renderer_CreateWindow = ImGui_ImplPlatform_CreateWindow;
	platform_io.Renderer_DestroyWindow = ImGui_ImplPlatform_DestroyWindow;
	platform_io.Renderer_SetWindowSize = ImGui_ImplPlatform_SetWindowSize;
	platform_io.Renderer_RenderWindow = ImGui_ImplPlatform_RenderWindow;
	platform_io.Renderer_SwapBuffers = ImGui_ImplPlatform_SwapBuffers;
}

void ImGui_ImplPlatform_ShutdownPlatformInterface()
{
	ImGui::DestroyPlatformWindows();
}
