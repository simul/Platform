#ifndef SIMUL_MATH_MATRIXVECTOR3_H
#define SIMUL_MATH_MATRIXVECTOR3_H


#include "Platform/Math/Matrix.h"  
#include "Platform/Math/Matrix4x4.h"  
#include "Platform/Math/Vector3.h"
#include "Platform/Math/Export.h"


#if defined(_DEBUG) && defined(_MSC_VER)
#define CHECK_MATRIX_BOUNDS
#endif

namespace simul
{
	namespace math
	{
		void SIMUL_MATH_EXPORT_FN Precross(Matrix4x4 &M,const Vector3 &v);
		void SIMUL_MATH_EXPORT_FN TransposeMultiply(Vector3 &V2,const Matrix &M,const Vector3 &V1);
		void SIMUL_MATH_EXPORT_FN Multiply3(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);
		void SIMUL_MATH_EXPORT_FN Multiply3(Vector3 &V2,const Vector3 &V1,const Matrix4x4 &M);
		void SIMUL_MATH_EXPORT_FN Multiply4(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);
		void SIMUL_MATH_EXPORT_FN MultiplyAndAdd(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);
		void SIMUL_MATH_EXPORT_FN MultiplyAndSubtract(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);
		void SIMUL_MATH_EXPORT_FN InsertNegativeVectorAsRow(Matrix &M,const Vector3 &v,const unsigned row);
		void SIMUL_MATH_EXPORT_FN AddDoublePrecross(Matrix4x4 &M,float f,const Vector3 &v);
		void SIMUL_MATH_EXPORT_FN SubtractDoublePrecross(Matrix4x4 &M,float f,const Vector3 &v);
		void SIMUL_MATH_EXPORT_FN AddVectorToRow(Matrix &M,unsigned Row,Vector3 &V);
		void SIMUL_MATH_EXPORT_FN AddRow(Vector3 &V1,Matrix &M2,unsigned Row2);   
		void SIMUL_MATH_EXPORT_FN RowToVector(const Matrix &M,Vector3 &V,unsigned Row);
		float SIMUL_MATH_EXPORT_FN MatrixRowTimesVector(const Matrix &M,const unsigned Row,const Vector3 &V);
		void SIMUL_MATH_EXPORT_FN InsertVectorAsColumn(Matrix &M,const Vector3 &v,const unsigned col);
		void SIMUL_MATH_EXPORT_FN InsertNegativeVectorAsColumn(Matrix &M,const Vector3 &v,const unsigned col);
		void SIMUL_MATH_EXPORT_FN TransposeMultiply4(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);
		void SIMUL_MATH_EXPORT_FN TransposeMultiply3(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);
	}
}

#endif