#include "Platform/Core/BaseMouseHandler.h"

using namespace platform;
using namespace core;

BaseMouseHandler::BaseMouseHandler()
	:fDeltaX(0)
	,fDeltaY(0)
	,MouseX(0)
	,MouseY(0)
	,view_width(750)
	,view_height(750)
	,mouseButtons(core::MouseButtons::NoButton)
{
}

BaseMouseHandler::~BaseMouseHandler()
{
}

void BaseMouseHandler::mouseRelease(MouseButtons button, int x, int y)
{
	mouseButtons&=~button;
	MouseX=x;
	MouseY=y;
	UpdateViews();
}

void BaseMouseHandler::mousePress(MouseButtons button, int x, int y)
{
	mouseButtons|=button;
	MouseX=x;
	MouseY=y;
	UpdateViews();
}

void BaseMouseHandler::mouseDoubleClick(MouseButtons, int x, int y)
{
	MouseX=x;
	MouseY=y;
	UpdateViews();
}
void BaseMouseHandler::UpdateViews()
{
	if(updateViews)
		updateViews();
}

void BaseMouseHandler::ValuesChanged()
{
	if (valuesChanged)
		valuesChanged();
}

void BaseMouseHandler::mouseMove(int x,int y)
{
	int dx=x-MouseX;
	int dy=y-MouseY;
	fDeltaX=dx/float(view_width);
	fDeltaY=dy/float(view_height);
	MouseX=x;
	MouseY=y;
	UpdateViews();
}

void BaseMouseHandler::getMousePosition(int &x,int &y) const
{
	x=MouseX;
	y=MouseY;
}

void BaseMouseHandler::mouseWheel(int ,int )
{
	UpdateViews();
}

void BaseMouseHandler::SetViewSize(int w, int h)
{
	view_width = w;
	view_height= h;
}


void BaseMouseHandler::KeyboardProc(KeyboardModifiers modifiers, bool bKeyDown)
{
	if(updateViews)
		UpdateViews();
}

MouseButtons BaseMouseHandler::getMouseButtons() const
{
	return mouseButtons;
}