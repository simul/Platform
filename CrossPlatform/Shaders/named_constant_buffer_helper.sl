//Copyright (c) 2026 Simul Software Ltd. All rights reserved.

#ifndef SIMUL_NAMED_CONSTANT_BUFFER_HELPER_SL
#define SIMUL_NAMED_CONSTANT_BUFFER_HELPER_SL

#ifdef __INTELLISENSE__ 
#define PLATFORM_NAMED_CONSTANT_BUFFER(type, name, binding, group) struct type {
#define PLATFORM_NAMED_CONSTANT_BUFFER_END };
#define PLATFORM_NAMED_CONSTANT_BUFFER_HELPER(type, name, binding, group) ConstantBuffer<type> name : register(b##binding, space##group);

#else
#define PLATFORM_NAMED_CONSTANT_BUFFER_HELPER(type, name, binding, group)
#endif

#endif