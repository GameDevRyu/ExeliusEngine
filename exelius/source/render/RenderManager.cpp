#include "EXEPCH.h"
#include "source/render/RenderManager.h"
#include "source/os/interface/graphics/Window.h"

#include "source/os/interface/graphics/VertexArray.h"

// TEMP
#include "source/os/interface/graphics/Sprite.h"
#include "source/engine/resources/resourcetypes/TextureResource.h"

#include <EASTL/numeric_limits.h>
#include <EASTL/sort.h>


/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	bool RenderCommand::operator<(const RenderCommand& command) const
	{
		// Sort By Z
		if (m_zOrder != command.m_zOrder)
			return (m_zOrder < command.m_zOrder);

		float leftLowestY = m_destinationFrame.y + m_destinationFrame.h;
		float rightLowestY = command.m_destinationFrame.y + command.m_destinationFrame.h;

		// Sort By Bottom of Sprite y + h
		if (leftLowestY != rightLowestY)
			return (leftLowestY < rightLowestY);

		// sort by x
		return (m_destinationFrame.x < command.m_destinationFrame.x);
	}

	RenderManager::RenderManager()
		: m_renderManagerLog("RenderManager")
		#if !FORCE_SINGLE_THREADED_RENDERER
		, m_quitThread(false)
		, m_framesBehind(0)
		#endif // !FORCE_SINGLE_THREADED_RENDERER
		, m_pWindow(nullptr)
	{
		//
	}

	RenderManager::~RenderManager()
	{
		m_advancedBuffer.clear();

		#if !FORCE_SINGLE_THREADED_RENDERER
			m_intermediateBufferMutex.lock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
		m_intermediateBuffer.clear();
		#if !FORCE_SINGLE_THREADED_RENDERER
			m_intermediateBufferMutex.unlock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER

		#if !FORCE_SINGLE_THREADED_RENDERER
			// Tell the thread we are done.
			m_quitThread = true;
			SignalAndWaitForRenderThread();
			m_renderThread.join();
		#endif // !FORCE_SINGLE_THREADED_RENDERER

		#if !FORCE_SINGLE_THREADED_RENDERER
		m_pWindow->SetActive(true);
		#endif // !FORCE_SINGLE_THREADED_RENDERER

		delete m_pWindow;
		m_pWindow = nullptr;
	}

	bool RenderManager::Initialize(const eastl::string& title, const Vector2u& windowSize, bool isVsyncEnabled)
	{
		m_advancedBuffer.clear();

		#if !FORCE_SINGLE_THREADED_RENDERER
			m_intermediateBufferMutex.lock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
			m_intermediateBuffer.clear();
		#if !FORCE_SINGLE_THREADED_RENDERER
			m_intermediateBufferMutex.unlock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER

		EXE_ASSERT(!m_pWindow);
		m_pWindow = new Window(title, windowSize);
		EXE_ASSERT(m_pWindow);
		m_pWindow->SetVSync(isVsyncEnabled);

		#if !FORCE_SINGLE_THREADED_RENDERER
			m_pWindow->SetActive(false);
		#endif // !FORCE_SINGLE_THREADED_RENDERER

		#if !FORCE_SINGLE_THREADED_RENDERER
			m_renderThread = std::thread(&RenderManager::RenderThread, this);
		#endif // !FORCE_SINGLE_THREADED_RENDERER
		return true;
	}

	void RenderManager::PushRenderCommand(RenderCommand renderCommand)
	{
		m_advancedBuffer.emplace_back(renderCommand);
	}

	void RenderManager::Update()
	{
		EXE_ASSERT(m_pWindow);
		m_pWindow->Update();
	}

	void RenderManager::EndRenderFrame()
	{

		#if !FORCE_SINGLE_THREADED_RENDERER
			if (!m_advancedBuffer.empty())
			{
				SwapRenderCommandBuffer(m_advancedBuffer);
				++m_framesBehind;
				m_advancedBuffer.clear();
			}

			if (m_framesBehind > s_kMaxFramesBehind)
			{
				// wait for render thread.
				SignalAndWaitForRenderThread();
			}
			else
			{
				m_signalThread.notify_one();
			}

			EXE_ASSERT(m_framesBehind <= s_kMaxFramesBehind);
			EXE_ASSERT(m_renderThread.joinable());
		#else
			RenderThread();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
	}

	Window* RenderManager::GetWindow()
	{
		EXE_ASSERT(m_pWindow);
		return m_pWindow;
	}

	void RenderManager::AddView(const StringIntern& viewID, const View& view)
	{
		// TODO:
		// Check if this view already exists?
		// Maybe not necessary, as it should replace any existing view.
		#if !FORCE_SINGLE_THREADED_RENDERER
			m_viewListLock.lock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
		m_views.emplace_back(viewID, view);
		#if !FORCE_SINGLE_THREADED_RENDERER
			m_viewListLock.unlock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
	}

	void RenderManager::RenderThread()
	{
		#if !FORCE_SINGLE_THREADED_RENDERER
		m_renderManagerLog.Info("Instantiating Render Thread.");
		eastl::vector<RenderCommand> backBuffer;

		EXE_ASSERT(m_pWindow);
		if (!m_pWindow->SetActive(true))
		{
			m_renderManagerLog.Fatal("Failed to activate Window on Render Thread. TODO: Bail Here");
		}

		// While the thread is still active (Not shut down)
		while (!m_quitThread)
		{
			// Wait until we are signaled to work.
			std::unique_lock<std::mutex> waitLock(m_signalMutex);
			m_signalThread.wait(waitLock, [this]()
				{
					bool empty = true;

					m_intermediateBufferMutex.lock();
					empty = m_intermediateBuffer.empty();
					m_intermediateBufferMutex.unlock();

					return m_quitThread || !empty;
				});

			// Don't do any work if we're exiting.
			if (m_quitThread)
				break;

			/// Capture the deferred queue into the processing queue (swap buffers)
			SwapRenderCommandBuffer(backBuffer);
			m_framesBehind = 0;

			SortRenderCommands(backBuffer);

			// Render Clear
			m_pWindow->Clear();

			// Render Draw
			m_viewListLock.lock();
			if (m_views.empty())
				DrawToWindow(backBuffer);
			else
				DrawToViews(backBuffer);
			m_viewListLock.unlock();

			backBuffer.clear();

			// Render Display
			m_pWindow->Render();

			waitLock.unlock();
			m_signalThread.notify_one();
		}

		m_pWindow->SetActive(false);

		// Let the main thread know we are fully exiting in case they are waiting.
		m_renderManagerLog.Info("Signaled Main Thread: Render Thread Terminating.");
		m_signalThread.notify_one();
		#else
		SortRenderCommands(m_advancedBuffer);

		if (m_advancedBuffer.empty())
			return;

		// Render Clear
		m_pWindow->Clear();

		if (m_views.empty())
			DrawToWindow(m_advancedBuffer);
		else
			DrawToViews(m_advancedBuffer);

		m_advancedBuffer.clear();

		// Render Display
		m_pWindow->Render();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
	}

	void RenderManager::DrawToWindow(const eastl::vector<RenderCommand>& backBuffer)
	{
		IRectangle windowRect({ 0,0 }, static_cast<Vector2i>(m_pWindow->GetWindowSize()));

		// TODO:
		//	Does this really need to be dynamic?
		//	We could set this to the max possible size,
		//	meaning we would only allocate memory once.
		//	Further, we could get the highest number of
		//	"Like" textures and use that as max size.
		VertexArray vertices;

		ResourceID currentTexture = backBuffer.front().m_texture;

		// For each rendercommand...
		for (auto& command : backBuffer)
		{
			//if (!IsInViewBounds(command, windowRect))
				//continue;

			// If rendercommand can't be batched
			if (command.m_texture != currentTexture)
			{
				ResourceHandle texture(currentTexture);
				auto* pTextureResource = texture.GetAs<TextureResource>();

				if (pTextureResource)
				{
					// Render current vertex buffer
					m_pWindow->Draw(vertices, *pTextureResource->GetTexture());
				}
				else
				{
					// Render current vertex buffer
					m_pWindow->Draw(vertices);
				}

				// Clear Vertex Buffer
				vertices.Clear();

				// Set the new current texture.
				currentTexture = command.m_texture;

				if (!currentTexture.IsValid())
					continue;
			}

			// Batch it
			AddVertexToArray(vertices, command);
		}

		// If we still have stuff to draw, then draw it.
		if (vertices.GetVertexCount() > 0)
		{
			ResourceHandle texture(currentTexture);
			auto* pTextureResource = texture.GetAs<TextureResource>();

			if (pTextureResource)
			{
				// Render current vertex buffer
				m_pWindow->Draw(vertices, *pTextureResource->GetTexture());
			}
			else
			{
				// Render current vertex buffer
				m_pWindow->Draw(vertices);
			}

			// Clear Vertex Buffer
			vertices.Clear();
		}
	}

	void RenderManager::DrawToViews(const eastl::vector<RenderCommand>& backBuffer)
	{
		for (auto& view : m_views)
		{
			m_pWindow->SetView(view.second);

			// Build the view rectangle.
			IRectangle viewRect;
			viewRect.m_top = static_cast<int>(view.second.GetCenter().y - (view.second.GetSize().h / 2.0f));
			viewRect.m_left = static_cast<int>(view.second.GetCenter().x - (view.second.GetSize().w / 2.0f));
			viewRect.m_width = static_cast<int>(view.second.GetSize().w);
			viewRect.m_height = static_cast<int>(view.second.GetSize().h);

			// TODO:
			//	Does this really need to be dynamic?
			//	We could set this to the max possible size,
			//	meaning we would only allocate memory once.
			//	Further, we could get the highest number of
			//	"Like" textures and use that as max size.		
			VertexArray vertices;

			ResourceID currentTexture = backBuffer.front().m_texture;

			// For each rendercommand...
			for (auto& command : backBuffer)
			{
				if (!IsInViewBounds(command, viewRect))
					continue;

				// If rendercommand can't be batched
				if (command.m_texture != currentTexture)
				{
					ResourceHandle texture(currentTexture);
					auto* pTextureResource = texture.GetAs<TextureResource>();

					if (pTextureResource)
					{
						// Render current vertex buffer
						m_pWindow->Draw(vertices, *pTextureResource->GetTexture());
					}
					else
					{
						// Render current vertex buffer
						m_pWindow->Draw(vertices);
					}

					// Clear Vertex Buffer
					vertices.Clear();

					// Set the new current texture.
					currentTexture = command.m_texture;
				}

				// Batch it
				AddVertexToArray(vertices, command);
			}

			// If we still have stuff to draw, then draw it.
			if (vertices.GetVertexCount() > 0)
			{
				ResourceHandle texture(currentTexture);
				auto* pTextureResource = texture.GetAs<TextureResource>();

				if (!pTextureResource)
				{
					m_renderManagerLog.Warn("Attempting to render nullptr texture: {}", currentTexture.Get().c_str());
					return;
				}

				// Render current vertex buffer
				m_pWindow->Draw(vertices, *pTextureResource->GetTexture());

				// Clear Vertex Buffer
				vertices.Clear();
			}
		}
	}

	void RenderManager::SwapRenderCommandBuffer(eastl::vector<RenderCommand>& bufferToSwap)
	{
		#if !FORCE_SINGLE_THREADED_RENDERER
			m_intermediateBufferMutex.lock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
		bufferToSwap.swap(m_intermediateBuffer);
		#if !FORCE_SINGLE_THREADED_RENDERER
			m_intermediateBufferMutex.unlock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
	}

	void RenderManager::SortRenderCommands(eastl::vector<RenderCommand>& bufferToSort)
	{
		// TODO:
			// Maybe eastl::stable_sort is what I want here?
		// Sort based on the Z, Y, or X in that order.
		eastl::sort(bufferToSort.begin(), bufferToSort.end());

		// Without disturbing above sort, sort by texture.
		eastl::stable_sort(bufferToSort.begin(), bufferToSort.end(), [](const RenderCommand& left, const RenderCommand& right)
			{
				// Sort By Texture
				if (left.m_texture != right.m_texture)
					return (left.m_texture < right.m_texture);

				// Sort By Shader
				return (1 < 0); // TODO: This may be a bug if m_shader is not set?
			});
	}

	bool RenderManager::IsInViewBounds(const RenderCommand& command, const IRectangle& viewBounds) const
	{
		// TODO:
			//	Crappy Conversion.
		IRectangle rectToDraw((int)command.m_destinationFrame.x, (int)command.m_destinationFrame.y, (int)command.m_destinationFrame.w, (int)command.m_destinationFrame.h);
		if (!viewBounds.Intersects(rectToDraw))
			return false;

		return true;
	}

	void RenderManager::AddVertexToArray(VertexArray& vertexArray, const RenderCommand& command) const
	{
		/*Vector2f posTopLeft(command.m_position);

		Vector2f posTopRight;
		posTopRight.x = command.m_position.x + (command.m_scaleFactor.x * command.m_spriteFrame.m_width);
		posTopRight.y = command.m_position.y;

		Vector2f posBottomRight;
		posBottomRight.x = command.m_position.x + (command.m_scaleFactor.x * command.m_spriteFrame.m_width);
		posBottomRight.y = command.m_position.y + (command.m_scaleFactor.y * command.m_spriteFrame.m_height);

		Vector2f posBottomLeft;
		posBottomLeft.x = command.m_position.x;
		posBottomLeft.y = command.m_position.y + (command.m_scaleFactor.y * command.m_spriteFrame.m_height);

		Vector2f uvTopLeft(static_cast<Vector2f>(command.m_spriteFrame.GetPosition()));

		Vector2f uvTopRight;
		uvTopRight.x = static_cast<float>(command.m_spriteFrame.m_left + command.m_spriteFrame.m_width);
		uvTopRight.y = static_cast<float>(command.m_spriteFrame.m_top);

		Vector2f uvBottomRight;
		uvBottomRight.x = static_cast<float>(command.m_spriteFrame.m_left + command.m_spriteFrame.m_width);
		uvBottomRight.y = static_cast<float>(command.m_spriteFrame.m_top + command.m_spriteFrame.m_height);

		Vector2f uvBottomLeft;
		uvBottomLeft.x = static_cast<float>(command.m_spriteFrame.m_left);
		uvBottomLeft.y = static_cast<float>(command.m_spriteFrame.m_top + command.m_spriteFrame.m_height);*/

		Vector2f posTopLeft(command.m_destinationFrame.m_left, command.m_destinationFrame.m_top);
		Vector2f posTopRight(command.m_destinationFrame.m_left + command.m_destinationFrame.m_width, command.m_destinationFrame.m_top);
		Vector2f posBottomRight(command.m_destinationFrame.m_left + command.m_destinationFrame.m_width, command.m_destinationFrame.m_top + command.m_destinationFrame.m_height);
		Vector2f posBottomLeft(command.m_destinationFrame.m_left, command.m_destinationFrame.m_top + command.m_destinationFrame.m_height);

		Vector2f uvTopLeft(command.m_sourceFrame.m_left, command.m_sourceFrame.m_top);
		Vector2f uvTopRight(command.m_sourceFrame.m_left + command.m_sourceFrame.m_width, command.m_sourceFrame.m_top);
		Vector2f uvBottomRight(command.m_sourceFrame.m_left + command.m_sourceFrame.m_width, command.m_sourceFrame.m_top + command.m_sourceFrame.m_height);
		Vector2f uvBottomLeft(command.m_sourceFrame.m_left, command.m_sourceFrame.m_top + command.m_sourceFrame.m_height);

		// TOP LEFT
		vertexArray.Append(Vertex(posTopLeft, command.m_tint, uvTopLeft));

		// TOP RIGHT
		vertexArray.Append(Vertex(posTopRight, command.m_tint, uvTopRight));

		// BOTTOM RIGHT
		vertexArray.Append(Vertex(posBottomRight, command.m_tint, uvBottomRight));

		// BOTTOM LEFT
		vertexArray.Append(Vertex(posBottomLeft, command.m_tint, uvBottomLeft));
	}

	void RenderManager::SignalAndWaitForRenderThread()
	{
		#if !FORCE_SINGLE_THREADED_RENDERER
		EXE_ASSERT(m_renderThread.joinable());
		m_signalThread.notify_one();

		std::unique_lock<std::mutex> waitLock(m_signalMutex);
		m_signalThread.wait(waitLock, [this]()
			{
				return (m_framesBehind == 0);
			});

		waitLock.unlock();
		#endif // !FORCE_SINGLE_THREADED_RENDERER
	}
}