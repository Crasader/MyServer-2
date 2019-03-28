#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace net{
	class EventLoop;
	class EventLoopThread;

	class EventLoopThreadPool
	{
	public:
		typedef std::function<void(EventLoop*)> ThreadInitCallBack;

		EventLoopThreadPool();
		~EventLoopThreadPool();

		void Init(EventLoop* baseLoop, int numThreads);
		void start(const ThreadInitCallBack &cb = ThreadInitCallBack());

		EventLoop* getNextLoop();

		/// with the same hash code, it will always return the same EventLoop
		EventLoop* getLoopForHash(size_t hashCode);

		std::vector<EventLoop*> getAllLoops();

		bool started()const{ return started_; }

		const std::string& name()const{ return name_; }

		const std::string info()const;
	private:
		EventLoop* baseLoop_;
		std::string name_;
		bool started_;
		int numThreads_;
		int next_;
		std::vector<std::shared_ptr<EventLoopThread>> threads_;
		std::vector<EventLoop*> loops_;
	};


}