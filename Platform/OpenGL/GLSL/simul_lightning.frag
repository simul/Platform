
uniform sampler1D lightningTexture;
varying vec2 texc;

void main(void)
{
    vec4 c=texc.y*texture1D(lightningTexture,texc.x);
    gl_FragColor=c;
}
