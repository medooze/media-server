#ifndef THREADREGISTRY_H
#define THREADREGISTRY_H

#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

class ThreadRegistry
{
public:
	
	class ScopedRegister
	{
	public:
		ScopedRegister(std::thread::id threadId, const std::function<std::future<void>()>& onThreadRegistryClose, 
				const std::function<void()> &onExitScope) :
			threadId(threadId),
			onExitScope(onExitScope)
		{
			if (!ThreadRegistry::Register(threadId, onThreadRegistryClose))
				throw std::runtime_error("Failed to register thread");
		}
		
		~ScopedRegister()
		{
			onExitScope();
			ThreadRegistry::Unregister(threadId);
		}
		
		std::thread::id threadId;
		std::function<void()> onExitScope;
	};
	
	static std::unique_ptr<ScopedRegister> RegisterCurrentThread(const std::function<std::future<void>()>& onThreadRegistryClose,
								const std::function<void()> &onExitScope)
	{
		std::unique_ptr<ScopedRegister> scopedregister;
		try
		{
			scopedregister = std::make_unique<ScopedRegister>(std::this_thread::get_id(), onThreadRegistryClose, onExitScope);
		}
		catch(const std::exception& e)
		{
			// Failed, ThreadRegistry was already closed
		}
		
		return scopedregister;
	}


	static bool Register(std::thread::id threadId, const std::function<std::future<void>()>& onThreadRegistryClose)
	{
		std::lock_guard<std::mutex> lock(callbackMutex);
		if (closed) return false;
		
		callbacks[threadId] = onThreadRegistryClose;
		return true;
	}
	
	static void Unregister(std::thread::id threadId)
	{
		std::lock_guard<std::mutex> lock(callbackMutex);
		callbacks.erase(threadId);
	}
	
	static void Close()
	{
		std::vector<std::future<void>> futures;
		
		{
			std::lock_guard<std::mutex> lock(callbackMutex);
			closed = true;
			
			for (auto& cb : callbacks)
			{	
				futures.push_back(cb.second());
			}
		}
		
		for (auto& f : futures)
		{
			f.wait();
		}
	}

private:
	static std::unordered_map<std::thread::id, std::function<std::future<void>()>> callbacks;
	
	static bool	closed;
	static std::mutex	callbackMutex;
};

#endif