
uniform sampler1D lightningTexture;
uniform vec3 lightningColour;
varying vec2 texCoords;
varying float brightness;

void main(void)
{
    vec4 c=vec4(lightningColour,1.0)*brightness;
	c.a=clamp(c.a,0.0,1.0);
    gl_FragColor=c;
}
