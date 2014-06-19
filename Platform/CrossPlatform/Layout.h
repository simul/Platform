#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
struct ID3D11InputLayout;
namespace simul
{
	namespace crossplatform
	{
		class RenderPlatform;
		struct DeviceContext;
		//! A cross-platform equivalent to the OpenGL and DirectX pixel formats
		enum PixelFormat
		{
			RGBA_32_FLOAT
			,RGB_32_FLOAT
			,RG_32_FLOAT
			,R_32_FLOAT
		};
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
		public:
			Layout();
			virtual ~Layout();
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
