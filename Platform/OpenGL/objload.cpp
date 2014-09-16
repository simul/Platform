//			objload.cpp
//	author	Rob Bateman, mailto:robthebloke@hotmail.com

#include <GL/glew.h>
#include "objload.h"
#include "SimulGLUtilities.h"
#include "LoadGLImage.h"
#include <math.h>
#include <stdio.h>
#include <iostream>

using namespace Obj;

void BezierPatch::SetLod(unsigned new_lod)
{
	delete[] VertexData;
	delete[] BlendFuncs;
	delete[] IndexData;
	LOD = new_lod;
	// allocate new blend funcs array. This just caches the values for tesselation
	BlendFuncs = new float[4 * (LOD + 1)];float
	* ptr = BlendFuncs;
	for (unsigned i = 0; i <= LOD; ++i)
	{
		float t = static_cast<float>(i / LOD);float t2 = t*t;
		float t3 = t2*t;
		float it =1.0f-t;
		float it2 = it*it;
		float it3 = it2*it;

		*ptr = t3; ++ptr;
		*ptr = 3*it*t2; ++ptr;
		*ptr = 3*it2*t; ++ptr;
		*ptr = it3; ++ptr;

		// calculate texture coordinates since they never change
				{

				}
				// calculate texture coordinates since they never change
				{

				}
			}

		// allocate vertex data array
	VertexData = new float[8 * (LOD + 1) * (LOD + 1) ];

	// allocate indices for triangle strips to render the patch
IndexData	= new unsigned[(LOD + 1) * LOD * 2 ];

{		// calculate the vertex indices for the triangle strips.
		unsigned *iptr = IndexData;
		unsigned *end = IndexData + (LOD + 1) * LOD * 2;
		unsigned ii = 0;
		for (; iptr != end; ++ii)
		{
			*iptr = ii;
			++iptr;
			*iptr = ii + LOD + 1;
			++iptr;
		}
	}
}

void BezierPatch::gl() const
{

}

void DoFaceCalc(const Vertex& v1, const Vertex& v2, const Vertex& v3,
		Normal& n1, Normal& n2, Normal& n3)
{
	// calculate vector between v2 and v1
	Vertex e1;
	e1.x = v1.x - v2.x;
	e1.y = v1.y - v2.y;
	e1.z = v1.z - v2.z;

	// calculate vector between v2 and v3
	Vertex e2;
	e2.x = v3.x - v2.x;
	e2.y = v3.y - v2.y;
	e2.z = v3.z - v2.z;

	// cross product them
	Vertex e1_cross_e2;
	e1_cross_e2.x = e2.y * e1.z - e2.z * e1.y;
	e1_cross_e2.y = e2.z * e1.x - e2.x * e1.z;
	e1_cross_e2.z = e2.x * e1.y - e2.y * e1.x;

	float itt = 1.0f
			/ ((float) sqrt(
					e1_cross_e2.x * e1_cross_e2.x
							+ e1_cross_e2.y * e1_cross_e2.y
							+ e1_cross_e2.z * e1_cross_e2.z));

	// normalise
	e1_cross_e2.x *= itt;
	e1_cross_e2.y *= itt;
	e1_cross_e2.z *= itt;

	// sum the face normal into all the vertex normals this face uses
	n1.x += e1_cross_e2.x;
	n1.y += e1_cross_e2.y;
	n1.z += e1_cross_e2.z;

	n2.x += e1_cross_e2.x;
	n2.y += e1_cross_e2.y;
	n2.z += e1_cross_e2.z;

	n3.x += e1_cross_e2.x;
	n3.y += e1_cross_e2.y;
	n3.z += e1_cross_e2.z;
}

void NormaliseNormals(std::vector<Normal>& norms)
{
	std::vector<Normal>::iterator itn = norms.begin();
	for (; itn != norms.end(); ++itn)
	{
		float itt = 1.0f
				/ ((float) sqrt(
						itn->x * itn->x + itn->y * itn->y + itn->z * itn->z));
		itn->x *= itt;
		itn->y *= itt;
		itn->z *= itt;
	}
}

void ZeroNormals(std::vector<Normal>& norms)
{
	// zero normals
	std::vector<Normal>::iterator itn = norms.begin();
	for (; itn != norms.end(); ++itn)
		itn->x = itn->y = itn->z = 0;
}

Material::Material() :
		name(), illum(4), Ni(1), Ns(10), Bm(1), map_Ka(), map_Kd(), map_Ks(), map_Bump()
{
	Ka[0] = Ka[1] = Ka[2] = Kd[0] = Kd[1] = Kd[2] = Ks[0] = Ks[1] = Ks[2] = 0;
	Ka[3] = Kd[3] = Ks[3] = 1;
	Tf[0] = Tf[1] = Tf[2] = 1;
	gltex_Ka = gltex_Kd = gltex_Ks = gltex_Bump = 0;
}
Material::~Material()
{
}
void Material::InvalidateGLObjects()
{
	if (gltex_Ka)
		glDeleteTextures(1, &gltex_Ka);
	if (gltex_Kd)
		glDeleteTextures(1, &gltex_Kd);
	if (gltex_Ks)
		glDeleteTextures(1, &gltex_Ks);
	if (gltex_Bump)
		glDeleteTextures(1, &gltex_Bump);
}

Material::Material(const Material& mat)
{
	Ka[0] = mat.Ka[0];
	Ka[1] = mat.Ka[1];
	Ka[2] = mat.Ka[2];
	Ka[3] = mat.Ka[3];
	Kd[0] = mat.Kd[0];
	Kd[1] = mat.Kd[1];
	Kd[2] = mat.Kd[2];
	Kd[3] = mat.Kd[3];
	Ks[0] = mat.Ks[0];
	Ks[1] = mat.Ks[1];
	Ks[2] = mat.Ks[2];
	Ks[3] = mat.Ks[3];
	Tf[0] = mat.Tf[0];
	Tf[1] = mat.Tf[1];
	Tf[2] = mat.Tf[2];
	Ni = mat.Ni;
	Ns = mat.Ns;
	name = mat.name;
	map_Ka = mat.map_Ka;
	map_Kd = mat.map_Kd;
	map_Ks = mat.map_Ks;
	map_Bump = mat.map_Bump;
	illum = mat.illum;
	Bm = mat.Bm;

	gltex_Ka = mat.gltex_Ka;
	gltex_Kd = mat.gltex_Kd;
	gltex_Ks = mat.gltex_Ks;
	gltex_Bump = mat.gltex_Bump;
	Bm = mat.Bm;
}

