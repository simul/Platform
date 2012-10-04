
varying vec2 texc;
varying vec3 wPosition;

void main()
{
    gl_Position				=ftransform();
    texc					=gl_MultiTexCoord0.xy;

	vec4 pos				=gl_Vertex;
    wPosition				=pos.xyz;
    
	vec3 eyespacePosition	=(gl_ModelViewMatrix*pos).xyz;
}
