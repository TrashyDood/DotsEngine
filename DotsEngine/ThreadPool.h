#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

class ThreadPool
{
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;

	std::mutex queueMutex;
	std::condition_variable condition;
	bool stopping = false;

public:
	explicit ThreadPool(size_t threadCount = std::thread::hardware_concurrency());
	~ThreadPool();

	template<typename F, typename... Args>
	auto Submit(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
	{
		using ReturnType = typename std::result_of<F(Args...)>::type; // alias

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
	void WorkerLoop();
};

inline ThreadPool& GlobalThreadPool()
{
	static ThreadPool pool(12);
	return pool;
}