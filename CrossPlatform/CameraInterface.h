#ifndef SIMUL_GRAPH_CAMERA_CAMERAINTERFACE_H
#define SIMUL_GRAPH_CAMERA_CAMERAINTERFACE_H
#include "Platform/CrossPlatform/Export.h"

namespace simul
{
	namespace math
	{
		class Vector3;
	}
	namespace crossplatform
	{
		struct CameraViewStruct;
		///
		enum WhichEye
		{
			LEFT_EYE,RIGHT_EYE
		};
		// An interface for receiving camera information.
		class CameraOutputInterface
		{
		public:
			virtual float GetHorizontalFieldOfViewDegrees() const=0;
			virtual float GetVerticalFieldOfViewDegrees() const=0;

			virtual const float *MakeViewMatrix() const=0;
			virtual const float *MakeDepthReversedProjectionMatrix(float aspect) const=0;
			virtual const float *MakeProjectionMatrix(float aspect) const=0;
			
			virtual const float *MakeStereoProjectionMatrix(WhichEye whichEye,float aspect,bool ReverseDepth) const=0;
			virtual const float *MakeStereoViewMatrix(WhichEye whichEye) const=0;
			virtual void SetCameraViewStruct(const CameraViewStruct &c)=0;
			virtual const CameraViewStruct &GetCameraViewStruct() const=0;
		};
		// An interface for receiving or altering camera information.
		class CameraInterface:public CameraOutputInterface
		{
		public:
			virtual void SetHorizontalFieldOfViewDegrees(float f)=0;
			virtual void SetVerticalFieldOfViewDegrees(float f)=0;
			virtual void LookInDirection(const float *view_dir,const float *view_up)=0;
			virtual void LookInDirection(const float *view_dir)=0;
		};
	}
}

#endif