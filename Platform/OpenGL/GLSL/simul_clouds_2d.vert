uniform float globalScale;
uniform float detailScale;
uniform vec2 origin;
varying vec2 texc_global;
varying vec2 texc_detail;
varying vec3 wPosition;

void main()
{
    gl_Position				=ftransform();

	vec4 pos				=gl_Vertex;
    wPosition				=pos.xyz;
    texc_global				=(pos.xy-origin.xy)/globalScale;
    texc_detail				=(pos.xy-origin.xy)/detailScale;
    
	vec3 eyespacePosition	=(gl_ModelViewMatrix*pos).xyz;
}
