#pragma once
#include "Export.h"
#include "Platform/CrossPlatform/Layout.h"
#include "SimulDirectXHeader.h"

//! Forward declaration for this, means we needn't include the dx11 headers yet.
enum D3D_PRIMITIVE_TOPOLOGY;
namespace simul
{
	namespace dx12
	{
		class Layout:public crossplatform::Layout
		{
			D3D_PRIMITIVE_TOPOLOGY	previousTopology;

		public:
			D3D12_INPUT_LAYOUT_DESC					Dx12LayoutDesc;
			std::vector<D3D12_INPUT_ELEMENT_DESC>	Dx12InputLayout;

			Layout();
			~Layout();
			void InvalidateDeviceObjects();
			void Apply(crossplatform::DeviceContext &deviceContext);
			void Unapply(crossplatform::DeviceContext &deviceContext);

		private:
			UINT mAppliedCount;
		};
	}
}