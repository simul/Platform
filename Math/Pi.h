#ifndef SIMUL_PI_H
#define SIMUL_PI_H
//! This may not be included before windows.h or gl.h as it defines as a constant
//! something that these headers use as a variable name!
#define SIMUL_PI_F (3.1415926536f)
#define SIMUL_PI_D (3.1415926535897932384626433832795)
#define SIMUL_PI_OVER_180 (3.1415926535897932384626433832795/180.0)
#define SIMUL_RAD_TO_DEG (180.0/3.1415926535897932384626433832795)
#define SIMUL_RAD_TO_DEGF (180.0f/3.1415926535897932384626433832795f)
#endif

