
#include "Text3DRenderer.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include "Platform/CrossPlatform/Macros.h"
#include <algorithm>
#if PLATFORM_USE_FREETYPE
#include <ft2build.h>
#include <Platform/External/magic_enum/include/magic_enum.hpp>
#include <Platform/Core/FileLoader.h>
#include FT_FREETYPE_H
#endif
using namespace platform;
using namespace crossplatform;

#if PLATFORM_USE_FREETYPE
FT_Library  library;
#endif

Text3DRenderer::FontIndex defaultFontIndices1[]={
{0.0f		,0.0f			,5},
{0.0f		,0.000976563f	,1},
{0.00195313f,0.00488281f	,3},
{0.00585938f,0.0136719f		,8},
{0.0146484f	,0.0195313f		,5},
{0.0205078f	,0.0302734f		,10},
{0.03125f	,0.0390625f		,8},
{0.0400391f	,0.0410156f		,1},
{0.0419922f	,0.0449219f		,3},
{0.0458984f	,0.0488281f		,3},
{0.0498047f	,0.0546875f		,5},
{0.0556641f	,0.0625f		,7},
{0.0634766f	,0.0644531f		,1},
{0.0654297f	,0.0683594f		,3},
{0.0693359f	,0.0703125f		,1},
{0.0712891f	,0.0751953f		,4},
{0.0761719f	,0.0820313f		,6},
{0.0830078f	,0.0859375f		,3},
{0.0869141f	,0.0927734f		,6},
{0.09375f	,0.0996094f		,6},
{0.100586f	,0.106445f		,6},
{0.107422f	,0.113281f		,6},
{0.114258f	,0.120117f		,6},
{0.121094f	,0.126953f		,6},
{0.12793f	,0.133789f		,6},
{0.134766f	,0.140625f		,6},
{0.141602f	,0.142578f		,1},
{0.143555f	,0.144531f		,1},
{0.145508f	,0.151367f		,6},
{0.152344f	,0.15918f		,7},
{0.160156f	,0.166016f		,6},
{0.166992f	,0.171875f		,5},
{0.172852f	,0.18457f		,12},
{0.185547f	,0.194336f		,9},
{0.195313f	,0.202148f		,7},
{0.203125f	,0.209961f		,7},
{0.210938f	,0.217773f		,7},
{0.21875f	,0.225586f		,7},
{0.226563f	,0.232422f		,6},
{0.233398f	,0.241211f		,8},
{0.242188f	,0.249023f		,7},
{0.25f		,0.250977f		,1},
{0.251953f	,0.256836f		,5},
{0.257813f	,0.265625f		,8},
{0.266602f	,0.272461f		,6},
{0.273438f	,0.282227f		,9},
{0.283203f	,0.290039f		,7},
{0.291016f	,0.298828f		,8},
{0.299805f	,0.306641f		,7},
{0.307617f	,0.31543f		,8},
{0.316406f	,0.323242f		,7},
{0.324219f	,0.331055f		,7},
{0.332031f	,0.338867f		,7},
{0.339844f	,0.34668f		,7},
{0.347656f	,0.356445f		,9},
{0.357422f	,0.370117f		,13},
{0.371094f	,0.37793f		,7},
{0.378906f	,0.385742f		,7},
{0.386719f	,0.393555f		,7},
{0.394531f	,0.396484f		,2},
{0.397461f	,0.401367f		,4},
{0.402344f	,0.404297f		,2},
{0.405273f	,0.410156f		,5},
{0.411133f	,0.417969f		,7},
{0.418945f	,0.420898f		,2},
{0.421875f	,0.426758f		,5},
{0.427734f	,0.432617f		,5},
{0.433594f	,0.438477f		,5},
{0.439453f	,0.444336f		,5},
{0.445313f	,0.450195f		,5},
{0.451172f	,0.455078f		,4},
{0.456055f	,0.460938f		,5},
{0.461914f	,0.466797f		,5},
{0.467773f	,0.46875f		,1},
{0.469727f	,0.472656f		,3},
{0.473633f	,0.478516f		,5},
{0.479492f	,0.480469f		,1},
{0.481445f	,0.490234f		,9},
{0.491211f	,0.496094f		,5},
{0.49707f	,0.501953f		,5},
{0.50293f	,0.507813f		,5},
{0.508789f	,0.513672f		,5},
{0.514648f	,0.517578f		,3},
{0.518555f	,0.523438f		,5},
{0.524414f	,0.527344f		,3},
{0.52832f	,0.533203f		,5},
{0.53418f	,0.539063f		,5},
{0.540039f	,0.548828f		,9},
{0.549805f	,0.554688f		,5},
{0.555664f	,0.560547f		,5},
{0.561523f	,0.566406f		,5},
{0.567383f	,0.570313f		,3},
{0.571289f	,0.572266f		,1},
{0.573242f	,0.576172f		,3},
{0.577148f	,0.583984f		,7},
};
static int max_chars=1500;

