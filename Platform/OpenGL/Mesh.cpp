#include <GL/glew.h>
#include "Mesh.h"
#include "SimulGLUtilities.h"

using namespace simul;
using namespace opengl;

namespace
{
    const float ANGLE_TO_RADIAN = 3.1415926f / 180.f;
    const GLfloat BLACK_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const GLfloat WHITE_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
    const GLfloat WIREFRAME_COLOR[] = {0.5f, 0.5f, 0.5f, 1.0f};
}

Mesh::Mesh() :crossplatform::Mesh()
{
    // Reset every VBO to zero, which means no buffer.
    for (int lVBOIndex = 0; lVBOIndex < VBO_COUNT; ++lVBOIndex)
    {
        mVBONames[lVBOIndex] = 0;
    }
}

Mesh::~Mesh()
{
    // Delete VBO objects, zeros are ignored automatically.
    glDeleteBuffers(VBO_COUNT, mVBONames);
}

bool Mesh::Initialize(crossplatform::RenderPlatform *,int lPolygonVertexCount,const float *lVertices,const float *lNormals,const float *lUVs,int lPolygonCount,const unsigned int *lIndices)
{
    // Create VBOs
    glGenBuffers(VBO_COUNT, mVBONames);

    // Save vertex attributes into GPU
    glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
    glBufferData(GL_ARRAY_BUFFER, lPolygonVertexCount * VERTEX_STRIDE * sizeof(float), lVertices, GL_STATIC_DRAW);
  //  delete [] lVertices;

    if (mHasNormal)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[NORMAL_VBO]);
        glBufferData(GL_ARRAY_BUFFER, lPolygonVertexCount * NORMAL_STRIDE * sizeof(float), lNormals, GL_STATIC_DRAW);
     //   delete [] lNormals;
    }
    
    if (mHasUV)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[UV_VBO]);
        glBufferData(GL_ARRAY_BUFFER, lPolygonVertexCount * UV_STRIDE * sizeof(float), lUVs, GL_STATIC_DRAW);
   //     delete [] lUVs;
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, lPolygonCount * TRIANGLE_VERTEX_COUNT * sizeof(unsigned int), lIndices, GL_STATIC_DRAW);
  //  delete [] lIndices;
	
	mSubMeshes.resize(1);
	mSubMeshes[0]=new SubMesh;
	mSubMeshes[0]->IndexOffset=0;
	mSubMeshes[0]->TriangleCount=lPolygonCount;
	mSubMeshes[0]->drawAs=SubMesh::AS_TRIANGLES;

	GL_ERROR_CHECK
    return true;
}

bool Mesh::Initialize(const std::vector<vec3> &vertices,const std::vector<unsigned int> &indices)
{
    glGenBuffers(1, &(mVBONames[VERTEX_VBO]));
    glGenBuffers(1, &(mVBONames[INDEX_VBO]));
    // Save vertex attributes into GPU
    glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
	float * v=new float[vertices.size()*3];
	memset(v,0,sizeof(float)*vertices.size()*3);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * VERTEX_STRIDE * sizeof(float),&(vertices[0]), GL_STATIC_DRAW);
	delete [] v;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), &(indices[0]), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	mHasNormal=mHasUV=false;
	//pShadingMode=scene::SHADING_MODE_WIREFRAME;
	mSubMeshes.resize(1);
	mSubMeshes[0]=new SubMesh;
	mSubMeshes[0]->IndexOffset=0;
	mSubMeshes[0]->TriangleCount=(int)indices.size();
	mSubMeshes[0]->drawAs=SubMesh::AS_TRISTRIP;
	GL_ERROR_CHECK
	return true;
}

void Mesh::UpdateVertexPositions(int lVertexCount, float *lVertices) const
{
    // Transfer into GPU.
    if (lVertices)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
        glBufferData(GL_ARRAY_BUFFER, lVertexCount * VERTEX_STRIDE * sizeof(float), lVertices, GL_STATIC_DRAW);
    }
}