void Material::gl() const
{
	float average_transp = (Tf[0] + Tf[1] + Tf[2]) / 3.0f;
	Ka[3] = Kd[3] = Ks[3] = average_transp;
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, Ka);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, Kd);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, Ks);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, Ns);

	if (gltex_Kd)
	{
		// maya always sets the diffuse colour to black when a texture is applied.
		float color[4] =
		{ 0.6f, 0.6f, 0.6f, average_transp };
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, color);
		glActiveTexture(GL_TEXTURE0);
		
		// apply texture
		glBindTexture(GL_TEXTURE_2D, gltex_Kd);
	}
		
}

Surface::Surface() :
		m_Vertices(), m_Normals(), m_TexCoords(), m_Triangles(), m_AssignedMaterials(), name()
{
}

Surface::Surface(const Surface& surface) :
		m_Vertices(surface.m_Vertices), m_Normals(surface.m_Normals), m_TexCoords(
				surface.m_TexCoords), m_Triangles(surface.m_Triangles), m_AssignedMaterials(
				surface.m_AssignedMaterials), name(surface.name)
{
}

void Surface::CalculateNormals()
{
	// resize normal array if not present
	if (!m_Normals.size())
		m_Normals.resize(m_Vertices.size());

	ZeroNormals(m_Normals);

	// loop through each triangle in face
	std::vector<Face>::iterator it = m_Triangles.begin();
	for (; it != m_Triangles.end(); ++it)
	{
		// if no indices exist for normals, create them
		if (it->n[0] == -1)
			it->n[0] = it->v[0];
		if (it->n[1] == -1)
			it->n[1] = it->v[1];
		if (it->n[2] == -1)
			it->n[2] = it->v[2];

		// calc face normal and sum into normal array
		DoFaceCalc(m_Vertices[it->v[0]], m_Vertices[it->v[1]],
				m_Vertices[it->v[2]], m_Normals[it->n[0]], m_Normals[it->n[1]],
				m_Normals[it->n[2]]);
	}

	NormaliseNormals(m_Normals);
}

void Surface::DrawRange(unsigned start, unsigned end_face) const
{
	// draw each face with this material assigned.
	std::vector<Face>::const_iterator itf = m_Triangles.begin() + start;
	std::vector<Face>::const_iterator end = m_Triangles.begin() + end_face;
	for (; itf != end; ++itf)
	{
		for (int i = 0; i != 3; ++i)
		{
			if (itf->n[i] != -1)
				m_Normals[itf->n[i]].gl();
			if (itf->t[i] != -1)
				m_TexCoords[itf->t[i]].gl();
			m_Vertices[itf->v[i]].gl();
		}
	}
}


void Surface::gl() const
{
	if (m_AssignedMaterials.size())
	{
		std::vector<MaterialGroup>::const_iterator it =
				m_AssignedMaterials.begin();
		for (; it != m_AssignedMaterials.end(); ++it)
		{
GL_ERROR_CHECK
			// if file valid, assign materials
			if (m_pFile && m_pFile->m_Materials.size() > it->m_MaterialIdx)
				m_pFile->OnBindMaterial(
						m_pFile->m_Materials[it->m_MaterialIdx]);
GL_ERROR_CHECK
			glBegin(GL_TRIANGLES);
			// draw each face with this material assigned.
			DrawRange(it->m_StartFace, it->m_EndFace);
			glEnd();
GL_ERROR_CHECK
		}
	}
	else
	{
	glBegin(GL_TRIANGLES);
		DrawRange(0, static_cast<unsigned>(m_Triangles.size()));
		glEnd();
GL_ERROR_CHECK
	}
}


VertexBuffer::VertexBuffer() :
		m_Vertices()
		, m_Normals()
		, m_TexCoords()
		, m_Indices()
		, m_AssignedMaterials()
		, name()
		, m_pFile(0)
		, m_pTransform(NULL)
{
}

VertexBuffer::VertexBuffer(const VertexBuffer& vb) :
		m_Vertices(vb.m_Vertices)
		,m_Normals(vb.m_Normals)
		,m_TexCoords(vb.m_TexCoords)
		,m_Indices(vb.m_Indices)
		,m_AssignedMaterials(vb.m_AssignedMaterials)
		,name(vb.name)
		,m_pFile(vb.m_pFile)
		,m_pTransform(NULL)
{
}

void VertexBuffer::CalculateNormals()
{
	// resize normal array if not present
	if (m_Normals.size() != m_Vertices.size())
		m_Normals.resize(m_Vertices.size());

	ZeroNormals(m_Normals);

	// loop through each triangle in face
	std::vector<unsigned int>::const_iterator it = m_Indices.begin();
	for (; it < m_Indices.end(); it += 3)
		DoFaceCalc(m_Vertices[*it], m_Vertices[*(it + 1)],
				m_Vertices[*(it + 2)], m_Normals[*it], m_Normals[*(it + 1)],
				m_Normals[*(it + 2)]);

	NormaliseNormals(m_Normals);
}

