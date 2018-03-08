#pragma once

#ifndef DIRECTX12_LIGHT_H
#define DIRECTX12_LIGHT_H

#include "Export.h"
#include "Material.h"
#include "Simul/Platform/CrossPlatform/Light.h"

namespace simul
{
	namespace dx12
	{
		class SIMUL_DIRECTX12_EXPORT Light:public crossplatform::Light
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