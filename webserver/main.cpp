#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <sys/resource.h>

#include "../base/configfilereader.h"
#include "../base/logging.h"
#include "../base/timestamp.h"
#include "../base/asynclogging.h"
#include "../utils/DaemonRun.h"
#include "../base/singleton.h"
#include "../net/eventloop.h"
#include "../net/eventloopthreadpool.h"
#include "../mysql/mysqlmanager.h"
#include "HttpServer.h"

extern void defaultOutput(const char* msg, int len);

using namespace net;
EventLoop g_mainLoop;
AsyncLogging* g_asyncLog = NULL;
void asyncOutput(const char* msg, int len)
{
	if (g_asyncLog != NULL)
	{
		g_asyncLog->append(msg, len);
		std::cout << msg << std::endl;
	}
}


void prog_exit(int signo)
{
	std::cout << "program recv signal [" << signo << "] to exit." << std::endl;

	Singleton<EventLoopThreadPool>::Instance().stop();
	g_mainLoop.quit();

	Logger::setOutputFunc(defaultOutput);
}

int main(int argc, char* argv[])
{

	//�����źŴ���
	signal(SIGCHLD, SIG_DFL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, prog_exit);
	signal(SIGTERM, prog_exit);

	int ch;
	bool bdaemon = false;
	while ((ch = getopt(argc, argv, "d")) != -1)
	{
		switch (ch)
		{
		case 'd':
			bdaemon = true;
			break;
		}
	}

	if (bdaemon)
		daemon_run();

	CConfigFileReader config("../../etc/webserver.conf");
	const char* logfilepath = config.getConfigName("logfiledir");
	if (logfilepath == NULL)
	{
		LOG_SYSFATAL << "logdir is not set in config file";
		return 1;
	}
	else
	{
		LOG_INFO << logfilepath;
	}

	//	���logĿ¼�������򴴽�֮
	DIR* dp = opendir(logfilepath);
	if (dp == NULL)
	{
		if (mkdir(logfilepath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
		{
			LOG_SYSFATAL << "create base dir error, " << logfilepath << ", errno: " << errno << ", " << strerror(errno);
			return 1;
		}
	}
	closedir(dp);

	const char* logfilename = config.getConfigName("logfilename");
	if (logfilename == NULL)
	{
		LOG_SYSFATAL << "logfilename is not set in config file";
		return 1;
	}
	else
	{

		LOG_INFO << logfilename;
	}

	std::string strLogFileFullPath(logfilepath);
	strLogFileFullPath += logfilename;
	Logger::setLoglevel(Logger::DEBUG);
	int kRollSize = 500 * 1000 * 1000;
	AsyncLogging log(strLogFileFullPath.c_str(), kRollSize);
	log.start();
	g_asyncLog = &log;
	Logger::setOutputFunc(asyncOutput);

	const char* dbserver = config.getConfigName("dbserver");
	const char* dbuser = config.getConfigName("dbuser");
	const char* dbpassword = config.getConfigName("dbpassword");
	const char* dbname = config.getConfigName("dbname");
	if (!Singleton<CMysqlManager>::Instance().Init(dbserver, dbuser, dbpassword, dbname))
	{
		LOG_FATAL << "Init mysql failed, please check your database config..............";
	}

	Singleton<EventLoopThreadPool>::Instance().Init(&g_mainLoop, 4);
	Singleton<EventLoopThreadPool>::Instance().start();

	const char* httplistenip = config.getConfigName("httplistenip");
	short httplistenport = (short)atol(config.getConfigName("httplistenport"));
	int maxConnections = atoi(config.getConfigName("maxconnections"));
	Singleton<HttpServer>::Instance().Init(httplistenip, httplistenport, &g_mainLoop, maxConnections);

	g_mainLoop.loop();

	LOG_INFO << "exit webserver.";

	return 0;

}