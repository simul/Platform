#include <GL/glew.h>
#include "Mesh.h"

using namespace simul;

namespace
{
    const float ANGLE_TO_RADIAN = 3.1415926f / 180.f;
    const GLfloat BLACK_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const GLfloat WHITE_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
    const GLfloat WIREFRAME_COLOR[] = {0.5f, 0.5f, 0.5f, 1.0f};
}

opengl::Mesh::Mesh() :scene::Mesh()
{
    // Reset every VBO to zero, which means no buffer.
    for (int lVBOIndex = 0; lVBOIndex < VBO_COUNT; ++lVBOIndex)
    {
        mVBONames[lVBOIndex] = 0;
    }
}

opengl::Mesh::~Mesh()
{
    // Delete VBO objects, zeros are ignored automatically.
    glDeleteBuffers(VBO_COUNT, mVBONames);
}

bool opengl::Mesh::Initialize(int lPolygonVertexCount,float *lVertices,float *lNormals,float *lUVs,int lPolygonCount,unsigned int *lIndices)
{
    // Create VBOs
    glGenBuffers(VBO_COUNT, mVBONames);

    // Save vertex attributes into GPU
    glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
    glBufferData(GL_ARRAY_BUFFER, lPolygonVertexCount * VERTEX_STRIDE * sizeof(float), lVertices, GL_STATIC_DRAW);
    delete [] lVertices;

    if (mHasNormal)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[NORMAL_VBO]);
        glBufferData(GL_ARRAY_BUFFER, lPolygonVertexCount * NORMAL_STRIDE * sizeof(float), lNormals, GL_STATIC_DRAW);
        delete [] lNormals;
    }
    
    if (mHasUV)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[UV_VBO]);
        glBufferData(GL_ARRAY_BUFFER, lPolygonVertexCount * UV_STRIDE * sizeof(float), lUVs, GL_STATIC_DRAW);
        delete [] lUVs;
    }
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, lPolygonCount * TRIANGLE_VERTEX_COUNT * sizeof(unsigned int), lIndices, GL_STATIC_DRAW);
    delete [] lIndices;

    return true;
}

void opengl::Mesh::UpdateVertexPositions(int lVertexCount, float *lVertices) const
{
    // Transfer into GPU.
    if (lVertices)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
        glBufferData(GL_ARRAY_BUFFER, lVertexCount * VERTEX_STRIDE * sizeof(float), lVertices, GL_STATIC_DRAW);
    }
}


void opengl::Mesh::BeginDraw(scene::ShadingMode pShadingMode,const double* mat) const
{
    glPushMatrix();
    glMultMatrixd((const double*)mat);
    // Push OpenGL attributes.
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glPushAttrib(GL_ENABLE_BIT);
    glPushAttrib(GL_CURRENT_BIT);
    glPushAttrib(GL_LIGHTING_BIT);
    glPushAttrib(GL_TEXTURE_BIT);

    // Set vertex position array.
    glBindBuffer(GL_ARRAY_BUFFER, mVBONames[VERTEX_VBO]);
    glVertexPointer(VERTEX_STRIDE, GL_FLOAT, 0, 0);
    glEnableClientState(GL_VERTEX_ARRAY);

    // Set normal array.
    if (mHasNormal && pShadingMode == scene::SHADING_MODE_SHADED)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[NORMAL_VBO]);
        glNormalPointer(GL_FLOAT, 0, 0);
        glEnableClientState(GL_NORMAL_ARRAY);
    }
    
    // Set UV array.
    if (mHasUV && pShadingMode == scene::SHADING_MODE_SHADED)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mVBONames[UV_VBO]);
        glTexCoordPointer(UV_STRIDE, GL_FLOAT, 0, 0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    // Set index array.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mVBONames[INDEX_VBO]);

    if (pShadingMode == scene::SHADING_MODE_SHADED)
    {
        glEnable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_NORMALIZE);
		glEnable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
		float unity4[]={1.0f,1.0f,1.0f,1.0f};
		glLightfv(GL_LIGHT0,GL_AMBIENT	,unity4);
		glLightfv(GL_LIGHT0,GL_DIFFUSE	,unity4);
		glLightfv(GL_LIGHT0,GL_SPECULAR	,unity4);
    }
    else
    {
        glColor4fv(WIREFRAME_COLOR);
    }
	glUseProgram(0);
}

void opengl::Mesh::Draw(int pMaterialIndex,scene::ShadingMode pShadingMode) const
{
    // Where to start.
    GLsizei lOffset = mSubMeshes[pMaterialIndex]->IndexOffset * sizeof(unsigned int);
    if ( pShadingMode == scene::SHADING_MODE_SHADED)
    {
        const GLsizei lElementCount = mSubMeshes[pMaterialIndex]->TriangleCount * 3;
        glDrawElements(GL_TRIANGLES, lElementCount, GL_UNSIGNED_INT, reinterpret_cast<const GLvoid *>(lOffset));
    }
    else
    {
        for (int lIndex = 0; lIndex < mSubMeshes[pMaterialIndex]->TriangleCount; ++lIndex)
        {
            // Draw line loop for every triangle.
            glDrawElements(GL_LINE_LOOP, TRIANGLE_VERTEX_COUNT, GL_UNSIGNED_INT, reinterpret_cast<const GLvoid *>(lOffset));
            lOffset += sizeof(unsigned int) * TRIANGLE_VERTEX_COUNT;
        }
    }
}

void opengl::Mesh::EndDraw() const
{
    // Reset VBO binding.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Pop OpenGL attributes.
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopAttrib();
    glPopClientAttrib();

    glPopMatrix();
}