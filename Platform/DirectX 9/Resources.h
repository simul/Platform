#include "Simul/Platform/DirectX 9/Export.h"

//! This function should return the ID for a resource (e.g. a shader) whose source was the specified filename. This way, the executable can build-in shaders or textures, or use the original files if available.
extern unsigned SIMUL_DIRECTX9_EXPORT (*GetResourceId)(const char *filename);