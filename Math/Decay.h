#ifndef SIMUL_MATH_DECAY_H
#define SIMUL_MATH_DECAY_H

namespace simul
{
	namespace math
	{
		//! Apply a first-order decay to a float variable, based on the timestep dt.
		inline void FirstOrderDecay(float &variable,float target,float rate,float dt)
		{
			float mix=1.f-1.f/(1.f+dt*rate);
			variable*=1.f-mix;
			variable+=mix*target;
		}
		template<class T> void Decay(T &variable,const T &target,float rate,float dt)
		{
			float mix=1.f-1.f/(1.f+dt*rate);
			variable*=1.f-mix;
			variable+=mix*target;
		}
	}
}

#endif