#ifndef PLATFORM_CORE_MOUSEHANDLER_H
#define PLATFORM_CORE_MOUSEHANDLER_H

#include <cstdint>
#include <functional>
#include <stdint.h>
#include "Platform/Core/Export.h"
#include "Platform/Core/EnumClassBitwiseOperators.h"

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace core
	{
		enum class MouseButtons : uint16_t
		{
			NoButton = 0,
			LeftButton = 1,
			RightButton = 2,
			MiddleButton = 4,
			Button4 = 8,
			Button5 = 16,
		};
		MouseButtons operator|(MouseButtons a,MouseButtons b)
		{
			return (MouseButtons)((int)a|(int)b);
		}
		enum class KeyboardModifiers : uint16_t
		{
			NoModifier = 0,
			Shift = 1,
			Alt = 2,
			Ctrl = 4,
		};

		// A simple render delegate, it will usually be a function partially bound with std::bind.
		typedef std::function<void()> VoidFnDelegate;
		class PLATFORM_CORE_EXPORT BaseMouseHandler
		{
		public:
			BaseMouseHandler();
			virtual ~BaseMouseHandler();
			virtual void mousePress(MouseButtons button, int x, int y);
			virtual void mouseRelease(MouseButtons button, int x, int y);
			virtual void mouseDoubleClick(MouseButtons button, int x, int y);
			virtual void mouseMove(int x, int y);
			virtual void getMousePosition(int &x, int &y) const;
			virtual void mouseWheel(int delta, int modifiers);
			virtual void KeyboardProc(KeyboardModifiers modifiers, bool bKeyDown);
			virtual MouseButtons getMouseButtons() const;
			void SetViewSize(int w, int h);

		public:
			VoidFnDelegate updateViews;
			VoidFnDelegate valuesChanged;

		protected:
			float fDeltaX, fDeltaY;
			int MouseX, MouseY;
			int view_width;
			int view_height;

			MouseButtons mouseButtons;
			void UpdateViews();
			void ValuesChanged();
		};
	}
}
#endif
