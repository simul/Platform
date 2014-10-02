#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/Topology.h"
#include <vector>
struct ID3D11InputLayout;
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		//! A cross-platform equivalent to D3D11_INPUT_ELEMENT_DESC, used
		//! to create layouts.
		struct LayoutDesc
		{
			const char *semanticName;
			int			semanticIndex;
			PixelFormat	format;
			int			inputSlot;
			int			alignedByteOffset;
			bool		perInstance;		// otherwise it's per vertex.
			int			instanceDataStepRate;
		};
		//! A cross-platform class representing vertex input layouts. Create with RenderPlatform::CreateLayout.
		class SIMUL_CROSSPLATFORM_EXPORT Layout
		{
		protected:
			int apply_count;
			std::vector<LayoutDesc> parts;
		public:
			Layout();
			virtual ~Layout();
			void SetDesc(const LayoutDesc *d,int num);
			virtual void InvalidateDeviceObjects(){}
			virtual ID3D11InputLayout *AsD3D11InputLayout()
			{
				return 0;
			}
			virtual void Apply(DeviceContext &deviceContext)=0;
			virtual void Unapply(DeviceContext &deviceContext)=0;
		};
	}
}
