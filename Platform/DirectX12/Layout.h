#pragma once
#include "Export.h"
#include "Simul/Platform/CrossPlatform/Layout.h"
#include "SimulDirectXHeader.h"

//! Forward declaration for this, means we needn't include the dx11 headers yet.
enum D3D_PRIMITIVE_TOPOLOGY;
namespace simul
{
	namespace dx11on12
	{
		class Layout:public crossplatform::Layout
		{
			ID3D11InputLayout*		previousInputLayout;
			D3D_PRIMITIVE_TOPOLOGY	previousTopology;

		public:
			D3D12_INPUT_LAYOUT_DESC					Dx12LayoutDesc;
			std::vector<D3D12_INPUT_ELEMENT_DESC>	Dx12InputLayout;

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

		private:
			UINT mAppliedCount;
		};
	}
}