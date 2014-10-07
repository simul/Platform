#pragma once
namespace simul
{
	namespace crossplatform
	{
		//! A cross-platform equivalent to the OpenGL and DirectX vertex topology formats
		enum Topology
		{
			UNDEFINED			
			,POINTLIST			
			,LINELIST			
			,LINESTRIP			
			,TRIANGLELIST		
			,TRIANGLESTRIP		
			,LINELIST_ADJ		
			,LINESTRIP_ADJ		
			,TRIANGLELIST_ADJ	
			,TRIANGLESTRIP_ADJ
		};
		enum StandardRenderState
		{
			STANDARD_OPAQUE_BLENDING
			,STANDARD_ALPHA_BLENDING
			,STANDARD_DEPTH_GREATER_EQUAL
			,STANDARD_DEPTH_LESS_EQUAL
			,STANDARD_DEPTH_DISABLE
		};
		enum QueryType
		{
			QUERY_UNKNWON
			,QUERY_OCCLUSION		// Like GL_SAMPLES_PASSED
			,QUERY_TIMESTAMP		// like GL_TIMESTAMP
			,QUERY_TIMESTAMP_DISJOINT
		};
	}
}
