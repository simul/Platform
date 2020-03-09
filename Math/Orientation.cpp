#include "Platform/Math/MatrixVector3.h"
#include "Platform/Math/Orientation.h"
#include "Platform/Math/Vector3.h"
#include "Platform/Math/Quaternion.h"
#include <math.h>
#include <iostream>
#include <algorithm>
#undef NDEBUG
#include <assert.h>

using namespace simul::math;
using namespace simul::geometry;
namespace simul
{
	namespace math
	{
		extern SIMUL_MATH_EXPORT void QuaternionToMatrix(simul::math::Matrix4x4 &m,const simul::math::Quaternion &q); 
		extern SIMUL_MATH_EXPORT void MatrixToQuaternion(simul::math::Quaternion &q,const simul::math::Matrix4x4 &M);
	}
}

namespace simul
{
	namespace geometry
	{
		//------------------------------------------------------------------------------
		void RelativeOrientationFrom1To2(SimulOrientation &Orientation1To2,SimulOrientation *O1,SimulOrientation *O2)
		{
			Orientation1To2.scaled=false;
			if(O1)
			{
				O1->ConvertQuaternionToMatrix();
				Orientation1To2.scaled|=O1->scaled;
			}
			if(O2)
			{
				O2->ConvertQuaternionToMatrix();
				Orientation1To2.scaled|=O2->scaled;
			}
			Matrix4x4 i;
			i.ResetToUnitMatrix();
			if(O1&&O2)
				Multiply4x4	(Orientation1To2.T4		,O1->T4		,O2->Inv);
			else if(O1)
				Multiply4x4	(Orientation1To2.T4		,O1->T4		,i);
			else if(O2)
				Multiply4x4	(Orientation1To2.T4		,i			,O2->Inv);
			else
				Orientation1To2.T4.ResetToUnitMatrix();
			if(O1&&O2)
				Multiply4x4	(Orientation1To2.Inv	,O2->T4		,O1->Inv);
			else if(O2)
				Multiply4x4	(Orientation1To2.Inv	,O2->T4		,i);
			else if(O1)
				Multiply4x4	(Orientation1To2.Inv	,i			,O1->Inv);
			else
				Orientation1To2.Inv.ResetToUnitMatrix();
			Orientation1To2.MatrixValid=true;
		}

//------------------------------------------------------------------------------
#define pi 3.1415926536f
int OrientationUnitTest();
class OrientationError{};
SimulOrientation::SimulOrientation()
{                       
	q.Define(1,0,0,0);
	T4.Zero();
	T4(0,3)=0;
	T4(1,3)=0;
	T4(2,3)=0;
	T4(3,3)=0;
	MatrixValid=InverseValid=false;
	scaled=false;
	max_iso_scale=1.f;
	max_inv_iso_scale=1.f;
	T4.ResetToUnitMatrix();
}
//------------------------------------------------------------------------------
SimulOrientation::SimulOrientation(const SimulOrientation &o)
{
	T4.Zero();
	T4(0,3)=0;
	T4(1,3)=0;
	T4(2,3)=0;
	T4(3,3)=0;  
	Vector3 *Xe=reinterpret_cast<Vector3*>(T4.RowPointer(3));
	const Vector3 *Xe2=reinterpret_cast<const Vector3*>(o.T4.RowPointer(3));
	*Xe=*Xe2;
	q=o.q;          
	MatrixValid=o.MatrixValid;
	T4=o.T4;
	Inv=o.Inv;
	scaled=o.scaled;
	max_iso_scale=o.max_iso_scale;
	max_inv_iso_scale=o.max_inv_iso_scale;
	Inv=o.Inv;
	ConvertQuaternionToMatrix();
	q.MakeUnit();
}

SimulOrientation::SimulOrientation(const Quaternion &Q)
{
	T4.Zero();
	T4(0,3)=0;
	T4(1,3)=0;
	T4(2,3)=0;
	T4(3,3)=0;  
	q=Q;          
	MatrixValid=InverseValid=false;
	Inv.Zero();
	scaled=false;
	max_iso_scale=1.f;
	max_inv_iso_scale=1.f;
	ConvertQuaternionToMatrix();
	q.MakeUnit();
}

SimulOrientation::SimulOrientation(const simul::math::Matrix4x4 &M)
{
	MatrixValid=InverseValid=false;
	Inv.Zero();
	scaled=false;
	max_iso_scale=1.f;
	max_inv_iso_scale=1.f;
	T4=M;
	ConvertMatrixToQuaternion();
}

SimulOrientation::SimulOrientation(const float *m)
{
	MatrixValid=InverseValid=false;
	Inv.Zero();
	scaled=false;
	max_iso_scale=1.f;
	max_inv_iso_scale=1.f;
	T4=m;
	ConvertMatrixToQuaternion();
}

SimulOrientation::~SimulOrientation(void)
{
}

void SimulOrientation::Clear(void)
{
	T4.ResetToUnitMatrix();
	MatrixValid=true;
	q.Reset();
	scaled=false;
	max_iso_scale=1.f;
	max_inv_iso_scale=1.f;
}
//------------------------------------------------------------------------------
// operators
//------------------------------------------------------------------------------
void SimulOrientation::operator=(const SimulOrientation &o)
{
	q=o.q;
	Vector3 *Xe=reinterpret_cast<Vector3*>(T4.RowPointer(3));
	const Vector3 *Xe2=reinterpret_cast<const Vector3*>(o.T4.RowPointer(3));
	*Xe=*Xe2;
	MatrixValid=o.MatrixValid;
	
	scaled=o.scaled;
	max_iso_scale=o.max_iso_scale;
	max_inv_iso_scale=o.max_inv_iso_scale;
	
	T4=o.T4;
	Inv=o.Inv;
}

void SimulOrientation::operator=(const float *m)
{
	T4=m;
	simul::math::MatrixToQuaternion(q,T4);
}

void SimulOrientation::operator=(const simul::math::Matrix4x4 &M)
{
	T4=M;
	simul::math::MatrixToQuaternion(q,M);
}
//------------------------------------------------------------------------------
void SimulOrientation::SetPosition(const Vector3 &v)
{
	T4(3,0)=v(0);
	T4(3,1)=v(1);
	T4(3,2)=v(2);
	T4(3,3)=1;
	if(MatrixValid)
		InverseMatrix();
	else
		ConvertQuaternionToMatrix();
}

void SimulOrientation::Translate(const math::Vector3 &DX)
{
	Vector3 X=GetPosition();
	X+=DX;
	SetPosition(X);
}

void SimulOrientation::LocalTranslate(const math::Vector3 &DX)
{
	Vector3 X=GetPosition();
	Vector3 dx;
	LocalToGlobalDirection(dx,DX);
	X+=dx;
	SetPosition(X);
}
//------------------------------------------------------------------------------
void SimulOrientation::SetOrientation(const Quaternion &Q)
{
	q=Q;
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}
//------------------------------------------------------------------------------
void SimulOrientation::SetFromMatrix(const math::Matrix4x4 &m)
{
	T4=m;
	ConvertMatrixToQuaternion();
	MatrixValid=true;
	InverseValid=false;
}

const math::Vector3 &SimulOrientation::GetPosition() const								/// Get a reference to the position.
{
	const math::Vector3 *Xe=reinterpret_cast<const math::Vector3*>(T4.RowPointer(3));
	return *Xe;
}

// Find angles from matrix
//------------------------------------------------------------------------------
void SimulOrientation::GetAircraft(Vector3 &angles)
{
	ConvertQuaternionToMatrix();
	float val1,val2;
	val1=-T4(1,0);
	val2=T4(1,1);
	if(Fabs(val1)>1e-6f||Fabs(val2)>1e-6f)
	{
		angles(0)=InverseTangent(val1,val2);
		val1=-T4(0,2);
		val2=T4(2,2);
		if(Fabs(val1)>1e-6f||Fabs(val2)>1e-6f)
			angles(2)=InverseTangent(val1,val2);
		else
			angles(2)=0;    
		val1=T4(1,2);
		float cos0,sin0;
		cos0=cos(angles(0));
		sin0=sin(angles(0));
		if(Fabs(cos0)>1e-1f)
			val2=T4(1,1)/cos0;
		else
			val2=-T4(1,0)/sin0;
		if(Fabs(val1)>1e-6f||Fabs(val2)>1e-6f)
			angles(1)=InverseTangent(val1,val2);
		else
		{
			angles(1)=0;
		}   
		if(angles(1)>pi/2)
		{
			angles(1)=pi-angles(1);
			angles(0)-=pi;
		}
		if(angles(1)<-pi/2)
		{
			angles(1)=-pi-angles(1);
			angles(0)-=pi;
		}
		while(angles(0)>pi)
			angles(0)-=2*pi;
		while(angles(0)<-pi)
			angles(0)+=2*pi;
	}
	else
	{
		angles(0)=0;
		val1=T4(2,0);
		val2=T4(0,0);
		if(T4(1,2)>0)
			angles(1)=pi/2.f;
		else
			angles(1)=-pi/2.f;
		angles(2)=InverseTangent(val1,val2);
	}
}
//---------------------------------------------------------------------------
Vector3 SimulOrientation::GetClassicalFromMatrix(void)
{               
	Vector3 angles(0,0,0);
	float val1,val2;
	val1=T4(2,0);
	val2=-T4(2,1);
	if(val1!=0||val2!=0)
		angles(0)=InverseTangent(val1,val2);
	val1=T4(0,2);
	val2=-T4(1,2);
	if(val1!=0||val2!=0)
		angles(2)=InverseTangent(val1,val2);
	val1=T4(1,2);
	val2=T4(2,2)*cos(angles(2));
	if(val1!=0||val2!=0)
		angles(1)=InverseTangent(val1,val2);
	return angles;
}
//---------------------------------------------------------------------------
void SimulOrientation::DefineFromAircraftEuler(float heading,float pitch,float roll,int vertical_axis)
{
	QuaternionFromAircraft(heading,pitch,roll,vertical_axis); 
	q.MakeUnit();         
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}
//---------------------------------------------------------------------------
void SimulOrientation::DefineFromCameraEuler(float a,float b,float c)
{
	ALIGN16 Vector3 temp;
	ALIGN16 Vector3 temp3a;
	QuaternionFromAircraft(a,b,c); 
	temp.Define(1,0,0);
	Multiply(temp3a,q,temp);
	Quaternion q3(pi/2.f,temp3a);
	q=q3*q;
	q.MakeUnit();         
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}

void SimulOrientation::DefineFromClassicalEuler(float a,float b,float c)
{
	QuaternionFromClassical(a,b,c);
	q.MakeUnit();               
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}

void SimulOrientation::DefineFromYZ(const math::Vector3 &y_dir,const math::Vector3 &z_dir)
{
	Vector3 x_dir;
	Vector3 y_ortho;
	Vector3 z_ortho;
	CrossProduct(x_dir,y_dir,z_dir);
	CrossProduct(y_ortho,z_dir,x_dir);
	CrossProduct(z_ortho,x_dir,y_ortho);
	T4.InsertRow(0,x_dir);
	T4.InsertRow(1,y_ortho);
	T4.InsertRow(2,z_ortho);
	simul::math::MatrixToQuaternion(q,T4);
	MatrixValid=true;
}
//---------------------------------------------------------------------------
// Extract unit vectors
//------------------------------------------------------------------------------
void SimulOrientation::x(Vector3 &Xx) const
{
	if(MatrixValid)
	{
		RowToVector((const Matrix4x4&)T4,Xx,0);
		return;
	}
	const float &X=q.x;
	const float &Y=q.y;
	const float &Z=q.z;
	const float &S=q.s;
	Xx.Define(X*X-Y*Y-Z*Z+S*S,2*(X*Y+Z*S),2*(X*Z-Y*S));
}
//------------------------------------------------------------------------------
void SimulOrientation::y(Vector3 &Yy) const
{
	if(MatrixValid)
	{
		RowToVector(T4,Yy,1);
		return;
	}
	const float &X=q.x;
	const float &Y=q.y;
	const float &Z=q.z;
	const float &S=q.s;
	Yy.Define(2*(X*Y-Z*S),-X*X+Y*Y-Z*Z+S*S,2*(Y*Z+X*S));
}
//------------------------------------------------------------------------------
void SimulOrientation::z(Vector3 &Zz) const
{
	if(MatrixValid)
	{
		RowToVector(T4,Zz,2);
		return;
	}
	const float &X=q.x;
	const float &Y=q.y;
	const float &Z=q.z;
	const float &S=q.s;
	Zz.Define(2*(X*Z+Y*S),2*(Y*Z-X*S),-X*X-Y*Y+Z*Z+S*S);
}
//------------------------------------------------------------------------------
const Vector3 &SimulOrientation::Tx(void) const
{								
	static Vector3 x;		
	if(MatrixValid)
	{
		RowToVector((const Matrix4x4&)T4,x,0);
		return x;
	}			
	const float &X=q.x;
	const float &Y=q.y;
	const float &Z=q.z;
	const float &S=q.s;
	x.Define(X*X-Y*Y-Z*Z+S*S,2*(X*Y+Z*S),2*(X*Z-Y*S));
	return x;
}
//------------------------------------------------------------------------------
const Vector3 &SimulOrientation::Ty(void) const
{
	static Vector3 y;	
	if(MatrixValid)
	{
		RowToVector((const Matrix4x4&)T4,y,1);
		return y;
	}			
	const float &X=q.x;
	const float &Y=q.y;
	const float &Z=q.z;
	const float &S=q.s;                                   
	y.Define(2*(X*Y-Z*S),-X*X+Y*Y-Z*Z+S*S,2*(Y*Z+X*S));
	return y;
}
//------------------------------------------------------------------------------
const Vector3 &SimulOrientation::Tz(void) const
{
	static Vector3 z;
	if(MatrixValid)
	{
		RowToVector((const Matrix4x4&)T4,z,2);
		return z;
	}			
	const float &X=q.x;
	const float &Y=q.y;
	const float &Z=q.z;
	const float &S=q.s;
	z.Define(2*(X*Z+Y*S),2*(Y*Z-X*S),-X*X-Y*Y+Z*Z+S*S);
	return z;
}           

void SimulOrientation::GlobalToLocalOrientation(SimulOrientation &ol,const SimulOrientation &og) const
{
	// to transform a local vector in og to global:
/*	xg=(og.T4)t xl;
	// to transform a global vector to local in this:
	xl=(this.Inv)t xg;

	// to transform a local vector in ol/og to local in this:
	xl=(ol.T4)t xl;
	// and this is
	xl=(this.Inv)t ( (og.T4)t xl )
		= ( (this.Inv)t (og.T4)t ) xl
		= (og.T4 this.Inv)t xl*/
	Multiply4x4(ol.T4,og.T4,Inv);
	ol.ConvertMatrixToQuaternion();
}

void SimulOrientation::GlobalToLocalPosition(Vector3 &xl,const Vector3 &xg) const
{
	assert(MatrixValid);
	if(scaled)
	{
		TransposeMultiply4(xl,Inv,xg);
		return;
	}
	Vector3 temp3;
	Subtract(temp3,xg,GetPosition());
	Multiply3(xl,T4,temp3);
}
//------------------------------------------------------------------------------
void SimulOrientation::LocalUnitToGlobalUnit(Vector3 &dg,const Vector3 &dl) const
{
#ifdef DEBUG5
	if(dl.SquareSize()
#endif
	if(scaled)
	{
		assert(MatrixValid);
		Multiply3(dg,Inv,dl);
		dg.Unit();
	}
	else
		LocalToGlobalDirection(dg,dl);
}
//------------------------------------------------------------------------------
void SimulOrientation::GlobalUnitToLocalUnit(Vector3 &dl,const Vector3 &dg) const
{
#ifdef DEBUG5
	if(dl.SquareSize()
#endif
	if(scaled)
	{
		assert(MatrixValid);
		Multiply3(dl,T4,dg);
		dl.Unit();
	}
	else
		GlobalToLocalDirection(dl,dg);
}
//------------------------------------------------------------------------------
void SimulOrientation::GlobalToLocalDirection(Vector3 &dl,const Vector3 &dg) const
{
	assert(MatrixValid);
	if(scaled)
	{
		TransposeMultiply3(dl,Inv,dg);
		return;
	}
	Multiply3(dl,T4,dg);
}
//------------------------------------------------------------------------------
// Actions
// REQUIRE axis to be a unit vector.
//------------------------------------------------------------------------------
void SimulOrientation::Rotate(float angle,const Vector3 &axis)
{
	ALIGN16 Quaternion dq;
#ifdef DEBUG
	if(axis.Magnitude()>1.01f||axis.Magnitude()<.99f)
		throw OrientationError();
#endif
	dq.Define(angle,axis);
	q/=dq;
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}
#ifdef __GNUC__
	#define _finite(a) !isnan(a)
#endif
#define FIX_NAN(a) if(!_finite(a)) a=0.0f;
//------------------------------------------------------------------------------
// Rotate in local ref frame
// REQUIRE axis to be a unit vector.
//------------------------------------------------------------------------------
void SimulOrientation::LocalRotate(float angle,const Vector3 &axis)
{
	ALIGN16 Quaternion dq;
#ifdef DEBUG
	if(axis.Magnitude()>1.01f||axis.Magnitude()<.99f)
		throw OrientationError();
#endif
	Vector3 local_axis;
	LocalUnitToGlobalUnit(local_axis,axis);
	dq.Define(angle,local_axis);
	if(angle!=0.0f)
	q/=dq;
	
	FIX_NAN(q.x);
	FIX_NAN(q.y);
	FIX_NAN(q.z);
	FIX_NAN(q.s);
	q.MakeUnit();
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}

void SimulOrientation::LocalRotate(const math::Vector3 &dir_sin)
{
	ALIGN16 Quaternion dq;
	Vector3 local_axis;
	LocalToGlobalDirection(local_axis,dir_sin);
	dq.Define(local_axis);
	q/=dq;
	q.MakeUnit();
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}

void SimulOrientation::Move(const math::Vector3 &offset)
{
	math::Vector3 pos=GetPosition();
	pos+=offset;
	SetPosition(pos);
}

void SimulOrientation::LocalMove(const math::Vector3 &local_offset)
{
	math::Vector3 pos=GetPosition();
	math::Vector3 offset;
	LocalToGlobalDirection(offset,local_offset);
	pos+=offset;
	SetPosition(pos);
}

//------------------------------------------------------------------------------
void SimulOrientation::RotateLinear(float angle,const Vector3 &axis)
{
	ALIGN16 Quaternion dq;
	dq.DefineSmall(angle,axis);
	q/=dq;
	MatrixValid=InverseValid=false;
}
//------------------------------------------------------------------------------
void SimulOrientation::Define(const Quaternion &s)
{                                          
	q=s;
	q.MakeUnit();
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}          
//------------------------------------------------------------------------------
void SimulOrientation::Define(const Matrix4x4 &M)
{
	T4=M;
	InverseMatrix();
	MatrixValid=true;
	math::MatrixToQuaternion(q,M);
}          
//------------------------------------------------------------------------------
void SimulOrientation::Define(float s,float x,float y,float z)
{
	q.s=s;                                  
	q.x=x;
	q.y=y;
	q.z=z;
	q.MakeUnit();
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
}
void SimulOrientation::Define(const simul::math::Vector3 &X,const simul::math::Vector3 &Y,const simul::math::Vector3 &Z)
{
#ifndef NDEBUG
	float test_x=1.f-X.SquareSize();
	float test_y=1.f-Y.SquareSize();
	float test_z=1.f-Z.SquareSize();
	assert(test_x*test_x<.1f);
	assert(test_y*test_y<.1f);
	assert(test_z*test_z<.1f);
	float text_xy=X*Y;
	float text_yz=Y*Z;
	float text_zx=Z*X;
	assert(text_xy*text_xy<.1f);
	assert(text_yz*text_yz<.1f);
	assert(text_zx*text_zx<.1f);
#endif
	T4.Zero();
	T4.InsertRow(0,X);
	T4.InsertRow(1,Y);
	T4.InsertRow(2,Z);
	MatrixValid=true;
	math::MatrixToQuaternion(q,T4);
}

void SimulOrientation::Define(const simul::math::Vector3 &X,const simul::math::Vector3 &Y,const simul::math::Vector3 &Z,const simul::math::Vector3 &pos)
{
	Define(X,Y,Z);
	SetPosition(pos);
}
			

void SimulOrientation::ConvertQuaternionToMatrix() const
{
	if(MatrixValid)
		return;
	scaled=false;
	max_iso_scale=1.f;
	max_inv_iso_scale=1.f;
	simul::math::QuaternionToMatrix(T4,q);
	T4(3,3)=1.f;
	InverseMatrix();
	MatrixValid=true;
}

void SimulOrientation::ConvertMatrixToQuaternion()
{
	MatrixValid=true;
	simul::math::MatrixToQuaternion(q,T4);
}

void SimulOrientation::QuaternionFromAircraft(float heading,float pitch,float roll,int vertical_axis)
{
//	Aircraft order is psi0,psi1,psi2.
	Quaternion q0,q1,q2;
	q.Define(1.f,0,0,0);
// rotation about earth vertical
	ALIGN16 Vector3 temp(0,0,0);
	temp(vertical_axis)=1.f;
	q0.Define(heading,temp);
// rotation about intermediate x axis
	temp.Define(1.f,0,0);
	ALIGN16 Vector3 temp3a;
	Multiply(temp3a,q0,temp);
	q1.Define(pitch,temp3a);
// rotation about final y axis
	if(vertical_axis==2)
		temp.Define(0,1.f,0);
	else if(vertical_axis==1)
		temp.Define(0,0,-1.f);
	Multiply(temp3a,q0,temp);
	ALIGN16 Vector3 TempVector3;
	Multiply(TempVector3,q1,temp3a);
	q2.Define(roll,TempVector3);
// Combine all 3 rotations
	q=q2*q1*q0;
}
void SimulOrientation::QuaternionFromClassical(float a,float b,float c)
{
//	Aircraft order is psi0,psi1,psi2.
	Vector3 v0,v1,v2;
	float s0,s1,s2;
	Quaternion q0,q1,q2;
	q=Quaternion(1,0,0,0,true);
// rotation about earth vertical
	v0.Define(0,0,1);
	v0*=sin(a/2);
	s0=cos(a/2);
	q0=Quaternion(s0,v0);
// rotation about intermediate y axis
	Vector3 y1=q0*Vector3(1,0,0);
	v1=y1*sin(b/2);
	s1=cos(b/2);
	q1=Quaternion(s1,v1);
// rotation about final z axis
	Vector3 z1=q0*Vector3(0,0,1);
	Vector3 z2=q1*z1;
	v2=z2*sin(c/2);
	s2=cos(c/2);
	q2=Quaternion(s2,v2);
// Combine all 3 rotations
	q=q2*q1*q0;
}
void SimulOrientation::RescaleStatic(float xscale,float yscale,float zscale)
{
	MatrixValid=InverseValid=false;
	ConvertQuaternionToMatrix();
	T4(0,0)*=xscale;
	T4(0,1)*=xscale;
	T4(0,2)*=xscale;

	T4(1,0)*=yscale;
	T4(1,1)*=yscale;
	T4(1,2)*=yscale;

	T4(2,0)*=zscale;
	T4(2,1)*=zscale;
	T4(2,2)*=zscale;
	scaled=true;
	InverseMatrix();
	CalcMaxIsotropicScales();
}
void SimulOrientation::CalcMaxIsotropicScales()
{
	Vector3 *XX=reinterpret_cast<Vector3*>(T4.RowPointer(0));
	Vector3 *YY=reinterpret_cast<Vector3*>(T4.RowPointer(1));
	Vector3 *ZZ=reinterpret_cast<Vector3*>(T4.RowPointer(2));
	float xx=(*XX)*(*XX);
	float yy=(*YY)*(*YY);
	float zz=(*ZZ)*(*ZZ);
	max_iso_scale=sqrt(std::max(xx,std::max(yy,zz)));
	XX=reinterpret_cast<Vector3*>(Inv.RowPointer(0));
	YY=reinterpret_cast<Vector3*>(Inv.RowPointer(1));
	ZZ=reinterpret_cast<Vector3*>(Inv.RowPointer(2));
	xx=(*XX)*(*XX);
	yy=(*YY)*(*YY);
	zz=(*ZZ)*(*ZZ);
	max_inv_iso_scale=sqrt(std::max(xx,std::max(yy,zz)));
}
float SimulOrientation::GetMaxIsotropicScale()
{
	return max_iso_scale;
}
float SimulOrientation::GetMaxInverseIsotropicScale()
{
	return max_inv_iso_scale;
}

void SimulOrientation::InverseMatrix() const
{
	const Vector3 *XX=reinterpret_cast<const Vector3*>(T4.RowPointer(0));
	const Vector3 *YY=reinterpret_cast<const Vector3*>(T4.RowPointer(1));
	const Vector3 *ZZ=reinterpret_cast<const Vector3*>(T4.RowPointer(2));
	T4.Transpose(Inv);
	const Vector3 &xe=GetPosition();
	if(!scaled)
	{
		Inv(0,3)=0;
		Inv(1,3)=0;
		Inv(2,3)=0;
		Inv(3,0)=-((xe)*(*XX));
		Inv(3,1)=-((xe)*(*YY));
		Inv(3,2)=-((xe)*(*ZZ));
		Inv(3,3)=1.f;
		return;
	}
	float xscale2=(*XX)*(*XX);
	float yscale2=(*YY)*(*YY);
	float zscale2=(*ZZ)*(*ZZ);
	Inv(0,0)/=xscale2;
	Inv(1,0)/=xscale2;
	Inv(2,0)/=xscale2;
	Inv(0,3)=0;
		 
	Inv(0,1)/=yscale2;
	Inv(1,1)/=yscale2;
	Inv(2,1)/=yscale2;
	Inv(1,3)=0;
		 
	Inv(0,2)/=zscale2;
	Inv(1,2)/=zscale2;
	Inv(2,2)/=zscale2;
	Inv(2,3)=0;

	Inv(3,0)=-((xe)*(*XX))/xscale2;
	Inv(3,1)=-((xe)*(*YY))/yscale2;
	Inv(3,2)=-((xe)*(*ZZ))/zscale2;
	Inv(3,3)=1.f;
}

void SimulOrientation::LocalToGlobalPosition(Vector3 &xg,const Vector3 &xl) const
{
	assert(MatrixValid);
	TransposeMultiply4(xg,T4,xl);
}
void SimulOrientation::LocalToGlobalDirection(Vector3 &dg,const Vector3 &dl) const
{                     
	assert(MatrixValid);
	TransposeMultiply3(dg,T4,dl);
}

int OrientationUnitTest()
{
	int ret=0;
	Vector3 X1(56,4,10);
	Vector3 XG,X2,x1,x2,xg,n1,n2,ng,N1,N2,NG,d1,d2,D1,D2;
	SimulOrientation O1,O2,Orientation1To2;

	O1.LocalToGlobalPosition(XG,X1);
	O1.GlobalToLocalPosition(x1,XG);
	if((x1-X1).SquareSize()>1e-4f)
		ret++;
	Vector3 ax1=Vector3(2,1,2);
	ax1.Unit();
	O1.Rotate(.33f,ax1);

	O1.LocalToGlobalPosition(XG,X1);
	O1.GlobalToLocalPosition(x1,XG);
	if((x1-X1).SquareSize()>1e-4f)
		ret++;

	Vector3 ax2(1,0,1);
	ax2.Unit();
	O2.Rotate(.11f,ax2);
	RelativeOrientationFrom1To2(Orientation1To2,&O1,&O2);

	O1.LocalToGlobalPosition(XG,X1);
	O2.GlobalToLocalPosition(X2,XG);
	Orientation1To2.LocalToGlobalPosition(x2,X1);
	if((x2-X2).SquareSize()>1e-4f)
		ret++;
	Orientation1To2.GlobalToLocalPosition(x1,x2);
	if((x1-X1).SquareSize()>1e-4f)
		ret++;

	O1.RescaleStatic(.4f,.5f,1.2f);
	O1.LocalToGlobalPosition(XG,X1);
	O1.GlobalToLocalPosition(x1,XG);
	if((x1-X1).SquareSize()>1e-4f)
		ret++;

	O2.RescaleStatic(2.4f,.15f,6.2f);
	O2.LocalToGlobalPosition(XG,X2);
	O2.GlobalToLocalPosition(x2,XG);
	if((x2-X2).SquareSize()>1e-4f)
		ret++;
	RelativeOrientationFrom1To2(Orientation1To2,&O1,&O2);
	O1.LocalToGlobalPosition(XG,X1);
	O2.GlobalToLocalPosition(X2,XG);
	Orientation1To2.LocalToGlobalPosition(x2,X1);
	if((x2-X2).SquareSize()>1e-4f)
		ret++;
	Orientation1To2.GlobalToLocalPosition(x1,x2);
	if((x1-X1).SquareSize()>1e-4f)
		ret++;

	d1.Define(3,5,1);
	d2.Define(-1,3,9);
	CrossProduct(N1,d1,d2);
	N1.Unit();
	O1.LocalUnitToGlobalUnit(ng,N1);
	O1.LocalToGlobalDirection(D1,d1);
	O1.LocalToGlobalDirection(D2,d2);
	CrossProduct(NG,D1,D2);
	NG.Unit();
	if((ng-NG).SquareSize()>1e-4f)
		ret++;
	O1.GlobalUnitToLocalUnit(n1,ng);
	n1.Unit();
	if((n1-N1).SquareSize()>1e-4f)
		ret++;

	Orientation1To2.LocalUnitToGlobalUnit(n2,N1);
	n2.Unit();
	Orientation1To2.LocalToGlobalDirection(D1,d1);
	Orientation1To2.LocalToGlobalDirection(D2,d2);
	CrossProduct(N2,D1,D2);
	N2.Unit();
	if((n2-N2).SquareSize()>1e-4f)
		ret++;
	return ret;
}
std::ostream &operator<<(std::ostream &out, SimulOrientation const &O)
{
	out.write((const char*)(O.T4.GetValues()),sizeof(O.T4));
	char mv=O.MatrixValid;
	out.write((const char*)(&mv),sizeof(mv));
	out.write((const char*)(&O.q),sizeof(O.q));
	//	private:				
	out.write((const char*)(O.Inv.GetValues()),sizeof(O.Inv));
	out.write((const char*)(&O.max_iso_scale),sizeof(O.max_iso_scale));
	out.write((const char*)(&O.max_inv_iso_scale),sizeof(O.max_inv_iso_scale));
	mv=O.scaled;
	out.write((const char*)(&mv),sizeof(mv));
	return out;
}
std::istream &operator>>(std::istream &in, SimulOrientation &O)
{
	char mv=0;
	in.read((char*)(O.T4.GetValues()),sizeof(O.T4));
	in.read((char*)(&mv),sizeof(mv));
	O.MatrixValid=(mv!=0);
	in.read((char*)(&O.q),sizeof(O.q));
	//	private:				
	in.read((char*)(O.Inv.GetValues()),sizeof(O.Inv));
	in.read((char*)(&O.max_iso_scale),sizeof(O.max_iso_scale));
	in.read((char*)(&O.max_inv_iso_scale),sizeof(O.max_inv_iso_scale));
	in.read((char*)(&mv),sizeof(mv));
	O.scaled=(mv!=0);
	return in;
}
}
}