void VertexBuffer::gl() const
{
	if(m_pTransform)
	{
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glMultMatrixf(m_pTransform);
	}
	// enable vertices
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, &m_Vertices[0]);

	// if normals present, enabble them.
	if (m_Normals.size())
	{
		glEnableClientState(GL_NORMAL_ARRAY);
		glNormalPointer(GL_FLOAT, 0, &m_Normals[0]);
	}

	// enable tex coords if present
	if (m_TexCoords.size())
	{
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, &m_TexCoords[0]);
	}

	// if materials assigned to object.
	if (m_AssignedMaterials.size())
	{
		std::vector<MaterialGroup>::const_iterator it=m_AssignedMaterials.begin();
		for (; it != m_AssignedMaterials.end(); ++it)
		{
			// if file valid, assign materials
			if (m_pFile && m_pFile->m_Materials.size() > it->m_MaterialIdx)
				m_pFile->OnBindMaterial(m_pFile->m_Materials[it->m_MaterialIdx]);
			glDrawElements(GL_TRIANGLES, (it->m_EndFace - it->m_StartFace) * 3,GL_UNSIGNED_INT, &(m_Indices[it->m_StartFace * 3]));
		}
	}
	else
		// just draw all elements in array
		glDrawElements(GL_TRIANGLES, static_cast<unsigned>(m_Indices.size()), GL_UNSIGNED_INT,&(m_Indices[0]));

		// disable vertex arrays.
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if(m_pTransform)
	{
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	}
}

namespace Obj
{
	std::ostream& operator<<(std::ostream& ofs, const Obj::Face& f)
	{
		ofs << "f ";
		for (int i = 0; i != 3; ++i)
		{
			ofs << (f.v[i] + 1);
			if (f.n[i] != -1 || f.t[i] != -1)
			{
				ofs << "/";
				if (f.t[i] != -1)
					ofs << (f.t[i] + 1);
				ofs << "/";
				if (f.n[i] != -1)
					ofs << (f.n[i] + 1);
			}
			ofs << " ";
		}
		return ofs << std::endl;
	}
}

bool HasOnlyVertex(const std::string& s)
{
	std::string::const_iterator it = s.begin();
	for (; it != s.end(); ++it)
	{
		if (*it == '/')
			return false;
	}
	return true;
}

bool MissingUv(const std::string& s)
{
	std::string::const_iterator it = s.begin();
	while (*it != '/')
	{
		if (it == s.end())
			return true;
		++it;
	}
	return *(it + 1) == '/';
}

bool MissingNormal(const std::string& s)
{
	return s[s.size() - 1] == '/';
}

/// quick utility function to copy a range of data from the obj file arrays
/// into a surface array.
template<typename T>
void CopyArray(std::vector<T>& output, const std::vector<T>& input,
		unsigned start, unsigned end)
{
	output.resize(end - start + 1);
	typename std::vector<T>::iterator ito = output.begin();
	typename std::vector<T>::const_iterator it = input.begin() + start;
	typename std::vector<T>::const_iterator itend = input.begin() + end + 1;
	for (; it != itend; ++it, ++ito)
	{
		*ito = *it;
	}
}

void DetermineIndexRange(unsigned int& s_vert, unsigned int& e_vert,
		int& s_norm, int& e_norm, int& s_uv, int& e_uv,
		std::vector<Face>::const_iterator it,
		std::vector<Face>::const_iterator end)
{
	// need to determine start and end vertex/normal and uv indices
	s_vert = 0xFFFFFFF;
	s_norm = 0xFFFFFFF;
	s_uv = 0xFFFFFFF;
	e_vert = 0;
	e_norm = -1;
	e_uv = -1;

	// loop through faces to find max/min indices
	for (; it != end; ++it)
	{
		for (int i = 0; i != 3; ++i)
		{
			if (it->v[i] < s_vert)
				s_vert = it->v[i];
			if (it->v[i] > e_vert)
				e_vert = it->v[i];
			if (it->n[i] != -1)
			{
				if (it->n[i] < s_norm)
					s_norm = it->n[i];
				if (it->n[i] > e_norm)
					e_norm = it->n[i];
			}
			if (it->t[i] != -1)
			{
				if (it->t[i] < s_uv)
					s_uv = it->t[i];
				if (it->t[i] > e_uv)
					e_uv = it->t[i];
			}
		}
	}
}

template<typename T> void WriteArrayRange(std::ostream& ofs, const std::vector<T>& the_array,
		unsigned start, unsigned end)
{
	typename std::vector<T>::const_iterator it = the_array.begin() + start;
	typename std::vector<T>::const_iterator itend = the_array.begin() + end + 1;
	for (; it != itend; ++it)
		ofs << *it;
}

std::string File::ReadChunk(std::istream& ifs)
{

	std::string s;
	do
	{
		char c = (char)ifs.get();
		if (c == '\\')
		{
			while (ifs.get() != '\n')
			{ /*empty*/
				if (ifs.eof())
				{
					break;
				}
			}
		}
		else if (c != '\n')
		{
			break;
		}
		else
			s += c;

		if (ifs.eof())
		{
			break;
		}
	} while (true);
	return s;
}

int File::GetIndex(const char *name)
{
	for(int i=0;i<(int)vertex_buffers.size();i++)
	{
		if(strcmp(name,vertex_buffers[i].name.c_str())==0)
		{
			return i;
		}
	}
	return -1;
}

void File::SetTransform(int index,const float *trans)
{
	if(index<0||index>=(int)vertex_buffers.size())
		return;
	vertex_buffers[index].m_pTransform=trans;
}

/// releases all object data
void File::Release()
{
	MF_FUNC(File__Release);
	m_Vertices.clear();
	m_Normals.clear();
	m_TexCoords.clear();
	m_Groups.clear();
	m_Materials.clear();
	m_Patches.clear();
	m_Triangles.clear();
}

void File::ReadPoints(std::istream& ifs)
{
	MF_FUNC(File__ReadPoints);
	char c;
	std::vector < std::string > VertInfo;

	c = (char) ifs.get();
	// store all strings
	do
	{
		// strip white spaces
		if (ifs.eof())
		{
			goto vinf;
		}
		while (c == ' ' || c == '\t')
		{
			c = (char) ifs.get();
			if (c == '\\')
			{
				while (ifs.get() != '\n')
				{
					if (ifs.eof())
					{
						goto vinf;
					}
				}
				c = (char) ifs.get();
			}
			if (ifs.eof())
			{
				goto vinf;
			}
		}
		std::string s;

		// read vertex info
		while (c != ' ' && c != '\t' && c != '\n')
		{
			s += c;
			c = (char) ifs.get();
			if (ifs.eof())
			{
				goto vinf;
			}
		}

		// store string
		VertInfo.push_back(s);
	} while (c != '\n'); // loop till end of line
	vinf: ;
	std::vector<std::string>::iterator it = VertInfo.begin();
	for (; it != VertInfo.end(); ++it)
	{
		int i;
		sscanf(it->c_str(), "%d", &i);
		if (i < 0)
		{
			i = static_cast<int>(m_Vertices.size()) + i;
		}
		else
			--i;
		m_Points.push_back(i);
	}
}

