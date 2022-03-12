/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef ENGINE_NET_H
#define ENGINE_NET_H

#include <cstdlib>

#include "tier0/platform.h"
#include "netadr.h"

// Draw client info
void		SCR_DrawFPS( void );
void		SCR_NetGraph( void );

/**
*	Maximum size for a fragment buffer.
*/
//TODO: unrelated to actual net constants, remove - Solokiller
const size_t NET_MAX_FRAG_BUFFER = 1400;

#pragma pack(push, 1)
typedef struct SPLITPACKET_t
{
	int netID;
	int sequenceNumber;
	unsigned char packetID;
} SPLITPACKET;
#pragma pack(pop)

#define	MAX_MSGLEN		4000		// max length of a reliable message
#define	MAX_DATAGRAM	4000		// max length of unreliable message

#define MAX_ROUTEABLE_PACKET 1400 // probably should be 0x579 (1401 in decimal)
#define SPLIT_SIZE		(MAX_ROUTEABLE_PACKET - sizeof(SPLITPACKET))

#define NET_HEADER_FLAG_QUERY					-1
#define NET_HEADER_FLAG_SPLITPACKET				-2
#define NET_HEADER_FLAG_COMPRESSEDPACKET		-3

#define MAX_STREAMS			2  

#define HEADER_BYTES	( 8 + MAX_STREAMS * 9 )	

#define	NET_MAX_PAYLOAD			96000

#define	NET_MAX_MESSAGE	PAD_NUMBER( ( NET_MAX_PAYLOAD + HEADER_BYTES ), 16 )

extern sizebuf_t net_message;

const int FRAGMENT_MAX_SIZE = 1024;

typedef struct flowstats_s
{
	// Size of message sent/received
	int size;
	// Time that message was sent/received
	double time;
} flowstats_t;

const int MAX_LATENT = 32;

typedef struct flow_s
{
	// Data for last MAX_LATENT messages
	flowstats_t stats[MAX_LATENT];
	// Current message position
	int current;
	// Time when we should recompute k/sec data
	double nextcompute;
	// Average data
	float kbytespersec;
	float avgkbytespersec;
} flow_t;

typedef struct fragbuf_s
{
	fragbuf_s* next;
	int bufferid;
	sizebuf_t frag_message;
	byte frag_message_buf[FRAGMENT_MAX_SIZE];
	qboolean isfile;
	qboolean isbuffer;
	qboolean iscompressed;
	char filename[MAX_PATH];
	int foffset;
	int size;
} fragbuf_t;

typedef struct fragbufwaiting_s
{
	fragbufwaiting_s* next;
	int fragbufcount;
	fragbuf_t* fragbufs;
} fragbufwaiting_t;

void NET_Config( bool multiplayer );

void NET_Shutdown();

void NET_Init();

#endif //ENGINE_NET_H
