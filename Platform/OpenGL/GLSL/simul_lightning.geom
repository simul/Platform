//  wide_line.gs

//  Geometry shader for rendering wide lines
//  with GL 3.3+ with forward compatible context.
//
//  Input and output positions must be in clip space.
//
//  Not optimized.
//
//  Timo Suoranta - tksuoran@gmail.com

#version 330

layout(lines_adjacency) in;

layout(triangle_strip, max_vertices = 10) out;

uniform vec2 viewportPixels;
uniform vec2 _line_width;

in vec3 _position[];
in vec3 texc[];

out vec2 texcoord;
out float width;
vec2 screen_space(vec4 vertex)
{
	return vec2(vertex.xy/vertex.w)*viewportPixels;
}

void main(void) 
{
    //  a - - - - - - - - - - - - - - - - b
    //  |      |                   |      |
    //  |      |                   |      |
    //  |      |                   |      |
    //  | - - -start - - - - - - end- - - |
    //  |      |                   |      |
    //  |      |                   |      |
    //  |      |                   |      |
    //  d - - - - - - - - - - - - - - - - c
	vec2 p0		=screen_space(gl_in[0].gl_Position);
	vec2 p1		=screen_space(gl_in[1].gl_Position);
	vec2 p2		=screen_space(gl_in[2].gl_Position);
	vec2 p3		=screen_space(gl_in[3].gl_Position);
	vec2 area = viewportPixels * 1.2;
	if(p1.x<-area.x||p1.x>area.x) return;
	if(p1.y<-area.y||p1.y>area.y) return;
	if(p2.x<-area.x||p2.x>area.x) return;
	if(p2.y<-area.y||p2.y>area.y) return;
    vec4 start  =gl_in[0].gl_Position;
    vec4 end    =gl_in[1].gl_Position;
	// determine the direction of each of the 3 segments (previous, current, next
	vec2 v0 = normalize(p1-p0);
	vec2 v1 = normalize(p2-p1);
	vec2 v2 = normalize(p3-p2);
	// determine the normal of each of the 3 segments (previous, current, next)
	vec2 n0=vec2(-v0.y,v0.x);
	vec2 n1=vec2(-v1.y,v1.x);
	vec2 n2=vec2(-v2.y,v2.x);
	// determine miter lines by averaging the normals of the 2 segments
	vec2 miter_a = normalize(n0 + n1);	// miter at start of current segment
	vec2 miter_b = normalize(n1 + n2);	// miter at end of current segment
	// determine the length of the miter by projecting it onto normal and then inverse it
	float widthPixels1=texc[1].x;
	float widthPixels2=texc[2].x;
	float length_a = widthPixels1/gl_in[1].gl_Position.w*viewportPixels.x/dot(miter_a, n1);
	float length_b = widthPixels2/gl_in[2].gl_Position.w*viewportPixels.x/dot(miter_b, n1);
	const float	MITER_LIMIT=1.0;//0.75;
	// prevent excessively long miters at sharp corners
	if( dot(v0,v1) < -MITER_LIMIT )
	{
		miter_a = n1;
		length_a = widthPixels1;
		// close the gap
		if( dot(v0,n1) > 0 )
		{
			texcoord = vec2(0.0,texc[1].z);
			width=texc[1].y;
			gl_Position = vec4( (p1 + widthPixels1 * n0) / viewportPixels, 0.0, 1.0 );
			EmitVertex();
			texcoord = vec2(0.0,texc[1].z);
			width=texc[1].y;
			gl_Position = vec4( (p1 + widthPixels1 * n1) / viewportPixels, 0.0, 1.0 );
			EmitVertex();
			texcoord = vec2(0.5,texc[1].z);
			width=texc[1].y;
			gl_Position = vec4( p1 / viewportPixels, 0.0, 1.0 );
			EmitVertex();
			EndPrimitive();
		}
		else
		{
			texcoord = vec2(1.0,texc[1].z);
			width=texc[1].y;
			gl_Position = vec4( (p1 - widthPixels2 * n1) / viewportPixels, 0.0, 1.0 );
			EmitVertex();		
			texcoord = vec2(1.0,texc[1].z);
			width=texc[1].y;
			gl_Position = vec4( (p1 - widthPixels2 * n0) / viewportPixels, 0.0, 1.0 );
			EmitVertex();
			texcoord = vec2(0.5,texc[1].z);
			width=texc[1].y;
			gl_Position = vec4( p1 / viewportPixels, 0.0, 1.0 );
			EmitVertex();
			EndPrimitive();
		}
	}
	if( dot(v1,v2) < -MITER_LIMIT )
	{
		miter_b = n1;
		length_b = widthPixels2;
	}
  // generate the triangle strip
	texcoord = vec2(0.0,texc[1].z);
	width=texc[1].y;
	gl_Position = vec4( (p1 + length_a * miter_a)/viewportPixels,0.0,1.0);
	EmitVertex();
	texcoord = vec2(1.0,texc[1].z);
	width=texc[1].y;
	gl_Position = vec4( (p1 - length_a * miter_a)/viewportPixels,0.0,1.0);
	EmitVertex();
	texcoord = vec2(0.0,texc[2].z);
	width=texc[2].y;
	gl_Position = vec4( (p2 + length_b * miter_b)/viewportPixels,0.0,1.0);
	EmitVertex();
	texcoord = vec2(1.0,texc[2].z);
	width=texc[2].y;
	gl_Position = vec4( (p2 - length_b * miter_b)/viewportPixels,0.0,1.0);
	EmitVertex();
    EndPrimitive();
}