
uniform float maxFadeDistanceMetres;
varying vec2 texc;
varying vec2 fade_texc;
varying vec3 wPosition;
varying vec3 view;

void main(void)
{
    gl_Position				=ftransform();
    texc					=gl_MultiTexCoord0.xy;
    
    
	vec4 pos				=gl_Vertex;
    vec3 wPosition			=pos.xyz;
    
	vec3 eyespacePosition	=(gl_ModelViewMatrix*pos).xyz;
	view					=normalize(wPosition);
	float sine				=view.z;
	fade_texc				=vec2(length(eyespacePosition.xyz)/maxFadeDistanceMetres,0.5*(1.0-sine));
}
