#pragma once

#ifndef DIRECTX11_LIGHT_H
#define DIRECTX11_LIGHT_H

#include "Export.h"
#include "Material.h"
#include "Platform/CrossPlatform/Light.h"

namespace simul
{
	namespace dx11
	{
		class SIMUL_DIRECTX11_EXPORT Light:public crossplatform::Light
		{
		public:
			Light();
			~Light();
			void UpdateLight(const double *mat,float lConeAngle,const float lLightColor[4]) const;
		protected:
			//GLuint mLightIndex;
		};
	}
}
#endif