void File::ReadLine(std::istream& ifs)
{
	MF_FUNC(File__ReadPoints);
	char c;
	std::vector < std::string > VertInfo;

	c = (char)ifs.get();
	// store all strings
	do
	{
		// strip white spaces
		if (ifs.eof())
		{
			goto vinf;
		}
		while (c == ' ' || c == '\t')
		{
			c = (char) ifs.get();
			if (c == '\\')
			{
				while (ifs.get() != '\n')
				{
					if (ifs.eof())
					{
						goto vinf;
					}
				}
				c = (char) ifs.get();
			}
			if (ifs.eof())
			{
				goto vinf;
			}
		}
		std::string s;

		// read vertex info
		while (c != ' ' && c != '\t' && c != '\n')
		{
			s += c;
			c =(char)ifs.get();
			if (ifs.eof())
			{
				goto vinf;
			}
		}

		// store string
		VertInfo.push_back(s);
	} while (c != '\n'); // loop till end of line
	vinf: ;
	Line l;

	l.m_Vertices.resize(VertInfo.size());
	l.m_TexCoords.resize(m_TexCoords.size());

	std::vector<std::string>::iterator it = VertInfo.begin();
	for (; it != VertInfo.end(); ++it)
	{

		if (HasOnlyVertex(*it))
		{
			int i;
			sscanf(it->c_str(), "%d", &i);
			if (i < 0)
			{
				i = static_cast<int>(m_Vertices.size()) + i;
			}
			else
				--i;
			l.m_Vertices.push_back(i);
		}
		else
		{
			int i, j;
			sscanf(it->c_str(), "%d/%d", &i, &j);
			if (i < 0)
			{
				i = static_cast<int>(m_Vertices.size()) + i;
			}
			else
				--i;
			if (j < 0)
			{
				j = static_cast<int>(m_TexCoords.size()) + j;
			}
			else
				--j;
			l.m_Vertices.push_back(i);
			l.m_TexCoords.push_back(j);
		}
	}
	m_Lines.push_back(l);
}

void File::ReadFace(std::istream& ifs)
{
	MF_FUNC(File__ReadFace);
	char c;
	std::vector < std::string > VertInfo;

	// store all strings
	do
	{
		// strip white spaces
		c = (char) ifs.get();
		if (ifs.eof())
		{
			goto vinf;
		}
		while (c == ' ' || c == '\t')
		{
			c =(char)ifs.get();
			if (ifs.eof())
			{
				goto vinf;
			}
		}
		std::string s;

		// read vertex info
		while (c != ' ' && c != '\t' && c != '\n')
		{
			s += c;
			c =(char)ifs.get();
			if (ifs.eof())
			{
				goto vinf;
			}
		}

		// store string
		VertInfo.push_back(s);
	} while (c != '\n'); // loop till end of line

	vinf: ;
	std::vector<int> verts;
	std::vector<int> norms;
	std::vector<int> uvs;
	// split strings into individual indices
	std::vector<std::string>::const_iterator it = VertInfo.begin();
	for (; it != VertInfo.end(); ++it)
	{
		int v, n = 0, t = 0;

		if (HasOnlyVertex(*it))
			sscanf(it->c_str(), "%d", &v);
		else if (MissingUv(*it))
			sscanf(it->c_str(), "%d//%d", &v, &n);
		else if (MissingNormal(*it))
			sscanf(it->c_str(), "%d/%d/", &v, &t);
		else
			sscanf(it->c_str(), "%d/%d/%d", &v, &t, &n);

		if (v < 0)
		{
			v = static_cast<int>(m_Vertices.size()) + v + 1;
		}
		if (n < 0)
		{
			n = static_cast<int>(m_Normals.size()) + n + 1;
		}
		if (t < 0)
		{
			t = static_cast<int>(m_TexCoords.size()) + t + 1;
		}

		// obj indices are 1 based, change them to zero based indices
		--v;
		--n;
		--t;

		verts.push_back(v);
		norms.push_back(n);
		uvs.push_back(t);
	}

	// construct triangles from indices
	for (unsigned i = 2; i < verts.size(); ++i)
	{
		Face f;

		// construct triangle
		f.v[0] = verts[0];
		f.n[0] = norms[0];
		f.t[0] = uvs[0];
		f.v[1] = verts[i - 1];
		f.n[1] = norms[i - 1];
		f.t[1] = uvs[i - 1];
		f.v[2] = verts[i];
		f.n[2] = norms[i];
		f.t[2] = uvs[i];

		// append to list
		m_Triangles.push_back(f);
	}
}

void File::ReadGroup(std::istream& ifs)
{
	MF_FUNC(File__ReadGroup);
	std::string s;
	ifs >> s;
	// ignore the default group, it just contains the verts, normals & uv's for all
	// surfaces. Might as well ignore it!
	if (s != "default")
	{
		if (m_Groups.size())
		{
			Group& gr = m_Groups[m_Groups.size() - 1];
			gr.EndFace = static_cast<unsigned int>(m_Triangles.size());

if(			gr.m_AssignedMaterials.size())
			gr.m_AssignedMaterials[gr.m_AssignedMaterials.size()-1].m_EndFace
			= static_cast<unsigned int>(m_Triangles.size());
		}
		Group g;
		g.name = s;
		g.StartFace = static_cast<unsigned int>(m_Triangles.size());
		m_Groups.push_back(g);
	}
}

