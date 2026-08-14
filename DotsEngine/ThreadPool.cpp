#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threadCount)
{
	for (size_t i = 0; i < threadCount; i++)
		workers.emplace_back([this]() { WorkerLoop(); });
}

ThreadPool::~ThreadPool()
{
	{
		std::lock_guard<std::mutex> lock(queueMutex);
		stopping = true;
	}
	condition.notify_all();
	for (auto& worker : workers)
		worker.join();
}

void ThreadPool::WorkerLoop()
{
	while (true)
	{
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			condition.wait(lock, [this]() { return stopping || !tasks.empty(); });

			if (stopping && tasks.empty())
				return;

			task = std::move(tasks.front());
			tasks.pop();
		}
		task();
	}
}