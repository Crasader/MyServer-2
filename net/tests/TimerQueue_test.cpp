#include <stdio.h>
#include <unistd.h>
#include <thread>

#include "../eventloop.h"
using namespace net;

int cnt = 0;
EventLoop* g_loop;

void printTid()
{
	printf("pid = %d, tid = %d\n", getpid(), std::this_thread::get_id());
	printf("now %s\n", Timestamp::now().toString().c_str());
}

void print(const char* msg)
{
	printf("msg %s %s\n", Timestamp::now().toString().c_str(), msg);
	if (++cnt == 20)
	{
		g_loop->quit();
	}
}

void cancel(TimerId timer)
{
	g_loop->cancel(timer);
	printf("cancelled at %s\n", Timestamp::now().toString().c_str());
}

void threadFunc()
{
	g_loop->loop();
}

int main(void)
{
	//printTid();
	sleep(1);
	{
		EventLoop loop;
		g_loop = &loop;

// 		std::thread thread(threadFunc);
// 		thread.join();
		print("main");
		//loop.runAfter(1, std::bind(print, "once1"));
// 		loop.runAfter(1.5, std::bind(print, "once1.5"));
// 		loop.runAfter(2.5, std::bind(print, "once2.5"));
// 		loop.runAfter(3.5, std::bind(print, "once3.5"));
// 		TimerId t45 = loop.runAfter(4.5, std::bind(print, "once4.5"));
// 		loop.runAfter(4.2, std::bind(cancel, t45));
// 		loop.runAfter(4.8, std::bind(cancel, t45));
		loop.runEvery(2, std::bind(print, "every2"));
// 		TimerId t3 = loop.runEvery(3, std::bind(print, "every3"));
// 		loop.runAfter(9.001, std::bind(cancel, t3));

 		loop.loop();
		print("main loop exits");
	}
}
