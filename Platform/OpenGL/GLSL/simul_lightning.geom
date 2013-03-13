#version 330 compatibility

layout(triangles) in;
layout(triangle_strip, max_vertices=6) out;

void main(void)
{
	//increment variable
	int i;
	/////////////////////////////////////////////////////////////
	//This example has two parts
	//	step a) draw the primitive pushed down the pipeline
	//		 there are gl_Vertices # of vertices
	//		 put the vertex value into gl_Position
	//		 use EmitVertex => 'create' a new vertex
	// 		use EndPrimitive to signal that you are done creating a primitive!
	//	step b) create a new piece of geometry (I.E. WHY WE ARE USING A GEOMETRY SHADER!)
	//		I just do the same loop, but swizzle the x and y values
	//	result => the line we want to draw, and the same line, but along the other axis

	//Pass-thru!
	for(i=0; i< gl_in.length (); i++)
	{
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
	}
	EndPrimitive();																						
	//New piece of geometry!  We just swizzle the x and y terms
	for(i=0; i< gl_in.length (); i++){
		gl_Position = gl_in[i].gl_Position;
		gl_Position.xy = gl_Position.yx;
		EmitVertex();
	}
	EndPrimitive();	
}

