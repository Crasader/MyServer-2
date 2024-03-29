#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <sstream>
#include <iostream>

#include "../base/logging.h"
#include "timerQueue.h"
#include "eventloop.h"
#include "channel.h"
#include "epollpoller.h"
#include "sockets.h"

using namespace net;

namespace
{
	__thread EventLoop* t_loopInThisThread = 0;

	const int kPollTimeMs = 1;

	int createEventfd()
	{
		int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if (evtfd < 0)
		{
			LOG_SYSERR << "Failed in eventfd";
			abort();
		}
		return evtfd;
	}

#pragma GCC diagnostic ignored "-Wold-style-cast"
	class IgnoreSigPipe
	{
	public:
		IgnoreSigPipe()
		{
			::signal(SIGPIPE, SIG_IGN);
			// LOG_TRACE << "Ignore SIGPIPE";
		}
	};
#pragma GCC diagnostic error "-Wold-style-cast"

	IgnoreSigPipe initObj;
}

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
	return t_loopInThisThread;
}


EventLoop::EventLoop()
: looping_(false),
quit_(false),
eventHandling_(false),
callingPendingFunctors_(false),
iteration_(0),
threadId_(std::this_thread::get_id()),
poller_(new EpollPoller(this)),
timerQueue_(new TimerQueue(this)),
wakeupFd_(createEventfd()),
wakeupChannel_(new Channel(this, wakeupFd_)),
currentActiveChannel_(NULL)
{
	if (t_loopInThisThread)
	{
		LOG_FATAL << "Another EventLoop  exists in this thread ";
	}
	else
	{
		t_loopInThisThread = this;
	}
	wakeupChannel_->setReadCallBack(std::bind(&EventLoop::handleRead, this));
	// we are always reading the wakeupfd
	wakeupChannel_->enableReading();

// 	std::stringstream ss;	
// 	ss << "eventloop create threadid = " << threadId_;
// 	std::cout << ss.str() << std::endl;
}

EventLoop::~EventLoop()
{
	LOG_DEBUG << "EventLoop destructs in other thread";

// 	std::stringstream ss;
// 	ss << "eventloop destructs threadid = " << threadId_;
// 	std::cout << ss.str() << std::endl;

	wakeupChannel_->disableAll();
	wakeupChannel_->remove();
	::close(wakeupFd_);
	t_loopInThisThread = NULL;
}


void EventLoop::loop()
{
	assert(!looping_);
	assertInLoopThread();
	looping_ = true;
	quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
	LOG_TRACE << "EventLoop " << this << " start looping";

	while (!quit_)
	{
		activeChannels_.clear();
		pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
		++iteration_;
		if (Logger::logLevel() <= Logger::TRACE)
		{
			printActiveChannels();
		}
		// TODO sort channel by priority
		eventHandling_ = true;
		for (ChannelList::iterator it = activeChannels_.begin();
			it != activeChannels_.end(); ++it)
		{
			currentActiveChannel_ = *it;
			currentActiveChannel_->handleEvent(pollReturnTime_);
		}
		currentActiveChannel_ = NULL;
		eventHandling_ = false;
		doPendingFunctors();

		if (frameFunctor_)
		{
			frameFunctor_();
		}
	}

	LOG_TRACE << "EventLoop " << this << " stop looping";
	looping_ = false;
}

void EventLoop::quit()
{
	quit_ = true;
	// There is a chance that loop() just executes while(!quit_) and exists,
	// then EventLoop destructs, then we are accessing an invalid object.
	// Can be fixed using mutex_ in both places.
	if (!isInLoopThread())
	{
		wakeup();
	}
}

void EventLoop::setFrameFunctor(const Functor& cb)
{
	frameFunctor_ = cb;
}

TimerId EventLoop::runAt(const Timestamp& time, const TimerCallBack& cb)
{
	return timerQueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const TimerCallBack& cb)
{
	Timestamp time(addTime(Timestamp::now(), delay));
	return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, const TimerCallBack& cb)
{
	Timestamp time(addTime(Timestamp::now(), interval));
	return timerQueue_->addTimer(cb, time, interval);
}

TimerId EventLoop::runAt(const Timestamp& time, TimerCallBack&& cb)
{
	return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallBack&& cb)
{
	Timestamp time(addTime(Timestamp::now(), delay));
	return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallBack&& cb)
{
	Timestamp time(addTime(Timestamp::now(), interval));
	return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId)
{
	return timerQueue_->cancel(timerId);
}


void EventLoop::updateChannel(Channel* channel)
{
	assert(channel->ownerLoop() == this);
	assertInLoopThread();
	poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
	assert(channel->ownerLoop() == this);
	assertInLoopThread();
	if (eventHandling_)
	{
		assert(currentActiveChannel_ == channel ||
			std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
	}
	poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
	assert(channel->ownerLoop() == this);
	assertInLoopThread();
	return poller_->hasChannel(channel);
}

void EventLoop::wakeup()
{
	uint64_t one = 1;
	ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
	if (n != sizeof one)
	{
		LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
	}
}

void EventLoop::runInLoop(const Functor& cb)
{
	if (isInLoopThread())
	{
		cb();
	}
	else
	{
		queueInLoop(cb);
	}
}

void EventLoop::queueInLoop(const Functor& cb)
{
	{
		std::unique_lock<std::mutex> lock(mutex_);
		pendingFunctors_.push_back(cb);
	}

	if (!isInLoopThread() || callingPendingFunctors_)
	{
		wakeup();
	}
}

void EventLoop::handleRead()
{
	uint64_t one = 1;
	ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
	if (n != sizeof one)
	{
		LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
	}
}



void EventLoop::abortNotInLoopThread()
{
	std::stringstream ss;
	ss << "threadid_ = " << threadId_ << " this_thread::get_id() = " << std::this_thread::get_id();
	LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << ss.str();
}

void EventLoop::doPendingFunctors()
{
	std::vector<Functor> functors;
	callingPendingFunctors_ = true;

	{
		std::unique_lock<std::mutex> lock(mutex_);
		functors.swap(pendingFunctors_);
	}

	for (size_t i = 0; i < functors.size(); ++i)
	{
		functors[i]();
	}
	callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
	for (ChannelList::const_iterator it = activeChannels_.begin();
		it != activeChannels_.end(); ++it)
	{
		const Channel* ch = *it;
		LOG_TRACE << "{" << ch->reventsToString() << "} ";
	}
}