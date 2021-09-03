#include "EXEPCH.h"
#include "Source/Resource/ResourceManager.h"
#include "Source/Resource/ResourceFactory.h"
#include "Source/Utility/String/StringTransformation.h"

#include <fstream>
#include <zlib/zlib.h>

/// <summary>
/// Engine namespace. Everything owned by the engine will be inside this namespace.
/// </summary>
namespace Exelius
{
	/// ------------------------------------------------------------------------------------------
	/// ZLIB Structs - Provided by Rez in GAP 301
	/// ------------------------------------------------------------------------------------------
	#pragma region ZLIB_STRUCTS
		#pragma pack(1)
		struct ZipLocalHeader
	{
		static constexpr unsigned int kSignature = 0x04034b50;

		uint32_t sig;
		uint16_t version;
		uint16_t flag;
		uint16_t compression;	// Z_NO_COMPRESSION or Z_DEFLATED
		uint16_t modTime;
		uint16_t modDate;
		uint32_t crc32;
		uint32_t cSize;
		uint32_t ucSize;
		uint16_t fnameLen;	// Filename string follows header.
		uint16_t xtraLen;	// Extra field follows filename.
	};

		struct ZipDirHeader
	{
		static constexpr unsigned int kSignature = 0x06054b50;

		uint32_t sig;
		uint16_t nDisk;
		uint16_t nStartDisk;
		uint16_t totalDirEntries;
		uint32_t dirSize;
		uint32_t dirOffset;
		uint16_t cmntLen;
	};

		struct ZipDirFileHeader
	{
		static constexpr unsigned int kSignature = 0x02014b50;

		uint32_t sig;
		uint16_t verMade;
		uint16_t verNeeded;
		uint16_t flag;
		uint16_t compression;	// COMP_xxxx
		uint16_t modTime;
		uint16_t modDate;
		uint32_t crc32;
		uint32_t cSize;			// Compressed size
		uint32_t ucSize;		// Uncompressed size
		uint16_t fnameLen;		// Filename string follows header.
		uint16_t xtraLen;		// Extra field follows filename.
		uint16_t cmntLen;		// Comment field follows extra field.
		uint16_t diskStart;
		uint16_t intAttr;
		uint32_t extAttr;
		uint32_t hdrOffset;

		char* GetName() const { return (char*)(this + 1); }
		char* GetExtra() const { return GetName() + fnameLen; }
		char* GetComment() const { return GetExtra() + xtraLen; }
	};
		#pragma pack()
		static constexpr ptrdiff_t kPtrDelta = -(static_cast<ptrdiff_t>(sizeof(ZipDirHeader)));
	#pragma endregion

	ResourceManager::ResourceManager()
		: m_pResourceFactory(nullptr)
		, m_quitThread(false)
		, m_engineResourcePath("Invalid Engine Resource Path.")
		, m_useRawAssets(false)
	{
	}

	ResourceManager::~ResourceManager()
	{
		// Remove resources to be loaded.
		m_deferredQueueLock.lock();
		m_deferredQueue.clear();
		m_deferredQueueLock.unlock();

		m_listenerMapLock.lock();
		m_deferredResourceListenersMap.clear();
		m_listenerMapLock.unlock();

		// Tell the thread we are done.
		m_quitThread = true;
		SignalAndWaitForLoaderThread();
		m_loaderThread.join();

		// Don't delete, this lives on the Application/Engine.
		// TODO: Should this be the case? Can I make this more explicit?
		m_pResourceFactory = nullptr;
	}

	bool ResourceManager::Initialize(ResourceFactory* pResourceFactory, const char* pEngineResourcePath, bool useRawAssets)
	{
		// Default resource factory MUST exist.
		EXE_ASSERT(pResourceFactory);
		m_pResourceFactory = pResourceFactory;

		if (pEngineResourcePath)
			m_engineResourcePath = pEngineResourcePath;

		m_useRawAssets = useRawAssets;

		// Should not contain data, but just in case.
		m_deferredQueueLock.lock();
		m_deferredQueue.clear();
		m_deferredQueueLock.unlock();

		m_listenerMapLock.lock();
		m_deferredResourceListenersMap.clear();
		m_listenerMapLock.unlock();

		// Spin up the loader thread.
		m_loaderThread = std::thread(&ResourceManager::ProcessResourceQueueThreaded, this);

		return true;
	}

