// Copyright (c) 2007-2011 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.cpp Create a DirectX .fx effect and report errors.

#include "CreateEffectDX1x.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Geometry/Orientation.h"
#include "Simul/Base/RuntimeError.h"
#include <tchar.h>
#include "CompileShaderDX1x.h"
#include <string>
typedef std::basic_string<TCHAR> tstring;
tstring shader_path=TEXT("media/hlsl/dx11");
tstring texture_path=TEXT("media/textures");
static DWORD default_effect_flags=0;

#include <vector>
#include <iostream>
#include <assert.h>
#include <fstream>
#include <d3dx9.h>
#include "MacrosDX1x.h"
#include <dxerr.h>

#pragma comment(lib,"d3dx9.lib")
#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"d3dx11.lib")
#pragma comment(lib,"Effects11.lib")
#pragma comment(lib,"dxerr.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"d3dcompiler.lib")

// winmm.lib comctl32.lib
static bool shader_path_set	=false;
static bool texture_path_set=false;
static bool pipe_compiler_output=false;

ID3D1xDevice		*m_pd3dDevice		=NULL;
ID3D1xDeviceContext	*m_pImmediateContext=NULL;
using namespace simul;
using namespace dx11;
namespace simul
{
	namespace dx1x_namespace
	{
		tstring tstring_of(const char *c)
		{
			tstring t;
		#ifdef UNICODE
			// tstring and TEXT cater for the confusion between wide and regular strings.
			t.resize(strlen(c),L' '); // Make room for characters
			// Copy string to wstring.
			std::copy(c,c+strlen(c),t.begin());
		#else
			t=c;
		#endif
			return t;
		}
		void GetCameraPosVector(D3DXMATRIX &view,bool y_vertical,float *dcam_pos,float *view_dir)
		{
			D3DXMATRIX tmp1;
			D3DXMatrixInverse(&tmp1,NULL,&view);
			
			dcam_pos[0]=tmp1._41;
			dcam_pos[1]=tmp1._42;
			dcam_pos[2]=tmp1._43;
			if(view_dir)
			{
				if(y_vertical)
				{
					view_dir[0]=view._13;
					view_dir[1]=view._23;
					view_dir[2]=view._33;
				}
				else
				{
					view_dir[0]=-view._13;
					view_dir[1]=-view._23;
					view_dir[2]=-view._33;
				}
			}
		}

		const float *GetCameraPosVector(D3DXMATRIX &view,bool y_vertical)
		{
			static float cam_pos[4],view_dir[4];
			GetCameraPosVector(view,y_vertical,cam_pos,view_dir);
			return cam_pos;
		}
		void PipeCompilerOutput(bool p)
		{
			pipe_compiler_output=p;
		}
		void SetShaderPath(const char *path)
		{
			shader_path=tstring_of(path);
			shader_path+=_T("/");
			shader_path_set=true;
		}
		void SetTexturePath(const char *path)
		{
			texture_path=tstring_of(path);
			texture_path+=_T("/");
			texture_path_set=true;
		}
		void SetDevice(ID3D1xDevice* dev)
		{
			m_pd3dDevice=dev;
		#ifdef DX10
			m_pImmediateContext=dev;
		#else
			m_pd3dDevice->GetImmediateContext(&m_pImmediateContext);
		#endif
			UtilityRenderer::RestoreDeviceObjects(dev);
		}
		void UnsetDevice()
		{
			UtilityRenderer::InvalidateDeviceObjects();
			SAFE_RELEASE(m_pImmediateContext);
			m_pd3dDevice=NULL;
		}
		void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj)
		{
			//set up matrices
			D3DXMATRIX tmp1, tmp2;
			D3DXMatrixInverse(&tmp1,NULL,&view);
			D3DXMatrixMultiply(&tmp1, &world,&view);
			D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
			D3DXMatrixTranspose(wvp,&tmp2);
		}
	}
}
int UtilityRenderer::instance_count=0;
int UtilityRenderer::screen_width=0;
int UtilityRenderer::screen_height=0;
D3DXMATRIX UtilityRenderer::view,UtilityRenderer::proj;
ID3D1xEffect *UtilityRenderer::m_pDebugEffect=NULL;
UtilityRenderer utilityRenderer;

UtilityRenderer::UtilityRenderer()
{
	instance_count++;
}

