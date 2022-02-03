#include "Buffer.h"
using namespace simul;
using namespace crossplatform;

Buffer::Buffer():stride(0),count(0)
{
}

void Buffer::SetName(const char* n)
{
	name = n;
}

Buffer::~Buffer()
{
}
