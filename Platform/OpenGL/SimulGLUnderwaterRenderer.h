#pragma once
#include <GL/glew.h>
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
		bool IsYVertical() const;
	protected:
	// Make up to date with respect to keyframer:
		void EnsureCorrectTextureSizes();
		void EnsureTexturesAreUpToDate();
		void EnsureTextureCycle();
		bool IsYVertical(){return false;}
	};
	}
}