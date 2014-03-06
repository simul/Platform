#pragma once
#include "Simul/Terrain/BaseUnderwaterRenderer.h"
#include "Simul/Platform/OpenGL/Export.h"
namespace simul
{
	namespace opengl
	{
	class SimulGLUnderwaterRenderer:public simul::terrain::BaseUnderwaterRenderer
	{
	public:
		SimulGLUnderwaterRenderer();
		~SimulGLUnderwaterRenderer();
		void Render(bool blend);
		void RecompileShaders();
	protected:
	// Make up to date with respect to keyframer:
		void EnsureCorrectTextureSizes();
		void EnsureTexturesAreUpToDate();
		void EnsureTextureCycle();
	};
	}
}