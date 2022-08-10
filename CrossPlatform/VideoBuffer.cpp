#include "VideoBuffer.h"

using namespace platform;
using namespace crossplatform;

VideoBuffer::VideoBuffer() 
	: mBufferType(VideoBufferType::UNKNOWN)
	, mHasData(false)
{
}


VideoBuffer::~VideoBuffer()
{
}

