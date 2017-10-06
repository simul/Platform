#include "Fence.h"
#include "Simul/Platform/DirectX12/RenderPlatform.h"
#include "Simul/Base/Timer.h"

namespace simul
{
	namespace dx12
	{
		Fence::Fence():
			mFence(nullptr),
			mFenceValue(1),
			mFenceSignal(0),
			mFenceEvent(nullptr),
			mSignaled(false)
		{
		}

		Fence::~Fence()
		{
			Release();
		}

		void Fence::Restore(dx12::RenderPlatform * renderPlatform)
		{
			Release();
			HRESULT res = S_FALSE;
			auto rPlat = (dx12::RenderPlatform*)renderPlatform;

			mFenceSignal	= 0;
			mFenceValue		= 1;
			res				= rPlat->AsD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence));
			SIMUL_ASSERT(res == S_OK);
			mFenceEvent		= CreateEvent(nullptr, false, false, nullptr);
			SIMUL_ASSERT(mFenceEvent != nullptr);
		}

		void Fence::Signal(dx12::RenderPlatform* renderPlatform)
		{
			if (mSignaled)
			{
				SIMUL_CERR << "Trying to signal without a previous wait." << std::endl;
				return;
			}

			HRESULT res		= S_FALSE;
			mSignaled		= true;
			auto rPlat		= (dx12::RenderPlatform*)renderPlatform;
			// Save the signal and increase the fence value
			mFenceSignal	= mFenceValue++;
			res				= rPlat->GetCommandQueue()->Signal(mFence, mFenceSignal);
			SIMUL_ASSERT(res == S_OK);
		}

		void Fence::WaitUntilCompleted(dx12::RenderPlatform* renderPlatform)
		{
			if (!mSignaled)
			{
				SIMUL_CERR << "Trying to wait without a previous signal." << std::endl;
				return;
			}

			HRESULT res = S_FALSE;
			mSignaled	= false;
			if (mFence->GetCompletedValue() < mFenceSignal)
			{
				res = mFence->SetEventOnCompletion(mFenceSignal, mFenceEvent);
				SIMUL_ASSERT(res == S_OK);
				const DWORD waitTime = 70;// mili seconds, in a 30FPS game, this is 2 frames!
				
				//simul::base::Timer timer;
				auto waitRes = WaitForSingleObject(mFenceEvent, waitTime);
				//std::cout<< "[INFO] This fence waited for:" << timer.FinishTime() << ".ms\n";

				if (waitRes != WAIT_OBJECT_0)
				{
					SIMUL_CERR << "------ > This wait failed < ------ \n";
					//SIMUL_E("This wait failed!!!");
				}
			}
		}

		void Fence::WaitGPU(dx12::RenderPlatform * renderPlatform)
		{
			if (!mSignaled)
			{
				SIMUL_CERR << "Trying to wait without a previous signal." << std::endl;
				return;
			}

			HRESULT res = S_FALSE;
			mSignaled	= false;
			res			= renderPlatform->GetCommandQueue()->Wait(mFence, mFenceSignal);
			SIMUL_ASSERT(res == S_OK);
		}

		void Fence::Release()
		{
			if (mSignaled)
				SIMUL_COUT << "Releasing fence while it is signaled..." << std::endl;

			if(mFenceEvent)
				CloseHandle(mFenceEvent);
			mFenceEvent = nullptr;
			if (mFence)
				mFence->Release();
			mFence = nullptr;
		}
	}
}