	void ResourceManager::QueueLoad(const ResourceID& resourceID, bool signalLoaderThread, ResourceListenerPtr pListener)
	{
		EXE_ASSERT(resourceID.IsValid());
		EXE_ASSERT(m_loaderThread.joinable());
		Log log("ResourceManager");

		log.Trace("Queueing Resource: {}", resourceID.Get().c_str());

		// Check if the resource is already in the resource database.
		if (!m_resourceDatabase.IsFound(resourceID))
		{
			log.Trace("Creating new resource entry.");
			m_resourceDatabase.CreateEntry(resourceID);

			m_listenerMapLock.lock();
			m_deferredResourceListenersMap[resourceID].emplace_back(pListener);
			m_listenerMapLock.unlock();
		}
		else if (m_resourceDatabase.GetLoadStatus(resourceID) == ResourceLoadStatus::kLoaded || m_resourceDatabase.GetLoadStatus(resourceID) == ResourceLoadStatus::kLoading)
		{
			log.Trace("Resource already loaded or queued.");

			if (!pListener.expired()) // This may seem unnecessary, but it is a catch in case no listener was passed in.
				pListener.lock()->OnResourceLoaded(resourceID);

			return; // Bail here. We do NOT want to change the status of the resource.
		}
		else
		{
			log.Warn("Resource was unloaded or unloading. SEE TODO HERE.");
			/// TODO:
			///		It is possible that neither of the 2 branches were entered.
			///		This means that the Resource exists in the database && its
			///		LoadStatus is neither Loaded or Loading, possibly Unloading
			///		or Unloaded. This could cause an issue in the future if not
			///		carefully protected against. For example: The pListener will
			///		not be called in this case, which is already a bug. Hack fix
			///		is as follows.
			m_listenerMapLock.lock();
			m_deferredResourceListenersMap[resourceID].emplace_back(pListener);
			m_listenerMapLock.unlock();
		}

		m_resourceDatabase.SetLoadStatus(resourceID, ResourceLoadStatus::kLoading);
		
		m_deferredQueueLock.lock();
		m_deferredQueue.emplace_back(resourceID);
		m_deferredQueueLock.unlock();

		if (signalLoaderThread)
			SignalLoaderThread();

		log.Trace("QueueLoad Complete.");
	}

	void ResourceManager::LoadNow(const ResourceID& resourceID, ResourceListenerPtr pListener)
	{
		EXE_ASSERT(resourceID.IsValid());
		Log log("ResourceManager");
		log.Trace("Loading Resource On Main Thread: {}", resourceID.Get().c_str());

		// Check if the resource is already in the resource database.
		if (!m_resourceDatabase.IsFound(resourceID))
		{
			log.Trace("Creating new resource entry.");
			m_resourceDatabase.CreateEntry(resourceID);
		}
		else if (m_resourceDatabase.GetLoadStatus(resourceID) == ResourceLoadStatus::kLoaded || m_resourceDatabase.GetLoadStatus(resourceID) == ResourceLoadStatus::kLoading)
		{
			log.Trace("Resource already loaded or queued.");
			return;
		}

		m_resourceDatabase.SetLoadStatus(resourceID, ResourceLoadStatus::kLoading);
		LoadResource(resourceID);

		if (!pListener.expired()) // This may seem unnecessary, but it is a catch in case no listener was passed in.
			pListener.lock()->OnResourceLoaded(resourceID);

		log.Trace("Load Complete.");
	}

	void ResourceManager::ReleaseResource(const ResourceID& resourceID)
	{
		EXE_ASSERT(resourceID.IsValid());
		Log log("ResourceManager");
		log.Trace("Releasing Resource: {}", resourceID.Get().c_str());

		ResourceEntry* pResourceEntry = m_resourceDatabase.GetEntry(resourceID);
		if (!pResourceEntry)
		{
			log.Info("Resource did not exist.");
			return;
		}

		// Decrement the reference count of this resource.
		// If there is no longer any references to this resource, then unload it.
		if (pResourceEntry->DecrementRefCount())
			m_resourceDatabase.Unload(resourceID);

		log.Trace("Release Complete.");
	}

	void ResourceManager::SignalLoaderThread()
	{
		EXE_ASSERT(m_loaderThread.joinable());
		Log log("ResourceManager");
		log.Trace("Signaling Loader Thread.");

		m_signalThread.notify_one();
	}

	void ResourceManager::SignalAndWaitForLoaderThread()
	{
		SignalLoaderThread();

		std::mutex waitMutex;
		std::unique_lock<std::mutex> waitLock(waitMutex);

		Log log("ResourceManager");
		log.Info("Waiting for response from LoaderThread.");
		m_signalThread.wait(waitLock);
	}

