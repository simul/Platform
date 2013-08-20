
varying vec2 in_texcoord;

void main(void)
{
    vec4 outpos	=ftransform();
	gl_Position =outpos;
	in_texcoord	=gl_Vertex.xy;
}
