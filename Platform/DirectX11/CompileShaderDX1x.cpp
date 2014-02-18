
#include "CompileShaderDX1x.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include <vector>

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

typedef std::basic_string<TCHAR> tstring;

namespace simul
{
	namespace dx11
	{
		extern std::vector<std::string> shaderPathsUtf8;
	}
}

using namespace simul::dx11;

HRESULT CompileShaderFromFile( const char* szFileNameUtf8, const char* szEntryPoint, const char* szShaderModel, ID3DBlob** ppBlobOut )
{
	std::string fn_utf8;
	for(int i=(int)shaderPathsUtf8.size()-1;i>=0;i--)
	{
		fn_utf8=(shaderPathsUtf8[i]+"/")+szFileNameUtf8;
		if(simul::base::FileExists(fn_utf8))
			break;
	}
	if(!simul::base::FileExists(fn_utf8))
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
    hr = D3DCompile( pFileData, FileSize.LowPart, szFileNameUtf8, NULL
		//, NULL
		,&shaderIncludeHandler		//ID3DInclude *pInclude,
		, szEntryPoint
		, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob );

    delete []pFileData;

    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        throw simul::base::RuntimeError(std::string("Failed to load: ")+(szFileNameUtf8));
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}
