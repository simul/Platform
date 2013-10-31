
varying vec2 texCoords;

void main(void)
{
    vec4 outpos	=ftransform();
	gl_Position =outpos;
	texCoords	=gl_Vertex.xy;
}
