// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

// This below work is a rewrite of the imguidatechooser
// See https://github.com/Flix01/imgui/tree/imgui_with_addons/addons/imguidatechooser

#ifndef IMGUIDATETIME_H_
#define IMGUIDATETIME_H_

#ifndef IMGUI_API
#include <imgui.h>
#endif

struct tm; // defined in <time.h>

namespace ImGui {

	// See date formats https://man7.org/linux/man-pages/man1/date.1.html
	IMGUI_API bool InputDate(const char* label, struct tm& date, const char* format = "%d/%m/%Y", bool close_on_leave = true, const char* left_button = "<", const char* right_button = ">", const char* today_button = "o");

	IMGUI_API bool InputTime(const char* label, struct tm& date, bool with_seconds = false, bool close_on_leave = true);

	IMGUI_API bool InputDateTime(const char* label, struct tm& date, const char* dateformat = "%d/%m/%Y %H:%M", bool with_seconds = false, bool close_on_leave = true, const char* left_button = "<", const char* right_button = ">", const char* today_button = "o");

} // namespace ImGui

#endif