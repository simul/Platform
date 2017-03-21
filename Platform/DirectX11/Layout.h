#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
//! Forward declaration for this, means we needn't include the dx11 headers yet.
enum D3D_PRIMITIVE_TOPOLOGY;
namespace simul
{
	namespace dx11
	{
		class Layout:public crossplatform::Layout
		{
			ID3D11InputLayout* previousInputLayout;
			//D3D_PRIMITIVE_TOPOLOGY topology;
			D3D_PRIMITIVE_TOPOLOGY previousTopology;
		public:
			ID3D11InputLayout *d3d11InputLayout;
			Layout();
			~Layout();
			void InvalidateDeviceObjects();
			ID3D11InputLayout *AsD3D11InputLayout()
			{
				return d3d11InputLayout;
			}
			void Apply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext);
		};
	}
}