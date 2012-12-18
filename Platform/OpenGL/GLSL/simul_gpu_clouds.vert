
varying vec2 in_texcoord;

void main(void)
{
    gl_Position	=ftransform();
    in_texcoord	=gl_MultiTexCoord0.xy;
}
