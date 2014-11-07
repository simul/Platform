#ifndef COMPILE_SHADER_DX11_H
#define COMPILE_SHADER_DX11_H
#include "SimulDirectXHeader.h"
#ifdef _XBOX_ONE
#include <D3Dcompiler_x.h>
#else
#if defined(SIMUL_WIN8_SDK) && defined(WIN64)
#include <D3Dcompiler_xdk.h>
#else
#include <D3Dcompiler.h>
#endif
#endif
#include <string>
#include <vector>

//! An include-handler for Direct3D 11 shader compilation.
class ShaderIncludeHandler : public ID3DInclude
{
public:
	ShaderIncludeHandler(const char* shaderDirUtf8, const char* systemDirUtf8);
	HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType,LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	HRESULT __stdcall Close(LPCVOID pData);
private:
	std::vector<std::string> m_pathsUtf8;
	std::string m_ShaderDirUtf8;
	std::string m_SystemDirUtf8;
};

//! An include-handler that detects whether any include file has been changed since a given date/time.
class DetectChangesIncludeHandler : public ID3DInclude
{
public:
	DetectChangesIncludeHandler(const char* shaderDirUtf8, double binaryTime = 0.0);
	HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType,LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	HRESULT __stdcall Close(LPCVOID pData);
	double GetNewestIncludeDateJDN() const
	{
		return newest;
	}
private:
	std::vector<std::string> m_pathsUtf8;
	std::string m_ShaderDirUtf8;
	std::string m_SystemDirUtf8;
	double lastCompileTime;
	double newest;
};

HRESULT CompileShaderFromFile( const char* szFileNameUtf8, const char*  szEntryPoint, const char*  szShaderModel, ID3DBlob** ppBlobOut );
#endif