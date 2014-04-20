#version 140
in int gl_VertexID;
in vec4 gl_Vertex;

#include "CppGlsl.hs"
#include "../../CrossPlatform/SL/simul_cloud_constants.sl"

out vec2 texCoords;

void main(void)
{
    vec2 pos	=rect.xy+gl_Vertex.xy*rect.zw;
	gl_Position	=vec4(pos.xy,1.0,1.0);
    texCoords	=gl_Vertex.xy;
}
