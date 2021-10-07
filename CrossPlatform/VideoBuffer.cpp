#include "VideoBuffer.h"

using namespace simul;
using namespace crossplatform;

VideoBuffer::VideoBuffer() 
	: mBufferType(VideoBufferType::UNKNOWN)
	, mHasData(false)
{
}


VideoBuffer::~VideoBuffer()
{
}

