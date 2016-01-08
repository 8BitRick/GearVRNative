/************************************************************************************

PublicHeader:   OVR_Capture.h
Filename    :   OVR_Capture_Socket.h
Content     :   Misc network communication functionality.
Created     :   January, 2015
Notes       : 

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_CAPTURE_SOCKET_H
#define OVR_CAPTURE_SOCKET_H

#include <OVR_Capture.h>
#include <OVR_Capture_Packets.h>
#include "OVR_Capture_Thread.h"

#if defined(OVR_CAPTURE_POSIX)
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <unistd.h>
#endif

#if defined(OVR_CAPTURE_WINDOWS)
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

namespace OVR
{
namespace Capture
{

	class SocketAddress
	{
		friend class Socket;
		public:
			static SocketAddress Any(UInt16 port);
			static SocketAddress Broadcast(UInt16 port);

			SocketAddress(void);

		private:
		#if defined(OVR_CAPTURE_POSIX) || defined(OVR_CAPTURE_WINDOWS)
			struct sockaddr_in m_addr;
		#endif
	};

	class Socket
	{
		public:
			enum Type
			{
				Type_Stream = 0,
				Type_Datagram,
			};
			enum WaitFlag
			{
				WaitFlag_Read      = 1,
				WaitFlag_Write     = 2,
				WaitFlag_Shutdown  = 4,
				WaitFlag_Timeout   = 8,

				WaitFlag_Any       = WaitFlag_Read | WaitFlag_Write | WaitFlag_Shutdown
			};
		public:
			static Socket *Create(Type type);

			void Release(void);

			bool    SetBroadcast(void);
			bool    Bind(const SocketAddress &addr);
			bool    Listen(UInt32 maxConnections);
			Socket *Accept(SocketAddress &addr);
			void    Shutdown(void);
			bool    Send(const void *buffer, UInt32 size);
			bool    SendTo(const void *buffer, UInt32 size, const SocketAddress &addr);
			UInt32  Receive(void *buffer, UInt32 size);
			UInt32  WaitFor(UInt32 waitflags, UInt32 timeoutInSeconds=0);

		private:
			Socket(void);
			~Socket(void);

		private:
		#if defined(OVR_CAPTURE_POSIX)
			typedef int SocketType;
			static const SocketType s_invalidSocket = -1;
			SocketType              m_socket;
			// these pipes are used to wake up sleeping select() calls...
			int                     m_sendPipe;
			int                     m_recvPipe;
		#elif defined(OVR_CAPTURE_WINDOWS)
			typedef SOCKET SocketType;
			static const SocketType s_invalidSocket = INVALID_SOCKET;
			SocketType              m_socket;
		#endif
	};

	class ZeroConfigHost : public Thread
	{
		public:
			static ZeroConfigHost *Create(UInt16 udpPort, UInt16 tcpPort, const char *packageName);

			void Release(void);

		private:
			ZeroConfigHost(Socket *broadcastSocket, UInt16 udpPort, UInt16 tcpPort, const char *packageName);
			~ZeroConfigHost(void);

		private:
			virtual void OnThreadExecute(void);

		private:
			Socket          *m_socket;
			UInt16           m_udpPort;
			ZeroConfigPacket m_packet;
	};

} // namespace Capture
} // namespace OVR

#endif