Text3DRenderer::Text3DRenderer()
{
#if PLATFORM_USE_FREETYPE
	FT_Error error = FT_Init_FreeType( &library );
	if ( error )
	{
		SIMUL_CERR<<"Error"<<error<<" in FT_Init_FreeType.\n";
	}

	error = FT_Init_FreeType( &library );
	if ( error )
	{
		SIMUL_CERR<<"Error"<<error<<" in FT_Init_FreeType.\n";
	}
	#endif
}

Text3DRenderer::~Text3DRenderer()
{
	InvalidateDeviceObjects();
}

void Text3DRenderer::PushFontPath(const char *p)
{
	fontPaths.push_back(p);
}

#if PLATFORM_USE_FREETYPE
	FT_Face     face;      /* handle to face object */
	#endif
void Text3DRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	ERRNO_CHECK
	renderPlatform=r;
	
#if PLATFORM_USE_FREETYPE
	{
		platform::core::FileLoader *fileLoader=platform::core::FileLoader::GetFileLoader();
		std::string filename = fileLoader->FindFileInPathStack("Exo2-SemiBold.ttf", fontPaths);
		if(filename.length()>0)
		{
			void *ttf_data=nullptr;
			unsigned ttf_size=0;
			fileLoader->AcquireFileContents(ttf_data,ttf_size,filename.c_str(),false);
			FT_Error error = FT_New_Memory_Face( library,
									(const FT_Byte*)ttf_data,    /* first byte in memory */
									ttf_size,					 /* size in bytes        */
									0,							 /* face_index           */
									&face );
			if ( error == FT_Err_Unknown_File_Format )
			{
				SIMUL_CERR<<"FT_New_Face: font file could be opened and read, but its font format is unsupported\n";
			}
			else if ( error )
			{
			  //another error code means that the font file could not
			  //be opened or read, or that it is broken...
				SIMUL_CERR<<"Error"<<error<<" in FT_New_Face.\n";
			}
			
		}
	}
	#endif

	constantBuffer.InvalidateDeviceObjects();
	ERRNO_BREAK
	constantBuffer.RestoreDeviceObjects(renderPlatform);
	ERRNO_BREAK
	LoadShaders();
	ERRNO_BREAK
	SAFE_DELETE(font_texture);
	font_texture = renderPlatform->CreateTexture("Font16-11.png");
	fontWidth = 11;
	if (font_texture)
	{
		defaultTextHeight = font_texture->length;
		fontIndices = new FontIndex[128];
		for (int i = 0; i < 128; i++)
		{
			FontIndex &f = fontIndices[i];
			f.pixel_width = 9;
			if (i)
			{
				f.x = float((i - 1) * 11) / float(font_texture->width);
				f.w = f.x+float(f.pixel_width) / float(font_texture->width);
			}
			else
				f.x = f.w = 0.f;
		}
	}
	else
	{
		fontIndices = defaultFontIndices1;
		font_texture = renderPlatform->CreateTexture("Font16.png");
		defaultTextHeight=font_texture->length;
		fontWidth = 0;
	}
}

void Text3DRenderer::InvalidateDeviceObjects()
{
	for(auto &f:fontChars)
	{
		f.second.InvalidateDeviceObjects();
	}
	fontChars.clear();
	constantBuffer.InvalidateDeviceObjects();
	effect = nullptr;
	SAFE_DELETE(font_texture);
	renderPlatform=NULL;
	if (fontWidth)
		delete[] fontIndices;
	fontIndices = nullptr;
	fontWidth = 0;
}

void Text3DRenderer::RecompileShaders()
{
	if (!renderPlatform)
		return;
	renderPlatform->ScheduleRecompileEffects({"font"},[this](){recompile=true;});
}

void Text3DRenderer::LoadShaders()
{
	recompile = false;
	ERRNO_CHECK
	effect = renderPlatform->GetOrCreateEffect("font");
	ERRNO_CHECK
	backgTech	=effect->GetTechniqueByName("backg");
	textTech	=effect->GetTechniqueByName("text");
	textureResource	=effect->GetShaderResource("fontTexture");
	_fontChars		=effect->GetShaderResource("fontChars");
}
int Text3DRenderer::GetDefaultTextHeight() const
{
	return defaultTextHeight;
}

