
#include "CompileShaderDX1x.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"

#if _MSC_VER==1700
#pragma message("_MSC_VER=1700")
#elif _MSC_VER==1600
#pragma message("_MSC_VER=1600")
#else
#pragma message("_MSC_VER other")
#endif

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

typedef std::basic_string<TCHAR> tstring;

namespace simul
{
	namespace dx11
	{
		extern std::string *shaderPathUtf8;
	}
}

using namespace simul::dx11;

HRESULT CompileShaderFromFile( char* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
	std::string fn=*shaderPathUtf8+szFileName;
    HRESULT hr = S_OK;

    // open the file
    HANDLE hFile = CreateFileW(simul::base::Utf8ToWString(fn).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        throw simul::base::RuntimeError(std::string("File not found: ")+(fn.c_str()));

    // Get the file size
    LARGE_INTEGER FileSize;
    GetFileSizeEx( hFile, &FileSize );

    // create enough space for the file data
    BYTE* pFileData = new BYTE[ FileSize.LowPart ];
    if( !pFileData )
        throw simul::base::RuntimeError(std::string("Out of memory loading ")+(fn.c_str()));

    // read the data in
    DWORD BytesRead;
    if( !ReadFile( hFile, pFileData, FileSize.LowPart, &BytesRead, NULL ) )
        throw simul::base::RuntimeError(std::string("Failed to load: ")+(fn.c_str()));

    CloseHandle( hFile );

    // Compile the shader
  //  char pFilePathName[MAX_PATH];        
   // WideCharToMultiByte(CP_ACP, 0, szFileName, -1, pFilePathName, MAX_PATH, NULL, NULL);
    ID3DBlob* pErrorBlob;
    hr = D3DCompile( pFileData, FileSize.LowPart, szFileName, NULL, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob );

    delete []pFileData;

    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        throw simul::base::RuntimeError(std::string("Failed to load: ")+(szFileName));
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}
