
#include "CompileShaderDX1x.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/FileLoader.h"
#include <vector>

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

using namespace platform;
using namespace dx11;

ShaderIncludeHandler::ShaderIncludeHandler(const char* shaderDirUtf8, const char* systemDirUtf8,const std::vector<std::string> &shaderPathsUtf8)
	: m_ShaderDirUtf8(shaderDirUtf8), m_SystemDirUtf8(systemDirUtf8)
{
	m_pathsUtf8 = shaderPathsUtf8;
	m_pathsUtf8.push_back(shaderDirUtf8);
	m_pathsUtf8.push_back(systemDirUtf8);
}

HRESULT __stdcall ShaderIncludeHandler::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
{
	try
	{
		ERRNO_CHECK
		std::string finalPathUtf8;
		switch(IncludeType)
		{
		case D3D_INCLUDE_LOCAL:
		{
			int index = platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(pFileNameUtf8, m_pathsUtf8); //m_ShaderDirUtf8 + "\\" + pFileNameUtf8;
			if(index<0)
				finalPathUtf8=pFileNameUtf8;
			else if(index<m_pathsUtf8.size())
				finalPathUtf8=(m_pathsUtf8[index]+"/")+pFileNameUtf8;
		}
			break;
		case D3D_INCLUDE_SYSTEM:
			finalPathUtf8	=m_SystemDirUtf8+"\\"+pFileNameUtf8;
			break;
		default:
			assert(0);
		}
		void *buf=NULL;
		unsigned fileSize=0;
ERRNO_CHECK
		platform::core::FileLoader::GetFileLoader()->AcquireFileContents(buf,fileSize,finalPathUtf8.c_str(),false);
		*ppData = buf;
		*pBytes = (UINT)fileSize;
		if(!*ppData)
		{
			SIMUL_CERR<<"Can't find file "<<finalPathUtf8.c_str()<<std::endl;
			return E_FAIL;
		}
		std::string pathOnly = finalPathUtf8;
		int last_slash = (int)pathOnly.find_last_of("/");
		int last_bslash = (int)pathOnly.find_last_of("\\");
		if (last_bslash>last_slash)
			last_slash = last_bslash;
		if (last_slash>0)
			pathOnly = pathOnly.substr(0, last_slash);
		m_pathsUtf8.push_back(pathOnly);
		return S_OK;
	}
	catch(std::exception& e)
	{
		std::cerr<<e.what()<<std::endl;
		return E_FAIL;
	}
}

HRESULT __stdcall ShaderIncludeHandler::Close(LPCVOID pData)
{
	platform::core::FileLoader::GetFileLoader()->ReleaseFileContents((void*)pData);
	return S_OK;
}

DetectChangesIncludeHandler::DetectChangesIncludeHandler(const char* shaderDirUtf8,const std::vector<std::string> &shaderPathsUtf8, uint64_t binaryTime )
	: m_ShaderDirUtf8(shaderDirUtf8), lastCompileTime(binaryTime), newest(0)
{
	m_pathsUtf8 = shaderPathsUtf8;
	m_pathsUtf8.push_back(shaderDirUtf8);
}

HRESULT __stdcall DetectChangesIncludeHandler::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
{
	try
	{
ERRNO_CHECK
		std::string finalPathUtf8;
		switch(IncludeType)
		{
		case D3D_INCLUDE_LOCAL:
		{
			int index = platform::core::FileLoader::GetFileLoader()->FindIndexInPathStack(pFileNameUtf8, m_pathsUtf8); //m_ShaderDirUtf8 + "\\" + pFileNameUtf8;
			if(index<0)
				finalPathUtf8=pFileNameUtf8;
			else if(index<m_pathsUtf8.size())
				finalPathUtf8=(m_pathsUtf8[index]+"/")+pFileNameUtf8;
		}
			break;
		case D3D_INCLUDE_SYSTEM:
			finalPathUtf8	=m_SystemDirUtf8+"\\"+pFileNameUtf8;
			break;
		default:
			return E_FAIL;
		}
		if(finalPathUtf8.length()==0)
		{
			newest=0;
			SIMUL_CERR<<"Can't find include file "<<pFileNameUtf8<<std::endl;
			return E_FAIL;
		}
		void *buf=NULL;
		unsigned fileSize=0;
		uint64_t timestamp= platform::core::FileLoader::GetFileLoader()->GetFileDate(finalPathUtf8.c_str());
		if(timestamp>newest)
			newest=timestamp;
		if(timestamp>lastCompileTime)
		{
			// Early-out if we're testing against a specific compile time.
			return E_FAIL;
		}
		platform::core::FileLoader::GetFileLoader()->AcquireFileContents(buf,fileSize,finalPathUtf8.c_str(),false);
		*ppData = buf;
		*pBytes = (UINT)fileSize;
		if(!*ppData)
			return E_FAIL;
		std::string pathOnly = finalPathUtf8;
		int last_slash =(int) pathOnly.find_last_of("/");
		int last_bslash =(int) pathOnly.find_last_of("\\");
		if (last_bslash>last_slash)
			last_slash = last_bslash;
		if (last_slash>0)
			pathOnly = pathOnly.substr(0, last_slash);
		m_pathsUtf8.push_back(pathOnly);
		return S_OK;
	}
	catch(std::exception& e)
	{
		std::cerr<<e.what()<<std::endl;
		return E_FAIL;
	}
}

HRESULT __stdcall DetectChangesIncludeHandler::Close(LPCVOID pData)
{
	platform::core::FileLoader::GetFileLoader()->ReleaseFileContents((void*)pData);
	return S_OK;
}