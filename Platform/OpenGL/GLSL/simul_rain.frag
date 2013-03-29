uniform sampler2D rainTexture;
varying vec2 texc;
void main()
{
	vec4 lookup=texture(rainTexture,texc);
    gl_FragColor=vec4(lookup.rgb,1.0);
}
