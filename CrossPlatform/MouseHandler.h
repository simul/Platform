#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/Quaterniond.h"
#include "Platform/Core/BaseMouseHandler.h"
#include "Platform/CrossPlatform/Camera.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace platform
{
	namespace crossplatform
	{
		///
		enum CameraMode
		{
			FLYING,		//The camera flies like an aircraft.
			LOOKAROUND,	//The camera has 6 degrees of freedom for translation and rotation.
			CENTRED		//The camera orbits about a centred position.
		};

		enum CameraSpactialCoordinates
		{
			EUCLIDEAN,
			SPHERICAL,

			CARTESIAN = EUCLIDEAN
		};

		class SIMUL_CROSSPLATFORM_EXPORT MouseHandler : public platform::core::BaseMouseHandler
		{
		public:
			MouseHandler(bool y_vertical=false);
			virtual ~MouseHandler();
			void setCentre(const float *c);
			void setShiftMultiplier(float c)
			{
				shift_multiplier=c;
			}
			void setEnginePose(const posed& pose);
			void setLocalRadiusKmCallback(std::function<float(float)> pfn);
			void setAltitudeRange(float m, float M);
			void SetYVertical(bool v)
			{
				y_vertical = v;
			}

			void mouseMove(int x, int y);
			void mouseWheel(int delta, int modifiers);
			void KeyboardProc(unsigned int nChar, bool bKeyDown, bool bAltDown);

			float getFov() const;
			void setFov(float f);
			void Update(float time_step);
			void SetCameraDamping(float d)
			{
				CameraDamping = d;
			}
			float GetSpeed() const;
			void SetSpeedFactor(float s);
			float GetSpeedFactor() const;
			platform::crossplatform::Camera *GetCamera();
			void getMousePosition(int &x, int &y) const;
			bool getLeftButton() const;
			bool getRightButton() const;
			bool getMiddleButton() const;

			//! @brief Given the last mouse position, what direction is the mouse pointer in?
			vec3 getMouseDirection(int x, int y, int viewport_x, int viewport_y) const;

			CameraMode &GetCameraMode() { return cameraMode; }
			CameraSpactialCoordinates &GetCameraSpactialCoordinates() { return cameraSpatial; }

		protected:
			int step_rotate_x, step_rotate_y;
			CameraMode cameraMode;
			CameraSpactialCoordinates cameraSpatial = CameraSpactialCoordinates::EUCLIDEAN;
			std::function<float(float)> LocalRadiusKmCallback;
			posed enginePose;
			float centre[3];
			float CameraDamping;
			float minAlt, maxAlt;
			float speed_factor = 1.0f;
			bool y_vertical = false;
			float aspect;
			platform::crossplatform::Camera *camera;
			bool move_forward, move_backward, move_left, move_right, move_up, move_down, shift_down, alt_down;
			float up_down_spd;
			float forward_back_spd;
			float right_left_spd;
			float shift_multiplier=100.0f;
			float lastTimeStep=0.f;
			int wheel_forward=0;
			int wheel_backward=0;
		};

	}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif