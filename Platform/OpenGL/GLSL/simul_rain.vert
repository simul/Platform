varying vec3 texc;
varying vec3 view;
void main(void)
{
	gl_Position		=ftransform();
	view			=normalize(gl_Vertex.xyz);
    texc			=gl_MultiTexCoord0.xyz;
}

