#ifndef OrientationH
#define OrientationH
#include <assert.h>
#include "Platform/Math/Matrix4x4.h"
#include "Platform/Math/Quaternion.h" 
#include "Platform/Math/VirtualVector.h"
#include "Platform/Math/MatrixVector3.inl"
#include "Platform/Math/Export.h"

namespace simul
{
	namespace math { class Vector3; }
	namespace geometry
	{
		/// A spatial orientation comprising position and rotation.

		/// The orientation holds position and rotation.
		/// Position is a vector which can be accessed using #GetPosition().
		/// Rotation is stored as a quaternion q.
		/// To transform use #LocalToGlobalPosition, #GlobalToLocalPosition,
		/// #LocalToGlobalDirection and #GlobalToLocalDirection.
		SIMUL_MATH_EXPORT_CLASS SimulOrientation
		{
		private:
			simul::math::Quaternion q;	
			mutable simul::math::Matrix4x4 T4;	
			mutable bool MatrixValid;			
			mutable bool InverseValid;			
		protected:
			mutable simul::math::Matrix4x4 Inv;	
			mutable float max_iso_scale;		
			mutable float max_inv_iso_scale;	
			mutable bool scaled;				

			void InverseMatrix() const;
			void ConvertQuaternionToMatrix() const;
			void ConvertMatrixToQuaternion();
		public:
			const simul::math::Matrix4x4 &GetMatrix() const
			{
				if(!MatrixValid)
					ConvertQuaternionToMatrix();
				return T4;
			}
			const simul::math::Quaternion &GetQuaternion() const
			{
				return q;
			}
			const simul::math::Matrix4x4 &GetInverseMatrix() const
			{
				if(!MatrixValid)
					ConvertQuaternionToMatrix();
				if(!InverseValid)
					InverseMatrix();
				return Inv;
			}
			SimulOrientation();
			SimulOrientation(const SimulOrientation &o);
			SimulOrientation(const simul::math::Quaternion &q);
			SimulOrientation(const simul::math::Matrix4x4 &M);
			SimulOrientation(const float *m);
			~SimulOrientation();
			void Clear();
		// operators
			void operator=(const SimulOrientation &o);
			void operator=(const float *m);
			void operator=(const simul::math::Matrix4x4 &M);
			bool operator==(const SimulOrientation &o)
			{
				math::Vector3 xe=GetPosition();
				math::Vector3 oxe=o.GetPosition();
				return (q==o.q&&xe==oxe);
			}  
			bool operator!=(const SimulOrientation &o)
			{
				math::Vector3 xe=GetPosition();
				math::Vector3 oxe=o.GetPosition();
				return (q!=o.q||xe!=oxe);
			}  
			void SetOrientation(const math::Quaternion &Q);
			void SetFromMatrix(const math::Matrix4x4 &m);
			const math::Vector3 &GetPosition() const;								///< Get a reference to the position.
			void SetPosition(const math::Vector3 &X);								///< Set the position in global co-ordinates to equal X.
			void Translate(const math::Vector3 &DX);								///< Move the position in global co-ordinates by DX.
			void LocalTranslate(const math::Vector3 &DX);								///< Move the position in global co-ordinates by DX.
			void DefineFromAircraftEuler(float a,float b,float c,int vertical_axis=2);
			void DefineFromCameraEuler(float a,float b,float c);
			void DefineFromClassicalEuler(float a,float b,float c);
			void DefineFromYZ(const math::Vector3 &y_dir,const math::Vector3 &z_dir);
		// Find angles from matrix
			void GetAircraft(math::Vector3 &angles);
			math::Vector3 GetClassicalFromMatrix(void);
		/// Transform a position from local to global co-ordinates.
		/// Note QuaternionToMatrix() is not called - must be certain that the matrix is valid
		/// before using functions like this.
			void LocalToGlobalPosition(math::Vector3 &xg,const math::Vector3 &xl) const;
		//! Transform a local direction to a global one.
			void LocalToGlobalDirection(math::Vector3 &dg,const math::Vector3 &dl) const;
			void GlobalToLocalOrientation(SimulOrientation &ol,const SimulOrientation &og) const;
		//! Transform a position from global to local co-ordinates.
			void GlobalToLocalPosition(math::Vector3 &xl,const math::Vector3 &xg) const;
		//! Transform a direction from global to local co-ordinates.
			void GlobalToLocalDirection(math::Vector3 &xl,const math::Vector3 &xg) const;
			void LocalUnitToGlobalUnit(math::Vector3 &dg,const math::Vector3 &dl) const;
			void GlobalUnitToLocalUnit(math::Vector3 &dl,const math::Vector3 &dg) const;
		// Extract unit vectors
			void x(math::Vector3 &X) const;										///< Put the oriented x-axis in vector X.
			void y(math::Vector3 &Y) const;										///< Put the oriented y-axis in vector Y.
			void z(math::Vector3 &Z) const;										///< Put the oriented z-axis in vector Z.
			const math::Vector3 &Tx(void) const;								///< Return the oriented x-axis.
			const math::Vector3 &Ty(void) const;								///< Return the oriented y-axis.
			const math::Vector3 &Tz(void) const;								///< Return the oriented z-axis.
		// Actions
			void Rotate(float angle,const math::Vector3 &axis);					///< Rotate the orientation by \param angle radians about the global axis, \param axis .
			void LocalRotate(float angle,const math::Vector3 &axis);			///< Rotate the orientation by \param angle radians about the local axis, \param axis .
			void LocalRotate(const math::Vector3 &dir_sin);						///< Rotate the orientation by the angle whose sine is the magnitude of dir_sin, on the axis of its direction.
			void Move(const math::Vector3 &offset);								///< Move the orientation by \param offset in global co-ordinates.
			void LocalMove(const math::Vector3 &offset);						///< Move the orientation by \param offset in local co-ordinates.
			void RotateLinear(float angle,const math::Vector3 &axis);			///< Rotate the orientation approximately angle radians about the global axis, axis.
			void Define(const simul::math::Quaternion &s);
			void Define(const simul::math::Matrix4x4 &M);
			void Define(const simul::math::Vector3 &X,const simul::math::Vector3 &Y,const simul::math::Vector3 &Z);
			void Define(const simul::math::Vector3 &X,const simul::math::Vector3 &Y,const simul::math::Vector3 &Z,const simul::math::Vector3 &pos);
			void Define(float s,float x,float y,float z);
			void QuaternionFromAircraft(float a,float b,float c,int vertical_axis=2);				///< Set rotation from the Aircraft Euler angles, yaw pitch and roll.
			void QuaternionFromClassical(float a,float b,float c);
			void RescaleStatic(float xscale,float yscale,float zscale);
			void CalcMaxIsotropicScales();
			float GetMaxIsotropicScale();
			float GetMaxInverseIsotropicScale();
			friend SIMUL_MATH_EXPORT void RelativeOrientationFrom1To2(SimulOrientation &Orientation1To2,simul::geometry::SimulOrientation *O1,simul::geometry::SimulOrientation *O2);
			friend SIMUL_MATH_EXPORT std::ostream &operator<<(std::ostream &, SimulOrientation const &);
			friend SIMUL_MATH_EXPORT std::istream &operator>>(std::istream &, SimulOrientation &);
		};
	}
}
#endif