bool File::Load(const char filename[])
{
	MF_FUNC(File__Load);
	Filename = filename;
	// just in case a model is already loaded
	Release();

	std::ifstream ifs(filename);
	if (ifs)
	{
		PreLoad(ifs);

		// loop through the file to the end
		while (!ifs.eof())
		{
			std::string s;

			ifs >> s;

			// comment, skip line
			if (s[0] == '#')
			{
				while (ifs.get() != '\n')
					;
			}
			else if (s == "deg")
			{
				std::cerr << "[ERROR] Unable to handle deg yet. Sorry! RB.\n";
			}
			else

			// a new group of faces, ie a seperate mesh
			if (s == "cstype")
				std::cerr
						<< "[ERROR] Unable to handle cstype yet. Sorry! RB.\n";
			else

			// a new group of faces, ie a seperate mesh
			if (s == "bzp")
			{
				BezierPatch bzp;
				std::string text = ReadChunk(ifs);

				sscanf(text.c_str(), "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
						&bzp.VertexIndices[0][0], &bzp.VertexIndices[0][1],
						&bzp.VertexIndices[0][2], &bzp.VertexIndices[0][3],

						&bzp.VertexIndices[1][0], &bzp.VertexIndices[1][1],
						&bzp.VertexIndices[1][2], &bzp.VertexIndices[1][3],

						&bzp.VertexIndices[2][0], &bzp.VertexIndices[2][1],
						&bzp.VertexIndices[2][2], &bzp.VertexIndices[2][3],

						&bzp.VertexIndices[3][0], &bzp.VertexIndices[3][1],
						&bzp.VertexIndices[3][2], &bzp.VertexIndices[3][3]);

				// subtract 1 from all indices
				for (unsigned i = 0; i != 4; ++i)
					for (unsigned j = 0; j != 4; ++j)
						--bzp.VertexIndices[i][j];
			}
			else

			// a new group of faces, ie a seperate mesh
			if (s == "g")
				ReadGroup(ifs);
			else

			// face
			if (s == "f" || s == "fo")
				ReadFace(ifs);
			else

			// points
			if (s == "p")
				ReadPoints(ifs);
			else

			// lines
			if (s == "l")
				ReadLine(ifs);
			else

			// texture coord
			if (s == "vt")
				m_TexCoords.push_back(TexCoord(ifs));
			else

			// normal
			if (s == "vn")
				m_Normals.push_back(Normal(ifs));
			else

			// vertex
			if (s == "v")
				m_Vertices.push_back(Vertex(ifs));
			else

			// vertex parameter
			if (s == "vp")
				m_VertexParams.push_back(VertexParam(ifs));
			else

			// material library
			if (s == "mtllib")
			{
				char buffer[512];
				char* ptr = buffer;
				ifs.getline(buffer, 512);
				while (*ptr == ' ' || *ptr == '\t')
					++ptr;
				// ptr now equals filename. Need to construct path relative to the
				// the files path.

				std::string mtl_file = filename;

				size_t size = mtl_file.size() - 1;
				while (size && (mtl_file[size] != '/')
						&& (mtl_file[size] != '\\'))
					--size;

				if (size)
				{
					mtl_file.resize(size + 1);
					mtl_file += ptr;
				}
				else
				{
					mtl_file = ptr;
				}

				if (!LoadMtl(mtl_file.c_str()))
				{
					if (!LoadMtl(ptr))
					{
						std::cerr << "[WARNING] Unable to load material file\n";
					}
				}
				// "test.mtl";
			}
			else

			// material to apply
			if (s == "usemtl")
			{

				char buffer[512];
				char* ptr = buffer;
				ifs.getline(buffer, 512);
				while (*ptr == ' ' || *ptr == '\t')
					++ptr;

				if (m_Materials.size())
				{

					// find material index
					unsigned mat = 0;
					for (; mat != m_Materials.size(); ++mat)
					{
						if (m_Materials[mat].name == ptr)
						{
							break;
						}
					}

					// if found
					if (mat != m_Materials.size())
					{
						if (m_Groups.size())
						{
							Group& gr = m_Groups[m_Groups.size() - 1];
							if (gr.m_AssignedMaterials.size())
								gr.m_AssignedMaterials[gr.m_AssignedMaterials.size()
										- 1].m_EndFace =
										static_cast<unsigned int>(m_Triangles.size());

MaterialGroup 							mg;
							mg.m_MaterialIdx = mat;
							mg.m_StartFace =
									static_cast<unsigned int>(m_Triangles.size());gr
							.m_AssignedMaterials.push_back(mg);
						}
					}
				}
			}
			else
			// check for any erroneous parametric curve/surface commands out of place - probably don't need this.
			if (s == "end" || s == "parm" || s == "stech" || s == "ctech"
					|| s == "curv" || s == "curv2" || s == "surf" || s == "bmat"
					|| s == "res" || s == "sp" || s == "trim" || s == "hole")
			{

				std::cerr << "[ERROR] Unable to handle " << s
						<< " outside of cstype/end pair\n";
				std::cerr
						<< "[ERROR] Unable to handle cstype yet. Sorry! RB.\n";
				ReadChunk(ifs);
			}
			// dunno what it is, thunk it to derived class.
			else
				HandleUnknown(s, ifs);
		}

		// if groups exist, terminate it.
		if (m_Groups.size())
		{
			Group& gr = m_Groups[m_Groups.size() - 1];

			gr.EndFace = static_cast<unsigned int>(m_Triangles.size());

			// terminate any assigned materials
			if(	gr.m_AssignedMaterials.size())
				gr.m_AssignedMaterials[gr.m_AssignedMaterials.size()-1].m_EndFace= static_cast<unsigned int>(m_Triangles.size());
		}
		GroupsToVertexArrays();
		PostLoad(ifs);
		return true;
	}
	return false;
}

