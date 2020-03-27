#include "Platform/Math/ReferenceFrame.h"
#include "Platform/Math/Vector3.inl"

using namespace simul::math;
using namespace simul::geometry;

SimulReferenceFrame::SimulReferenceFrame() : SimulOrientation()
{
	Ve.Zero();
	Omega.Zero();
}
//------------------------------------------------------------------------------
void SimulReferenceFrame::Clear()
{
	SimulOrientation::Clear();
	Ve.Zero();
	Omega.Zero();
}
//------------------------------------------------------------------------------
void SimulReferenceFrame::operator=(const SimulOrientation &o)
{
	SimulOrientation::operator=(o);
}
//------------------------------------------------------------------------------
void SimulReferenceFrame::PositionToVelocity(Vector3 &ve,const Vector3 &xe) const
{
	ALIGN16 Vector3 temp;
	Subtract(temp,xe,GetPosition());
	ve=Ve;
	AddCrossProduct(ve,Omega,temp);
}
//------------------------------------------------------------------------------
void SimulReferenceFrame::LocalPositionToGlobalVelocity(Vector3 &ve,const Vector3 &xl) const
{
	static Vector3 temp;
	LocalToGlobalDirection(temp,xl);
	ve=Ve;
	AddCrossProduct(ve,Omega,temp);
}
//------------------------------------------------------------------------------
const Vector3 &SimulReferenceFrame::GetVelocity() const
{
	return Ve;
}
//------------------------------------------------------------------------------
const Vector3 &SimulReferenceFrame::GetAngularVelocity() const
{
	return Omega;
}
//------------------------------------------------------------------------------
Vector3 SimulReferenceFrame::GetLocalAngularVelocity() const
{
	Vector3 omega;
	GlobalToLocalDirection(omega,Omega);
	return omega;
}
