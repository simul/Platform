uniform sampler3D density_texture;
uniform sampler3D light_texture;
uniform sampler3D ambient_texture;
uniform mat4 transformMatrix;
uniform float zPixel;
uniform float zSize;

varying vec2 in_texcoord;

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}

vec2 saturate(vec2 v)
{
	return clamp(v,vec2(0.0,0.0),vec2(1.0,1.0));
}

void main(void)
{
	vec2 texcoord				=in_texcoord.xy;
	//texcoord+=texCoordOffset;
	texcoord.y					*=zSize;
	float zPos					=floor(texcoord.y)/zSize+0.5*zPixel;
	texcoord.y					=fract(texcoord.y);
	vec3 densityspace_texcoord	=vec3(texcoord.xy,zPos);
	vec3 ambient_texcoord		=vec3(texcoord.xy,1.0-zPixel/2.0-zPos);

	vec3 lightspace_texcoord	=(transformMatrix*vec4(densityspace_texcoord,1.0)).xyz;
	vec2 light_lookup			=saturate(texture3D(light_texture,lightspace_texcoord).xy);
	vec2 amb_texel				=texture3D(ambient_texture,ambient_texcoord).xy;
	float ambient_lookup		=saturate(0.5*(amb_texel.x+amb_texel.y));
	float density				=saturate(texture3D(density_texture,densityspace_texcoord).x);

    gl_FragColor=vec4(ambient_lookup,density,light_lookup.x,light_lookup.y);
}
