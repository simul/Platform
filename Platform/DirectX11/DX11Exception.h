#pragma once
#include <exception>
#include <d3d11.h>
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#else
#include <string>
//! Name of an error
extern const char *DXGetErrorStringA(HRESULT hr);
//! Text of an error
extern const char *DXGetErrorDescriptionA(HRESULT hr);
#endif
#include <string>

// Exception thrown when a DirectX Function fails
class DX11Exception : public std::exception
{

public:

	// DX11Exception a string for the specified HRESULT error code
	DX11Exception (HRESULT hresult)
		: errorCode(hresult)
	{
		std::string errorString = DXGetErrorDescriptionA(hresult);

		std::string message = "DirectX11 Error: ";
		message += errorString;
	}

	DX11Exception (HRESULT hresult, LPCSTR errorMsg)
		: errorCode(hresult)
	{
		std::string message = "DirectX11 Error: ";
		message += errorMsg;
	}

	// Retrieve the error code
	HRESULT GetErrorCode () const throw ()
	{
		return errorCode;
	}

protected:

	HRESULT		errorCode;		// The DX error code
};