bool File::Save(const char filename[]) const
{
	MF_FUNC(File__Save);

	// if we have materials in the model, save the mtl file
	if (m_Materials.size())
	{
		std::string file = filename;
		size_t len = file.size();
		// strip "obj" extension
		while (file[--len] != '.')
			/*empty*/;
		file.resize(len);
		file += ".mtl";
		SaveMtl(file.c_str());
	}

	std::ofstream ofs(filename);
	if (ofs)
	{
		PreSave(ofs);
		if (m_Groups.size())
		{
			std::vector<Group>::const_iterator itg = m_Groups.begin();
			for (; itg != m_Groups.end(); ++itg)
			{

				// need to determine start and end vertex/normal and uv indices
				unsigned int s_vert, e_vert;
				int s_norm, s_uv, e_norm, e_uv;

				DetermineIndexRange(s_vert, e_vert, s_norm, e_norm, s_uv, e_uv,
						m_Triangles.begin() + itg->StartFace,
						m_Triangles.begin() + itg->EndFace);

				// write default group
				ofs << "g default\n";

				// write groups vertices
				WriteArrayRange < Vertex > (ofs, m_Vertices, s_vert, e_vert);

				// write groups normals (if present)
				if (e_norm != -1)
					WriteArrayRange < Normal > (ofs, m_Normals, s_norm, e_norm);

				// write groups uv coords (if present)
				if (e_uv != -1)
					WriteArrayRange < TexCoord > (ofs, m_TexCoords, s_uv, e_uv);

				// write group name
				ofs << "g " << itg->name << std::endl;

				// write triangles in group
				if (itg->m_AssignedMaterials.size())
				{
					// write out each material group
					std::vector<MaterialGroup>::const_iterator itmg =
							itg->m_AssignedMaterials.begin();
					for (; itmg != itg->m_AssignedMaterials.end(); ++itmg)
					{
						unsigned int mat = itmg->m_MaterialIdx;
						// write use material flag
						ofs << "usemtl " << m_Materials[mat].name << "\n";

						WriteArrayRange<Face> (ofs, m_Triangles, itmg->m_StartFace, itmg->m_EndFace- 1);
					}
				}
				else
					WriteArrayRange<Face> (ofs, m_Triangles, itg->StartFace, itg->EndFace- 1);
			}
		}
		else
		{
			// all part of default group
			ofs << "g default\n";
			WriteArrayRange < Vertex> (ofs, m_Vertices, 0, static_cast<unsigned>(m_Vertices.size())
							- 1);
			WriteArrayRange < TexCoord> (ofs, m_TexCoords, 0, static_cast<unsigned>(m_TexCoords.size())
							- 1);
			WriteArrayRange < Normal> (ofs, m_Normals, 0, static_cast<unsigned>(m_Normals.size())
							- 1);
			WriteArrayRange < Face> (ofs, m_Triangles, 0, static_cast<unsigned>(m_Triangles.size())
							- 1);
		}
		PostSave(ofs);
		ofs.close();
		return true;
	}
	return false;
}

// loads the specified material file
bool File::LoadMtl(const char filename[])
{
	MF_FUNC(File__LoadMtl);
	std::ifstream ifs(filename);
	if (ifs)
	{
		Material* pmat = 0;
		while (!ifs.eof())
		{
			std::string s;
			ifs >> s;
			if (s == "newmtl")
			{
				Material mat;
				ifs >> mat.name;
				m_Materials.push_back(mat);
				pmat = &(m_Materials[m_Materials.size() - 1]);
			}
			else if (s == "illum")
				ifs >> pmat->illum;
			else if (s == "Kd")
				ifs >> pmat->Kd[0] >> pmat->Kd[1] >> pmat->Kd[2];
			else if (s == "Ka")
				ifs >> pmat->Ka[0] >> pmat->Ka[1] >> pmat->Ka[2];
			else if (s == "Ks")
				ifs >> pmat->Ks[0] >> pmat->Ks[1] >> pmat->Ks[2];
			else if (s == "Tf")
				ifs >> pmat->Tf[0] >> pmat->Tf[1] >> pmat->Tf[2];
			else if (s == "Ni")
				ifs >> pmat->Ni;
			else if (s == "Ns")
				ifs >> pmat->Ns;
			else if (s == "map_Ka")
			{
				char buffer[512];
				char* ptr = buffer;
				ifs.getline(buffer, 512);
				while (*ptr == ' ' || *ptr == '\t')
					++ptr;
				pmat->map_Ka = ptr;
				pmat->gltex_Ka = OnLoadTexture(pmat->map_Ka.c_str());
			}
			else if (s == "map_Kd")
			{
				char buffer[512];
				char* ptr = buffer;
				ifs.getline(buffer, 512);
				while (*ptr == ' ' || *ptr == '\t')
					++ptr;
				pmat->map_Kd = ptr;
				pmat->gltex_Kd = OnLoadTexture(pmat->map_Kd.c_str());
			}
			else if (s == "map_Ks")
			{
				char buffer[512];
				char* ptr = buffer;
				ifs.getline(buffer, 512);
				while (*ptr == ' ' || *ptr == '\t')
					++ptr;
				pmat->map_Ks = ptr;
				pmat->gltex_Ks = OnLoadTexture(pmat->map_Ks.c_str());
			}
			else if (s == "bump")
			{
				char buffer[512];
				char* ptr = buffer;
				ifs.getline(buffer, 512);
				while (*ptr == ' ' || *ptr == '\t')
					++ptr;

				char* flag = ptr;
				while (strncmp(flag, "-bm", 3) != 0)
				{
					++flag;
					if (*flag == '\0')
						return false;
				}
				// null terminate the filename
				*(flag - 1) = '\0';

				pmat->map_Bump = ptr;
				flag += 3;
				sscanf(flag, "%d", &pmat->Bm);
				pmat->gltex_Bump = OnLoadTexture(pmat->map_Bump.c_str());
			}
		}
		ifs.close();
		return true;
	}
	return false;
}
namespace Obj
{
	std::ostream& operator <<(std::ostream& ofs, const Material& f)
	{

		MF_FUNC(operator__output__Material);
		ofs << "newmtl " << f.name << "\n";
		ofs << "illum " << f.illum << "\n";
		ofs << "Kd " << f.Kd[0] << " " << f.Kd[1] << " " << f.Kd[2] << "\n";
		ofs << "Ka " << f.Ka[0] << " " << f.Ka[1] << " " << f.Ka[2] << "\n";
		ofs << "Ks " << f.Ks[0] << " " << f.Ks[1] << " " << f.Ks[2] << "\n";
		ofs << "Tf " << f.Tf[0] << " " << f.Tf[1] << " " << f.Tf[2] << "\n";
		ofs << "Ni " << f.Ni << "\n";
		ofs << "Ns " << f.Ns << "\n";
		if (f.map_Kd.size())
			ofs << "map_Kd " << f.map_Kd << "\n";
		if (f.map_Ka.size())
			ofs << "map_Ka " << f.map_Ka << "\n";
		if (f.map_Ks.size())
			ofs << "map_Ks " << f.map_Ks << "\n";
		if (f.map_Bump.size())
			ofs << "bump " << f.map_Bump << " -bm " << f.Bm << "\n";
		return ofs;
	}
}

