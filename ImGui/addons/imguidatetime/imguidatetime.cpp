#include "imguidatetime.h"
#include <imgui_internal.h>

#include <time.h>  // very simple and common plain C header file (it's NOT the c++ <sys/time.h>). If not available it's probably better to implement it yourself rather than modifying this file.
#include <ctype.h> // toupper()

namespace ImGui
{

    inline struct tm GetCurrentDate()
    {
        time_t now;
        time(&now);
        tm tm_now;
        localtime_s(&tm_now, &now);
        return tm_now;
    }

    // Tip: we modified tm (its fields can even be negative!) and then we call this method to retrieve a valid date
    inline static void UpdateDate(struct tm& date)
    {
        date.tm_isdst = -1;   // This tries to detect day time savings too
        time_t tmp = mktime(&date);
        localtime_s(&date, &tmp);
    }

    struct tm GetDateZero()
    {
        struct tm date;
        memset(&date, 0, sizeof(tm));     // Mandatory for emscripten. Please do not remove!
        date.tm_isdst = -1;
        date.tm_sec = 0;	//	 Seconds.	[0-60] (1 leap second)
        date.tm_min = 0;	//	 Minutes.	[0-59]
        date.tm_hour = 0;	//	 Hours.	[0-23]
        date.tm_mday = 0;	//	 Day.		[1-31]
        date.tm_mon = 0;	//	 Month.	[0-11]
        date.tm_year = 0;	//	 Year	- 1900.
        date.tm_wday = 0;  //	 Day of week.	[0-6]
        date.tm_yday = 0;	//	 Days in year.[0-365]
        return date;
    }