UtilityRenderer::~UtilityRenderer()
{
	InvalidateDeviceObjects();
}

void UtilityRenderer::RestoreDeviceObjects(void *m_pd3dDevice)
{
	RecompileShaders();
}

void UtilityRenderer::RecompileShaders()
{
	SAFE_RELEASE(m_pDebugEffect);
	CreateEffect(m_pd3dDevice,&m_pDebugEffect,_T("simul_debug.fx"));
}

void UtilityRenderer::InvalidateDeviceObjects()
{
	SAFE_RELEASE(m_pDebugEffect);
}

void UtilityRenderer::SetMatrices(D3DXMATRIX v,D3DXMATRIX p)
{
	view=v;
	proj=p;
}

void UtilityRenderer::SetScreenSize(int w,int h)
{
	screen_width=w;
	screen_height=h;
}

void UtilityRenderer::PrintAt3dPos(const float *p,const char *text,const float* colr,int offsetx,int offsety)
{
}

void UtilityRenderer::DrawLines(VertexXyzRgba *vertices,int vertex_count,bool strip)
{
	PIXWrapper(0xFF0000FF,"DrawLines")
	{
	HRESULT hr=S_OK;
	D3DXMATRIX world, tmp1, tmp2;
	D3DXMatrixIdentity(&world);
	ID3D1xEffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("simul_direct");
	ID3D1xEffectMatrixVariable*	worldViewProj=m_pDebugEffect->GetVariableByName("worldViewProj")->AsMatrix();

	D3DXMATRIX wvp;
	MakeWorldViewProjMatrix(&wvp,world,view,proj);
	worldViewProj->SetMatrix(&wvp._11);
	
	ID3D1xBuffer *					vertexBuffer=NULL;
#if 1
	// Create the vertex buffer:
	D3D1x_BUFFER_DESC desc=
	{
        vertex_count*sizeof(VertexXyzRgba),
        D3D1x_USAGE_DYNAMIC,
        D3D1x_BIND_VERTEX_BUFFER,
        D3D1x_CPU_ACCESS_WRITE,
        0
	};
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(VertexXyzRgba);
	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&vertexBuffer);

	const D3D1x_INPUT_ELEMENT_DESC decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
    };
	D3D1x_PASS_DESC PassDesc;
	ID3D1xEffectPass *pass=tech->GetPassByIndex(0);
	hr=pass->GetDesc(&PassDesc);

	ID3D1xInputLayout*				m_pVtxDecl=NULL;
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
	m_pImmediateContext->IASetPrimitiveTopology(strip?D3D1x_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D1x_PRIMITIVE_TOPOLOGY_LINELIST);
	UINT stride = sizeof(VertexXyzRgba);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = stride;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,				// the first input slot for binding
												1,				// the number of buffers in the array
												&vertexBuffer,	// the array of vertex buffers
												&stride,		// array of stride values, one for each buffer
												&offset);		// array of 
	hr=ApplyPass(tech->GetPassByIndex(0));
	m_pImmediateContext->Draw(vertex_count,0);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(m_pVtxDecl);
#endif
	}
}


ID3D1xShaderResourceView* simul::dx1x_namespace::LoadTexture(const TCHAR *filename)
{
	ID3D1xShaderResourceView* tex=NULL;
	D3DX1x_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory( &loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO) );
	loadInfo.BindFlags = D3D1x_BIND_SHADER_RESOURCE;
	loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	loadInfo.MipLevels=0;
	HRESULT hr=D3DX11CreateShaderResourceViewFromFile(
										m_pd3dDevice,
										(texture_path+filename).c_str(),
										&loadInfo,
										NULL,
										&tex,
										&hr);
	return tex;
}

ID3D1xShaderResourceView* simul::dx1x_namespace::LoadTexture(const char *filename)
{
	return LoadTexture(tstring_of(filename).c_str());
}

