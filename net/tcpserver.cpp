#include <stdio.h>  // snprintf
#include <functional>

#include "../base/logging.h"
#include "../base/singleton.h"
#include "accept.h"
#include "eventloop.h"
#include "eventloopthreadpool.h"
#include "sockets.h"
#include "tcpserver.h"

using namespace net;

TcpServer::TcpServer(EventLoop* loop,
	const InetAddress& listenAddr,
	const std::string& nameArg,
	Option option)
	: loop_(CHECK_NOTNULL(loop)),
	hostport_(listenAddr.toIpPort()),
	name_(nameArg),
	acceptor_(new Accept(loop, listenAddr, option == kReusePort)),
	//threadPool_(new EventLoopThreadPool(loop, name_)),
	connectionCallBack_(defaultConnectionCallBack),
	messageCallBack_(defaultMessageCallBack),
	started_(0),
	nextConnId_(1)
{
	acceptor_->setNewConnectCallBack(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
	loop_->assertInLoopThread();
	LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

	for (ConnectionMap::iterator it(connections_.begin());
		it != connections_.end(); ++it)
	{
		TcpConnectionPtr conn = it->second;
		it->second.reset();
		conn->getLoop()->runInLoop(
			std::bind(&TcpConnection::connectDestroyed, conn));
		conn.reset();
	}
}

//void TcpServer::setThreadNum(int numThreads)
//{
//  assert(0 <= numThreads);
//  threadPool_->setThreadNum(numThreads);
//}

void TcpServer::start()
{
	if (started_ == 0)
	{
		//threadPool_->start(threadInitCallback_);
		assert(!acceptor_->listening());
		loop_->runInLoop(std::bind(&Accept::listen, acceptor_.get()));
		started_ = 1;
	}
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
	loop_->assertInLoopThread();
	//EventLoop* ioLoop = threadPool_->getNextLoop();
	EventLoop* ioLoop = Singleton<EventLoopThreadPool>::Instance().getNextLoop();
	char buf[32];
	snprintf(buf, sizeof buf, ":%s#%d", hostport_.c_str(), nextConnId_);
	++nextConnId_;
	string connName = name_ + buf;

	LOG_INFO << "TcpServer::newConnection [" << name_
		<< "] - new connection [" << connName
		<< "] from " << peerAddr.toIpPort();
	InetAddress localAddr(sockets::getLocalAddr(sockfd));
	// FIXME poll with zero timeout to double confirm the new connection
	// FIXME use make_shared if necessary
	TcpConnectionPtr conn(new TcpConnection(ioLoop,
		connName,
		sockfd,
		localAddr,
		peerAddr));
	connections_[connName] = conn;
	conn->setConnectionCallBack(connectionCallBack_);
	conn->setMessageCallBack(messageCallBack_);
	conn->setWriteCompleteCallBack(writeCompleteCallBack_);
	conn->setCloseCallBack(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)); // FIXME: unsafe
	//该线程分离完io事件后，立即调用TcpConnection::connectEstablished
	ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
	// FIXME: unsafe
	loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
	loop_->assertInLoopThread();
	LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
		<< "] - connection " << conn->name();
	size_t n = connections_.erase(conn->name());
	(void)n;
	assert(n == 1);
	EventLoop* ioLoop = conn->getLoop();
	ioLoop->queueInLoop(
		std::bind(&TcpConnection::connectDestroyed, conn));
}

