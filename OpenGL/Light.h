#pragma once

#ifndef OPENGL_LIGHT_H
#define OPENGL_LIGHT_H

#include "Export.h"
#include "Platform/CrossPlatform/Light.h"
#include "glad/glad.h"

namespace platform
{
	namespace opengl
	{
		class SIMUL_OPENGL_EXPORT Light:public crossplatform::Light
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