	void ResourceManager::ReloadResource(const ResourceID& resourceID, bool forceLoad, ResourceListenerPtr pListener)
	{
		EXE_ASSERT(resourceID.IsValid());
		Log log("ResourceManager");
		log.Trace("Reloading: {}", resourceID.Get().c_str());

		// Check if the resource is not already loaded.
		if (m_resourceDatabase.GetLoadStatus(resourceID) != ResourceLoadStatus::kLoaded)
		{
			log.Warn("Attempted to Reload Resource that is not loaded. Queueing/Loading now.");

			if (forceLoad)
				LoadNow(resourceID, pListener);
			else
				QueueLoad(resourceID, true, pListener);
			return;
		}

		// TODO:
		//	This may be unnecesary and could potentially lead to bugs.
		//	This will heavily depend on the handling of resource lifetimes/accessors.
		m_resourceDatabase.Unload(resourceID);

		if (forceLoad)
			LoadNow(resourceID, pListener);
		else
			QueueLoad(resourceID, true, pListener);

		log.Trace("Reload Complete.");
	}

	Resource* ResourceManager::GetResource(const ResourceID& resourceID, bool forceLoad)
	{
		EXE_ASSERT(resourceID.IsValid());
		Log log("ResourceManager");
		log.Trace("Attempting to Retrieve Resource: {}", resourceID.Get().c_str());

		ResourceEntry* pResourceEntry = m_resourceDatabase.GetEntry(resourceID);

		if (!pResourceEntry && forceLoad)
		{
			log.Info("Forcing Resource Creation and Retrieving.");
			LoadNow(resourceID);
			return GetResource(resourceID, false); // Should be guaranteed, but false will prevent infinite recursion.
		}
		else if (!pResourceEntry)
		{
			log.Info("ResourceEntry not found.");
			return nullptr;
		}

		// TODO:
		//	This needs error checking, though, with ResourceHandle implmentations, this may be fixed.
		log.Trace("Resource Retrieved.");
		return pResourceEntry->GetResource();
	}

	// TODO:
	// Currently incorrect. This returns true if the *ResourceEntry* exists.
	bool ResourceManager::IsResourceLoaded(const ResourceID& resourceID)
	{
		EXE_ASSERT(resourceID.IsValid());
		ResourceEntry* pResourceEntry = m_resourceDatabase.GetEntry(resourceID);

		if (!pResourceEntry)
			return false;

		return true;
	}

	void ResourceManager::LockResource(const ResourceID& resourceID)
	{
		EXE_ASSERT(resourceID.IsValid());
		Log log("ResourceManager");
		log.Trace("Locking Resource: {}", resourceID.Get().c_str());

		ResourceEntry* pResourceEntry = m_resourceDatabase.GetEntry(resourceID);

		if (!pResourceEntry)
		{
			log.Trace("Could not find resource to lock.");
			return;
		}

		pResourceEntry->IncrementLockCount();
		log.Trace("Resource Locked.");
	}

	void ResourceManager::UnlockResource(const ResourceID& resourceID)
	{
		EXE_ASSERT(resourceID.IsValid());
		Log log("ResourceManager");
		log.Trace("Unlocking Resource: {}", resourceID.Get().c_str());

		ResourceEntry* pResourceEntry = m_resourceDatabase.GetEntry(resourceID);

		if (!pResourceEntry)
		{
			log.Trace("Could not find resource to unlock.");
			return;
		}

		pResourceEntry->DecrementLockCount();
		log.Trace("Resource Unlocked.");
	}

	void ResourceManager::ProcessResourceQueueThreaded()
	{
		Log log("ResourceManager");
		log.Info("Instantiating Resource Loader Thread.");
		std::mutex waitMutex;
		eastl::deque<ResourceID> processingQueue;
		ListenersMap processingResourceListenersMap;
		std::unique_lock<std::mutex> waitLock(waitMutex);

		// While the thread is still active (Not shut down)
		while (!m_quitThread)
		{
			// Wait until we are signaled to work.
			m_signalThread.wait(waitLock, [this]()
			{
				bool empty = true;

				m_deferredQueueLock.lock();
				empty = m_deferredQueue.empty();
				m_deferredQueueLock.unlock();

				return m_quitThread || !empty;
			});

			log.Trace("Loader Thread Recieved Signal");

			// Don't do any work if we're exiting.
			if (m_quitThread)
				break;

			/// Capture the deferred queue into the processing queue (swap buffers)
			m_deferredQueueLock.lock();
			processingQueue.swap(m_deferredQueue);
			m_deferredQueueLock.unlock();

			m_listenerMapLock.lock();
			processingResourceListenersMap.swap(m_deferredResourceListenersMap);
			m_listenerMapLock.unlock();

			// Process the queue
			while (!processingQueue.empty())
			{
				log.Trace("Loader Thread Loading: {}", processingQueue.front().Get().c_str());
				LoadResource(processingQueue.front());

				// Notify all the listeners that we are done loading.
				for (auto& listener : processingResourceListenersMap[processingQueue.front()])
				{
					if (listener.expired())
						continue;

					listener.lock()->OnResourceLoaded(processingQueue.front());
				}
				processingResourceListenersMap[processingQueue.front()].clear();

				log.Trace("Loader Thread Loading Complete.");
				processingQueue.pop_front();
			}

			// Done loading this pass, so signal the main thread in case it is waiting.
			m_signalThread.notify_one();
			log.Trace("Signaled Main Thread: Queue Finished");
		}

		// Let the main thread know we are fully exiting in case they are waiting.
		log.Info("Signaled Main Thread: Thread Terminating.");
		m_signalThread.notify_one();
	}

