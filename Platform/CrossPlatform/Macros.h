#ifndef PLATFORM_CROSSPLATFORM_MACROS_H
#define PLATFORM_CROSSPLATFORM_MACROS_H

#ifndef SAFE_DELETE
	#define SAFE_DELETE(p)		{ if(p) { delete p; (p)=NULL; } }
#endif
#ifndef SAFE_DESTROY
#define SAFE_DESTROY(renderPlatform,ob)		{ if(renderPlatform) { renderPlatform->Destroy(ob); (ob)=NULL; } }
#endif

#endif