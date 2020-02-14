#ifndef SIMUL_META_ORIENTATION_INTERFACE_H
#define SIMUL_META_ORIENTATION_INTERFACE_H
namespace simul
{
	namespace geometry
	{
		class OrientationOutputInterface
		{
		public:
			virtual const float *GetOrientationAsPermanentMatrix() const=0;		//! Permanent: this means that for as long as the interface exists, the address is valid.
			virtual const float *GetRotationAsQuaternion() const=0;
			virtual const float *GetPosition() const=0;
		};
		class OrientationInterface:public OrientationOutputInterface
		{
		public:
			virtual void SetOrientationAsMatrix(const float *)=0;
			virtual void SetOrientationAsQuaternion(const float *)=0;
			virtual void SetPosition(const float *)=0;
			virtual void SetPositionAsXYZ(float,float,float)=0;
			virtual void Move(const float *)=0;
			virtual void LocalMove(const float *)=0;
			virtual void Rotate(float,const float *)=0;
		};
	}
}
#endif