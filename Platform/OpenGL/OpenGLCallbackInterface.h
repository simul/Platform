#ifndef HANDLEMOUSEINTERFACE
#define HANDLEMOUSEINTERFACE
class HandleMouseInterface
{
public:
	virtual void	mouseRelease(int button,int x,int y)=0;
	virtual void	mousePress(int button,int x,int y)=0;
	virtual void	mouseDoubleClick(int button,int x,int y)=0;
	virtual void	mouseMove(int x,int y)=0;
	virtual void	mouseWheel(int delta)=0;
	virtual void    KeyboardProc(unsigned int nChar, bool bKeyDown, bool bAltDown)=0;
};
#endif

#ifndef OPENGLCALLBACKINTERFACE_H
#define OPENGLCALLBACKINTERFACE_H
class OpenGLCallbackInterface
{
public:
	//! Add a view. This tells the renderer to create any internal stuff it needs to handle a viewport, so that it is ready when Render() is called. It returns an identifier for that view.
	virtual int AddGLView()=0;
	virtual void RenderGL(int)=0;
	virtual void ResizeGL(int,int,int)=0;
	virtual void InitializeGL()=0;
	virtual void ShutdownGL()=0;
};
#endif