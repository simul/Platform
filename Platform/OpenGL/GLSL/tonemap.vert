
varying vec2 texc;

void main(void)
{
    gl_Position		= ftransform();
    texc=gl_MultiTexCoord0.xy;
}
