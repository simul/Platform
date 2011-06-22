
uniform sampler2D sceneTex;
uniform float exposure;
uniform float gamma;
varying vec2 texc;

void main(void)
{
    // original image
    vec4 c = texture2D(sceneTex,texc);
    // exposure
	c = c*exposure;
    // gamma correction
	c.rgb = pow(c.rgb,vec3(gamma,gamma,gamma));
	c.a=1.0-pow(1.0-c.a,gamma);

    gl_FragColor=c;
}
