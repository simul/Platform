#include "CommandLine.h"
#include "StringFunctions.h"
#include "StringToWString.h"
#include "RuntimeError.h"

using namespace platform;
using namespace core;

#ifdef WIN64
#include <Windows.h>

bool platform::core::RunCommandLine(const char *command_utf8,  OutputDelegate outputDelegate)
{
	bool pipeChildProcessOutput=true;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	si.wShowWindow=false;
	wchar_t com[7500];
	std::wstring wstr=Utf8ToWString(command_utf8);
	wcscpy_s(com,wstr.c_str());
	si.dwFlags |= STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;

	HANDLE hReadOutPipe = NULL;
	HANDLE hWriteOutPipe = NULL;
	HANDLE hReadErrorPipe = NULL;
	HANDLE hWriteErrorPipe = NULL;
	SECURITY_ATTRIBUTES saAttr; 
	
	if (pipeChildProcessOutput)
	{
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
		saAttr.bInheritHandle = TRUE; // Set the bInheritHandle flag so pipe handles are inherited. 
		saAttr.lpSecurityDescriptor = NULL; 
		CreatePipe( &hReadOutPipe, &hWriteOutPipe, &saAttr, 0 );
		CreatePipe( &hReadErrorPipe, &hWriteErrorPipe, &saAttr, 0 );
		SetHandleInformation(hReadOutPipe, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(hReadErrorPipe, HANDLE_FLAG_INHERIT, 0);
	}

	si.hStdOutput = hWriteOutPipe;
	si.hStdError= hWriteErrorPipe;
	CreateProcessW( NULL,		// No module name (use command line)
			com,				// Command line
			NULL,				// Process handle not inheritable
			NULL,				// Thread handle not inheritable
			TRUE,				// Set handle inheritance to FALSE
			CREATE_NO_WINDOW,	// No nasty console windows
			NULL,				// Use parent's environment block
			NULL,				// Use parent's starting directory 
			&si,				// Pointer to STARTUPINFO structure
			&pi);				// Pointer to PROCESS_INFORMATION structure

	if(pi.hProcess==nullptr)
	{
		SIMUL_CERR << "Error: Could not find the executable for " << WStringToUtf8(com) << std::endl;
		return false;
	}
	HANDLE WaitHandles[] = {pi.hProcess, hReadOutPipe, hReadErrorPipe};

	bool has_errors=false;
	const DWORD BUFSIZE = 4096;
	BYTE buff[BUFSIZE];
	DWORD dwBytesRead, dwBytesAvailable;
	while (true)
	{
		DWORD dwWaitResult = WaitForMultipleObjects(pipeChildProcessOutput ? 3 : 1, WaitHandles, FALSE, 60000L);
	
		// Read from the pipes...
		if (pipeChildProcessOutput)
		{
			while (PeekNamedPipe(hReadOutPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable)
			{
				bool success = ReadFile(hReadOutPipe, buff, BUFSIZE - 1, &dwBytesRead, 0);
				if (!success)
					break;

				std::string str((char *)buff, (size_t)dwBytesRead);
				if (outputDelegate)
					outputDelegate(str);
				else
					std::cout << str.c_str();
			}
			while (PeekNamedPipe(hReadErrorPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable)
			{
				bool success = ReadFile(hReadErrorPipe, buff, BUFSIZE - 1, &dwBytesRead, 0);
				if (!success)
					break;

				std::string str((char *)buff, (size_t)dwBytesRead);
				if (outputDelegate)
					outputDelegate(str);
				else
					std::cerr << str.c_str();
			}
		}

		// Process is done, or we timed out:
		if (dwWaitResult == WAIT_OBJECT_0)
			break;
		if (dwWaitResult == WAIT_TIMEOUT)
			continue;

		// Process wait failed, so output a formatted message:
		if (dwWaitResult == WAIT_FAILED)
		{
			DWORD err = GetLastError();
			char* msg = nullptr;
			// Ask Windows to prepare a standard message for a GetLastError() code:
			DWORD msgSize = FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, err, 0, (char *)&msg, 0, nullptr);
			if (msg != nullptr && msgSize > 0)
			{
				SIMUL_CERR << "Error message: " << msg << std::endl;
			}
			has_errors = true;
			break;
		}
	}

	CloseHandle(hReadOutPipe);
	CloseHandle(hWriteOutPipe);
	CloseHandle(hReadErrorPipe);
	CloseHandle(hWriteErrorPipe);
 	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (!has_errors);
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