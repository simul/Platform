#include "Buffer.h"
using namespace platform;
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

 void Buffer::EnsureVertexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_vertices,const Layout *layout,const void *data,bool cpu_access,bool streamout_target)
{
	if(data)
	{
		upload_data=std::make_shared<std::vector<uint8_t>>(num_vertices*layout->GetStructSize());
		memcpy(upload_data->data(),data,upload_data->size());
	}
	EnsureVertexBuffer(renderPlatform,num_vertices,layout,upload_data,cpu_access,streamout_target);
}

 void Buffer::EnsureIndexBuffer(crossplatform::RenderPlatform *renderPlatform,int num_indices,int index_size_bytes,const void * data, bool cpu_access)
{
	if(data)
	{
		upload_data=std::make_shared<std::vector<uint8_t>>(num_indices*index_size_bytes);
		memcpy(upload_data->data(),data,upload_data->size());
	}
	EnsureIndexBuffer(renderPlatform,num_indices,index_size_bytes,upload_data,cpu_access);
}