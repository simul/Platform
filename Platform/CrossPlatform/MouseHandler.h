#pragma once
#include <functional>
#include "Simul/Platform/CrossPlatform/Export.h"

#include "Simul/Base/SmartPtr.h"
#include "Simul/Base/HandleMouseInterface.h"
#include "Simul/Platform/CrossPlatform/Camera.h"

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif

namespace simul
{
	namespace crossplatform
	{
		///
		enum CameraMode
		{
			FLYING,LOOKAROUND,CENTRED
		};
		// A simple render delegate, it will usually be a function partially bound with std::bind.
		typedef std::function<void()> UpdateViewsDelegate;

		class SIMUL_CROSSPLATFORM_EXPORT MouseHandler: public simul::base::HandleMouseInterface
		{
		public:
			MouseHandler();
			~MouseHandler();
			void	setSpeed(float s);
			void	setCentre(const float *c);
			void	setCameraMode(CameraMode c);
			void	setAltitudeRange(float m,float M);

			void	mousePress(int button,int x,int y);
			void	mouseRelease(int button,int x,int y);
			void	mouseDoubleClick(int button,int x,int y);
			void	mouseMove(int x,int y);
			void	mouseWheel(int delta);
			void	KeyboardProc(unsigned int nChar, bool bKeyDown, bool bAltDown);

			float	getFov() const{return fov;}
			void	setFov(float f) ;
			void	Update(float time_step);
			void	SetCameraDamping(float d)
			{
				CameraDamping=d;
			}
			float GetSpeed() const;
			simul::crossplatform::Camera *GetCamera();
			void	getMousePosition(int &x,int &y) const;
			bool	getLeftButton() const;
			bool	getRightButton() const;
			bool	getMiddleButton() const;
			UpdateViewsDelegate updateViews;
		protected:
			int step_rotate_x,step_rotate_y;
			CameraMode cameraMode;
			float centre[3];
			float CameraDamping;
			float fDeltaX,fDeltaY;
			float minAlt,maxAlt;
			float fov;
			float speed_factor;
			int MouseX,MouseY;
			int MouseButtons;
			bool y_vertical;
			float aspect;
			simul::crossplatform::Camera *camera;
			bool move_forward,move_backward,move_left,move_right,move_up,move_down,shift_down,alt_down;
			float up_down_spd;
			float forward_back_spd;
			float right_left_spd;
		};

	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif