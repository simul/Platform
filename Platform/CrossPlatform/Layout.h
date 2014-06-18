#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
struct ID3D11InputLayout;
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		class SIMUL_CROSSPLATFORM_EXPORT Layout
		{
		public:
			Layout();
			virtual ~Layout();
			virtual ID3D11InputLayout *AsD3D11InputLayout()
			{
				return 0;
			}
		};
	}
}
