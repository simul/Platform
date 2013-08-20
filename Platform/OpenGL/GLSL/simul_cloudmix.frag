uniform sampler2D image_texture;
uniform sampler2D clouds_texture;
varying vec2 texCoords;
uniform vec2 texOffset;

void main(void)
{
	vec4 lookup			=texture2D(image_texture,texCoords+texOffset);
    vec4 clouds_lookup	=texture2D(clouds_texture,texCoords+texOffset);
	float vis			=lookup.a;
	if(vis<=0.0)
		discard;
	vis=1.0-(1.0-vis)*(1.0-clouds_lookup.a);
	vec3 colour=clouds_lookup.rgb*(1.0-vis)+lookup.rgb*vis;
    gl_FragColor=vec4(colour.rgb,1.0);
}
