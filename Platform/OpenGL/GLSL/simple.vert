#version 140
in int gl_VertexID;
in vec4 gl_Vertex;
out vec2 texc;

void main(void)
{
    vec2 pos		=vec2(-1.0,-1.0)+gl_Vertex.xy*2.0;
	gl_Position		=vec4(pos.xy,1.0,1.0);
    texc			=gl_Vertex.xy;
}
