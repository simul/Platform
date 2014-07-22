
#include "CompileShaderDX1x.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/FileLoader.h"
#include <vector>

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

namespace simul
{
	namespace dx11
	{
		extern std::vector<std::string> shaderPathsUtf8;
	}
}

using namespace simul::dx11;


HRESULT __stdcall ShaderIncludeHandler::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
{
	try
	{
ERRNO_CHECK
		std::string finalPathUtf8;
		switch(IncludeType)
		{
		case D3D_INCLUDE_LOCAL:
			finalPathUtf8	=m_ShaderDirUtf8+"\\"+pFileNameUtf8;
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
		simul::base::FileLoader::GetFileLoader()->AcquireFileContents(buf,fileSize,finalPathUtf8.c_str(),false);
		*ppData = buf;
		*pBytes = (UINT)fileSize;
		if(!*ppData)
			return E_FAIL;
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
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents((void*)pData);
	return S_OK;
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
			finalPathUtf8	=m_ShaderDirUtf8+"\\"+pFileNameUtf8;
			break;
		case D3D_INCLUDE_SYSTEM:
			finalPathUtf8	=m_SystemDirUtf8+"\\"+pFileNameUtf8;
			break;
		default:
			assert(0);
		}
		void *buf=NULL;
		unsigned fileSize=0;
		double dateTimeJdn=simul::base::FileLoader::GetFileLoader()->GetFileDate(finalPathUtf8.c_str());
		if(dateTimeJdn>newest)
			newest=dateTimeJdn;
		if(dateTimeJdn>lastCompileTime)
		{
			// Early-out if we're testing against a specific compile time.
			return E_FAIL;
		}
		simul::base::FileLoader::GetFileLoader()->AcquireFileContents(buf,fileSize,finalPathUtf8.c_str(),false);
		*ppData = buf;
		*pBytes = (UINT)fileSize;
		if(!*ppData)
			return E_FAIL;
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
	simul::base::FileLoader::GetFileLoader()->ReleaseFileContents((void*)pData);
	return S_OK;
}



HRESULT CompileShaderFromFile( const char* filename_utf8, const char* szEntryPoint, const char* szShaderModel, ID3DBlob** ppBlobOut )
{
	std::string fn_utf8=simul::base::FileLoader::GetFileLoader()->FindFileInPathStack(filename_utf8,shaderPathsUtf8);
	if(!simul::base::FileLoader::GetFileLoader()->FileExists(fn_utf8.c_str()))
		return NULL;
    HRESULT hr = S_OK;

    // open the file
    HANDLE hFile = CreateFileW(simul::base::Utf8ToWString(fn_utf8).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        throw simul::base::RuntimeError(std::string("File not found: ")+(fn_utf8.c_str()));

    // Get the file size
    LARGE_INTEGER FileSize;
    GetFileSizeEx( hFile, &FileSize );

    // create enough space for the file data
    BYTE* pFileData = new BYTE[ FileSize.LowPart ];
    if( !pFileData )
        throw simul::base::RuntimeError(std::string("Out of memory loading ")+(fn_utf8.c_str()));

    // read the data in
    DWORD BytesRead;
    if( !ReadFile( hFile, pFileData, FileSize.LowPart, &BytesRead, NULL ) )
        throw simul::base::RuntimeError(std::string("Failed to load: ")+(fn_utf8.c_str()));

    CloseHandle( hFile );

    ID3DBlob* pErrorBlob;
	int pos		=(int)fn_utf8.find_last_of("/");
	int bpos	=(int)fn_utf8.find_last_of("\\");
	if(pos<0||bpos>pos)
		pos=bpos;
	std::string path_utf8=fn_utf8.substr(0,pos);
	ShaderIncludeHandler shaderIncludeHandler(path_utf8.c_str(),"");
    hr = D3DCompile( pFileData, FileSize.LowPart, filename_utf8, NULL
		//, NULL
		,&shaderIncludeHandler		//ID3DInclude *pInclude,
		, szEntryPoint
		, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob );

    delete []pFileData;

    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        throw simul::base::RuntimeError(std::string("Failed to load: ")+(filename_utf8));
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}
