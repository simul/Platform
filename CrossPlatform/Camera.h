#ifndef SIMUL_GRAPH_CAMERA_CAMERA_H
#define SIMUL_GRAPH_CAMERA_CAMERA_H
#include "Simul/Geometry/OrientationInterface.h"
#include "Simul/Geometry/Orientation.h"
#include "Simul/Platform/CrossPlatform/CameraInterface.h"
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/Shaders/SL/CppSl.sl"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif
namespace simul
{
	namespace crossplatform
	{
		struct ViewStruct;
	}
	namespace crossplatform
	{
/// How to interpret the depth texture.
		enum DepthTextureStyle
		{
			/// Depth textures are interpreted as representing the z-output of the projection matrix transformation.
			PROJECTION
			/// Depth textures are interpreted as representing a linear distance in the z-direction from the near clipping plane.
			,DISTANCE_FROM_NEAR_PLANE
		};
		vec4 SIMUL_CROSSPLATFORM_EXPORT GetDepthToDistanceParameters(crossplatform::DepthTextureStyle depthTextureStyle, const math::Matrix4x4 &proj, float max_dist_metres);
		vec4 SIMUL_CROSSPLATFORM_EXPORT GetDepthToDistanceParameters(crossplatform::DepthTextureStyle depthTextureStyle, const crossplatform::ViewStruct &viewStruct, float max_dist_metres);
		vec4 SIMUL_CROSSPLATFORM_EXPORT GetDepthToDistanceParameters(const crossplatform::ViewStruct &viewStruct,float max_dist_metres);
		/// A useful class to represent a view frustum.
		struct SIMUL_CROSSPLATFORM_EXPORT Frustum
		{
			vec4 tanHalfFov;		//xy= tangent of half-angle, zw=offset
			float nearZ, farZ;
			bool reverseDepth;
		};
		/// A struct to represent the state of a mouse-controlled camera.
		struct SIMUL_CROSSPLATFORM_EXPORT MouseCameraState
		{
			MouseCameraState()
				:forward_back_spd(0.f)
				,right_left_spd(0.f)
				,up_down_spd(0.f)
			{
			}
			// state
			float forward_back_spd;
			float right_left_spd;
			float up_down_spd;
		};
		struct FovPort
		{
			float upTan;
			float downTan;
			float leftTan;
			float rightTan;
		};

		/// A struct to provide input to a mouse-controlled camera.
		/// Store persistently, as it needs to use the difference between current
		/// and previous mouse positions.
		struct SIMUL_CROSSPLATFORM_EXPORT MouseCameraInput
		{
		/// Identify the buttons
			enum 
			{
				LEFT_BUTTON		=0x01
				,MIDDLE_BUTTON	=0x02
				,RIGHT_BUTTON	=0x04
				,WHEEL			=0x08
			};
			MouseCameraInput()
				:MouseX(0)
				,MouseY(0)
				,LastMouseX(0)
				,LastMouseY(0)
				,MouseButtons(0)
				,forward_back_input(0.f)
				,right_left_input(0.f)
				,up_down_input(0.f)
			{
			}
			// input
			int MouseX;
			int MouseY;
			int LastMouseX;
			int LastMouseY;
			int MouseButtons;
			float forward_back_input;
			float right_left_input;
			float up_down_input;
		};
		enum Projection
		{
			FORWARD,DEPTH_REVERSE
		};
		// A struct containing information on how to interpret camera properties in a rendered view.
		struct SIMUL_CROSSPLATFORM_EXPORT CameraViewStruct
		{
			CameraViewStruct()
				:exposure(1.f)
				,gamma(0.44f)
				,projection(DEPTH_REVERSE)
				,nearZ(1.f)
				,farZ(300000.f)
				,InfiniteFarPlane(false)
			{
			}
			float exposure;
			float gamma;
			Projection projection;
			float nearZ,farZ;
			bool InfiniteFarPlane;
		};

		SIMUL_CROSSPLATFORM_EXPORT Frustum GetFrustumFromProjectionMatrix(const float *mat);
		SIMUL_CROSSPLATFORM_EXPORT float GetDepthAtDistance(float thresholdMetres, const float *matproj);
		SIMUL_CROSSPLATFORM_EXPORT void ModifyProjectionMatrix(float *mat,float new_near_plane,float new_far_plane);
		SIMUL_CROSSPLATFORM_EXPORT void ConvertReversedToRegularProjectionMatrix(float *proj);
		
		//! Make an inverse viewProj matrix, which converts clip position to worldspace direction.
		extern SIMUL_CROSSPLATFORM_EXPORT void MakeInvViewProjMatrix(float *ivp,const float *v,const float *p);
		//! Make an inverse worldViewProj matrix, which converts clip position to worldspace direction.
		extern SIMUL_CROSSPLATFORM_EXPORT void MakeInvWorldViewProjMatrix(float *ivp,const float *world,const float *v,const float *p);
		//! Make a viewProj matrix, which converts worldspace into clip position.
		extern SIMUL_CROSSPLATFORM_EXPORT void MakeViewProjMatrix(float *vp,const float *view,const float *proj);
		extern SIMUL_CROSSPLATFORM_EXPORT void MakeWorldViewMatrix(float *wv, const float *w, const float *v);
		//! Make a viewProj matrix, but ignore the view position.
		extern SIMUL_CROSSPLATFORM_EXPORT void MakeCentredViewProjMatrix(float *vp,const float *view,const float *proj);
		//! Make a worldViewProj matrix.
		extern SIMUL_CROSSPLATFORM_EXPORT void MakeWorldViewProjMatrix(float *wvp,const float *world,const float *view,const float *proj);
		
