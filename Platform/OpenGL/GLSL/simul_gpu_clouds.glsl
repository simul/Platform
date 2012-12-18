uniform float zPixel;
uniform float zSize;

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}

vec2 saturate(vec2 v)
{
	return clamp(v,vec2(0.0,0.0),vec2(1.0,1.0));
}

vec3 assemble3dTexcoord(vec2 texcoord2)
{
	vec2 texcoord	=texcoord2.xy;
	texcoord.y		*=zSize;
	float zPos		=floor(texcoord.y)/zSize+0.5*zPixel;
	texcoord.y		=fract(texcoord.y);
	vec3 texcoord3	=vec3(texcoord.xy,zPos);
	return texcoord3;
}