	void ResourceManager::LoadResource(const ResourceID& resourceID)
	{
		EXE_ASSERT(resourceID.IsValid());
		Log log("ResourceManager");
		log.Trace("Loading Resource Internally: {}", resourceID.Get().c_str());

		// TODO:
		//	Remove use of vector.
		eastl::vector<std::byte> rawData = LoadRawData(resourceID);

		if (rawData.empty())
		{
			log.Warn("Raw file data was empty.");

			// TODO:
			//	This may present an issue with this resource having references to it.

			m_resourceDatabase.Unload(resourceID);
			return;
		}

		Resource* pResource = m_pResourceFactory->CreateResource(resourceID);
		if (!pResource)
		{
			log.Warn("Failed to create resource from resource factory.");
			m_resourceDatabase.Unload(resourceID);
			return;
		}

		if (pResource->Load(std::move(rawData)) != Resource::LoadResult::kFailed)
		{
			m_resourceDatabase.GetEntry(resourceID)->SetResource(pResource);
		}
		else
		{
			log.Warn("Failed to load resource from raw data.");
			m_resourceDatabase.Unload(resourceID);
			return;
		}

		m_resourceDatabase.SetLoadStatus(resourceID, ResourceLoadStatus::kLoaded);

		log.Trace("Completed Loading Internally.");
	}

	eastl::vector<std::byte> ResourceManager::LoadRawData(const ResourceID& resourceID)
	{
		EXE_ASSERT(resourceID.IsValid());
		Log log("ResourceManager");
		log.Trace("Loading Resource Raw Data: {}", resourceID.Get().c_str());

		if (m_useRawAssets)
		{
			return LoadFromDisk(resourceID);
		}
		else
		{
			return LoadFromZip(resourceID);
		}
	}

	eastl::vector<std::byte> ResourceManager::LoadFromDisk(const ResourceID& resourceID)
	{
		Log log("ResourceManager");
		eastl::string strPath = resourceID.Get();
		String::ToFilepath(strPath);

		std::ifstream inFile;
		inFile.open(strPath.c_str(), std::ios_base::binary | std::ios_base::in);
		if (!inFile.is_open())
		{
			log.Warn("Failed to open file: {}", strPath.c_str());

			if (inFile.bad())
				log.Warn("I/O error while reading");
			else if (inFile.eof())
				log.Warn("End of file reached successfully");
			else
				log.Warn("Non-integer data encountered. Possible incorrect file path.");
			
			return eastl::vector<std::byte>();
		}

		// find the size of the file in bytes;
		inFile.seekg(0, std::ios::end);
		std::streampos size = inFile.tellg();
		inFile.seekg(0, std::ios::beg);

		// Read contents into vector of bytes
		eastl::vector<std::byte> data(static_cast<size_t>(size));
		inFile.read(reinterpret_cast<char*>(data.data()), size);

		inFile.close();

		return data;
	}

	eastl::vector<std::byte> ResourceManager::LoadFromZip(const ResourceID& resourceID)
	{
		eastl::string strPath = resourceID.Get().c_str();
		String::ToFilepath(strPath);

		//std::ifstream inFile(strPath.c_str(), std::ios_base::binary | std::ios_base::in);
		//if (inFile.fail())
		//{
		//	log.Warn("Failed to open file: {}", strPath.c_str());
		//	return eastl::vector<std::byte>();
		//}

		//ZipDirHeader dirHeader;
		//memset(&dirHeader, 0, sizeof(dirHeader));

		//inFile.seekg(kPtrDelta, std::ios_base::end);
		//const auto dirHeaderOffset = inFile.tellg();
		//inFile.read(reinterpret_cast<char*>(&dirHeader), sizeof(ZipDirHeader));
		//if (dirHeader.sig != ZipDirHeader::kSignature)
		//{
		//	log.Warn("Corrupted Zip file: {}", strPath.c_str());
		//	return eastl::vector<std::byte>();
		//}

		//// Now load the respective file? I need to think more about how I want this to operate.
		return eastl::vector<std::byte>();
	}
}