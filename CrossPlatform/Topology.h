#pragma once
namespace simul
{
	namespace crossplatform
	{
		//! A cross-platform equivalent to the OpenGL and DirectX vertex topology formats
		enum class Topology
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
			,STANDARD_TEST_DEPTH_GREATER_EQUAL
			,STANDARD_TEST_DEPTH_LESS_EQUAL
		};
		enum QueryType
		{
			QUERY_UNKNOWN
			,QUERY_OCCLUSION		// Like GL_SAMPLES_PASSED
			,QUERY_TIMESTAMP		// like GL_TIMESTAMP
			,QUERY_TIMESTAMP_DISJOINT
		};
		enum MeshType
		{
			NO_MESH_TYPE
			,CUBE_MESH
			,SPHERE_MESH
		};
	}
}
