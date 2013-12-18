#ifndef COMPILE_SHADER_DX11_H
#define COMPILE_SHADER_DX11_H
#include <D3Dcompiler.h>
#include <string>

//! An include-handler for Direct3D 11 shader compilation.
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

//! An include-handler that detects whether any include file has been changed since a given date/time.
class DetectChangesIncludeHandler : public ID3DInclude
{
public:
	DetectChangesIncludeHandler(const char* shaderDirUtf8, double t)
		: m_ShaderDirUtf8(shaderDirUtf8), lastCompileTime(t), anyChanges(false)
	{
	}
	HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType,LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	HRESULT __stdcall Close(LPCVOID pData);
	bool HasDetectedChanges() const
	{
		return anyChanges;
	}
private:
	std::string m_ShaderDirUtf8;
	std::string m_SystemDirUtf8;
	double lastCompileTime;
	bool anyChanges;
};

HRESULT CompileShaderFromFile( const char* szFileNameUtf8, const char*  szEntryPoint, const char*  szShaderModel, ID3DBlob** ppBlobOut );
#endif