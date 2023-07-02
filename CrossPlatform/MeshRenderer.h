#pragma once
#include "RenderPlatform.h"
#include "Platform/CrossPlatform/Shaders/camera_constants.sl"
#include "Platform/CrossPlatform/Shaders/solid_constants.sl"
#include "Platform/CrossPlatform/Mesh.h"
#include "Export.h"

#ifdef _MSC_VER
	#pragma warning(push)  
	#pragma warning(disable : 4251)  
#endif

namespace platform
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT MeshRenderer
		{
		public:
			MeshRenderer();
			~MeshRenderer();
			//! To be called when a rendering device has been initialized.
			void RestoreDeviceObjects(RenderPlatform *r);
			void RecompileShaders();
			//! To be called when the rendering device is no longer valid.
			void InvalidateDeviceObjects();
			//! Render the mesh.
			void Render(GraphicsDeviceContext &deviceContext, Mesh *mesh,mat4 model
						,Texture *diffuseCubemap,Texture *specularCubemap,Texture *screenspaceShadow);
			void ApplyMaterial(DeviceContext &deviceContext, Material *material);
		protected:
			void DrawSubMesh(GraphicsDeviceContext& deviceContext, Mesh* mesh, int);
			void DrawSubNode(GraphicsDeviceContext& deviceContext, Mesh* mesh, const Mesh::SubNode& subNode);
			ConstantBuffer<CameraConstants> cameraConstants;
			RenderPlatform *renderPlatform;
			Effect *effect;
			crossplatform::ConstantBuffer<SolidConstants> solidConstants;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)  
#endif