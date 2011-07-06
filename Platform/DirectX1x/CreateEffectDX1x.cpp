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

#include <tchar.h>
#include <string>
typedef std::basic_string<TCHAR> tstring;
static tstring shader_path=TEXT("");
static tstring texture_path=TEXT("");
static DWORD default_effect_flags=0;

#include <vector>
#include <iostream>
#include <assert.h>
#include <fstream>
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

ID3D1xDevice		*m_pd3dDevice		=NULL;
ID3D1xDeviceContext	*m_pImmediateContext=NULL;

namespace simul
{
	namespace dx11
	{
		void SetShaderPath(const char *path)
		{
		#ifdef UNICODE
			// tstring and TEXT cater for the confusion between wide and regular strings.
			shader_path.resize(strlen(path),L' '); // Make room for characters
			// Copy string to wstring.
			std::copy(path,path+strlen(path),shader_path.begin());

			shader_path+=L"/";
		#else
			shader_path=path;
			shader_path+="/";
		#endif
			shader_path_set=true;
		}
		void SetTexturePath(const char *path)
		{
		#ifdef UNICODE
			// tstring and TEXT cater for the confusion between wide and regular strings.
			texture_path.resize(strlen(path),L' '); // Make room for characters
			// Copy string to wstring.
			std::copy(path,path+strlen(path),texture_path.begin());
			texture_path+=L"/";
		#else
			texture_path=path;
			texture_path+="/";
		#endif
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
		}
		void UnsetDevice()
		{
			SAFE_RELEASE(m_pImmediateContext);
		}
	}
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

HRESULT WINAPI D3DX11CreateEffectFromFile(const TCHAR *filename, UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	// first try to find an existing text source with this filename, and compile it.
	std::string text_filename=simul::base::WStringToString(filename);
	std::ifstream ifs(text_filename.c_str(),std::ios_base::binary);
	if(ifs.good())
	{
		std::string command=simul::base::EnvironmentVariables::GetSimulEnvironmentVariable("DXSDK_DIR");
		if(command.length())
		{
//>"fxc.exe" /T fx_2_0 /Fo "..\..\gamma.fx"o "..\..\gamma.fx"
			command="\""+command;
			command+="Utilities\\Bin\\x86\\fxc.exe\"";
			command+=" /Tfx_5_0 /Fo \"";
			command+=text_filename+"o\" \"";
			command+=text_filename+"\"";
			//command+=" > \""+text_filename+".log\"";
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
			FILE *fp = fopen((text_filename+".log").c_str(), "w");
			si.hStdOutput = fp;
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
			WaitForSingleObject( pi.hProcess, INFINITE );
			CloseHandle( pi.hProcess );
			CloseHandle( pi.hThread );

			/**"C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Utilities\Bin\x86\fxc.exe" /T fx_5_0 /Fo "MEDIA/HLSL/DX11/simul_clouds_and_lightning.fxo" "MEDIA/HLSL/DX11/simul_clouds_and_lightning.fx""	char [200]

			
			 \author	Roderick
			 \date	01/07/2011
			
			 \param		The.
			 */

			fclose(fp);
#endif
#endif
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
		//errors->GetBufferSize();

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