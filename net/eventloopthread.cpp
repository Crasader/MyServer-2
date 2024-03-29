
#include "eventloop.h"
#include "eventloopthread.h"
#include <functional>

using namespace net;

EventLoopThread::EventLoopThread(const ThreadInitCallBack& cb,
	const std::string& name)
	: loop_(NULL),
	exiting_(false),
	callBack_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
	exiting_ = true;
	if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
	{
		// still a tiny chance to call destructed object, if threadFunc exits just now.
		// but when EventLoopThread destructs, usually programming is exiting anyway.
		loop_->quit();
		thread_->join();
	}
}

EventLoop* EventLoopThread::startLoop()
{
	//assert(!thread_.started());
	//thread_.start();

	thread_.reset(new std::thread(std::bind(&EventLoopThread::threadFunc, this)));// 执行threadFunc(); 构造函数初始化列表中thread_(std::bind(&EventLoopThread::threadFunc, this))

	{
		std::unique_lock<std::mutex> lock(mutex_);
		while (loop_ == NULL)
		{
			cond_.wait(lock);
		}
	}

	return loop_;
}

void EventLoopThread::stopLoop()
{
	if (loop_ != NULL)
		loop_->quit();

	thread_->join();
}

void EventLoopThread::threadFunc()
{
	EventLoop loop;

	if (callBack_)
	{
		callBack_(&loop);
	}

	{
		//一个一个的线程创建
		std::unique_lock<std::mutex> lock(mutex_);
		loop_ = &loop;
		cond_.notify_all();
	}

	loop.loop();
	//assert(exiting_);
	loop_ = NULL;
}