int Text3DRenderer::Render(GraphicsDeviceContext &deviceContext,float x0,float y,float screen_width,float screen_height,const char *txt,const float *clr,const float *bck,bool mirrorY)
{
	bool supportShaderViewID = renderPlatform->GetType() == crossplatform::RenderPlatformType::D3D11 ? false : true;
	int passIndex = supportShaderViewID ? 0 : 1;

	if (recompile)
		LoadShaders();
	float transp[]={0.f,0.f,0.f,0.f};
	float white[]={1.f,1.f,1.f,1.f};
	if(!clr)
		clr=white;
	if(!bck)
		bck=transp;
	static float fontScale=1.0f;
	constantBuffer.colour		=vec4(clr);
	constantBuffer.background	=vec4(bck);
	// Calc width and draw background:
	int w=0;
	int maxw = 0;
	int lines = 1;
	for(int i=0;i<max_chars;i++)
	{
		if(txt[i]==0)
			break;
		if (txt[i] == '\n')
		{
			w = 0;
			lines++;
			continue;
		}
		int idx=(int)txt[i]-32;
		if(idx<0||idx>100)
			continue;
		const FontIndex &f = fontIndices[idx];
		w += f.pixel_width*int(fontScale) + 1;
		maxw = std::max(w, maxw);
	}
	float ht=fontScale*float(GetDefaultTextHeight());
	//renderPlatform->SetStandardRenderState(deviceContext,crossplatform::STANDARD_ALPHA_BLENDING);
	constantBuffer.background_rect[0]		=vec4(2.0f*x0/screen_width-1.f,1.f-2.0f*(y+ht*lines)/screen_height,2.0f*(float)maxw/screen_width,2.0f*ht*lines/screen_height);
	float y_offset=-2.0f*ht/screen_height;
	if(mirrorY)
	{
		y_offset*=-1.0f;
		constantBuffer.background_rect[0].y=-constantBuffer.background_rect[0].y;
		constantBuffer.background_rect[0].w*=-1.0f;
	}
	renderPlatform->SetConstantBuffer(deviceContext, &constantBuffer);
	if(constantBuffer.background.w>0.0f)
	{
		effect->Apply(deviceContext,backgTech, passIndex);
		renderPlatform->DrawQuad(deviceContext);
		effect->Unapply(deviceContext);
	}
	float ypixel=1.0f/ screen_height;
	float ytexel=1.0f/ GetDefaultTextHeight();
	constantBuffer.background_rect[0] = vec4(0, 1.f - 2.0f*(y + ht)* ypixel, 0, 2.0f*(ht+1.0f)* ypixel);
	uint& n = constantBuffer.numChars;
	n=0;
	float u = 1024.f / font_texture->width;
	crossplatform::StructuredBuffer<FontChar> &f=fontChars[deviceContext.platform_context];
	if(max_chars>f.count)
		f.RestoreDeviceObjects(renderPlatform,max_chars,false,false,nullptr,"fontChars");
	FontChar *charList=f.GetBuffer(deviceContext);
	float x = x0;
	for(int i=0;i<f.count;i++)
	{
		if(txt[i]==0)
			break;
		if (txt[i] == '\n')
		{
			x = x0;
			constantBuffer.background_rect[0].y += y_offset;
			continue;
		}
		int idx=(int)txt[i]-32;
		if(idx<0||idx>94)
			continue;
		const FontIndex &fi=fontIndices[idx];
		if(idx>0)
		{
			if (charList != nullptr)
			{
				FontChar& c = charList[n];
				c.text_rect = constantBuffer.background_rect[0];
				c.text_rect.x = 2.0f * x / screen_width - 1.f;
				c.text_rect.z = 2.0f * (float)fi.pixel_width * fontScale / screen_width;
				c.texc = vec4(fi.x * u, 0.0f, (fi.w - fi.x) * u, 1.0f+ytexel);
			}
			n++;
		}
		x+=fi.pixel_width*fontScale+1;
	}
	constantBuffer.numChars = n;
	if(n>0)
	{
		renderPlatform->SetTexture(deviceContext, textureResource, font_texture);
		effect->Apply(deviceContext,textTech,passIndex);
		renderPlatform->SetConstantBuffer(deviceContext, &constantBuffer);
		renderPlatform->SetVertexBuffers(deviceContext,0,0,nullptr,nullptr);
		f.Apply(deviceContext,_fontChars);
		renderPlatform->SetTopology(deviceContext, Topology::TRIANGLELIST);
		renderPlatform->Draw(deviceContext,6*n,0);
		effect->UnbindTextures(deviceContext);
		effect->Unapply(deviceContext);
	}
	return lines;
}

