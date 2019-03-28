#include <stdio.h>
#include <assert.h>
#include <sstream>
#include <string>

#include "eventloopthreadpool.h"
#include "eventloop.h"
#include "eventloopthread.h"
#include "callback.h"

using namespace net;

EventLoopThreadPool::EventLoopThreadPool()
: baseLoop_(NULL),
started_(false),
numThreads_(0),
next_(0)
{

}

EventLoopThreadPool::~EventLoopThreadPool()
{

}

void EventLoopThreadPool::Init(EventLoop* baseLoop, int numThreads)
{
	baseLoop_ = baseLoop;
	numThreads_ = numThreads;
}

void EventLoopThreadPool::start(const ThreadInitCallBack &cb /* = ThreadInitCallBack() */)
{
	assert(baseLoop_);
	assert(!started_);
	baseLoop_->assertInLoopThread();

	started_ = true;

	for (int i = 0; i < numThreads_; i++)
	{
		char buf[name_.size() + 32];
		snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);

		std::shared_ptr<EventLoopThread> t(new EventLoopThread(cb, buf));

		threads_.push_back(t);
		loops_.push_back(t->startLoop());
	}

	if (numThreads_ == 0 && cb)
	{
		cb(baseLoop_);
	}
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
	baseLoop_->assertInLoopThread();
	assert(started_);
	EventLoop* loop = baseLoop_;

	if (!loops_.empty())
	{
		// round-robin
		loop = loops_[next_];
		++next_;
		if (implicit_cast<size_t>(next_) >= loops_.size())
		{
			next_ = 0;
		}
	}
	return loop;
}

EventLoop* EventLoopThreadPool::getLoopForHash(size_t hashCode)
{
	baseLoop_->assertInLoopThread();
	EventLoop* loop = baseLoop_;

	if (!loops_.empty())
	{
		loop = loops_[hashCode % loops_.size()];
	}
	return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
	baseLoop_->assertInLoopThread();
	assert(started_);
	if (loops_.empty())
	{
		return std::vector<EventLoop*>(1, baseLoop_);
	}
	else
	{
		return loops_;
	}
}

const std::string EventLoopThreadPool::info() const
{
	std::stringstream ss;
	ss << "print threads id info " << endl;
	for (size_t i = 0; i < loops_.size(); i++)
	{
		ss << i << ": id = " << loops_[i]->getThreadID() << endl;
	}
	return ss.str();
}
