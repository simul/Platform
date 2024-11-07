#pragma once
#ifndef SIMUL_EDITOR
#define SIMUL_EDITOR 0
#endif
#if SIMUL_EDITOR
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
#include "Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/Shaders/sphere_constants.sl"

namespace platform
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT SphereRenderer
		{
			RenderPlatform *renderPlatform;

		public:
			SphereRenderer();
			virtual ~SphereRenderer();
			void RestoreDeviceObjects(RenderPlatform *r);
			void InvalidateDeviceObjects();
			void LoadShaders();
			void RecompileShaders();

			void DrawLatLongSphere(GraphicsDeviceContext &deviceContext, int lat, int longi, vec3 origin, float sph_radius, vec4 colour);
			void DrawQuad(GraphicsDeviceContext &deviceContext, vec3 origin, vec4 orient_quat, float size, float sph_radius, vec4, vec4 fill_colour = vec4(0.f, 0.f, 0.f, 0.f), bool cheq = true);
			void DrawColouredSphere(GraphicsDeviceContext &deviceContext, vec3 origin, float radius, vec4 colour = vec4(1.f, 1.f, 1.f, 1.f));
			void DrawTexturedSphere(GraphicsDeviceContext &deviceContext, vec3 origin, float radius, crossplatform::Texture *texture, vec4 colour = vec4(1.f, 1.f, 1.f, 1.f));
			void DrawTexture(GraphicsDeviceContext &deviceContext, Texture *t, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour = vec4(1.f, 1.f, 1.f, 1.f));
			void DrawCurvedTexture(GraphicsDeviceContext &deviceContext, Texture *t, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour = vec4(1.f, 1.f, 1.f, 1.f));
			void DrawCircle(GraphicsDeviceContext &deviceContext, vec3 origin, vec4 orient_quat, float crc_rad, float sph_rad, vec4 line_colour, vec4 fill_colour = vec4(0.f, 0.f, 0.f, 0.f));
			void DrawCrossSection(GraphicsDeviceContext &deviceContext, Effect *effect, Texture *t, vec3 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour, int pass = 0);
			void DrawMultipleCrossSections(GraphicsDeviceContext &deviceContext, Effect *effect, Texture *t, vec3 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour, int slices = 4);
			void DrawArc(GraphicsDeviceContext &deviceContext, vec3 origin, vec4 q1, vec4 q2, float sph_rad, vec4 colour);
			void DrawAxes(GraphicsDeviceContext &deviceContext, vec4 orient_quat, vec3 pos, float size);

		protected:
			crossplatform::ConstantBuffer<SphereConstants> sphereConstants;
			std::shared_ptr<crossplatform::Effect> effect = nullptr;
			bool reload_shaders = true;
		};
	}
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif