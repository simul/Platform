#ifndef SIMUL_MATH_MMATRIX_H
#define SIMUL_MATH_MMATRIX_H

#include "Align.h"
#include <float.h>
#include "Platform/Math/Export.h"

#if defined(_DEBUG) && defined(_MSC_VER)
#define CHECK_MATRIX_BOUNDS
#else
#undef CHECK_MATRIX_BOUNDS
#endif
namespace simul
{
	namespace math
	{
		class Vector;
		class Vector3;
		class Quaternion;

///  \brief A Matrix class.
///
///  The Matrix class can be used for generic matrices of any size.
		class SIMUL_MATH_EXPORT Matrix
		{
		public:
			class BadSize
			{
			};
			class Singular
			{
			};
			class BadNumber
			{
			};
		protected:
			float *Values;
			unsigned BufferSize;
			unsigned W16;
		public:
			unsigned Height;					///< The number of rows.
			unsigned Width;						///< Number of columns.
		#ifdef CANNOT_ALIGN    
			float *V;
		#endif
			Matrix();                   ///< Default constructor.
			Matrix(const Matrix &m);
			Matrix(unsigned h,unsigned w);            ///< Constructor for a matrix of height h and width w.
			Matrix(unsigned h,unsigned w,float val);	///< Constructor for a matrix of height h and width w, filled with value val.
			void ZeroEdges(void);
			///  \brief Return the pointer to values in the matrix.

