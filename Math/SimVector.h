#ifndef SimVectorH
#define SimVectorH


#include <stdlib.h>
#include <float.h>
#if !defined(DOXYGEN) && !defined(__GNUC__)
	#define ALIGN16 _declspec(align(16))
#else
	#define ALIGN16
#endif

#include "Platform/Math/Export.h"     
#include "Platform/Math/Matrix.h"      
#include "Platform/Math/Matrix4x4.h"
#include "Platform/Math/MathFunctions.h"
#include <iostream>

namespace simul
{
	namespace math
	{
class Vector3;
class Quaternion;

/// A resizeable vector class
class SIMUL_MATH_EXPORT Vector
{
protected:
	float *Values;
public:
	unsigned size;
	unsigned S16;
protected:
	float *V;
public:
	Vector(void);
	Vector(unsigned psize);
	Vector(const float *x);
	Vector(float x1,float x2,float x3);
	Vector(const Vector &v);
	virtual ~Vector(void);
	void ClearMemory(void);					///< Resize to zero and free the memory which was used.
	float *FloatPointer(void) const
	{
		return Values;
	}			
	float *FloatPointer(unsigned idx) const
	{
		return &(Values[idx]);
	}
/// Multiply this vector by a scalar
	virtual void operator*=(float f);		///< Multiply by f.
	bool operator>(float R) const;			
/// Set all values to zero.
	void Zero(void);						///< Set all values to zero.
	bool ContainsNAN(void);					///< Is any value not a number?
	void Clear(void)
	{
		size=0;
	}					
/// Remove the value at Idx, shift the remainder down one place.
	void Cut(unsigned Idx);						///< Remove the value at Idx, shifting values after it back one place.
	void CutUnordered(unsigned Idx);
/// Insert a value at Idx.
	void Insert(unsigned Idx);					///< Insert a value at Idx, shifting succeeding values up one place.
	void Append(float s);					///< Increase vector size by one, putting s on the end.
	void CheckEdges(void) const;
	bool NonZero(void);
/// Scale the vector so its magnitude is unity.
	bool Unit(void);						///< Divide by the vector's magnitude.
	void CheckNAN(void);
/// Change the vector size.
	virtual void Resize(unsigned s);				///< Resize the vector, allocating memory if s>size.
protected:
	void ChangeSize(unsigned s);
public:
	class BadSize{};
	class BadNumber{};
/// The float at position given by index.
	float &operator() (unsigned index);
/// The float at position given by index.
	float operator() (unsigned index) const;
	float *Position(unsigned index) const;
	friend void SIMUL_MATH_EXPORT_FN CrossProduct(Matrix &result,const Vector &v1,const Matrix &M2);
	friend void SIMUL_MATH_EXPORT_FN CrossProductWithTranspose(Matrix &result,const Vector &V1,const Matrix &M2);
	friend void SIMUL_MATH_EXPORT_FN Multiply(Vector &ret,const Quaternion &q,const Vector &v);
	friend void SIMUL_MATH_EXPORT_FN MultiplyElements8(Vector &ret,const Vector &V1,const Vector &V2);
	friend void SIMUL_MATH_EXPORT_FN MultiplyElements(Vector &ret,const Vector &V1,const Vector &V2);
	friend void SIMUL_MATH_EXPORT_FN MultiplyElementsAndAdd(Vector &ret,const Vector &V1,const Vector &V2);
	friend void SIMUL_MATH_EXPORT_FN AddQuaternionTimesVector(Vector &ret,const Quaternion &q,const Vector &v);
	friend void SIMUL_MATH_EXPORT_FN Divide(Vector &ret,const Quaternion &q,const Vector &v);
	friend void SIMUL_MATH_EXPORT_FN Multiply(Vector &V2,const Matrix &M,const Vector &V1);       
	friend void SIMUL_MATH_EXPORT_FN MultiplyNegative(Vector &V2,const Matrix &M,const Vector &V1);
	friend void SIMUL_MATH_EXPORT_FN MultiplyVectorByTranspose(Vector &result,const Vector &v,const Matrix &M);
	friend void SIMUL_MATH_EXPORT_FN Multiply(Vector &v2,const Vector &v1,const Matrix &m);
	friend void SIMUL_MATH_EXPORT_FN MultiplyTransposeAndSubtract(Vector &v2,const Vector &v1,const Matrix &m);
	friend void SIMUL_MATH_EXPORT_FN MultiplyAndAdd(Vector &v2,const Matrix &m,const Vector &v1);
	friend void SIMUL_MATH_EXPORT_FN MultiplyAndSubtract(Vector &v2,const Vector &v1,const Matrix &m);
	friend void SIMUL_MATH_EXPORT_FN TransposeMultiply(Vector &v2,const Matrix &m,const Vector &v1);
	friend void SIMUL_MATH_EXPORT_FN TransposeMultiplyAndAdd(Vector &v2,const Matrix &m,const Vector &v1);
	friend void SIMUL_MATH_EXPORT_FN TransposeMultiplyAndSubtract(Vector &v2,const Matrix &m,const Vector &v1);
	friend void SIMUL_MATH_EXPORT_FN AddFloatTimesVector(Vector &V2,const float f,const Vector &V1);
	friend void SIMUL_MATH_EXPORT_FN AddFloatTimesLargerVector(Vector &V2,const float f,const Vector &V1);
	friend void SIMUL_MATH_EXPORT_FN SubtractFloatTimesVector(Vector &V2,const float f,const Vector &V1);
	friend void SIMUL_MATH_EXPORT_FN Multiply(Vector &V2,const float f,const Vector &V1);
	friend void SIMUL_MATH_EXPORT_FN Matrix::InsertVectorTimesMatrix(const Vector &V,const Matrix &M,unsigned Row,unsigned Col);
	friend void SIMUL_MATH_EXPORT_FN Matrix::InsertVectorTimesSubMatrix(const Vector &V,const Matrix &M,unsigned Row,unsigned Col,unsigned Width);
	friend void SIMUL_MATH_EXPORT_FN Matrix::InsertAddVectorTimesSubMatrix(const Vector &V,const Matrix &M,const unsigned Row,const unsigned Col,const unsigned Width);
	friend void SIMUL_MATH_EXPORT_FN MultiplyAndSubtract(Matrix &m,const Matrix &m1,const Matrix &m2);
	friend void SIMUL_MATH_EXPORT_FN MultiplyNegative(Vector &result,const Vector &v,const Matrix &m);
	friend void SIMUL_MATH_EXPORT_FN RowToVector(const Matrix &M,Vector &V,unsigned Row);
	friend void SIMUL_MATH_EXPORT_FN ColumnToVector(Matrix &M,Vector &V,unsigned Col);
	friend void SIMUL_MATH_EXPORT_FN Swap(Vector &V1,Vector &V2);
	friend bool SIMUL_MATH_EXPORT_FN operator==(Vector &v1,Vector &v2);
	friend bool SIMUL_MATH_EXPORT_FN operator!=(Vector &v1,Vector &v2);
	friend void SIMUL_MATH_EXPORT_FN Matrix::RowMatrix(Vector& v);
	friend void SIMUL_MATH_EXPORT_FN Matrix::InsertAddVectorTimesMatrix(const Vector &V,const Matrix &M,unsigned Row,unsigned Col);
	friend float SIMUL_MATH_EXPORT_FN MultiplyRows(Matrix &M,unsigned R1,unsigned R2);
	friend void SIMUL_MATH_EXPORT_FN Multiply(Matrix &M,const Vector &V1,const Matrix &M2,unsigned Column);
	friend void SIMUL_MATH_EXPORT_FN ShiftElement(Vector &V1,Vector &V2,unsigned Idx2);
	friend SIMUL_MATH_EXPORT class Matrix;
	friend SIMUL_MATH_EXPORT class Quaternion;     
	friend void SIMUL_MATH_EXPORT_FN TransposeMultiply3x3(Vector &V2,const Matrix4x4 &M,const Vector &V1);
// Divide this vector by a scalar
	void operator/=(float f);
	float & operator [] (unsigned index);
	float operator [] (unsigned index) const;
	float * operator! (void);
	float Magnitude(void) const;
	float SquareSize(void) const;
// Assignment operator - copy constr should do the work???
	virtual void operator=(const Vector &v);
	void operator=(const Vector3 &v);
	void operator=(const float *f);
// Assign a vector which we KNOW is the same size!
	void operator|=(const Vector &v);
// Unary operators
	Vector operator+()
	{
		return *this;
	}
	Vector operator-();
	void operator+=(const Vector &v);
	void operator+=(const Vector3 &v);
	void operator-=(const Vector3 &v);
	Vector& operator-= (const Vector &v);
	void AddLarger(const Vector &v);
// Vector times matrix functions
	Matrix operator^(const Matrix &m);
	Vector operator*(const Matrix &m);
// Sub-vector operations;
	Vector SubVector(unsigned start,unsigned length) const;
	void SubVector(Vector &v,unsigned start,unsigned length) const
	{
		unsigned i;
		for(i=0;i<length;++i)
		{
			v.Values[i]=Values[start+i];
		}
	}
	void SubVector(Vector3 &v,unsigned start) const;
	Vector RemoveElement(unsigned index)
	{
		Vector v(size-(unsigned)1);
		unsigned i;
		for(i=0;i<size-1;++i)
			v.Values[i]=Values[i+(i>=index)];
		return v;
	}
	void Insert(const Vector &v,unsigned pos);
	void Insert(const Vector3 &v,unsigned pos);
	void InsertSubVector(const Vector &v,unsigned InsertPos,unsigned SourcePos,unsigned Length)
	{
	#ifdef CHECK_MATRIX_BOUNDS
		if(InsertPos+Length>size||SourcePos+Length>v.size)
			throw BadSize();
	#endif
		unsigned i;
		for(i=0;i<Length;++i)
		{
			Values[InsertPos+i]=v.Values[SourcePos+i];
		}
	}
	void InsertAddSubVector(const Vector &v,unsigned InsertPos,unsigned SourcePos,unsigned Length)
	{
	#ifdef CHECK_MATRIX_BOUNDS
		if(InsertPos+Length>size||SourcePos+Length>v.size)
			throw BadSize();
	#endif
		unsigned i;
		for(i=0;i<Length;++i)
		{
			Values[InsertPos+i]+=v.Values[SourcePos+i];
		}
	}
	void InsertAdd(const Vector &v,unsigned pos)
	{
	#ifdef CHECK_MATRIX_BOUNDS
		if(pos+v.size>size)
			throw BadSize();
	#endif
		unsigned i;
		for(i=0;i<v.size;++i)
		{
			Values[pos+i]+=v[i];
		}
	}
	void InsertAdd(const Vector3 &v,unsigned pos);
/// Return the dot product of v1 and v2.
	friend float SIMUL_MATH_EXPORT_FN operator*(const Vector &v1,const Vector &v2);  
	friend float SIMUL_MATH_EXPORT_FN operator*(const Vector3 &v1,const Vector &v2);
	friend bool SIMUL_MATH_EXPORT_FN DotProductTestGE(const Vector &v1,const Vector &v2,float f); // is DP greater than f?
// Logical operators
	friend bool SIMUL_MATH_EXPORT_FN operator == (const Vector &v1,const Vector &v2);
	friend bool SIMUL_MATH_EXPORT_FN operator != (const Vector &v1,const Vector &v2);
// Calculation operators
	friend Vector SIMUL_MATH_EXPORT_FN operator+(const Vector &v1,const Vector &v2);		///< Return v1+v2.
	friend Vector SIMUL_MATH_EXPORT_FN operator-(const Vector &v1,const Vector &v2);		///< Return v1-v2.
	friend Vector SIMUL_MATH_EXPORT_FN operator*(float d,const Vector &v2);				///< Return d times v2.
	friend Vector SIMUL_MATH_EXPORT_FN operator*(const Vector &v1,float d);				///< Return d times v1.
	friend Vector SIMUL_MATH_EXPORT_FN operator/(const Vector &v1,float d);				///< Return v1 divided by d.
	friend Vector SIMUL_MATH_EXPORT_FN operator^(const Vector &v1,const Vector &v2);		///< Return the cross product of v1 and v2.
/// Put the cross product of v1 and v2 in result.
	friend void SIMUL_MATH_EXPORT_FN CrossProduct(Vector &result,const Vector &v1,const Vector &v2);
/// Add the cross product of v1 and v2 to result.
	friend void SIMUL_MATH_EXPORT_FN AddCrossProduct(Vector &result,const Vector &v1,const Vector &v2);
	friend void SIMUL_MATH_EXPORT_FN AddDoubleCross(Vector &result,const Vector &v1,const Vector &v2);
	friend void SIMUL_MATH_EXPORT_FN SubtractDoubleCross(Vector &result,const Vector &v1,const Vector &v2);
/// Return dot product of D and (X1-X2).
	friend float SIMUL_MATH_EXPORT_FN DotDifference(const Vector &D,const Vector &X1,const Vector &X2);
/// Return dot product of D and (X1+X2).
	friend float SIMUL_MATH_EXPORT_FN DotSum(const Vector &D,const Vector &X1,const Vector &X2);   
/// Put X1-X2 in R.
	friend void SIMUL_MATH_EXPORT_FN Subtract(Vector &R,const Vector &X1,const Vector &X2);
	friend void SIMUL_MATH_EXPORT_FN Subtract3(Vector &R,const Vector &X1,const Vector &X2);
/// Put X1+X2 in R.
	friend void SIMUL_MATH_EXPORT_FN Add(Vector &R,const Vector &X1,const Vector &X2);
	friend void SIMUL_MATH_EXPORT_FN AddRow(Vector3 &V1,Matrix &M2,unsigned Row2);
	friend void SIMUL_MATH_EXPORT_FN Matrix::AddOuter(const Vector &v1,const Vector &v2);
	friend void SIMUL_MATH_EXPORT_FN Matrix::SubtractOuter(const Vector &v1,const Vector &v2);
// outer product
	friend Matrix SIMUL_MATH_EXPORT_FN Outer(const Vector &v1,const Vector &v2);
	friend void SIMUL_MATH_EXPORT_FN Precross(Matrix &M,const Vector &v);
	friend void SIMUL_MATH_EXPORT_FN Matrix::InsertVectorAsColumn(const Vector &v,unsigned col); 
	friend void SIMUL_MATH_EXPORT_FN InsertVectorAsColumn(const Matrix &M,const Vector &v,const unsigned row);
	friend void SIMUL_MATH_EXPORT_FN Matrix::InsertNegativeVectorAsColumn(const Vector &v,unsigned col);
	friend void SIMUL_MATH_EXPORT_FN Matrix::InsertCrossProductAsColumn(const Vector &v1,const Vector &v2,unsigned col);
	friend void SIMUL_MATH_EXPORT_FN Matrix::InsertAddVectorCrossMatrix(const Vector &v,const Matrix &M);
	friend void SIMUL_MATH_EXPORT_FN InsertCrossProductAsRow(Matrix &M,const Vector &V1,const Vector &V2,const unsigned Row);
	friend float SIMUL_MATH_EXPORT_FN MatrixRowTimesVector(const Matrix &M,const unsigned Row,const Vector &V);      
	friend void SIMUL_MATH_EXPORT_FN InsertVectorAsRow(Matrix &M,const Vector &v,const unsigned row);
	friend void SIMUL_MATH_EXPORT_FN SubtractVectorFromRow(Matrix &M,unsigned Row,Vector &V);
	friend void SIMUL_MATH_EXPORT_FN InsertNegativeVectorAsRow(Matrix &M,const Vector &v,const unsigned row);
	friend void SIMUL_MATH_EXPORT_FN InsertVectorAsColumn(Matrix &M,const Vector &v,const unsigned col);
	friend void SIMUL_MATH_EXPORT_FN InsertNegativeVectorAsRow(Matrix &M,const Vector3 &v,const unsigned row);
	friend float SIMUL_MATH_EXPORT_FN LineInterceptPosition(Vector &i,const Vector &surf_x,const Vector &surf_n,const Vector &line_x,const Vector &line_d);
	friend bool SIMUL_MATH_EXPORT_FN FiniteLineIntercept(Vector &i,const Vector &surf_x,const Vector &surf_n,const Vector &line_x,const float l,const Vector &line_d);
	friend void SIMUL_MATH_EXPORT_FN AddFloatTimesRow(Vector &V1,float Multiplier,Matrix &M2,unsigned Row2);
	friend void SIMUL_MATH_EXPORT_FN AddFloatTimesColumn(Vector &V1,float Multiplier,Matrix &M2,unsigned Col2);
	friend void SIMUL_MATH_EXPORT_FN SolveFromCholeskyFactors(Vector &R,Matrix &Fac,Vector &X);
	friend void SIMUL_MATH_EXPORT_FN SolveFromLowerTriangular(Vector &R,Matrix &Fac,Vector &X);        
	friend void SIMUL_MATH_EXPORT_FN SSORP(Matrix &A,Vector &X,Vector &B,int Num,int Lim,Vector &InverseDiagonals);    
	friend void SIMUL_MATH_EXPORT_FN SSORP2(Matrix &A,Vector &X,Vector &B,int Num,int Lim,Vector &InverseDiagonals,void* Nv);

	friend void SIMUL_MATH_EXPORT_FN Multiply4(Vector &v2,const Matrix &m,const Vector &v1);

	friend SIMUL_MATH_EXPORT std::ostream &operator<<(std::ostream &, Vector const &);
	friend SIMUL_MATH_EXPORT std::istream &operator>>(std::istream &, Vector &);
#ifdef PLAYSTATION_2
}  __attribute__((aligned (16)));  
#else
};
#endif 
}
}
#endif
