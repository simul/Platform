#pragma once

#ifndef _LIGHT_CACHE_H
#define _LIGHT_CACHE_H

#include <map>
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/Mesh.h"
#include "Simul/Platform/CrossPlatform/Material.h"

namespace simul
{
	namespace crossplatform
	{
		struct AnimCurve
		{
			virtual ~AnimCurve(){}
			virtual float Evaluate(double time) const=0;
		};
		// Property cache, value and animation curve.
		struct SIMUL_CROSSPLATFORM_EXPORT PropertyChannel
		{
			PropertyChannel() : mValue(0.0f),mAnimCurve(NULL) {}
			~PropertyChannel();
			//! Query the channel value at specific time.
			float Get(double pTime) const;
			float mValue;
			AnimCurve * mAnimCurve;
		};
		// Cache for  lights
		class SIMUL_CROSSPLATFORM_EXPORT Light
		{
		public:
			Light();
			virtual ~Light();
			// Set ambient light. Turn on light0 and set its attributes to default (white directional light in Z axis).
			// If the scene contains at least one light, the attributes of light0 will be overridden.
			//static void IntializeEnvironment(const fbxsdk_201x_x_x::FbxColor & pAmbientLight);

			// Draw a geometry (sphere for point and directional light, cone for spot light).
			// And set light attributes.
			void SetLight(const double *,double time) const;
///
			enum LightType
			{
				ePoint, 
				eDirectional, 
				eSpot,
				eArea,
				eVolume
			};
			void setType(LightType type)
			{
				mType=type;
			}
			virtual void UpdateLight(const double *mat,float lConeAngle,const float lLightColor[4]) const=0;
			static int sLightCount;         // How many lights in this scene.
			LightType mType;
			PropertyChannel mColorRed;
			PropertyChannel mColorGreen;
			PropertyChannel mColorBlue;
			PropertyChannel mConeAngle;
		protected:
		};
	}
}
#endif