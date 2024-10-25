#pragma once

#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif


namespace platform::windows
{
	std::string GetLastErrorToString(DWORD error)
	{
#if defined(_WIN32)
		if (error != 0)
		{
			char *formatedMessage = nullptr;
			DWORD formatedMessageSize = FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, error, 0, (char *)&formatedMessage, 0, nullptr);

			if (formatedMessage != nullptr && formatedMessageSize > 0)
			{
				std::string result(formatedMessage, formatedMessageSize);
				LocalFree(formatedMessage);
				return result;
			}
		}
#endif
		return std::string();
	}
}
