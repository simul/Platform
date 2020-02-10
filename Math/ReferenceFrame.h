#ifndef ReferenceFrameH
#define ReferenceFrameH
                         
#include "Simul/Platform/Math/Vector3.h"
#include "Simul/Platform/Math/Orientation.h"

namespace simul
{
	namespace geometry
	{
		/// An extension of SimulOrientation which describes a moving frame of reference.

		/// The reference frame holds position, rotation, and linear and angular velocity.
		/// Velocity is a simul::math::Vector3 which can be accessed using #GetVelocity().
		/// Angular velocity is a simul::math::Vector3 which can be accessed using #GetAngularVelocity().
		SIMUL_MATH_EXPORT_CLASS SimulReferenceFrame : public SimulOrientation
		{
		public:
			SimulReferenceFrame();
			void Clear();
			META_BeginProperties
				META_Property(simul::math::Vector3,Ve		,"Velocity vector of this reference-frame.")
				META_Property(simul::math::Vector3,Omega	,"Angular velocity vector of the reference-frame.")
			META_EndProperties
		/// \name General
		///
			void operator=(const SimulOrientation &o);
			const math::Vector3 &GetVelocity() const;												///< Return the linear velocity.
			const math::Vector3 &GetAngularVelocity() const;										///< Return the angular velocity.   
			simul::math::Vector3 GetLocalAngularVelocity() const;									///< Return the angular velocity in local co-ordinates.   
			void PositionToVelocity(math::Vector3 &ve,const math::Vector3 &xe) const;				///< From a global position of a point in this frame, return the global velocity.
			void LocalPositionToGlobalVelocity(math::Vector3 &ve,const math::Vector3 &xl) const;	///< From a local position of a point in this frame, return the global velocity.
		///
		};
	}
}
#endif
