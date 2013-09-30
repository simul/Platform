#pragma once
#include <D3Dcompiler.h>
#include <string>

class ShaderIncludeHandler : public ID3DInclude
{
public:
	ShaderIncludeHandler(const char* shaderDirUtf8, const char* systemDirUtf8)
		: m_ShaderDirUtf8(shaderDirUtf8), m_SystemDirUtf8(systemDirUtf8)
	{
	}
	HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType,LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	HRESULT __stdcall Close(LPCVOID pData);
private:
	std::string m_ShaderDirUtf8;
	std::string m_SystemDirUtf8;
};

HRESULT CompileShaderFromFile( const char* szFileNameUtf8, const char*  szEntryPoint, const char*  szShaderModel, ID3DBlob** ppBlobOut );