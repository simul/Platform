#ifndef SIMUL_GRAPH_CAMERA_LENSFLARE_H
#define SIMUL_GRAPH_CAMERA_LENSFLARE_H
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Base/PropertyMacros.h"
#include <vector>
#include <string>
#ifdef _MSC_VER
	#pragma warning(push)
	// C4251 can be ignored in Visual C++ if you are deriving from a type in the Standard C++ Library
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace crossplatform
	{
		//! A helpful class to calculate lens flares.
		SIMUL_CROSSPLATFORM_EXPORT_CLASS LensFlare
		{
		public:
			LensFlare();
			~LensFlare();
			META_BeginProperties
				META_ValueRangeProperty(float,Strength	,0.f,0.f	,1.f	,"Strength from 0 to 1 of the flare effect, calculated each frame automatically.")
			META_EndProperties
			//! Set the camera and light directions. Do this once per frame to update the flare positions.
			void UpdateCamera(const float *cam_dir,const float *dir_to_light);
			void SetSunScale(float s);
			//! Get the number of flare artifacts.
			int GetNumArtifacts() const;
			//! Get the direction to the flare.
			const float *GetArtifactPosition(int i) const;
			//! Get the size of the flare.
			float GetArtifactSize(int i) const;
			int GetArtifactType(int i) const;
			const char *GetTypeName(int t) const;
			void AddArtifact(float pos,float size,const char *type);
			//! Returns the number of artifact types (halo, ring, flare, etc.) - this will be the number of textures needed.
			int GetNumArtifactTypes();
		protected:
			struct Artifact
			{
				simul::math::Vector3 pos;
				float coord;
				float size;
				int type;
			};
			std::vector<Artifact> artifacts;
			std::vector<std::string> types;
			float lightPosition[3];
			float visibility;
			float min_cosine;
			float SunScale;
		};
	}
}
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#endif