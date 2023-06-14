//  Copyright (c) 2015-17 Simul Software Ltd. All rights reserved.
#ifndef PLATFORM_CROSSPLATFORM_RANDOM_SL
#define PLATFORM_CROSSPLATFORM_RANDOM_SL

// This is the PCG hash by Jarzynski and Olano (https://jcgt.org/published/0009/03/02/) - see https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(uint seed)
{
	uint state = seed * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

float PcgRand(uint seed)
{
	return (pcg_hash(seed)) % uint(0xFFFFFFF) / float(0xFFFFFFF);
}

vec2 PcgRand2(uint seed)
{
	vec2 r;
	uint hash0 = pcg_hash(seed);
	uint hash1 = pcg_hash(seed^hash0);
	r.x=float(hash0 % uint(0xFFFFFFF));
	r.y=float(hash1 % uint(0xFFFFFFF));
	r /= float(0xFFFFFFF);
	return r;
}

vec3 PcgRand3(uint seed)
{
	vec3 r;
	uint hash0 = pcg_hash(seed);
	uint hash1 = pcg_hash(seed ^ hash0);
	uint hash2 = pcg_hash(seed ^ hash1);
	r.x = float(hash0% uint(0xFFFFFFF));
	r.y = float(hash1% uint(0xFFFFFFF));
	r.z = float(hash2% uint(0xFFFFFFF));
	r/=float(0xFFFFFFF);
	return r;
}

vec4 PcgRand4(uint seed)
{
	uint4 hash;
	hash.x	=pcg_hash(seed);
	hash.y	=pcg_hash(seed ^ hash.x);
	hash.z	=pcg_hash(seed ^ hash.y);
	hash.w	=pcg_hash(seed ^ hash.z);
	uint f	=uint(0xFFFFFFF);
	hash	=hash % f;
	vec4 r = vec4(hash);
	r /= float(0xFFFFFFF);
	return r;
}


#endif