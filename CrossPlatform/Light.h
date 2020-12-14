#pragma once

#ifndef _LIGHT_CACHE_H
#define _LIGHT_CACHE_H

#include <map>
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/Mesh.h"
#include "Platform/CrossPlatform/Material.h"
//#define UNIT_LIB_DEFAULT_TYPE float
#include "Platform/External/units/units.h"
using meter_t = units::unit_t<units::length::meter>;
using watts_per_sqm_per_nm = units::compound_unit<units::power::watts, units::inverse<units::squared<units::length::meter>>, units::inverse<units::length::nanometer>>;
typedef tvector3<watts_per_sqm_per_nm> vec3_watts_per_sqm_per_nm;
namespace simul
{
	namespace crossplatform
	{
		enum LightType
		{
			Sphere,			// There is no "point" light due to the degeneracy at the centre. Sphere is a better choice.
			Directional,	// Uniform lighting in a parallel direction.
			Spot			// Considered to be a sphere light with a limited cone in a given direction.
		};
		//! A light component.
		class SIMUL_CROSSPLATFORM_EXPORT Light
		{
		public:
			Light();
			virtual ~Light();
			LightType type;
			//! 
			vec3_watts_per_sqm_per_nm irradiance;
			//! For a sphere light, the radius at which its irradiance applies over the surface.
			meter_t radius;
		protected:
		};
	}
}
#endif