struct d3dMacro
{
	std::string name;
	std::string define;
};
#ifdef DX11
HRESULT WINAPI D3DX11CreateEffectFromBinaryFile(const TCHAR *filename, UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	HRESULT hr=(HRESULT)(-1);
	std::string compiled_filename=simul::base::WStringToString(filename)+"o";
	std::ifstream ifs(compiled_filename.c_str(),std::ios_base::binary);
	for(int i=0;i<10000&&!ifs.good();i++);
	if(ifs.good())
	{
		ifs.seekg(0,std::ios_base::end);
		size_t sz=(size_t)ifs.tellg();
		ifs.seekg(0,std::ios_base::beg);
		if(sz>0)
		{
			char *pData=new char[sz];
			ifs.read(pData,sz);

			hr=D3DX11CreateEffectFromMemory(pData,sz,FXFlags,pDevice,ppEffect);
			delete [] pData;
		}
	}
	return hr;
}

HRESULT WINAPI D3DX11CreateEffectFromFile(const TCHAR *filename,D3D10_SHADER_MACRO *macros,UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	// first try to find an existing text source with this filename, and compile it.
	std::string text_filename=simul::base::WStringToString(filename);
	std::ifstream ifs(text_filename.c_str(),std::ios_base::binary);
	if(ifs.good())
	{
		std::string output_filename=text_filename+"o";
		DeleteFileA(output_filename.c_str());
		std::string command=simul::base::EnvironmentVariables::GetSimulEnvironmentVariable("DXSDK_DIR");
		if(command.length())
		{
//>"fxc.exe" /T fx_2_0 /Fo "..\..\gamma.fx"o "..\..\gamma.fx"
			command="\""+command;
			command+="Utilities\\Bin\\x86\\fxc.exe\"";
			command+=" /nologo /Tfx_5_0 /Fo \"";
			command+=text_filename+"o\" \"";
			command+=text_filename+"\"";
			if(macros)
				while(macros->Name)
				{
					command+=" /D";
					command+=macros->Name;
					command+="=";
					command+=macros->Definition;
					macros++;
				}
		//	command+=" > \""+text_filename+".log\"";
#if 0
			system(command.c_str());
#else
#if 0
			HINSTANCE hi=ShellExecuteA(NULL,
				LPCTSTR lpOperation,
				LPCTSTR lpFile,
				LPCTSTR lpParameters,
				LPCTSTR lpDirectory,
				INT nShowCmd
			);
#else
			STARTUPINFOA si;
			PROCESS_INFORMATION pi;
			ZeroMemory( &si, sizeof(si) );
			si.cb = sizeof(si);
			ZeroMemory( &pi, sizeof(pi) );
			si.wShowWindow=false;
			char com[500];
			strcpy(com,command.c_str());
			si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			si.wShowWindow = SW_HIDE;
			//FILE *fp = fopen((text_filename+".log").c_str(), "w");

			HANDLE hReadOutPipe = NULL;
			HANDLE hWriteOutPipe = NULL;
			HANDLE hReadErrorPipe = NULL;
			HANDLE hWriteErrorPipe = NULL;
			SECURITY_ATTRIBUTES saAttr; 
// Set the bInheritHandle flag so pipe handles are inherited. 
		 
			if(pipe_compiler_output)
			{
				saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
				saAttr.bInheritHandle = TRUE; 
				saAttr.lpSecurityDescriptor = NULL; 
				CreatePipe( &hReadOutPipe, &hWriteOutPipe, &saAttr, 100 );
				CreatePipe( &hReadErrorPipe, &hWriteErrorPipe, &saAttr, 100 );
			}
			//SetHandleInformation(hReadOutPipe, HANDLE_FLAG_INHERIT, 0) ;

			//SetHandleInformation(hReadErrorPipe, HANDLE_FLAG_INHERIT, 0) ;

			si.hStdOutput = hWriteOutPipe;
			si.hStdError= hWriteErrorPipe;
			CreateProcessA( NULL,		// No module name (use command line)
					com,				// Command line
					NULL,				// Process handle not inheritable
					NULL,				// Thread handle not inheritable
					TRUE,				// Set handle inheritance to FALSE
					0,//CREATE_NO_WINDOW,	// No nasty console windows
					NULL,				// Use parent's environment block
					NULL,				// Use parent's starting directory 
					&si,				// Pointer to STARTUPINFO structure
					&pi )				// Pointer to PROCESS_INFORMATION structure
				;
			// Wait until child process exits.

		  HANDLE WaitHandles[] = {
			pi.hProcess, hReadOutPipe, hReadErrorPipe
		  };

		  const DWORD BUFSIZE = 4096;
		  BYTE buff[BUFSIZE];
			bool has_errors=false;
		  while (1)
		  {
			DWORD dwBytesRead, dwBytesAvailable;

			DWORD dwWaitResult = WaitForMultipleObjects(pipe_compiler_output?3:1, WaitHandles, FALSE, 60000L);

			// Read from the pipes...
			while( PeekNamedPipe(hReadOutPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable )
			{
			  ReadFile(hReadOutPipe, buff, BUFSIZE-1, &dwBytesRead, 0);
			  std::cout << std::string((char*)buff, (size_t)dwBytesRead).c_str();
			}
			while( PeekNamedPipe(hReadErrorPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable )
			{
			  ReadFile(hReadErrorPipe, buff, BUFSIZE-1, &dwBytesRead, 0);
			  std::string str((char*)buff, (size_t)dwBytesRead);
			  std::cerr << str.c_str();
			  size_t pos=str.find("rror");
			  if(pos<str.length())
				has_errors=true;
			}

			// Process is done, or we timed out:
			if(dwWaitResult == WAIT_OBJECT_0 || dwWaitResult == WAIT_TIMEOUT)
			break;
		  }

			//WaitForSingleObject( pi.hProcess, INFINITE );
			CloseHandle( pi.hProcess );
			CloseHandle( pi.hThread );

			/*"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\Bin\x86\fxc.exe" /T fx_5_0 /Fo "MEDIA/HLSL/DX11/simul_clouds_and_lightning.fxo" "MEDIA/HLSL/DX11/simul_clouds_and_lightning.fx""	char [200]
			 */

			//fclose(fp);
#endif
#endif
			if(has_errors)
				return S_FALSE;
		}
	}
	HRESULT hr=D3DX11CreateEffectFromBinaryFile(filename,FXFlags,pDevice,ppEffect);
	return hr;
}
#endif

HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename)
{
	std::map<std::string,std::string> defines;
	return CreateEffect(d3dDevice,effect,filename,defines);
}

HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3D1xEffect **effect,const TCHAR *filename,const std::map<std::string,std::string>&defines)
{
	HRESULT hr=S_OK;
	std::cout<<_T("Create effect: ")<<filename<<std::endl;
	//tstring fn=filename;
	tstring fn=shader_path+filename;
	
	D3D10_SHADER_MACRO *macros=NULL;
	std::vector<std::string> d3dmacros;
	{
		size_t num_defines=defines.size();
		if(num_defines)
		{
			macros=new D3D10_SHADER_MACRO[num_defines+1];
			macros[num_defines].Definition=0;
			macros[num_defines].Name=0;
		}
		size_t def=0;
		for(std::map<std::string,std::string>::const_iterator i=defines.begin();i!=defines.end();i++)
		{
			macros[def].Name=i->first.c_str();
			macros[def].Definition=i->second.c_str();
			def++;
		}
	}
	DWORD flags=default_effect_flags;
	SAFE_RELEASE(*effect);

#ifdef DX10
	ID3D10Blob *errors;
	if(FAILED(
		hr=D3DX10CreateEffectFromFile(
				  fn.c_str(),
				  macros,
				  NULL,
				  "fx_4_0",
				  flags,
				  flags,
				  d3dDevice,
				  NULL,
				  NULL,
				  effect,
				  &errors,
				  0
				)))
#else
	if(FAILED(
		hr=D3DX11CreateEffectFromFile(
				fn.c_str(),
				macros,
				flags,
				d3dDevice,
				effect)))
#endif
	{
		std::string err="";
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateEffect", hr );
#endif
#ifdef DX10
		if(errors)
			std::cerr<<(char*)errors->GetBufferPointer()<<std::endl;
#endif
		DebugBreak();
	}
	else
	{
		std::string err="";
	}
	assert((*effect)->IsValid());
	delete [] macros;
	return hr;
}

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif


