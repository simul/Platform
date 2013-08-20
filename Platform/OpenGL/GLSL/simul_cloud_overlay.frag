uniform sampler2D image_texture;
uniform sampler2D depthAlphaTexture;

varying vec2 texc;
uniform vec2 screenCoordOffset;

void main(void)
{
	vec2 depthTexCoord	=screenCoordOffset+texc;
	float depth			=texture(depthAlphaTexture,depthTexCoord).a;
#ifdef REVERSE_DEPTH
	//if(depth<=0.0)
	//	discard;
#else
	//if(depth>=1.0)
	//	discard;
#endif
    vec4 c = texture2D(image_texture,texc);
    gl_FragColor=c;
}