void Mesh::BeginDraw(crossplatform::DeviceContext &,crossplatform::ShadingMode pShadingMode) const
{
	GL_ERROR_CHECK
    // Set vertex position array.
	glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
	glVertexAttribPointer(0,VERTEX_STRIDE,GL_FLOAT,false,0,0);
	
	GL_ERROR_CHECK
    // Set normal array.
    if (mHasNormal && pShadingMode == crossplatform::SHADING_MODE_SHADED)
    {
		glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[NORMAL_VBO]);
		glVertexAttribPointer(2,3,GL_FLOAT,true,0,0);
    }
	else
	{
		glDisableVertexAttribArray(2);
	}
    
    // Set UV array.
    if (mHasUV && pShadingMode == crossplatform::SHADING_MODE_SHADED)
    {
		glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[UV_VBO]);
		glClientActiveTexture(GL_TEXTURE0); 
		glVertexAttribPointer(1,2,GL_FLOAT,false,0,0);
    }
	else
	{
		glDisableVertexAttribArray(1);
	}
    // Set index array.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);

    if (pShadingMode == crossplatform::SHADING_MODE_SHADED)
    {
       // glEnable(GL_LIGHTING);
        
      //  glDisable(GL_NORMALIZE);
		/*glEnable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);*/
//		float unity4[]={1.0f,1.0f,1.0f,1.0f};
		//glLightfv(GL_LIGHT0,GL_AMBIENT	,unity4);
		//glLightfv(GL_LIGHT0,GL_DIFFUSE	,unity4);
		//glLightfv(GL_LIGHT0,GL_SPECULAR	,unity4);
    }
    else
    {
        glColor4fv(WIREFRAME_COLOR);
    }
	GL_ERROR_CHECK
	//glUseProgram(0);
}

void Mesh::Draw(crossplatform::DeviceContext &deviceContext,int pMaterialIndex,crossplatform::ShadingMode pShadingMode) const
{
    // Where to start.
	const SubMesh *subMesh=GetSubMesh(pMaterialIndex);
	if(!subMesh)
		return;
	bool was_done=done_begin;
	if(!done_begin)
		BeginDraw(deviceContext,pShadingMode);
	GL_ERROR_CHECK
    GLsizei lOffset = subMesh->IndexOffset * sizeof(unsigned int);
    if ( pShadingMode == crossplatform::SHADING_MODE_SHADED)
    {
		const GLsizei lElementCount=subMesh->TriangleCount;
		if(subMesh->drawAs==SubMesh::AS_TRIANGLES)
		{
			glDrawElements(GL_TRIANGLES,lElementCount,GL_UNSIGNED_INT,reinterpret_cast<const GLvoid *>(lOffset));
		}
		else
		{
			glDrawElements(GL_TRIANGLE_STRIP,lElementCount,GL_UNSIGNED_INT,reinterpret_cast<const GLvoid *>(lOffset));
		}
    }
    else
    {
		if(subMesh->drawAs==SubMesh::AS_TRIANGLES)
		{
		   for (int lIndex = 0; lIndex<GetSubMesh(pMaterialIndex)->TriangleCount;++lIndex)
			{
				// Draw line loop for every triangle.
				glDrawElements(GL_LINE_LOOP,TRIANGLE_VERTEX_COUNT,GL_UNSIGNED_INT,reinterpret_cast<const GLvoid *>(lOffset));
				lOffset += sizeof(unsigned int)*TRIANGLE_VERTEX_COUNT;
			}
		}
		else
		{
			glDrawElements(GL_LINE_STRIP,subMesh->TriangleCount,GL_UNSIGNED_INT,reinterpret_cast<const GLvoid *>(lOffset));
		}
    }
	GL_ERROR_CHECK
	if(!was_done)
		EndDraw(deviceContext);
}

void Mesh::EndDraw(crossplatform::DeviceContext &) const
{
    // Reset VBO binding.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Pop OpenGL attributes.
}