#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
namespace simul
{
	namespace dx11
	{
		class Layout:public crossplatform::Layout
		{
			ID3D11InputLayout *d3d11InputLayout;
		public:
			Layout();
			virtual ~Layout();
			virtual ID3D11InputLayout *AsD3D11InputLayout()
			{
				return d3d11InputLayout;
			}
		};
	}
}