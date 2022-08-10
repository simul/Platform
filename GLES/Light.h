#pragma once

#ifndef GLES_LIGHT_H
#define GLES_LIGHT_H

#include "Export.h"
#include "Platform/CrossPlatform/Light.h"


namespace simul
{
	namespace gles
	{
		class SIMUL_GLES_EXPORT Light:public crossplatform::Light
		{
		public:
			Light();
			~Light();
			void UpdateLight(const double *mat,float lConeAngle,const float lLightColor[4]) const;
		protected:
			GLuint mLightIndex;
		};
	}
}
#endif