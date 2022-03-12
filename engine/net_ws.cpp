#include "quakedef.h"
#include "host.h"
#include "gl_draw.h"
#include "net_chan.h"
#include "client.h"
#include "ws2def.h"
#include <WinSock2.h>

// As reHLDS says - this is the original statement.
#define INV_SOCK 0

#define	MAX_LOOPBACK 4

SOCKET ip_sockets[NS_MAX] = { INV_SOCK, INV_SOCK, INV_SOCK };

sizebuf_t net_message;

typedef struct
{
	byte	data[NET_MAX_MESSAGE];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get;
	int			send;
} loopback_t;

static loopback_t	loopbacks[2];

extern cvar_t cl_showfps;

// https://github.com/dreamstalker/rehlds/blob/2f0a402f9d14f05463a3c1457694fe0d57a61012/rehlds/engine/net_ws.cpp#L126
void NetadrToSockadr(const netadr_t* a, struct sockaddr* s)
{
	Q_memset(s, 0, sizeof(*s));

	auto s_in = (sockaddr_in*)s;

	switch (a->type)
	{
	case NA_BROADCAST:
		s_in->sin_family = AF_INET;
		s_in->sin_addr.s_addr = INADDR_BROADCAST;
		s_in->sin_port = a->port;
		break;
	case NA_IP:
		s_in->sin_family = AF_INET;
		s_in->sin_addr.s_addr = *(int*)&a->ip;
		s_in->sin_port = a->port;
		break;
	default:
		break;
	}
}

void NET_Config( bool multiplayer )
{
	//TODO: implement - Solokiller
}

void NET_Init()
{
	//TODO: implement - Solokiller
}

void NET_Shutdown()
{
	//TODO: implement - Solokiller
}

char* NET_ErrorString(int __errnum) {
	return strerror(__errnum);
}

char* NET_AdrToString(netadr_t a)
{
	char* s;
	Q_memset(s, 0, 64);
	if (a.type == NA_LOOPBACK) {
		snprintf(s, 64, "loopback");
	} else if (a.type == NA_IP) {
		snprintf(s, 64, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2],
			a.ip[3], (a.port >> 8 | a.port << 8));
	}
	return s;
}

void SockadrToNetadr(sockaddr* s, netadr_t* a)
{
	if (s->sa_family != AF_INET) {
		return;
	}
	a->type = NA_IP;
	*(int*)&a->ip = ((struct sockaddr_in*)s)->sin_addr.s_addr;
	a->port = ((struct sockaddr_in*)s)->sin_port;
	// I don't think there should be return statement. - xWhitey
	//return;
}

int NET_SendTo(bool verbose, SOCKET s, const char FAR* buf, int len, int flags, const struct sockaddr FAR* to, int tolen)
{
	return sendto(s, buf, len, flags, to, tolen);
}

void NET_SendLoopPacket(netsrc_t sock, int length, void* data, netadr_t to)

