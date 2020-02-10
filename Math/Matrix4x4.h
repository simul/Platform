#ifndef MATRIX4X4H
#define MATRIX4X4H

#include "Simul/Platform/Math/Export.h"
#include "Simul/Platform/Math/Align.h" 
#include <assert.h>

#ifdef WIN32
	#pragma pack(push,16)
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201) // anonymous unions warning
#endif

namespace simul
{
	namespace math
	{
		class Vector;
		class Vector3;
		class Quaternion;

		/// A 4 by 4 Matrix class.

		/// A fixed-size 4 by 4 matrix class.
		/// It is used mainly for transformations - see SimulOrientation.
		SIMUL_MATH_EXPORT_CLASS Matrix4x4
		{
		public:
			union
			{
				float Values[16];
				struct
				{
					float        _11, _12, _13, _14;
					float        _21, _22, _23, _24;
					float        _31, _32, _33, _34;
					float        _41, _42, _43, _44;
				};
				struct
				{
					float        m00, m01, m02, m03;
					float        m10, m11, m12, m13;
					float        m20, m21, m22, m23;
					float        m30, m31, m32, m33;
				};
				float m[4][4];
			};
			Matrix4x4();
			Matrix4x4(const Matrix4x4 &M);
			Matrix4x4(const float *x);
			Matrix4x4(const double *x);
			Matrix4x4(	float x11,float x12,float x13,float x14,
						float x21,float x22,float x23,float x24,
						float x31,float x32,float x33,float x34,
						float x41,float x42,float x43,float x44);
			~Matrix4x4();
			class OutOfRange{};
			bool operator==(const Matrix4x4 &M)
			{
				for(unsigned i=0;i<16;i++)
				{
					if(Values[i]!=M.Values[i])
						return false;
				}
				return true;
			}
			bool operator!=(const Matrix4x4 &M)
			{
				return !(operator==(M));
			}
			operator const float*() const
			{
				return Values;
			}
			operator float*() 
			{
				return Values;
			}
			void operator+=(const Matrix4x4 &M);
			void operator-=(const Matrix4x4 &M);
			float &operator()(unsigned row,unsigned col);
			float &operator[](int pos)
			{
				assert(pos>=0&&pos<16);
				return Values[pos];
			}
			float operator[](int pos) const
			{
				assert(pos>=0&&pos<16);
				return Values[pos];
			}
			float &operator[](unsigned pos)
			{
				assert(pos>=0&&pos<16);
				return Values[pos];
			}
			float operator[](unsigned pos) const
			{
				assert(pos>=0&&pos<16);
				return Values[pos];
			}
			float operator()(unsigned row,unsigned col) const;
			const Vector3 &GetRowVector(unsigned r) const;
			void operator=(const Matrix4x4 &M);
			Matrix4x4& operator*=(float d);  
			Matrix4x4& operator/=(float d);
			void Transpose(Matrix4x4 &T) const;
			void Transpose();
			float *RowPointer(unsigned row)
			{
				assert(row>=0&&row<4);
				return &(Values[row*4]);
			}
			const float *RowPointer(unsigned row) const
			{
				assert(row>=0&&row<4);
				return &(Values[row*4]);
			}
			float *GetValues()	{return Values;}
			const float *GetValues() const	{return Values;}
			void Zero();						///< Set all values to zero.
			void ResetToUnitMatrix();			///< Make a unit matrix.
			static Matrix4x4 IdentityMatrix();	///< Make a unit matrix.
			void Set(const float []);			///< Set the 16 values.
			bool SymmetricInverse3x3(Matrix4x4 &Result);
			bool CholeskyFactorization(Matrix4x4 &Fac) const;
			void InverseFromCholeskyFactors(Matrix4x4 &Result,Matrix4x4 &Fac) const;
			void AddScalarTimesMatrix(float val,const Matrix4x4 &m2);
			void InsertColumn(int col,Vector3 v);
			void InsertRow(int row,Vector3 v);
			void ScaleRows(const float s[4]);
			void ScaleColumns(const float s[4]);
			void SimpleInverse(Matrix4x4 &Inv) const;
			void Inverse(Matrix4x4 &Inv) const;
			// Transform matrix from world space to an oblique space aligned with \em direction on the axis specified
			// and aligned with worldspace on the other two axes
			static Matrix4x4 ObliqueToCartesianTransform(int axis,Vector3 direction,Vector3 axis1,Vector3 origin,Vector3 scales);
			static Matrix4x4 DefineFromYZ(Vector3 y_dir,Vector3 z_dir);
			//! Simply exchange the Y and Z values of a transformation matrix.
			static void SwapYAndZ(Matrix4x4 &dest);
			//! Reverse the Y values of a transformation matrix - i.e. multiply the off-diagonal terms of the second row and column by -1.
			static void ReverseY(Matrix4x4 &dest);
			static Matrix4x4 RotationX(float rads);
			static Matrix4x4 RotationY(float rads);
			static Matrix4x4 RotationZ(float rads);
			static Matrix4x4 Translation(float x,float y,float z);
			static Matrix4x4 Translation(const float *x);
		};
		extern Matrix4x4 SIMUL_MATH_EXPORT_FN Rotate(const Matrix4x4 &A,float angle,const Vector3 &axis);
		extern void SIMUL_MATH_EXPORT_FN Multiply4x4(Matrix4x4 &M,const Matrix4x4 &A,const Matrix4x4 &B);
		extern void SIMUL_MATH_EXPORT_FN Multiply3x3(Matrix4x4 &M,const Matrix4x4 &A,const Matrix4x4 &B);
		extern void SIMUL_MATH_EXPORT_FN Multiply4x4ByTranspose(Matrix4x4 &M,const Matrix4x4 &M1,const Matrix4x4 &M2);   
		extern void SIMUL_MATH_EXPORT_FN Multiply3x3ByTranspose(Matrix4x4 &M,const Matrix4x4 &M1,const Matrix4x4 &M2);   
		extern void SIMUL_MATH_EXPORT_FN MultiplyTransposeByMatrix3x3(Matrix4x4 &M,const Matrix4x4 &M1,const Matrix4x4 &M2);
		extern void SIMUL_MATH_EXPORT_FN QuaternionToMatrix(Matrix4x4 &m,const Quaternion &q); 
		extern void SIMUL_MATH_EXPORT_FN MatrixToQuaternion(Quaternion &q,const Matrix4x4 &m); 
		extern void SIMUL_MATH_EXPORT_FN ColumnToVector(const Matrix4x4 &M,Vector3 &V,unsigned Row);
		extern void SIMUL_MATH_EXPORT_FN RowToVector(const Matrix4x4 &M,Vector3 &V,unsigned Row);
		extern void SIMUL_MATH_EXPORT_FN TransposeMultiply4(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);  
		extern void SIMUL_MATH_EXPORT_FN Multiply3(Vector3 &v2,const Matrix4x4 &m,const Vector3 &v1);    
		extern void SIMUL_MATH_EXPORT_FN FPUQuaternionToMatrix(Matrix4x4 &m,const Quaternion &q);
		extern void SIMUL_MATH_EXPORT_FN AddDoublePrecross(Matrix4x4 &M,float f,const Vector3 &v);
		extern void SIMUL_MATH_EXPORT_FN SubtractDoublePrecross(Matrix4x4 &M,float f,const Vector3 &v);
		extern void SIMUL_MATH_EXPORT_FN Multiply(Vector3 &v2,const Matrix4x4 &m,const Vector3 &v1);
		extern void SIMUL_MATH_EXPORT_FN MultiplyAndAdd(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);
		extern void SIMUL_MATH_EXPORT_FN MultiplyAndSubtract(Matrix4x4 &m,const Matrix4x4 &m1,const Matrix4x4 &m2);
		extern void SIMUL_MATH_EXPORT_FN MultiplyAndSubtract(Vector3 &V2,const Matrix4x4 &M,const Vector3 &V1);
		extern void SIMUL_MATH_EXPORT_FN MultiplyByScalar(Matrix4x4 &result,const float f,const Matrix4x4 &m);
		extern void SIMUL_MATH_EXPORT_FN AddFloatTimesMatrix4x4(Matrix4x4 &result,const float f,const Matrix4x4 &m);
		extern void SIMUL_MATH_EXPORT_FN Precross(Matrix4x4 &M,const Vector3 &v);
		extern void SIMUL_MATH_EXPORT_FN Multiply4x4(Matrix4x4 &M,const Matrix4x4 &A,const Matrix4x4 &B);
		extern void SIMUL_MATH_EXPORT_FN QuaternionToMatrix(Matrix4x4 &m,const Quaternion &q); 
	}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#ifdef WIN32
#pragma pack(pop)
#endif

#endif

