#include "CommandLine.h"
#include "StringFunctions.h"
#include "StringToWString.h"
#include "RuntimeError.h"

using namespace simul;
using namespace base;

#ifdef WIN64
#include <Windows.h>

bool platform::core::RunCommandLine(const char *command_utf8,  OutputDelegate outputDelegate)
{
	bool pipe_compiler_output=true;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	ZeroMemory( &pi, sizeof(pi) );
	si.wShowWindow=false;
	wchar_t com[7500];
	std::wstring wstr=Utf8ToWString(command_utf8);
	wcscpy_s(com,wstr.c_str());
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;

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

	si.hStdOutput = hWriteOutPipe;
	si.hStdError= hWriteErrorPipe;
	CreateProcessW( NULL,		// No module name (use command line)
			com,				// Command line
			NULL,				// Process handle not inheritable
			NULL,				// Thread handle not inheritable
			TRUE,				// Set handle inheritance to FALSE
			CREATE_NO_WINDOW,	//CREATE_NO_WINDOW,	// No nasty console windows
			NULL,				// Use parent's environment block
			NULL,				// Use parent's starting directory 
			&si,				// Pointer to STARTUPINFO structure
			&pi )				// Pointer to PROCESS_INFORMATION structure
		;
	// Wait until child process exits.
	if(pi.hProcess==nullptr)
	{
		SIMUL_CERR<<"Error: Could not find the executable for "<<WStringToUtf8(com)<<std::endl;
		return false;
	}
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
		if(pipe_compiler_output)
		{
			while( PeekNamedPipe(hReadOutPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable )
			{
			  ReadFile(hReadOutPipe, buff, BUFSIZE-1, &dwBytesRead, 0);
			  std::string str((char*)buff, (size_t)dwBytesRead);
			  if(outputDelegate)
				  outputDelegate(str);
			  else
				  std::cout<<str.c_str();
			}
			while( PeekNamedPipe(hReadErrorPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable )
			{
				ReadFile(hReadErrorPipe, buff, BUFSIZE-1, &dwBytesRead, 0);
				std::string str((char*)buff, (size_t)dwBytesRead);
				if(outputDelegate)
					outputDelegate(str);
			  else
				  std::cerr<<str.c_str();
			}
		}
		// Process is done, or we timed out:
		if(dwWaitResult == WAIT_OBJECT_0 || dwWaitResult == WAIT_TIMEOUT)
			break;
		if (dwWaitResult == WAIT_FAILED)
		{
			DWORD err = GetLastError();
			char* msg;
			// Ask Windows to prepare a standard message for a GetLastError() code:
			if (!FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL))
				break;

			SIMUL_CERR<<"Error message: "<<msg<<std::endl;
			{
				CloseHandle( pi.hProcess );
				CloseHandle( pi.hThread );
				exit(21);
			}
		}
	}

	//WaitForSingleObject( pi.hProcess, INFINITE );
 	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	if(has_errors)
		return false;
	return true;
}
#elif defined(UNIX)
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
bool platform::core::RunCommandLine(const char *cmd,  OutputDelegate )
{
	int res= system(cmd);

	return res==0;
}

#else
bool platform::core::RunCommandLine(const char *cmd,  OutputDelegate )
{
	SIMUL_CERR<<"RunCommandLine not implemented on this platform.\n";
	return false;
}

#endif