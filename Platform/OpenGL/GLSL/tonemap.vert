
varying vec2 texCoords;

void main(void)
{
    gl_Position		= ftransform();
    texc			=gl_MultiTexCoord0.xy;
}
