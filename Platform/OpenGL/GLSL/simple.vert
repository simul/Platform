#version 420

in vec4 gl_Vertex;
out vec2 texCoords;

void main(void)
{
    vec2 pos		=vec2(-1.0,-1.0)+gl_Vertex.xy*2.0;
	gl_Position		=vec4(pos.xy,1.0,1.0);
    texCoords		=gl_Vertex.xy;
}
