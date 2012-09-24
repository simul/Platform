// simul_terrain.vert - an OGLSL geometry shader
// Copyright 2012 Simul Software Ltd
#version 140

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
	for(i=0; i< gl_VerticesIn; i++)
	{
		gl_Position = gl_PositionIn[i];
		EmitVertex();
	}
	EndPrimitive();																						
	//New piece of geometry!  We just swizzle the x and y terms
	for(i=0; i< gl_VerticesIn; i++){
		gl_Position = gl_PositionIn[i];
		gl_Position.xy = gl_Position.yx;
		EmitVertex();
	}
	EndPrimitive();	
}

