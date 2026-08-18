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