			///  @param [in]       row unsigned     The row.
			///  @param [in]       col unsigned     The column
			///  @return float * Pointer to the values at (row,col)
			float *FloatPointer(unsigned row,unsigned col)
			{
		#ifdef CHECK_MATRIX_BOUNDS
				if(row>=Height||col>=Width||row<0||col<0)
					throw BadSize();
		#endif
				return &(Values[row*W16+col]);
			}    
			float *RowPointer(unsigned row)
			{
			#ifdef CHECK_MATRIX_BOUNDS
				if(row>=Height||row<0)
					throw BadSize();
			#endif
				return &(Values[row*W16]);
			}
			const float *RowPointer(unsigned row) const
			{
			#ifdef CHECK_MATRIX_BOUNDS
				if(row>=Height||row<0)
					throw BadSize();
			#endif
				return &(Values[row*W16]);
			}
			unsigned GetAlignedWidth(void){return W16;}
			~Matrix(void);					///< Default destructor.
			float *Position(const unsigned row,const unsigned col) const;
			void Clear(void);				///< Reset the matrix to height and width zero.
			void ClearMemory(void);         ///< Reset the matrix and free its buffer memory.
			/// Resize the matrix to height h, width w.
			void Resize(unsigned h,unsigned w)
			{
				if(h!=Height||w!=Width)
					ChangeSize(h,w);
			}
			void ResizeOnly(unsigned h,unsigned w)
			{
				if(h!=Height||w!=Width)
					ChangeSizeOnly(h,w);    
			}                
			void ResizeRetainingValues(unsigned h,unsigned w)
			{
				if(h!=Height||w!=Width)
					ChangeSizeRetainingValues(h,w);    
			}
			void ChangeSize(unsigned h,unsigned w);
			void ChangeSizeOnly(unsigned h,unsigned w);    
			void ChangeSizeRetainingValues(unsigned h,unsigned w);
			void ResizeRetainingStep(unsigned h,unsigned w);
			/// Set all values to zero.
			void Zero(void);
			void Zero(unsigned size);
			void Unit(unsigned row);
			void Unit(void);				///< Make a unit matrix.
			void MakeSymmetricFromUpper(void);
			void MakeSymmetricFromLower(void);
			bool CheckNAN(void);
			void Transpose(void);   
			void Transpose(Matrix &M) const;
			void InvertDiagonal(void);
			float DiagonalProduct(void);
			bool CheckEdges(void);          
			void SwapRows(unsigned R1,unsigned R2);	///< Swap rows R1 and R2.
			float &operator()(unsigned row,unsigned col);
			float operator()(unsigned row,unsigned col) const;
			// Calculation operators
			friend Matrix SIMUL_MATH_EXPORT_FN operator+(const Matrix &m1,const Matrix &m2);
			Matrix operator-(void);
			Matrix operator-(const Matrix& m2);
			Matrix operator/(const float d);
			Vector operator*(const Vector &v);
			void operator+=(const Matrix &M);
			Matrix& operator*=(float d);  
			Matrix& operator/=(float d);
			void operator-=(const Matrix& m);
			Matrix& operator=(const Matrix &m);
			Matrix& operator=(const float *m);
			Matrix(const float*);
			void operator|=(const Matrix &m);
			operator const float*() const
			{
				return Values;
			}
			void OuterProduct(const Vector &v);
			void InsertMatrix(const Matrix &m2,const unsigned row,const unsigned col);
			void InsertMatrix(const class Matrix4x4 &m2,const unsigned row,const unsigned col);
			void InsertNegativeMatrix(const Matrix &m2,const unsigned row,const unsigned col);
			void InsertTranspose(const Matrix &m2,const unsigned row,const unsigned col);
			void InsertTranspose(const class Matrix4x4 &m2,const unsigned row,const unsigned col);
			void InsertMultiply(const Matrix &m1,const Matrix &m2,const unsigned row,const unsigned col);
			void InsertSubtractMatrix(const Matrix &m2,const unsigned row,const unsigned col);
			void InsertAddMatrix(const Matrix &m2,const unsigned row,const unsigned col);   
			void InsertAddTranspose(const Matrix &m2,const unsigned row,const unsigned col);
			void InsertSubMatrix(const Matrix &m2,unsigned InsRow,unsigned InsCol,const unsigned row,const unsigned col,const unsigned h,const unsigned w);
			void InsertTransposeSubMatrix(const Matrix &m2,unsigned InsRow,unsigned InsCol,const unsigned row,const unsigned col,const unsigned h,const unsigned w);
			void InsertPartialMatrix(const Matrix &m,unsigned InsRow,const unsigned InsCol,const unsigned h,const unsigned w);
			void InsertNegativePartialMatrix(const Matrix &m,unsigned InsRow,const unsigned InsCol,const unsigned h,const unsigned w);
			void InsertSubtractPartialMatrix(const Matrix &m,unsigned InsRow,const unsigned InsCol,const unsigned h,const unsigned w);
			void InsertVectorTimesMatrix(const Vector &v,const Matrix &M,unsigned Row,unsigned Col);
			void InsertVectorTimesSubMatrix(const Vector &V,const Matrix &M,const unsigned Row,const unsigned Col,const unsigned Width);
			void InsertAddVectorTimesMatrix(const Vector &v,const Matrix &M,unsigned Row,unsigned Col);   
			void InsertAddVectorTimesSubMatrix(const Vector &v,const Matrix &M,unsigned Row,unsigned Col,unsigned Width);
			void InsertSubtractVectorTimesMatrix(const Vector &v,const Matrix &M,unsigned Row,unsigned Col);
			void InsertVectorCrossMatrix(const Vector &v,const Matrix &M);
			void InsertAddVectorCrossMatrix(const Vector &v,const Matrix &M);
			void InsertSubtractVectorCrossMatrix(const Vector &v,const Matrix &M);
			void InsertVectorAsColumn(const Vector &v,const unsigned col);   
			void InsertNegativeVectorAsColumn(const Vector &v,const unsigned col);
			void InsertCrossProductAsColumn(const Vector &v1,const Vector &v2,const unsigned col);
			void AddScalarTimesMatrix(float val,const Matrix &m2);
			void SubtractScalarTimesMatrix(float val,const Matrix &m2);
			void AddOuter(const Vector &v1,const Vector &v2);
			void SubtractOuter(const Vector &v1,const Vector &v2);
			void RowMatrix(Vector& v);
			void ColumnMatrix(Vector& v);
			Matrix SubMatrix(unsigned row,const unsigned col,unsigned h,unsigned w);
			unsigned Pivot(unsigned row,unsigned sz);     
			void Inverse(Matrix &Result);
			Vector Solve(const Vector& v);
			float Det(void);
			float DetNoPivot(void);
			float SubDet(unsigned h);
			float SubDetNoPivot(unsigned h,Matrix &Mul2);   
			float SubDetCholesky(unsigned size,Matrix &Mul2);
			float CPlusPlusSubDetNoPivot(unsigned size,Matrix &Mul2);
			float SubDetIncrement(float detVal,unsigned size,Matrix &Mul); 
			float SubDetIncrementCholesky(float &detVal,unsigned size,Matrix &Mul);
			Matrix HandleIndeterminacy(void);  
			float CheckSum(void);               
			bool SymmetricInverseNoPivot(Matrix &Result);
			bool SymmetricInverseNoPivot(Matrix &Result,Matrix &Fac);
			bool CholeskyFactorization(Matrix &Fac);
			bool CholeskyFactorizationFromRow(Matrix &Fac,unsigned Idx);
			void InverseFromCholeskyFactors(Matrix &Result,Matrix &Fac);
			float DetFromCholeskyFactors(unsigned size,Matrix &Fac);    
			friend void SIMUL_MATH_EXPORT_FN CopyRowToColumn(Matrix &M,unsigned R);
			friend float SIMUL_MATH_EXPORT_FN MultiplyRows(Matrix &M,unsigned R1,unsigned R2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyByTranspose(Matrix &M,const Matrix &M1,const Matrix &M2,unsigned Column);
			friend void SIMUL_MATH_EXPORT_FN CrossProduct(Matrix &result,const Vector &v1,const Matrix &M2);
			friend void SIMUL_MATH_EXPORT_FN Multiply(Matrix &M,const Matrix &A,const Matrix &B);
			friend void SIMUL_MATH_EXPORT_FN MultiplyAndAdd(Vector &V2,const Matrix &M,const Vector &V1);
			friend void SIMUL_MATH_EXPORT_FN MultiplyAndAdd(Vector3 &V2,const Matrix &M,const Vector3 &V1);
			friend void SIMUL_MATH_EXPORT_FN MultiplyNegative(Vector &V2,const Matrix &M,const Vector &V1);
			friend void SIMUL_MATH_EXPORT_FN MultiplyAndSubtract(Vector3 &V2,const Matrix &M,const Vector3 &V1);
			friend void SIMUL_MATH_EXPORT_FN Multiply(Matrix &m,const Matrix &m1,const Matrix &m2,unsigned rows,unsigned elements,unsigned columns);
			friend void SIMUL_MATH_EXPORT_FN Multiply2(Matrix &m,const Matrix &m1,const Matrix &m2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyTransposeByMatrix(Matrix &m,const Matrix &m1,const Matrix &m2);
			friend void SIMUL_MATH_EXPORT_FN SubtractTransposeTimesMatrix(Matrix &m,const Matrix &m1,const Matrix &m2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyMatrixByTranspose(Matrix &m,const Matrix &m1,const Matrix &m2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyDiagonalByTranspose(Matrix &M,const Matrix &M1,const Matrix &M2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyDiagonalByTranspose(Matrix &M,const Matrix &M1,const Matrix &M2,unsigned Column);
			friend void SIMUL_MATH_EXPORT_FN Multiply(Vector &v2,const Matrix &m,const Vector &v1);       
			friend void SIMUL_MATH_EXPORT_FN TransposeMultiply(Vector &v2,const Matrix &m,const Vector &v1);
			friend void SIMUL_MATH_EXPORT_FN TransposeMultiply(Vector3 &v2,const Matrix &m,const Vector3 &v1);
			friend void SIMUL_MATH_EXPORT_FN TransposeMultiplyAndAdd(Vector &v2,const Matrix &m,const Vector &v1);
			friend void SIMUL_MATH_EXPORT_FN TransposeMultiplyAndSubtract(Vector &v2,const Matrix &m,const Vector &v1);
			friend void SIMUL_MATH_EXPORT_FN MultiplyAndSubtract(Matrix &m,const Matrix &m1,const Matrix &m2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyNegative(Vector &result,const Vector &v,const Matrix &m);
			friend void SIMUL_MATH_EXPORT_FN MultiplyAndAdd(Vector &v2,const Matrix &m1,const Matrix &m2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyAndSubtract(Vector &v2,const Matrix &m1,const Matrix &m2);
			friend void SIMUL_MATH_EXPORT_FN Multiply(Matrix &result,const Vector &v,const Matrix &m);
			friend void SIMUL_MATH_EXPORT_FN TransposeMultiplyByTranspose(Matrix &M,const Matrix &A,const Matrix &B,unsigned Column);
			friend void SIMUL_MATH_EXPORT_FN MultiplyByScalar(Matrix &result,const float f,const Matrix &m);
			friend Vector SIMUL_MATH_EXPORT_FN GetAircraftFromMatrix(Matrix &T);
			friend void SIMUL_MATH_EXPORT_FN MultiplyNegative(Matrix &m,const Matrix &m1,const Matrix &m2);
			friend void SIMUL_MATH_EXPORT_FN swap(Matrix &M1,Matrix &M2);      
			friend Matrix SIMUL_MATH_EXPORT_FN operator*(const Matrix &m1,const Matrix &m2);
			friend Matrix SIMUL_MATH_EXPORT_FN operator*(float d,const Matrix &m);
			friend Matrix SIMUL_MATH_EXPORT_FN operator*(const Matrix &m,float d);
			friend Matrix SIMUL_MATH_EXPORT_FN SafePrecross(const Vector &v);
			friend void SIMUL_MATH_EXPORT_FN Precross(Matrix &M,const Vector3 &v);
			friend void SIMUL_MATH_EXPORT_FN MultiplyAndAdd(Matrix &M,const Matrix &M1,const Matrix &M2);
			friend void SIMUL_MATH_EXPORT_FN CrossProductWithTranspose(Matrix &result,const Vector &V1,const Matrix &M2);
			friend void SIMUL_MATH_EXPORT_FN MultiplyVectorByTranspose(Vector &result,const Vector &V,const Matrix &M);
			friend void SIMUL_MATH_EXPORT_FN MultiplyTransposeAndSubtract(Vector &V2,const Vector &V1,const Matrix &M);
			friend void SIMUL_MATH_EXPORT_FN AddColumnMultiple(Matrix &M1,unsigned Col1,float Multiplier,Matrix &M2,unsigned Col2);
			friend void SIMUL_MATH_EXPORT_FN AddRowMultiple(Matrix &M1,unsigned Row1,float Multiplier,Matrix &M2,unsigned Row2);
			friend void SIMUL_MATH_EXPORT_FN CopyRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Row2);
			friend void SIMUL_MATH_EXPORT_FN CopyLTRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Row2);
			friend void SIMUL_MATH_EXPORT_FN CopyLTRowColumn(Matrix &M,unsigned Row1,unsigned Row2);
			friend void SIMUL_MATH_EXPORT_FN CopyColumn(Matrix &M1,unsigned Col1,Matrix &M2,unsigned Col2);
			friend void SIMUL_MATH_EXPORT_FN CopyRowToColumn(Matrix &M1,unsigned Col1,Matrix &M2,unsigned Col2);    
			friend void SIMUL_MATH_EXPORT_FN CopyRowToColumn(Matrix &M1,unsigned Col1,unsigned StartRow,Matrix &M2,unsigned Row2);
			friend void SIMUL_MATH_EXPORT_FN CopyColumnToRow(Matrix &M1,unsigned Col1,Matrix &M2,unsigned Col2);
			friend void SIMUL_MATH_EXPORT_FN ZeroRow(Matrix &M1,unsigned Row1);
			friend void SIMUL_MATH_EXPORT_FN AddDoublePrecross(Matrix &M,float f,const Vector3 &v);
			friend void SIMUL_MATH_EXPORT_FN SubtractDoublePrecross(Matrix &M,float f,const Vector3 &v);
			friend void SIMUL_MATH_EXPORT_FN InsertVectorAsRow(Matrix &M,const Vector &v,const unsigned row);
			friend void SIMUL_MATH_EXPORT_FN InsertVectorAsRow(Matrix &M,const Vector3 &v,const unsigned row);
			friend void SIMUL_MATH_EXPORT_FN InsertVectorAsColumn(Matrix &M,const Vector &v,const unsigned col);
			friend void SIMUL_MATH_EXPORT_FN InsertVectorAsColumn(Matrix &M,const Vector3 &v,const unsigned row);
			friend void SIMUL_MATH_EXPORT_FN InsertNegativeVectorAsRow(Matrix &M,const Vector &v,const unsigned row);
			friend void SIMUL_MATH_EXPORT_FN InsertNegativeVectorAsRow(Matrix &M,const Vector3 &v,const unsigned row);
			friend void SIMUL_MATH_EXPORT_FN InsertNegativeVectorAsColumn(Matrix &M,const Vector3 &v,const unsigned row);
			friend void SIMUL_MATH_EXPORT_FN AddRow(Matrix &M1,unsigned Row1,Matrix &M2,unsigned Row2);      
			friend void SIMUL_MATH_EXPORT_FN AddVectorToRow(Matrix &M,unsigned Row,Vector3 &V);      
			friend void SIMUL_MATH_EXPORT_FN SubtractVectorFromRow(Matrix &M,unsigned Row,Vector &V);
			friend void SIMUL_MATH_EXPORT_FN RowToVector(const Matrix &M,Vector &v,unsigned Row);
			friend void SIMUL_MATH_EXPORT_FN RowToVector(const Matrix &M,Vector3 &V,unsigned Row);
			friend void SIMUL_MATH_EXPORT_FN ColumnToVector(Matrix &M,Vector &v,unsigned Col);
			friend void SIMUL_MATH_EXPORT_FN InsertCrossProductAsRow(Matrix &M,const Vector &V1,const Vector &V2,const unsigned Row);
			friend float SIMUL_MATH_EXPORT_FN MatrixRowTimesVector(const Matrix &M,const unsigned Row,const Vector &V);
			friend float SIMUL_MATH_EXPORT_FN MatrixRowTimesVector(const Matrix &M,const unsigned Row,const Vector3 &V);
			friend void SIMUL_MATH_EXPORT_FN CopyRow(Matrix &M1,unsigned Row1,unsigned Col1,Matrix &M2,unsigned Row2);
			friend void SIMUL_MATH_EXPORT_FN InsertRow(Matrix &M,unsigned Row);
			friend void SIMUL_MATH_EXPORT_FN InsertColumn(Matrix &M,unsigned Col);
			friend void SIMUL_MATH_EXPORT_FN AddFloatTimesRow(Vector &V1,float Multiplier,Matrix &M2,unsigned Row2);
			friend void SIMUL_MATH_EXPORT_FN AddRow(Vector3 &V1,Matrix &M2,unsigned Row2);            
			friend void SIMUL_MATH_EXPORT_FN Multiply(Matrix &M,const Matrix &A,const Matrix &B,unsigned Column);
			friend void SIMUL_MATH_EXPORT_FN Multiply(Matrix &M,const Vector &V1,const Matrix &M2,unsigned Column);
			friend float SIMUL_MATH_EXPORT_FN MultiplyRows(Matrix &M1,unsigned R1,Matrix &M2,unsigned R2);
			friend void SIMUL_MATH_EXPORT_FN AddFloatTimesColumn(Vector &V1,float Multiplier,Matrix &M2,unsigned Col2);    
			friend void SIMUL_MATH_EXPORT_FN SolveFromCholeskyFactors(Vector &R,Matrix &Fac,Vector &X); 
			friend void SIMUL_MATH_EXPORT_FN SolveFromLowerTriangular(Vector &R,Matrix &Fac,Vector &X);
			friend void SIMUL_MATH_EXPORT_FN RemoveRow(Matrix &M,unsigned Row);
			friend void SIMUL_MATH_EXPORT_FN RemoveColumn(Matrix &M,unsigned Col);
			friend void SIMUL_MATH_EXPORT_FN SSORP(Matrix &A,Vector &X,Vector &B,int Num,int Lim,Vector &InverseDiagonals);
			friend void SIMUL_MATH_EXPORT_FN SSORP2(Matrix &A,Vector &X,Vector &B,int Num,int Lim,Vector &InverseDiagonals,void* Nv);

			friend void SIMUL_MATH_EXPORT_FN TransposeMultiply4(Vector3 &V2,const Matrix &M,const Vector3 &V1);

			friend void SIMUL_MATH_EXPORT_FN Multiply(Vector3 &v2,const Matrix &m,const Vector3 &v1);     
		#ifdef PLAYSTATION_2
		}  __attribute__((aligned (16)));  
		#else
		};
		#endif
		extern Matrix SIMUL_MATH_EXPORT DetTemp;
	}
}
#undef SIMD
#endif