bool File::SaveMtl(const char filename[]) const
{
	MF_FUNC(File__SaveMtl);
	std::ofstream ofs(filename);
	if (ofs)
	{
		std::vector<Material>::const_iterator it = m_Materials.begin();
		for (; it != m_Materials.end(); ++it)
			ofs << *it;
		ofs.close();
		return true;
	}
	return false;
}

void File::DrawRange(unsigned int start_face, unsigned int end_face) const
{
	std::vector<Face>::const_iterator it = m_Triangles.begin() + start_face;
	std::vector<Face>::const_iterator end = m_Triangles.begin() + end_face;
	for (; it != end; ++it)
	{
		for (int i = 0; i != 3; ++i)
		{
			if (it->n[i] != -1)
				m_Normals[it->n[i]].gl();
			if (it->t[i] != -1)
				m_TexCoords[it->t[i]].gl();
			m_Vertices[it->v[i]].gl();
		}
	}
}

void File::Draw() const
{
	if (vertex_buffers.size())
	{
		std::vector<VertexBuffer>::const_iterator itvb = vertex_buffers.begin();
		for (; itvb != vertex_buffers.end(); ++itvb)
		{
			itvb->gl();
			GL_ERROR_CHECK
		}
	}
	else if(surfaces.size())
	{
		std::vector<Surface>::const_iterator its = surfaces.begin();
		for (; its != surfaces.end(); ++its)
		{
			its->gl();
		}
	}
	else if (m_Groups.size())
	{
		std::vector<Group>::const_iterator itg = m_Groups.begin();
		for (; itg != m_Groups.end(); ++itg)
		{
			// write triangles in group
			if (itg->m_AssignedMaterials.size())
			{
				// write out each material group
				std::vector<MaterialGroup>::const_iterator itmg =
						itg->m_AssignedMaterials.begin();
				for (; itmg != itg->m_AssignedMaterials.end(); ++itmg)
				{
					GL_ERROR_CHECK
					// bind the required material
					OnBindMaterial(m_Materials[itmg->m_MaterialIdx]);
					GL_ERROR_CHECK

					// draw faces that use this material
					glBegin(GL_TRIANGLES);
					DrawRange(itmg->m_StartFace, itmg->m_EndFace);
					glEnd();
					GL_ERROR_CHECK
				}
			}
			else
			{
				glBegin(GL_TRIANGLES);
				DrawRange(itg->StartFace, itg->EndFace);
				glEnd();
			}
			GL_ERROR_CHECK
		}
	}
	else
	{
		glBegin(GL_TRIANGLES);
		DrawRange(0, static_cast<unsigned int>(m_Triangles.size()));glEnd
		();
	}
	GL_ERROR_CHECK
	GL_ERROR_CHECK
}

void File::CalculateNormals()
{
	MF_FUNC(File__CalculateNormals);
	if (!m_Triangles.size())
	{
		return;
	}

	if (m_Triangles[0].n[0] == -1)
	{
		m_Normals.resize(m_Vertices.size());

		std::vector<Face>::iterator it = m_Triangles.begin();
		for (; it != m_Triangles.end(); ++it)
		{

			// use vertex indices for normal indices
			it->n[0] = it->v[0];
			it->n[1] = it->v[1];
			it->n[2] = it->v[2];
		}
	}

	// resize normal array if not present
	ZeroNormals(m_Normals);

	// loop through each triangle in face
	std::vector<Face>::const_iterator it = m_Triangles.begin();
	for (; it != m_Triangles.end(); ++it)
	{
		DoFaceCalc(m_Vertices[it->v[0]], m_Vertices[it->v[1]],
				m_Vertices[it->v[2]], m_Normals[it->n[0]], m_Normals[it->n[1]],
				m_Normals[it->n[2]]);
	}
	NormaliseNormals(m_Normals);
}

