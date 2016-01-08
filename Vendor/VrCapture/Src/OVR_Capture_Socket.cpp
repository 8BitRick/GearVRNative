/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_Socket.cpp
Content     :   Misc network communication functionality.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#include "OVR_Capture_Socket.h"
#include "OVR_Capture_Local.h"
#include "OVR_Capture_Thread.h"

#include <string.h> // memset

namespace OVR
{
namespace Capture
{

	static UInt16 HostToNet16(UInt16 x)
	{
	#if 0
		// Clang 3.5 on Arm64 throws the following...
		// error: value size does not match register size specified by the constraint and modifier [-Werror,-Wasm-operand-widths]
		//   __asm volatile ("rev16 %0, %0" : "+r" (_x));
		// note: use constraint modifier "w"
		//   __asm volatile ("rev16 %0, %0" : "+r" (_x));
		return htons(x);
	#elif BYTE_ORDER == LITTLE_ENDIAN
		// safe/portable but potentially slower solution to work around NDK's broken inline assembly.
		return ((x&0x00ff)<<8) | ((x&0xff00)>>8);
	#elif BYTE_ORDER == BIG_ENDIAN
		return x;
	#else
		#error Unknown Byte Order!
	#endif
	}


	SocketAddress SocketAddress::Any(UInt16 port)
	{
		SocketAddress addr;
		addr.m_addr.sin_family      = AF_INET;
		addr.m_addr.sin_addr.s_addr = INADDR_ANY;
		addr.m_addr.sin_port        = HostToNet16(port);
		return addr;
	}

	SocketAddress SocketAddress::Broadcast(UInt16 port)
	{
		SocketAddress addr;
		addr.m_addr.sin_family      = AF_INET;
		addr.m_addr.sin_addr.s_addr = INADDR_BROADCAST;
		addr.m_addr.sin_port        = HostToNet16(port);
		return addr;
	}

	SocketAddress::SocketAddress(void)
	{
		memset(&m_addr, 0, sizeof(m_addr));
	}