    int DaysInMonth(struct tm& date)
    {
        const static int num_days_per_month[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
        int days_in_month = num_days_per_month[date.tm_mon];
        if (days_in_month == 28)
        {
            const int year = 1900 + date.tm_year;
            const bool bis = ((year % 4) == 0) && ((year % 100) != 0 || (year % 400) == 0);
            if (bis) days_in_month = 29;
        }
        return days_in_month;
    }

    bool SameMonth(struct tm& d1, struct tm& d2)
    {
        return d1.tm_mon == d2.tm_mon && d1.tm_year == d2.tm_year;
    }

    bool InputDate(const char* label, struct tm& date, const char* format, bool close_on_leave, const char* left_button, const char* right_button, const char* today_button)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;

        static ImGuiID last_open_combo_id = 0;

#ifdef IMGUIDATETIME_DISPLAYSUNDAYFIRST
        static bool sunday_first = true; // display sundays first (like before)
#else
        static bool sunday_first = false; // display mondays first as ISO 8601 suggest
#endif
        static const int name_buf_size = 64;
        static char day_names[7][name_buf_size] = { "" };
        static char month_names[12][name_buf_size] = { "" };
        static int max_month_width_index = -1;

        // Initialize day/months names
        if (max_month_width_index == -1)
        {
            struct tm tmp = GetDateZero();
            for (int i = 0; i < 7; i++)
            {
                tmp.tm_wday = i;
                char* day_name = &day_names[i][0];
                strftime(day_name, name_buf_size, "%A", &tmp);
                // Warning: This part won't work for multibyte UTF-8 chars:
                // We want to keep the first letter only here, and we want it to be uppercase (because some locales provide it lowercase)
                if (strlen(day_name) == 0)
                {
                    static const char fallbacks[7] = { 'S','M','T','W','T','F','S' };
                    day_name[0] = fallbacks[i];
                    day_name[1] = '\0';
                }
                else {
                    day_name[0] = toupper(day_name[0]);
                    day_name[1] = '\0';
                }
            }
            float max_month_width = 0;
            for (int i = 0; i < 12; i++)
            {
                tmp.tm_mon = i;
                char* month_name = &month_names[i][0];
                strftime(month_name, name_buf_size, "%B", &tmp);
                if (strlen(month_name) == 0)
                {
                    static const char* fallbacks[12] = { "January","February","March","April","May","June","July","August","September","October","November","December" };
                    strcpy_s(month_name, sizeof(month_names[i]), fallbacks[i]);
                }
                else month_name[0] = toupper(month_name[0]);
                float mw = CalcTextSize(month_name).x;
                if (max_month_width < mw)
                {
                    max_month_width = mw;
                    max_month_width_index = i;
                }
            }
        }

        // Const values from inputs
        const char* arrow_left = left_button ? left_button : "<";
        const char* arrow_right = right_button ? right_button : ">";
        const char* today = today_button ? today_button : "o";
        char currentText[128];
        strftime(currentText, sizeof(currentText), format, &date);

        // Const values for layout
        const float arrow_left_width = CalcTextSize(arrow_left).x;
        const float arrow_right_width = CalcTextSize(arrow_right).x;
        const ImVec2 space = GetStyle().ItemSpacing;
        const float month_width = arrow_left_width + CalcTextSize(month_names[max_month_width_index]).x + arrow_right_width + 2 * space.x;

        // Use a fixed popup size to avoid any layout stretching policy
        const ImVec2 label_size = CalcTextSize(label, NULL, true);
        const float year_width = arrow_left_width + CalcTextSize("9990").x + arrow_right_width + 2 * space.x;
        const float calendar_width = month_width + year_width + CalcTextSize(today).x + 3 * space.x + 30.f;
        const float textbox_width = label_size.x + CalcTextSize(currentText).x;
        const float w = calendar_width > textbox_width ? calendar_width : textbox_width;
        const float h = 8 * label_size.y + 7 * space.y + 2 * style.FramePadding.y + 20;
        SetNextWindowSize(ImVec2(w, h));
        SetNextWindowSizeConstraints(ImVec2(w, h), ImVec2(w, h));

        bool value_changed = false;
        if (!BeginCombo(label, currentText, 0))
            return value_changed;

        const ImGuiID id = window->GetID(label);
        static struct tm d = GetDateZero();
        if (last_open_combo_id != id)
        {
            last_open_combo_id = id;
            d = date.tm_mday == 0 ? GetCurrentDate() : date;
            d.tm_mday = 1; // move at 1st day of the month: mandatory for the algo
            UpdateDate(d); // now d.tm_wday is correct
        }

        PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
        Spacing();

        static const ImVec4 transparent(1, 1, 1, 0);
        PushStyleColor(ImGuiCol_Button, transparent);

        PushID(label);
        int cid = 0;

        // Month selector
        PushID(cid++);
        if (SmallButton(arrow_left))
        {
            d.tm_mon -= 1;
            UpdateDate(d);
        }
        SameLine();
        Text("%s", month_names[d.tm_mon]);
        SameLine(month_width + ImGui::GetStyle().WindowPadding.x);
        if (SmallButton(arrow_right))
        {
            d.tm_mon += 1;
            UpdateDate(d);
        }
        PopID();

        SameLine(ImGui::GetWindowWidth() - month_width - 2 * ImGui::GetStyle().WindowPadding.x);

        // Year selector
        PushID(cid++);
        if (SmallButton(arrow_left))
        {
            d.tm_year -= 1;
            if (d.tm_year < 0)
                d.tm_year = 0;
            UpdateDate(d);
        }
        SameLine();
        Text("%d", 1900 + d.tm_year);
        SameLine();
        if (SmallButton(arrow_right))
        {
            d.tm_year += 1;
            UpdateDate(d);
        }
        PopID();

        SameLine();

        // Today selector
        PushID(cid++);
        if (SmallButton(today))
        {
            d = GetCurrentDate();
            d.tm_mday = 1;
            UpdateDate(d);
        }
        PopID();

        Spacing();

        PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_HeaderHovered]);
        PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_HeaderActive]);

        Separator();

        bool date_clicked = false;

        IM_ASSERT(d.tm_mday == 1);    // Otherwise the algo does not work
        // Display items
        char cur_day_str[4] = "";
        int days_in_month = DaysInMonth(d);
        struct tm cur_date = GetCurrentDate();
        PushID(cid++);
        for (int dwi = 0; dwi < 7; dwi++)
        {
            int dw = sunday_first ? dwi : (dwi + 1) % 7;
            bool sunday = dw == 0;

            BeginGroup(); // one column per day
            // -- Day name
            if (sunday) // style sundays in red
            {
                const ImVec4& tc = style.Colors[ImGuiCol_Text];
                const float l = (tc.x + tc.y + tc.z) * 0.33334f;
                PushStyleColor(ImGuiCol_Text, ImVec4(l * 2.f > 1 ? 1 : l * 2.f, l * .5f, l * .5f, tc.w));
            }
            Text(" %s", day_names[dw]);
            Spacing();
            // -- Day numbers
            int cur_day = dw - d.tm_wday; // tm_wday is in [0,6]. For this to work here d must point to the 1st day of the month: i.e.: d.tm_mday = 1;
            for (int row = 0; row < 6; row++)
            {
                int cday = cur_day + 7 * row;
                if (cday >= 0 && cday < days_in_month)
                {
                    PushID(cid + 7 * row + dwi);
                    if (!sunday_first && (dw > 0 && d.tm_wday == 0 && row == 0)) // 1st == synday case
                        TextUnformatted(" ");
                    if (cday < 9) sprintf_s(cur_day_str, sizeof(cur_day_str), " %.1d", (unsigned char)cday + 1);
                    else sprintf_s(cur_day_str, sizeof(cur_day_str), "%.2d", (unsigned char)cday + 1);

                    // Highligth input date and today
                    bool is_today = cday + 1 == cur_date.tm_mday && SameMonth(cur_date, d);
                    bool is_input_date = cday + 1 == date.tm_mday && SameMonth(date, d);
                    if (is_input_date)
                        PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_HeaderActive]);
                    else if (is_today)
                        PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_TableHeaderBg]);

                    // Day number as a button
                    if (SmallButton(cur_day_str))
                    {
                        date_clicked = true;
                        struct tm date_out = d;
                        date_out.tm_mday = cday + 1;
                        UpdateDate(date_out);
                        if (difftime(mktime(&date_out), mktime(&date)) != 0)
                        {
                            date = date_out;
                            value_changed = true;
                        }
                    }
                    if (is_today || is_input_date)
                        PopStyleColor();
                    PopID();
                }
                else if ((sunday_first || !sunday) && cday < days_in_month)
                    TextUnformatted(" ");
            }
            if (sunday) // sundays
                PopStyleColor();
            EndGroup();

            // horizontal space between columns
            if (dwi != 6)
                SameLine(ImGui::GetWindowWidth() - (6 - dwi) * (ImGui::GetWindowWidth() / 7.f));
        }
        PopID();

        PopID(); // ImGui::PushID(label)

        PopStyleColor(2);
        PopStyleColor();
        PopStyleVar();

        bool close_combo = date_clicked;
        if (close_on_leave && !close_combo) {
            const float distance = g.FontSize * 1.75f;
            ImVec2 pos = GetWindowPos();
            pos.x -= distance; pos.y -= distance;
            ImVec2 size = GetWindowSize();
            size.x += 2.f * distance; size.y += 2.f * distance;
            const ImVec2& mouse_pos = GetIO().MousePos;
            if (mouse_pos.x<pos.x || mouse_pos.y<pos.y || mouse_pos.x>pos.x + size.x || mouse_pos.y>pos.y + size.y) {
                close_combo = true;
            }
        }
        if (close_combo)
            CloseCurrentPopup();

        EndCombo();

        return value_changed;
    }

    bool InputTime(const char* label, struct tm& date, bool with_seconds, bool close_on_leave)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        bool value_changed = false;
        PushID(label);
        PushMultiItemsWidths(with_seconds ? 3 : 2, CalcItemWidth());
        {
            PushID(0);
            value_changed |= DragInt("", &date.tm_hour, 1, 0, 23, "%d", ImGuiSliderFlags_AlwaysClamp);
            PopID();
            PopItemWidth();
            SameLine(0, 0); Text(":"); SameLine(0, 0);
            PushID(1);
            value_changed |= DragInt("", &date.tm_min, 1, 0, 59, "%d", ImGuiSliderFlags_AlwaysClamp);
            PopID();
            PopItemWidth();

            if (with_seconds)
            {
                SameLine(0, 0); Text(":"); SameLine(0, 0);
                PushID(2);
                value_changed |= DragInt("", &date.tm_sec, 1, 0, 59, "%d", ImGuiSliderFlags_AlwaysClamp);
                PopID();
                PopItemWidth();
            }
        }
        PopID();
        return value_changed;
    }

    bool InputDateTime(const char* label, struct tm& date, const char* dateformat, bool with_seconds, bool close_on_leave, const char* left_button, const char* right_button, const char* today_button)
    {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const float min_time_width = (with_seconds ? 3 : 2) * CalcTextSize("23").x + (with_seconds ? 2 : 1) * CalcTextSize(":").x;
        const float best_time_width = (with_seconds ? 0.35f : 0.25f) * CalcItemWidth();
        const float time_width = best_time_width > min_time_width ? best_time_width : min_time_width;
        const float date_width = CalcItemWidth() - GetStyle().ItemSpacing.x - time_width - g.Style.FramePadding.x;

        bool value_changed = false;
        PushID(label);

        PushItemWidth(date_width);
        PushID(0);
        value_changed |= InputDate("", date, dateformat, close_on_leave, left_button, right_button, today_button);
        PopID();
        PopItemWidth();

        SameLine();

        PushItemWidth(time_width);
        PushID(1);
        value_changed |= InputTime("", date, with_seconds, close_on_leave);
        PopID();
        PopItemWidth();

        PopID();
        return value_changed;
    }


} // namespace ImGui