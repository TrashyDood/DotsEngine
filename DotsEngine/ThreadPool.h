#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

template<unsigned char ThreadCount>
class ThreadPool
{
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;

	std::mutex queueMutex;
	std::condition_variable condition;
	bool stopping = false;

public:
	static constexpr unsigned char threadCount = ThreadCount;

	explicit ThreadPool()
	{
		for (size_t i = 0; i < ThreadCount; i++)
			workers.emplace_back([this]() { WorkerLoop(); });
	}

	~ThreadPool()
	{
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			stopping = true;
		}
		condition.notify_all();
		for (auto& worker : workers)
			worker.join();
	}

	template<typename F, typename... Args>
	auto Submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
	{
		using ReturnType = typename std::result_of<F(Args...)>::type;

		auto task = std::make_shared<std::packaged_task<ReturnType()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...));

		std::future<ReturnType> result = task->get_future();

		{
			std::lock_guard<std::mutex> lock(queueMutex);
			if (stopping)
				throw std::runtime_error("Submit called on stopped ThreadPool");
			tasks.emplace([task]() { (*task)(); });
		}

		condition.notify_one();
		return result;
	}
	
	template<int ParallelForThreadCount, typename Func>
	void ParallelFor(size_t count, Func&& func)
	{
		size_t chunkSize = (count + ParallelForThreadCount - 1) / ParallelForThreadCount;
		std::future<void> threadResults[ParallelForThreadCount];
		int usedThreads = 0;

		for (int thread = 0; thread < ParallelForThreadCount; thread++)
		{
			uint32_t chunkStart = thread * chunkSize;
			if (chunkStart >= count)
				break;

			uint32_t chunkEnd = std::min(chunkStart + chunkSize, count);

			threadResults[thread] = Submit([chunkStart, chunkEnd, thread, &func]()
			{
				for (uint32_t index = chunkStart; index < chunkEnd; index++)
					func(thread, index);
			});

			usedThreads++;
		}

		for (int thread = 0; thread < usedThreads; thread++)
			threadResults[thread].get();
	}

private:
	void WorkerLoop()
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
};

inline ThreadPool<32>& GlobalThreadPool()
{
	static ThreadPool<32> threadPoolInstance;
	return threadPoolInstance;
}