{
	loopback_t* loop = &loopbacks[sock ^ 1];

	int i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	// Thanks to reHLDS for this check.
	if (sizeof(loop->msgs[i].data) < length) {
		Sys_Error("%s: data size is bigger than message storage size", __func__);
	}

	Q_memcpy(loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

int NET_SendLong(netsrc_t sock, SOCKET s, const char* buf, int len, const struct sockaddr* to, int tolen, int flags) {
	static long gSequenceNumber = 1;

	if (sock == NS_SERVER && len > 1401) {
		gSequenceNumber = gSequenceNumber + 1;
		if (gSequenceNumber < 0) {
			gSequenceNumber = 1;
		}

		char packet[MAX_ROUTEABLE_PACKET];
		SPLITPACKET* pPacket = (SPLITPACKET*)packet;
		pPacket->netID = NET_HEADER_FLAG_SPLITPACKET;
		pPacket->sequenceNumber = gSequenceNumber;
		int packetNumber = 0;
		int totalSent = 0;
		int packetCount = (len + SPLIT_SIZE - 1) / SPLIT_SIZE;

		while (len > 0)
		{
			int size = Q_min(int(SPLIT_SIZE), len);

			pPacket->packetID = (packetNumber << 8) + packetCount;

			memcpy(packet + sizeof(SPLITPACKET), buf + (packetNumber * SPLIT_SIZE), size);

			int ret = NET_SendTo(false, s, packet, size + sizeof(SPLITPACKET), flags, to, tolen);
			if (ret < 0)
			{
				return ret;
			}

			if (ret >= size)
			{
				totalSent += size;
			}

			len -= size;
			packetNumber++;

			// This can be 15, but if you have a lot of packets, that will pause the server for a long time - Source Beta 2003
			Sleep(1);

			/*if (net_showpackets.GetInt())
			{
				netadr_t adr;
				memset(&adr, 0, sizeof(adr));

				SockadrToNetadr((struct sockaddr*)to, &adr);

				Con_Printf("Split packet %i/%i (size %i dest:  %s:%s)\n",
					packetNumber,
					packetCount,
					originalSize,
					NET_GetPlayerNameForAdr(&adr),
					NET_AdrToString(adr));
			}*/
		}

		return totalSent;
	}
	else
	{
		int nSend = 0;
		nSend = NET_SendTo(true, s, buf, len, flags, to, tolen);
		return nSend;
	}
}

void NET_SendPacket(netsrc_t sock, int length, void* data, netadr_t to)
{
	SOCKET net_socket;
	if (to.type == NA_LOOPBACK)
	{
		NET_SendLoopPacket(sock, length, data, to);
		return;
	}
	if (to.type == NA_BROADCAST)
	{
		net_socket = ip_sockets[sock];
		if (net_socket == INV_SOCK) // Can be also like "if (!net_socket)"
			return;
	}
	else if (to.type == NA_IP)
	{
		net_socket = ip_sockets[sock];
		if (net_socket == INV_SOCK) // Can be also like "if (!net_socket)"
			return;
	}
	// Where is IPX type? - xWhitey
	else
	{
		Sys_Error("NET_SendPacket: bad address type");
		return;
	}

	struct sockaddr addr;
	NetadrToSockadr(&to, &addr);

	//NET_SendLong(netsrc_t sock, SOCKET s, const char* buf, int len, const struct sockaddr* to, int tolen, int flags)
	//reHLDS has flags param before sockaddr ptr named to
	int ret = NET_SendLong(sock, net_socket, (const char*)data, length, &addr, sizeof(addr), 0);
	if (ret == -1)
	{
		int err;

		err = GetLastError();

		if (err == WSAEWOULDBLOCK)
			return;

		if (err == WSAECONNRESET)
			return;

		if ((err == WSAEADDRNOTAVAIL) && (to.type == NA_BROADCAST))
			return;

		if (cls.state == ca_dedicated)
		{
			Con_Printf("NET_SendPacket ERROR: %s\n", NET_ErrorString(err));
		}
		else
		{
			if (err == WSAEADDRNOTAVAIL)
			{
				Con_DPrintf("NET_SendPacket Warning: %s : %s\n", NET_ErrorString(err), NET_AdrToString(to));
			}
			else
			{
				Sys_Error("NET_SendPacket ERROR: %s\n", NET_ErrorString(err));
			}
		}
	}
}

void NET_DrawString( int x, int y, int font, float r, float g, float b, char* fmt, ... )
{
	static char string[1024];
	va_list varargs;

	va_start(varargs, fmt);
	vsnprintf(string, sizeof(string), fmt, varargs);
	va_end(varargs);

	Draw_SetTextColor(r, g, b);
	Draw_String(x, y, string);
}

/*
==================
SCR_NetGraph

Visualizes data flow
==================
*/
void SCR_NetGraph( void )
{
	// TODO: Implement
}

static double rolling_fps;

void SCR_DrawFPS( void )
{
	if (!cl_showfps.value)
		return;

	if (host_frametime <= 0.0)
		return;

	rolling_fps = 0.6 * rolling_fps + host_frametime * 0.4;
	NET_DrawString(2, 2, 0, 1, 1, 1, "%d fps", (int)floor(1.0 / rolling_fps));
}