	Socket *Socket::Create(Type type)
	{
		Socket *newSocket = NULL;
	#if defined(OVR_CAPTURE_POSIX) || defined(OVR_CAPTURE_WINDOWS)
		int sdomain   = 0;
		int stype     = 0;
		int sprotocol = 0;
		switch(type)
		{
			case Type_Stream:
				sdomain   = AF_INET;
				stype     = SOCK_STREAM;
				sprotocol = IPPROTO_TCP;
				break;
			case Type_Datagram:
				sdomain   = AF_INET;
				stype     = SOCK_DGRAM;
				sprotocol = 0;
				break;
		}
		const SocketType s = ::socket(sdomain, stype, sprotocol);
		OVR_CAPTURE_ASSERT(s != s_invalidSocket);
		if(s != s_invalidSocket)
		{
		#if defined(SO_NOSIGPIPE)
			// Disable SIGPIPE on close...
			const UInt32 value = 1;
			::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value));
		#endif
			// Create the Socket object...
			newSocket = new Socket();
			newSocket->m_socket = s;
		}
	#else
		#error Unknown Platform!
	#endif
		return newSocket;
	}

	void Socket::Release(void)
	{
		delete this;
	}

	bool Socket::SetBroadcast(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		const UInt32 broadcast = 1;
		return ::setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == 0;
	#elif defined(OVR_CAPTURE_WINDOWS)
		const BOOL broadcast = 1;
		return ::setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast)) == 0;
	#else
		#error Unknown Platform!
	#endif
	}

	bool Socket::Bind(const SocketAddress &addr)
	{
	#if defined(OVR_CAPTURE_POSIX) || defined(OVR_CAPTURE_WINDOWS)
		return ::bind(m_socket, (const sockaddr*)&addr.m_addr, sizeof(addr.m_addr)) == 0;
	#else
		#error Unknown Platform!
	#endif
	}

	bool Socket::Listen(UInt32 maxConnections)
	{
	#if defined(OVR_CAPTURE_POSIX) || defined(OVR_CAPTURE_WINDOWS)
		return ::listen(m_socket, maxConnections) == 0;
	#else
		#error Unknown Platform!
	#endif
	}

	Socket *Socket::Accept(SocketAddress &addr)
	{
	#if defined(OVR_CAPTURE_POSIX) || defined(OVR_CAPTURE_WINDOWS)
		const UInt32 waitflags = WaitFor(WaitFlag_Read);

		// On error or timeout, abort!
		if(waitflags == 0)
			return NULL;

		// If we have been signaled to abort... then don't even try to accept...
		if(waitflags & WaitFlag_Shutdown)
			return NULL;

		// If the signal wasn't from the socket (WTF?)... then don't even try to accept...
		if(!(waitflags & WaitFlag_Read))
			return NULL;

		socklen_t addrlen = sizeof(addr.m_addr);

		// Finally... accept the pending socket that we know for a fact is pending...
		Socket *newSocket = NULL;
		SocketType s = ::accept(m_socket, (sockaddr*)&addr.m_addr, &addrlen);
		if(s != s_invalidSocket)
		{
		#if defined(SO_NOSIGPIPE)
			// Disable SIGPIPE on close...
			const UInt32 value = 1;
			::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value));
		#endif
			// Create the Socket object...
			newSocket = new Socket();
			newSocket->m_socket = s;
		}
	#else
		#error Unknown Platform!
	#endif
		return newSocket;
	}

	void Socket::Shutdown(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		::shutdown(m_socket, SHUT_RDWR);
		// By writing into m_sendPipe and never reading from m_recvPipe, we are forcing all blocking select()
		// calls to return that m_recvPipe is ready for reading...
		if(m_sendPipe != -1)
		{
			const unsigned int dummy = 0;
			::write(m_sendPipe, &dummy, sizeof(dummy));
		}
	#elif defined(OVR_CAPTURE_WINDOWS)
		::shutdown(m_socket, SD_BOTH);
	#else
		#error Unknown Platform!
	#endif
	}

	bool Socket::Send(const void *buffer, UInt32 bufferSize)
	{
	#if defined(OVR_CAPTURE_POSIX) || defined(OVR_CAPTURE_WINDOWS)
		const char  *bytesToSend    = (const char*)buffer;
		int          numBytesToSend = (int)bufferSize;
		while(numBytesToSend > 0)
		{
			int flags = 0;
		#if defined(MSG_NOSIGNAL)
			flags = MSG_NOSIGNAL;
		#endif
			const int numBytesSent = ::send(m_socket, bytesToSend, numBytesToSend, flags);
			if(numBytesSent >= 0)
			{
				bytesToSend    += numBytesSent;
				numBytesToSend -= numBytesSent;
			}
			else
			{
				// An error occured... just shutdown the socket because after this we are in an invalid state...
				Shutdown();
				return false;
			}    
		}
		return true;
	#else
		#error Unknown Platform!
	#endif
	}

	bool Socket::SendTo(const void *buffer, UInt32 size, const SocketAddress &addr)
	{
	#if defined(OVR_CAPTURE_POSIX)
		return ::sendto(m_socket, buffer, size, 0, (const sockaddr*)&addr.m_addr, sizeof(addr.m_addr)) != -1;
	#elif defined(OVR_CAPTURE_WINDOWS)
		return ::sendto(m_socket, (const char*)buffer, (int)size, 0, (const sockaddr*)&addr.m_addr, sizeof(addr.m_addr)) != -1;
	#else
		#error Unknown Platform!
	#endif
	}

	UInt32 Socket::Receive(void *buffer, UInt32 size)
	{
	#if defined(OVR_CAPTURE_POSIX)
		const ssize_t r = ::recv(m_socket, buffer, (size_t)size, 0);
		return r>0 ? (UInt32)r : 0;
	#elif defined(OVR_CAPTURE_WINDOWS)
		const int r = ::recv(m_socket, (char*)buffer, (int)size, 0);
		return r>0 ? (UInt32)r : 0;
	#else
		#error Unknown Platform!
	#endif
	}

	UInt32 Socket::WaitFor(UInt32 waitflags, UInt32 timeoutInSeconds)
	{
		// we always check for shutdown regardless of what waitflags is set to...
		waitflags |= WaitFlag_Shutdown;

		const bool useTimeout = (waitflags & WaitFlag_Timeout) ? true : false;

	#if defined(OVR_CAPTURE_POSIX) || defined(OVR_CAPTURE_WINDOWS)
		fd_set readfd;
		FD_ZERO(&readfd);
		if(waitflags & WaitFlag_Read)
		{
			FD_SET(m_socket, &readfd);
		}
	#if defined(OVR_CAPTURE_POSIX)
		if(waitflags & WaitFlag_Shutdown)
		{
			FD_SET(m_recvPipe, &readfd);
		}
	#endif

		fd_set writefd;
		FD_ZERO(&writefd);
		if(waitflags & WaitFlag_Write)
		{
			FD_SET(m_socket, &writefd);
		}

		struct timeval timeout = { (time_t)timeoutInSeconds, 0 };
	#if defined(OVR_CAPTURE_POSIX)
		const int sret = ::select((m_socket>m_recvPipe?m_socket:m_recvPipe)+1, &readfd, &writefd, NULL, (useTimeout ? &timeout : NULL));
	#elif defined(OVR_CAPTURE_WINDOWS)
		const int sret = ::select(((int)m_socket)+1, &readfd, &writefd, NULL, (useTimeout ? &timeout : NULL));
		if(sret == SOCKET_ERROR)
			return WaitFlag_Shutdown;
	#endif

		// on error or timeout...
		if(sret == 0)
			return WaitFlag_Timeout;
		else if(sret < 0)
			return 0;

		UInt32 retflags = 0;

		// Ready for read???
		if((waitflags & WaitFlag_Read) && FD_ISSET(m_socket, &readfd))
			retflags |= WaitFlag_Read;

		// Ready for write???
		if((waitflags & WaitFlag_Write) && FD_ISSET(m_socket, &writefd))
			retflags |= WaitFlag_Write;

		// Shutdown() was called???
	#if defined(OVR_CAPTURE_POSIX)
		if((waitflags & WaitFlag_Shutdown) && FD_ISSET(m_recvPipe, &readfd))
			retflags |= WaitFlag_Shutdown;
	#endif

		return retflags;

	#else
		#error Unknown Platform!
		return 0;
	#endif
	}

	Socket::Socket(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		m_socket   = s_invalidSocket;
		m_sendPipe = -1;
		m_recvPipe = -1;
		int p[2];
		if(::pipe(p) == 0)
		{
			m_recvPipe = p[0];
			m_sendPipe = p[1];
		}
	#elif defined(OVR_CAPTURE_WINDOWS)
		m_socket   = s_invalidSocket;
	#else
		#error Unknown Platform!
	#endif
	}

	Socket::~Socket(void)
	{
	#if defined(OVR_CAPTURE_POSIX)
		if(m_socket != s_invalidSocket)
		{
			Shutdown();
			::close(m_socket);
		}
		if(m_recvPipe != -1) ::close(m_recvPipe);
		if(m_sendPipe != -1) ::close(m_sendPipe);
	#elif defined(OVR_CAPTURE_WINDOWS)
		if(m_socket != s_invalidSocket)
		{
			Shutdown();
			::closesocket (m_socket);
		}
	#else
		#error Unknown Platform!
	#endif
	}


	ZeroConfigHost *ZeroConfigHost::Create(UInt16 udpPort, UInt16 tcpPort, const char *packageName)
	{
		OVR_CAPTURE_ASSERT(udpPort > 0);
		OVR_CAPTURE_ASSERT(tcpPort > 0); 
		OVR_CAPTURE_ASSERT(udpPort != tcpPort);
		OVR_CAPTURE_ASSERT(packageName);
		Socket *broadcastSocket = Socket::Create(Socket::Type_Datagram);
		if(broadcastSocket && !broadcastSocket->SetBroadcast())
		{
			broadcastSocket->Release();
			broadcastSocket = NULL;
		}
		if(broadcastSocket)
		{
			return new ZeroConfigHost(broadcastSocket, udpPort, tcpPort, packageName);
		}
		return NULL;
	}

	void ZeroConfigHost::Release(void)
	{
		delete this;
	}

	ZeroConfigHost::ZeroConfigHost(Socket *broadcastSocket, UInt16 udpPort, UInt16 tcpPort, const char *packageName)
	{
		m_socket             = broadcastSocket;
		m_udpPort            = udpPort;
		m_packet.magicNumber = m_packet.s_magicNumber;
		m_packet.tcpPort     = tcpPort;
		StringCopy(m_packet.packageName, packageName, m_packet.s_nameMaxLength);
	}

	ZeroConfigHost::~ZeroConfigHost(void)
	{
		QuitAndWait();
		if(m_socket)
			m_socket->Release();
	}

	void ZeroConfigHost::OnThreadExecute(void)
	{
		SetThreadName("CaptureZeroCfg");

		const SocketAddress addr = SocketAddress::Broadcast(m_udpPort);
		while(!QuitSignaled())
		{
			if(!m_socket->SendTo(&m_packet, sizeof(m_packet), addr))
			{
				// If an error occurs, just abort the broadcast...
				break;
			}
			// Sleep for 650ms between broadcasts. This should mean we get 3 broadcasts every 2 seconds.
			ThreadSleepMilliseconds(650);
		}
		m_socket->Shutdown();
	}

} // namespace Capture
} // namespace OVR
