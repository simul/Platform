#ifndef PLATFORM_CROSSPLATFORM_MACROS_H
#define PLATFORM_CROSSPLATFORM_MACROS_H

#ifndef SAFE_DELETE
	#define SAFE_DELETE(p)		{ if(p) { delete p; (p)=NULL; } }
#endif

#endif