int Text3DRenderer::Render(MultiviewGraphicsDeviceContext& deviceContext, float* xs, float* ys, float screen_width, float screen_height, const char* txt, const float* clr, const float* bck, bool mirrorY)
{
	bool supportShaderViewID = renderPlatform->GetType() == crossplatform::RenderPlatformType::D3D11 ? false : true;
	int passIndex = supportShaderViewID ? 0 : 1;
	SIMUL_ASSERT_WARN(supportShaderViewID, "Graphics API doesn't support SV_ViewID/gl_ViewIndex in the shader. Falling back to single view rendering.");

	if (recompile)
		LoadShaders();
	float transp[] = { 0.f,0.f,0.f,0.f };
	float white[] = { 1.f,1.f,1.f,1.f };
	if (!clr)
		clr = white;
	if (!bck)
		bck = transp;
	static float fontScale = 1.0f;
	constantBuffer.colour = vec4(clr);
	constantBuffer.background = vec4(bck);
	// Calc width and draw background:
	int w = 0;
	int maxw = 0;
	int lines = 1;
	for (int i = 0; i < max_chars; i++)
	{
		if (txt[i] == 0)
			break;
		if (txt[i] == '\n')
		{
			w = 0;
			lines++;
			continue;
		}
		int idx = (int)txt[i] - 32;
		if (idx < 0 || idx>100)
			continue;
		const FontIndex& f = fontIndices[idx];
		w += f.pixel_width * int(fontScale) + 1;
		maxw = std::max(w, maxw);
	}
	float ht = fontScale * float(GetDefaultTextHeight());
	//renderPlatform->SetStandardRenderState(deviceContext,crossplatform::STANDARD_ALPHA_BLENDING);

	uint n = 0;
	crossplatform::StructuredBuffer<FontChar>& f = fontChars[deviceContext.platform_context];
	FontChar* charList = f.GetBuffer(deviceContext);

	size_t countOfTextBackgroundArray = sizeof(constantBuffer.background_rect) / sizeof(constantBuffer.background_rect[0]);
	size_t viewCount = std::min(countOfTextBackgroundArray, deviceContext.viewStructs.size());
	for (size_t i = 0; i < viewCount; i++)
	{
		vec4& background_rect = constantBuffer.background_rect[i];
		const float& x = xs[i];
		const float& y = ys[i];

		background_rect = vec4(2.0f * x / screen_width - 1.f, 1.f - 2.0f * (y + ht * lines) / screen_height, 2.0f * (float)maxw / screen_width, 2.0f * ht * lines / screen_height);
		float y_offset = -2.0f * ht / screen_height;
		if (mirrorY)
		{
			y_offset *= -1.0f;
			background_rect.y = -background_rect.y;
			background_rect.w *= -1.0f;
		}
		//backgTech is multiview, so only call to render once per N views. 
		//Call on the last iteration, when both background_rect[0] and background_rect1 are set.
		if (constantBuffer.background.w > 0.0f && i == (viewCount - 1)) 
		{
			renderPlatform->SetConstantBuffer(deviceContext, &constantBuffer);
			effect->Apply(deviceContext, backgTech, passIndex);
			renderPlatform->DrawQuad(deviceContext);
			effect->Unapply(deviceContext);
		}
		float ypixel = 1.0f / screen_height;
		float ytexel = 1.0f / GetDefaultTextHeight();
		vec4 text_rect = vec4(0, 1.f - 2.0f * (y + ht) * ypixel, 0, 2.0f * (ht + 1.0f) * ypixel);
		
		float u = 1024.f / font_texture->width;
		
		if (max_chars > f.count)
			f.RestoreDeviceObjects(renderPlatform, max_chars, false, false, nullptr, "fontChars");
		
		float _x = x;
		for (int i = 0; i < f.count; i++)
		{
			if (txt[i] == 0)
				break;
			if (txt[i] == '\n')
			{
				_x = x;
				text_rect.y += y_offset;
				continue;
			}
			int idx = (int)txt[i] - 32;
			if (idx < 0 || idx>94)
				continue;
			const FontIndex& f = fontIndices[idx];
			if (idx > 0)
			{
				if (charList != nullptr)
				{
					FontChar& c = charList[n];
					c.text_rect = text_rect;
					c.text_rect.x = 2.0f * _x / screen_width - 1.f;
					c.text_rect.z = 2.0f * (float)f.pixel_width * fontScale / screen_width;
					c.texc = vec4(f.x * u, 0.0f, (f.w - f.x) * u, 1.0f + ytexel);
				}
				n++;
			}
			_x += f.pixel_width * fontScale + 1;
		}
	}
	n /= static_cast<uint>(viewCount);
	constantBuffer.numChars = n;
	if (n > 0)
	{
		renderPlatform->SetTexture(deviceContext, textureResource, font_texture);
		effect->Apply(deviceContext, textTech, passIndex);
		renderPlatform->SetConstantBuffer(deviceContext, &constantBuffer);
		renderPlatform->SetVertexBuffers(deviceContext, 0, 0, nullptr, nullptr);
		f.Apply(deviceContext, _fontChars);
		renderPlatform->SetTopology(deviceContext, Topology::TRIANGLELIST);
		renderPlatform->Draw(deviceContext, 6 * n, 0);
		effect->UnbindTextures(deviceContext);
		effect->Unapply(deviceContext);
	}
	return lines;
}
