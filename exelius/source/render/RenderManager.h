#pragma once
#include "source/utility/generic/Singleton.h"
#include "source/render/RenderCommand.h"
#include "source/resource/ResourceHandle.h"
#include "source/os/platform/PlatformForwardDeclarations.h"

#include "source/os/interface/graphics/View.h"
#include "source/os/interface/graphics/Vertex.h"
#include "source/debug/Log.h"

#include <EASTL/vector.h>
#include <EASTL/unordered_map.h>

#define FORCE_SINGLE_THREADED_RENDERER 1

#if !FORCE_SINGLE_THREADED_RENDERER
	#include <thread>
	#include <condition_variable>
	#include <atomic>
	#include <mutex>
#endif // !FORCE_SINGLE_THREADED_RENDERER



/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	FORWARD_DECLARE(Window);
	FORWARD_DECLARE(VertexArray);

	class RenderManager
		: public Singleton<RenderManager>
	{
		static constexpr int s_kMaxFramesBehind = 2;
		Window* m_pWindow;
		
		/// <summary>
		/// Log for the RenderManager.
		/// </summary>
		Log m_renderManagerLog;

		eastl::vector<RenderCommand> m_advancedBuffer; // Main loop adds to this buffer.
		
		eastl::vector<RenderCommand> m_intermediateBuffer; // Main loop will swap this buffer with advancedbuffer at the end of a frame. Render Thread will swap with this buffer if it is not processing.
		eastl::vector<eastl::pair<StringIntern, View>> m_views;

		#if !FORCE_SINGLE_THREADED_RENDERER
			std::mutex m_intermediateBufferMutex;
			std::thread m_renderThread;
			std::atomic_bool m_quitThread;
			std::atomic_int m_framesBehind;
			std::mutex m_signalMutex;
			std::condition_variable m_signalThread;
			std::mutex m_viewListLock;
		#endif // !FORCE_SINGLE_THREADED_RENDERER

	public:
		RenderManager();
		RenderManager(const RenderManager&) = delete;
		RenderManager(RenderManager&&) = delete;
		RenderManager& operator=(const RenderManager&) = delete;
		RenderManager& operator=(RenderManager&&) = delete;
		~RenderManager();

		// Spins up the thread.
		bool Initialize(const eastl::string& title, const Vector2u& windowSize, bool isVsyncEnabled);

		// Adds a command to the advanceBuffer (1 frame ahead of renderthread)
		void PushRenderCommand(RenderCommand renderCommand);

		void Update();

		void EndRenderFrame();

		void AddView(const StringIntern& viewID, const View& view);

		Window* GetWindow();

	private:
		// This is the thread::do_work() function.
		void RenderThread();

		void DrawToWindow(const eastl::vector<RenderCommand>& backBuffer);

		void DrawToViews(const eastl::vector<RenderCommand>& backBuffer);

		// Swap the input buffer with the temp buffer.
		void SwapRenderCommandBuffer(eastl::vector<RenderCommand>& bufferToSwap);
		
		// Sort RenderCommands
		void SortRenderCommands(eastl::vector<RenderCommand>& bufferToSort);

		bool IsInViewBounds(const RenderCommand& command, const IRectangle& viewBounds) const;

		void AddVertexToArray(VertexArray& vertexArray, const RenderCommand& command) const;

		// Used when this Manager is destroyed in order to stop the thread.
		void SignalAndWaitForRenderThread();
	};
}