void File::GroupsToSurfaces(std::vector<Surface>& surface_list)
{
	MF_FUNC(File__GroupsToSurfaces);
	if (m_Groups.size())
	{
		surface_list.resize(m_Groups.size());

		std::vector<Surface>::iterator its = surface_list.begin();

		std::vector<Group>::const_iterator itg = m_Groups.begin();
		for (; itg != m_Groups.end(); ++itg, ++its)
		{

			// need to determine start and end vertex/normal and uv indices
			unsigned int s_vert, e_vert;
			int s_norm, s_uv, e_norm, e_uv;

			DetermineIndexRange(s_vert, e_vert, s_norm, e_norm, s_uv, e_uv,
					m_Triangles.begin() + itg->StartFace,
					m_Triangles.begin() + itg->EndFace);

			// set file pointer
			its->m_pFile = this;

			// set name
			its->name = itg->name;

			// copy material groups for surface
			its->m_AssignedMaterials = itg->m_AssignedMaterials;

			// make material groups relative to material start
			std::vector<MaterialGroup>::iterator itmg =
					its->m_AssignedMaterials.begin();
			for (; itmg != its->m_AssignedMaterials.end(); ++itmg)
			{
				itmg->m_StartFace -= itg->StartFace;
				itmg->m_EndFace -= itg->StartFace;
			}

			// resize triangles
			its->m_Triangles.resize(itg->EndFace - itg->StartFace);

			std::vector<Face>::iterator ito = its->m_Triangles.begin();
			std::vector<Face>::const_iterator it = m_Triangles.begin()+itg->StartFace;
			std::vector<Face>::const_iterator end =m_Triangles.begin()+itg->EndFace;

			for (; it != end; ++it, ++ito)
			{
				for (int i = 0; i != 3; ++i)
				{
					ito->v[i] = it->v[i] - s_vert;
					int N=(e_norm==-1) ? -1 : (it->n[i]-s_norm);
					ito->n[i] = it->n[i]==-1?-1:N;
					int T=(e_uv==-1) ? -1 : (it->t[i]-s_uv);
					ito->t[i] = (it->t[i]==-1)?-1:T;
				}
			}

			// copy over vertices
			CopyArray < Vertex > (its->m_Vertices, m_Vertices, s_vert, e_vert);

			// copy over normals
			if (e_norm != -1)
				CopyArray < Normal
						> (its->m_Normals, m_Normals, s_norm, e_norm);

			// copy over tex coords
			if (e_uv != -1)
				CopyArray < TexCoord
						> (its->m_TexCoords, m_TexCoords, s_uv, e_uv);
		}
	}
	else
	{
		surface_list.resize(1);
		surface_list[0].m_Vertices = m_Vertices;
		surface_list[0].m_Normals = m_Normals;
		surface_list[0].m_TexCoords = m_TexCoords;
		surface_list[0].m_Triangles = m_Triangles;
		surface_list[0].m_pFile = this;
		surface_list[0].name = "default";
	}
}

void File::GroupsToVertexArrays()
{
	MF_FUNC(File__GroupsToVertexArrays);
	// first split into surfaces
	GroupsToSurfaces(surfaces);

	// now convert each surface into a vertex array
	vertex_buffers.resize(surfaces.size());

	std::vector<VertexBuffer>::iterator itb = vertex_buffers.begin();
	std::vector<Surface>::iterator its = surfaces.begin();

	for (; itb != vertex_buffers.end(); ++itb, ++its)
	{
		// set name
		itb->name = its->name;
		// set file
		itb->m_pFile = this;
		// copy material assignments
		itb->m_AssignedMaterials = its->m_AssignedMaterials;

		// determine new vertex and index arrays.
		std::vector<Face>::iterator itf = its->m_Triangles.begin();
		for (; itf != its->m_Triangles.end(); ++itf)
		{
			for (int i = 0; i != 3; ++i)
			{
				const Vertex* v = &its->m_Vertices[itf->v[i]];
				const Normal* n = 0;
				if (itf->n[i] != -1)
					n = &its->m_Normals[itf->n[i]];
				const TexCoord* t = 0;
				if (itf->t[i] != -1)
					t = &its->m_TexCoords[itf->t[i]];

				unsigned int idx = 0;
				if (n && t)
				{
					std::vector<Vertex>::const_iterator itv		=itb->m_Vertices.begin();
					std::vector<Normal>::const_iterator itn		=itb->m_Normals.begin();
					std::vector<TexCoord>::const_iterator itt	=itb->m_TexCoords.begin();
					//idx=itb->m_Vertices.size();
					for (; itt != itb->m_TexCoords.end()&&itn != itb->m_Normals.end();
							++itv, ++itn, ++itt, ++idx)
					{
						if (*v == *itv && *n == *itn && *t == *itt)
							break;
					}
					if(itt==itb->m_TexCoords.end()||itn==itb->m_Normals.end())
					{
						idx=(unsigned)itb->m_Vertices.size();
						itb->m_Vertices.push_back(*v);
						itb->m_Normals.push_back(*n);
						itb->m_TexCoords.push_back(*t);
					}
					itb->m_Indices.push_back(idx);
				}
				else if (n)
				{
					std::vector<Vertex>::const_iterator itv =itb->m_Vertices.begin();
					std::vector<Normal>::const_iterator itn =itb->m_Normals.begin();
					for (; itn != itb->m_Normals.end(); ++itv, ++itn, ++idx)
					{
						if (*v == *itv && *n == *itn)
							break;
					}
					if (itn == itb->m_Normals.end())
					{
						idx=(unsigned)itb->m_Vertices.size();
						itb->m_Vertices.push_back(*v);
						itb->m_Normals.push_back(*n);
						itb->m_TexCoords.push_back(TexCoord());
					}
					itb->m_Indices.push_back(idx);
				}
				else if (t)
				{
					std::vector<Vertex>::const_iterator itv =itb->m_Vertices.begin();
					std::vector<TexCoord>::const_iterator itt =itb->m_TexCoords.begin();
					for (; itt != itb->m_TexCoords.end(); ++itv, ++itt, ++idx)
					{
						if (*v == *itv && *t == *itt)
							break;
					}
					if (itt == itb->m_TexCoords.end())
					{
						idx=(unsigned)itb->m_Vertices.size();
						itb->m_Vertices.push_back(*v);
						itb->m_Normals.push_back(Normal());
						itb->m_TexCoords.push_back(*t);
					}
					itb->m_Indices.push_back(idx);
				}
				else
				{
					std::vector<Vertex>::const_iterator itv =itb->m_Vertices.begin();
					for (; itv != itb->m_Vertices.end(); ++itv, ++idx)
					{
						if (*v == *itv)
							break;
					}
					if (itv == itb->m_Vertices.end())
					{
						idx=(unsigned)itb->m_Vertices.size();
						itb->m_Vertices.push_back(*v);
						itb->m_Normals.push_back(Normal());
						itb->m_TexCoords.push_back(TexCoord());
					}
					itb->m_Indices.push_back(idx);
				}
			}
		}
	}
}

unsigned int File::OnLoadTexture(const char filename[])
{
	int last_slash = (int)Filename.find_last_of('/');
	int last_backslash = (int)Filename.find_last_of('\\');
	if (last_backslash < (int)Filename.length() && last_backslash > last_slash)
		last_slash = last_backslash;
	std::string fn = Filename.substr(0, last_slash);
	fn += "/";
	fn += filename;
	return LoadGLImage(fn.c_str(), GL_REPEAT);
}
