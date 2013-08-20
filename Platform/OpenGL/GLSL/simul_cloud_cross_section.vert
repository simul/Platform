varying vec2 texCoords;

void main(void)
{
    gl_Position		=ftransform();
    texCoords		=gl_MultiTexCoord0.xy;
	texCoords.y		=1.0-texCoords.y;
}
