#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"
#include "Simul/Platform/CrossPlatform/Topology.h"
#include <vector>
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
#endif
struct ID3D11InputLayout;
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		//! A cross-platform equivalent to D3D11_INPUT_ELEMENT_DESC, used
		//! to create layouts. MEMBERS: const char *semanticName,int semanticIndex,	PixelFormat	format,	int inputSlot,	int alignedByteOffset,	bool perInstance,int instanceDataStepRate
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
			int struct_size;
			std::vector<LayoutDesc> parts;
			Topology topology;
		public:
			Layout();
			virtual ~Layout();
			void SetDesc(const LayoutDesc *d,int num);
			const std::vector<LayoutDesc> &GetDesc() const
			{
				return parts;
			}
			virtual void InvalidateDeviceObjects(){}
			virtual ID3D11InputLayout *AsD3D11InputLayout()
			{
				return 0;
			}
			virtual ID3D11InputLayout *const AsD3D11InputLayout() const
			{
				return 0;
			}
			virtual void Apply(DeviceContext &deviceContext);
			virtual void Unapply(DeviceContext &deviceContext);
			int GetStructSize() const;
		};
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