		extern SIMUL_CROSSPLATFORM_EXPORT void MakeCentredWorldViewProjMatrix(float *wvp,const float *world,const float *view,const float *proj);
		//! Get the camera position, direction, and up-vector, from a view matrix v.
		extern SIMUL_CROSSPLATFORM_EXPORT void GetCameraPosVector(const float *v,float *dcam_pos,float *view_dir,float *up=0);
		//! Get the camera position from a view matrix.
		extern SIMUL_CROSSPLATFORM_EXPORT const float *GetCameraPosVector(const float *v);

		extern SIMUL_CROSSPLATFORM_EXPORT void UpdateMouseCamera(	class Camera *cam
															,float time_step
															,float cam_spd
															,MouseCameraState &state
															,MouseCameraInput &input
															,float max_height);
		
		math::Matrix4x4 SIMUL_CROSSPLATFORM_EXPORT MatrixLookInDirection(const float *dir,const float *view_up,bool lefthanded);
		// The x-y min and max (in z and w) of the bounds of the projection on the given face of a cubemap.
		vec4 SIMUL_CROSSPLATFORM_EXPORT GetFrustumRangeOnCubeFace(int face,const float *invViewProj);
		void SIMUL_CROSSPLATFORM_EXPORT MakeCubeMatrices(math::Matrix4x4 mat[],const float *cam_pos,bool ReverseDepth,bool ReverseDirection);
		void SIMUL_CROSSPLATFORM_EXPORT GetCubeMatrix(float *mat4x4,int i,bool ReverseDepth,bool ReverseDirection);
		void SIMUL_CROSSPLATFORM_EXPORT GetCubeMatrixAtPosition(float *mat4x4,int i,vec3 cam_pos,bool ReverseDepth,bool ReverseDirection);
		
		void SIMUL_CROSSPLATFORM_EXPORT GetCubeInvViewProjMatrix(float *mat4x4,int i,bool ReverseDepth,bool ReverseDirection);

		extern SIMUL_CROSSPLATFORM_EXPORT math::Matrix4x4 MakeOrthoProjectionMatrix(float left,
 																					float right,
 																					float bottom,
 																					float top,
 																					float nearVal,
 																					float farVal);
		//! A camera class. The orientation has the z-axis facing backwards, the x-axis right and the y-axis up.
		SIMUL_CROSSPLATFORM_EXPORT_CLASS Camera :
			public simul::geometry::OrientationInterface,
			public simul::crossplatform::CameraInterface
		{
			CameraViewStruct cameraViewStruct;
		public:
			Camera();
			virtual ~Camera();
			
			META_BeginProperties
				META_RangeProperty(float,HorizontalFieldOfViewInRadians,3.1416f*90.f	,0,3.1416f*179.f,"Horizontal Field Of View In Radians")
				META_RangeProperty(float,VerticalFieldOfViewInRadians	,0				,0,3.1416f*179.f,"Vertical Field Of View In Radians")
				META_Property(simul::geometry::SimulOrientation	,Orientation	,"Orientation")
			META_EndProperties
	
			void SetCameraViewStruct(const CameraViewStruct &c);
			const CameraViewStruct &GetCameraViewStruct() const;
			// virtual from OrientationInterface
			virtual const float *GetOrientationAsPermanentMatrix() const;		//! Permanent: this means that for as long as the interface exists, the address is valid.
			virtual const float *GetRotationAsQuaternion() const;
			virtual const float *GetPosition() const;

			virtual const float *MakeViewMatrix() const;
			virtual const float *MakeDepthReversedProjectionMatrix(float aspect) const;
			virtual const float *MakeProjectionMatrix(float aspect) const;
			
			virtual const float *MakeStereoProjectionMatrix(WhichEye whichEye,float aspect,bool ReverseDepth) const;
			virtual const float *MakeStereoViewMatrix(WhichEye whichEye) const;

			vec3 ScreenPositionToDirection(float x,float y,float aspect);

			virtual void SetOrientationAsMatrix(const float *);
			virtual void SetOrientationAsQuaternion(const float *);
			virtual void SetPosition(const float *);
			virtual void LookInDirection(const float *view_dir,const float *view_up);
			virtual void LookInDirection(const float *view_dir);
			virtual void SetPositionAsXYZ(float,float,float);
			virtual void Move(const float *);
			virtual void LocalMove(const float *);
			virtual void Rotate(float,const float *);
			virtual void LocalRotate(float,const float *);
			virtual void LocalRotate(const float *);

			virtual bool TimeStep(float delta_t);
			//
			float GetHorizontalFieldOfViewDegrees() const;
			void SetHorizontalFieldOfViewDegrees(float f);
			float GetVerticalFieldOfViewDegrees() const;
			void SetVerticalFieldOfViewDegrees(float f);
			static void CreateViewMatrix(float *mat, const float *view_dir, const float *view_up,const float *pos=0);
			static const float *MakeDepthReversedProjectionMatrix(float h, float v, float zNear, float zFar);
			static const float *MakeDepthReversedProjectionMatrix(const FovPort &fovPort, float zNear, float zFar);
			static const float *MakeProjectionMatrix(float h, float v, float zNear, float zFar);
			static const float *MakeProjectionMatrix(const FovPort &fovPort, float zNear, float zFar);
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif
