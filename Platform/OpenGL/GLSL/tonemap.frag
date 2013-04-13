
uniform sampler2D image_texture;
uniform sampler2D glowTexture;
uniform float exposure;
uniform float gamma;
varying vec2 texc;

void main(void)
{
    // original image
    vec4 c = texture2D(image_texture,texc);
    // exposure
	c.rgb*=exposure;

	vec4 glow=texture(glowTexture,texc);
	c.rgb+=glow.rgb;

    // gamma correction
	c.rgb = pow(c.rgb,vec3(gamma,gamma,gamma));
	//c.a=1.0-pow(1.0-c.a,gamma);

    gl_FragColor=c;
}