HRESULT Map2D(ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp)
{
#ifdef DX10
	return tex->Map(0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#else
	return m_pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#endif
}

HRESULT Map3D(ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp)
{
#ifdef DX10
	return tex->Map(0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#else
	return m_pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#endif
}

HRESULT Map1D(ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp)
{
#ifdef DX10
	return tex->Map(0,D3D1x_MAP_WRITE_DISCARD,0,&(mp->pData));
#else
	return m_pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
#endif
}

void Unmap2D(ID3D1xTexture2D *tex)
{
#ifdef DX10
	tex->Unmap(0);
#else
	m_pImmediateContext->Unmap(tex,0);
#endif
}

void Unmap3D(ID3D1xTexture3D *tex)
{
#ifdef DX10
	tex->Unmap(0);
#else
	m_pImmediateContext->Unmap(tex,0);
#endif
}

void Unmap1D(ID3D1xTexture1D *tex)
{
#ifdef DX10
	tex->Unmap(0);
#else
	m_pImmediateContext->Unmap(tex,0);
#endif
}

#ifdef DX10
HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,void **vert)
#else
HRESULT MapBuffer(ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE*vert)
#endif
{
#ifdef DX10
	return vertexBuffer->Map(D3D1x_MAP_WRITE_DISCARD,0,vert);
#else
	return m_pImmediateContext->Map(vertexBuffer,0,D3D1x_MAP_WRITE_DISCARD,0,vert);
#endif
}

void UnmapBuffer(ID3D1xBuffer *vertexBuffer)
{
#ifdef DX10
	vertexBuffer->Unmap();
#else
	m_pImmediateContext->Unmap(vertexBuffer,0);
#endif
}


HRESULT ApplyPass(ID3D1xEffectPass *pass)
{
#ifdef DX10
	return pass->Apply(0);
#else
	return pass->Apply(0,m_pImmediateContext);
#endif
}

void FixProjectionMatrix(D3DXMATRIX &proj,float zFar,bool y_vertical)
{
	float zNear;
	if(y_vertical)
	{
		zNear=-proj._43/proj._33;
		proj._33=zFar/(zFar-zNear);
	}
	else
	{
		zNear=proj._43/proj._33;
		proj._33=-zFar/(zFar-zNear);
	}
	proj._43=-zNear*zFar/(zFar-zNear);
}

void FixProjectionMatrix(D3DXMATRIX &proj,float zNear,float zFar,bool y_vertical)
{
	if(y_vertical)
	{
		proj._33=zFar/(zFar-zNear);
	}
	else
	{
		proj._33=-zFar/(zFar-zNear);
	}
	proj._43=-zNear*zFar/(zFar-zNear);
}


void MakeCubeMatrices(D3DXMATRIX g_amCubeMapViewAdjust[],const float *cam_pos)
{
    D3DXVECTOR3 vEyePt = D3DXVECTOR3( cam_pos );
    D3DXVECTOR3 vLookDir;
    D3DXVECTOR3 vUpDir;
    ZeroMemory(g_amCubeMapViewAdjust, 6*sizeof(D3DXMATRIX) );

    vLookDir =vEyePt+ D3DXVECTOR3( 1.0f, 0.0f, 0.0f );
     vUpDir = D3DXVECTOR3( 0.0f,-1.0f, 0.0f );
   D3DXMatrixLookAtRH( &g_amCubeMapViewAdjust[0], &vEyePt, &vLookDir, &vUpDir );
    vLookDir =vEyePt+D3DXVECTOR3( -1.0f, 0.0f, 0.0f );
    vUpDir = D3DXVECTOR3( 0.0f,-1.0f, 0.0f );
    D3DXMatrixLookAtRH( &g_amCubeMapViewAdjust[1], &vEyePt, &vLookDir, &vUpDir );
    vLookDir =vEyePt+D3DXVECTOR3( 0.0f,-1.0f, 0.0f );
    vUpDir = D3DXVECTOR3( 0.0f, 0.0f,-1.0f );
    D3DXMatrixLookAtRH( &g_amCubeMapViewAdjust[2], &vEyePt, &vLookDir, &vUpDir );
   vLookDir =vEyePt+D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    vUpDir = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
   D3DXMatrixLookAtRH( &g_amCubeMapViewAdjust[3], &vEyePt, &vLookDir, &vUpDir );
    vLookDir =vEyePt+D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
    vUpDir = D3DXVECTOR3( 0.0f,-1.0f, 0.0f );
    D3DXMatrixLookAtRH( &g_amCubeMapViewAdjust[4], &vEyePt, &vLookDir, &vUpDir );
  vLookDir = vEyePt+D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    vUpDir = D3DXVECTOR3( 0.0f,-1.0f, 0.0f );
    D3DXMatrixLookAtRH( &g_amCubeMapViewAdjust[5], &vEyePt, &vLookDir, &vUpDir );
}

void BreakIfDebugging()
{
	DebugBreak();
}


void RenderAngledQuad(ID3D1xDevice *m_pd3dDevice,const float *dr,bool y_vertical,float half_angle_radians,ID3D1xEffect* effect,ID3D1xEffectTechnique* tech,D3DXMATRIX view,D3DXMATRIX proj
					  ,D3DXVECTOR3 sun_dir)
{
	// If y is vertical, we have LEFT-HANDED rotations, otherwise right.
	// But D3DXMatrixRotationYawPitchRoll uses only left-handed, hence the change of sign below.
	D3DXVECTOR3 pos;
	D3DXVECTOR3 dir(dr);
	pos=GetCameraPosVector(view,y_vertical);
	float Yaw=atan2(dir.x,y_vertical?dir.z:dir.y);
	float Pitch=-asin(y_vertical?dir.y:dir.z);
	HRESULT hr=S_OK;
	D3DXMATRIX world, tmp1, tmp2;
	D3DXMatrixIdentity(&world);
	static D3DXMATRIX flip(1.f,0,0,0,0,0,1.f,0,0,1.f,0,0,0,0,0,1.f);
	if(y_vertical)
	{
		D3DXMatrixRotationYawPitchRoll(
			  &world,
			  Yaw,
			  Pitch,
			  0
			);
	}
	else
	{
		simul::geometry::SimulOrientation or;
		or.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
		or.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
		world=*((const D3DXMATRIX*)(or.T4.RowPointer(0)));
	}
	//set up matrices
	world._41=pos.x;
	world._42=pos.y;
	world._43=pos.z;
	D3DXVECTOR3 sun2;
	D3DXMATRIX inv_world;
	D3DXMatrixInverse(&inv_world,NULL,&world);
	D3DXVec3TransformNormal(  &sun2,
						  &sun_dir,
						  &inv_world);
	D3DXMatrixMultiply(&tmp1,&world,&view);
	D3DXMatrixMultiply(&tmp2,&tmp1,&proj);
	D3DXMatrixTranspose(&tmp1,&tmp2);
	if(effect)
	{
		ID3D1xEffectMatrixVariable*	worldViewProj=effect->GetVariableByName("worldViewProj")->AsMatrix();
		worldViewProj->SetMatrix((const float *)(&tmp1));
		ID3D1xEffectVectorVariable*	lightDir=effect->GetVariableByName("lightDir")->AsVector();
		lightDir->SetFloatVector((const float *)(&sun2));
	}
	struct Vertext
	{
		float x,y,z;
		float tx,ty;
	};
	// coverage is 2*atan(1/5)=11 degrees.
	// the sun covers 1 degree. so the sun circle should be about 1/10th of this quad in width.
	static float w=1.f;
	float d=w/tan(half_angle_radians);
	Vertext vertices[4] =
	{
		{ w,-w,	d, 1.f	,0.f},
		{ w, w,	d, 1.f	,1.f},
		{-w,-w,	d, 0.f	,0.f},
		{-w, w,	d, 0.f	,1.f},
	};
	ID3D1xBuffer *vertexBuffer=NULL;

	// Create the vertex buffer:
	D3D1x_BUFFER_DESC desc=
	{
        4*sizeof(Vertext),
        D3D1x_USAGE_DYNAMIC,
        D3D1x_BIND_VERTEX_BUFFER,
        D3D1x_CPU_ACCESS_WRITE,
        0
	};
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(Vertext);
	hr=m_pd3dDevice->CreateBuffer(&desc,&InitData,&vertexBuffer);

	const D3D1x_INPUT_ELEMENT_DESC decl[] =
    {
        { "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	12,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
    };
	D3D1x_PASS_DESC PassDesc;
	ID3D1xEffectPass *pass=tech->GetPassByIndex(0);
	hr=pass->GetDesc(&PassDesc);

	ID3D1xInputLayout*				m_pVtxDecl=NULL;
	SAFE_RELEASE(m_pVtxDecl);
	hr=m_pd3dDevice->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	m_pImmediateContext->IASetInputLayout(m_pVtxDecl);
	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	UINT stride = sizeof(Vertext);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,				// the first input slot for binding
												1,				// the number of buffers in the array
												&vertexBuffer,	// the array of vertex buffers
												&stride,		// array of stride values, one for each buffer
												&offset);		// array of 
	hr=ApplyPass(tech->GetPassByIndex(0));
	m_pImmediateContext->Draw(4,0);
	//hr=ApplyPass(tech->GetPassByIndex(0));
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(m_pVtxDecl);
}

void RenderTexture(ID3D1xDevice *m_pd3dDevice,int x1,int y1,int dx,int dy,ID3D1xEffectTechnique* tech)
{
	RenderTexture(m_pd3dDevice,(float)x1,(float)y1,(float)dx,(float)dy,tech);
}

void RenderTexture(ID3D1xDevice *m_pd3dDevice,float x1,float y1,float dx,float dy,ID3D1xEffectTechnique* tech)
{
	struct Vertext
	{
		D3DXVECTOR4 pos;
		D3DXVECTOR2 tex;
	};
	HRESULT hr=S_OK;
	const D3D1x_INPUT_ELEMENT_DESC decl[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,		0,	16,	D3D1x_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3D1x_PASS_DESC PassDesc;
	tech->GetPassByIndex(0)->GetDesc(&PassDesc);
	ID3D1xInputLayout *m_pBufferVertexDecl;
	hr=m_pd3dDevice->CreateInputLayout(decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pBufferVertexDecl);
	Vertext vertices[4] =
	{
		D3DXVECTOR4(x1		,y1			,0.f,	1.f), D3DXVECTOR2(0.f	,1.f),
		D3DXVECTOR4(x1+dx	,y1			,0.f,	1.f), D3DXVECTOR2(1.f	,1.f),
		D3DXVECTOR4(x1		,y1+dy		,0.f,	1.f), D3DXVECTOR2(0.f	,0.f),
		D3DXVECTOR4(x1+dx	,y1+dy		,0.f,	1.f), D3DXVECTOR2(1.f	,0.f),
	};
	D3D1x_BUFFER_DESC bdesc=
	{
        4*sizeof(Vertext),
        D3D1x_USAGE_DYNAMIC,
        D3D1x_BIND_VERTEX_BUFFER,
        D3D1x_CPU_ACCESS_WRITE,
        0
	};
    D3D1x_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = sizeof(Vertext);
    InitData.SysMemSlicePitch = 0;
	ID3D1xBuffer* m_pVertexBuffer;
	hr=m_pd3dDevice->CreateBuffer(&bdesc,&InitData,&m_pVertexBuffer);
	UINT stride = sizeof(Vertext);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	m_pImmediateContext->IASetVertexBuffers(	0,					// the first input slot for binding
												1,					// the number of buffers in the array
												&m_pVertexBuffer,	// the array of vertex buffers
												&stride,			// array of stride values, one for each buffer
												&offset);			// array of offset values, one for each buffer
	m_pImmediateContext->IASetPrimitiveTopology(D3D1x_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pImmediateContext->IASetInputLayout(m_pBufferVertexDecl);
	hr=ApplyPass(tech->GetPassByIndex(0));
	m_pImmediateContext->Draw(4,0);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pBufferVertexDecl);
}

// Stored states
static ID3D11DepthStencilState* m_pDepthStencilStateStored11=NULL;
static UINT m_StencilRefStored11;
static ID3D11RasterizerState* m_pRasterizerStateStored11=NULL;
static ID3D11BlendState* m_pBlendStateStored11=NULL;
static float m_BlendFactorStored11[4];
static UINT m_SampleMaskStored11;
static ID3D11SamplerState* m_pSamplerStateStored11=NULL;

void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMGetDepthStencilState( &m_pDepthStencilStateStored11, &m_StencilRefStored11 );
    pd3dImmediateContext->RSGetState( &m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMGetBlendState( &m_pBlendStateStored11, m_BlendFactorStored11, &m_SampleMaskStored11 );
    pd3dImmediateContext->PSGetSamplers( 0, 1, &m_pSamplerStateStored11 );
}

void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilStateStored11, m_StencilRefStored11 );
    pd3dImmediateContext->RSSetState( m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMSetBlendState( m_pBlendStateStored11, m_BlendFactorStored11, m_SampleMaskStored11 );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSamplerStateStored11 );

    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}