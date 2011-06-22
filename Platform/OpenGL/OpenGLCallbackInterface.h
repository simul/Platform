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
	virtual void paintGL()=0;
	virtual void resizeGL(int,int)=0;
	virtual void initializeGL()=0;
};
#endif