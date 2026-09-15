#include "StdAfx.h"
#include "NetLowest.h"
#include "Streams.h"
#include <ws2tcpip.h>
using namespace std;
namespace NNet
{
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CNodeAddress
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNodeAddress::SetInetName( const char *pszHost, int nDefaultPort )
{
	int nIdx, nPort = nDefaultPort;
	memset( &addr, 0, sizeof( addr ) );
	ASSERT( sizeof(addr) >= sizeof( sockaddr_in) );
  sockaddr_in &nameRemote = *(sockaddr_in*)&addr;
	
	nameRemote.sin_family = AF_INET;
	// extract port number from address
	string szAddr = pszHost;
	nIdx = static_cast<int>( szAddr.find( ':' ) );
	if ( nIdx != -1 )
	{
		nPort = atoi( string( szAddr, nIdx + 1 ).c_str() );
		szAddr = string( szAddr, 0, nIdx );
	}
	// determine host
	if ( InetPtonA( AF_INET, szAddr.c_str(), &nameRemote.sin_addr ) != 1 )
	{
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		addrinfo *pInfo = 0;
		if ( getaddrinfo( szAddr.c_str(), 0, &hints, &pInfo ) != 0 || pInfo == 0 )
			return false;
		nameRemote.sin_addr = reinterpret_cast<sockaddr_in*>( pInfo->ai_addr )->sin_addr;
		freeaddrinfo( pInfo );
	}
	nameRemote.sin_port = htons( nPort );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
string CNodeAddress::GetName( bool bResolve ) const
{
  sockaddr_in &nameRemote = *(sockaddr_in*)&addr;
	
	char szHost[NI_MAXHOST] = {};
	const bool bResolved = bResolve && getnameinfo( reinterpret_cast<const sockaddr*>( &nameRemote ), sizeof(nameRemote), szHost, sizeof(szHost), 0, 0, NI_NAMEREQD ) == 0;
	char szBuf[1024];
	if ( !bResolved )
	{
		in_addr &ia = nameRemote.sin_addr;
		sprintf_s( szBuf, sizeof(szBuf), "%i.%i.%i.%i:%i",
			(int) ia.S_un.S_un_b.s_b1,
			(int) ia.S_un.S_un_b.s_b2,
			(int) ia.S_un.S_un_b.s_b3,
			(int) ia.S_un.S_un_b.s_b4,
			(int) ntohs( nameRemote.sin_port ) );
	}
	else
	{
		sprintf_s( szBuf, sizeof(szBuf), "%s:%i",
			szHost,
			(int) ntohs( nameRemote.sin_port ) );
	}
	return szBuf;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CNodeAddressSet::GetAddress( int n, CNodeAddress *pRes ) const
{
	pRes->Clear();
	if ( n < 0 || n >= N_MAX_HOST_HOMES || ips[n] == 0 )
		return false;
	sockaddr_in *p = (sockaddr_in*)pRes->GetSockAddr();
	p->sin_family = AF_INET;
	p->sin_port = nPort;
	p->sin_addr.S_un.S_addr = ips[n];
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLinksManager
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CLinksManager::CLinksManager()
{
	WORD wVersionRequested = MAKEWORD( 2, 2 );
	WSADATA wsaData;
 
	int bRv = WSAStartup( wVersionRequested, &wsaData ) == 0;
	ASSERT( bRv );

	s = INVALID_SOCKET;
	// get host for broadcast addresses formation
	char szHost[1024];
	if ( gethostname( szHost, 1000 ) )
	{
		ASSERT(0);
		return;
	}
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo *pInfo = 0;
	if ( getaddrinfo( szHost, 0, &hints, &pInfo ) != 0 || pInfo == 0 )
 	{
		ASSERT(0);
		return;
	}
	// form addresses
	CNodeAddress addr;
  sockaddr_in &name = *(sockaddr_in*)&addr;
	name.sin_family = AF_INET;
	{
		name.sin_addr = reinterpret_cast<sockaddr_in*>( pInfo->ai_addr )->sin_addr;
		unsigned char bClass = name.sin_addr.S_un.S_un_b.s_b1;
		if ( bClass >= 1 && bClass <= 126 )
		{
			name.sin_addr.S_un.S_un_b.s_b2 = 255;
			name.sin_addr.S_un.S_un_b.s_b3 = 255;
			name.sin_addr.S_un.S_un_b.s_b4 = 255;
		}
		if ( bClass >= 128 && bClass <= 191 )
		{
			name.sin_addr.S_un.S_un_b.s_b3 = 255;
			name.sin_addr.S_un.S_un_b.s_b4 = 255;
		}
		if ( bClass >= 192 && bClass <= 223 )
			name.sin_addr.S_un.S_un_b.s_b4 = 255;
		broadcastAddr = addr;
	}
	freeaddrinfo( pInfo );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CLinksManager::~CLinksManager()
{
	Finish();
	WSACleanup();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::Start( int nPort )
{
	Finish();
	s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( s == INVALID_SOCKET )
		return false;
	sockaddr_in name;
	memset( &name, 0, sizeof(name) );
	name.sin_family = AF_INET;
	name.sin_addr.S_un.S_addr = INADDR_ANY;
	name.sin_port = htons( nPort );
	if ( nPort > 0 )
	{
		if ( ::bind( s, (sockaddr*)&name, sizeof( name ) ) != 0 )
		{
			closesocket( s );
			s = INVALID_SOCKET;
			return false;
		}
	}
	DWORD	dwOpt = 1;
	ioctlsocket( s, FIONBIO, &dwOpt ); // no block
	setsockopt( s, SOL_SOCKET, SO_BROADCAST, (const char*)&dwOpt, 4 );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CLinksManager::Finish()
{
	if ( s != INVALID_SOCKET )
		closesocket( s );
	s = INVALID_SOCKET;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::MakeBroadcastAddr( CNodeAddress *pRes, int nPort ) const
{
	*pRes = broadcastAddr;
  sockaddr_in &name = *(sockaddr_in*)&pRes->addr;
	name.sin_port = htons( nPort );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::IsLocalAddr( const CNodeAddress &test ) const
{
	const sockaddr_in &nt = *(sockaddr_in*)&test.addr;
	if ( nt.sin_addr.S_un.S_addr == 0x0100007f )
		return true;
	//for ( int i = 0; i < broadcastAddr.size(); ++i )
	{
		const CNodeAddress &broad = broadcastAddr;//[ i ];
	  const sockaddr_in &nb = *(sockaddr_in*)&broad.addr;
		DWORD dwB = nb.sin_addr.S_un.S_addr;
		DWORD dwT = nt.sin_addr.S_un.S_addr;
		DWORD dwMask = 0;
		for ( int k = 3; k >= 0; k-- )
		{
			DWORD dwTestMask = 0xFF << k*8;
			if ( (dwB & dwTestMask) == dwTestMask )
				dwMask |= dwTestMask;
			else
				break;
		}
		dwMask = ~dwMask;
		bool bTest = ( dwB & dwMask ) == ( dwT & dwMask );
		if ( bTest )
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CRAP{
extern int nTrafficPackets;
extern int nTrafficTotalSize;
// CRAP}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef NET_TEST_APPLICATION
bool bEmulateWeakNetwork = false;
float fLostRate = 0.7f;
struct SPacket
{
	CNodeAddress addr;
	CMemoryStream pkt;
};
#endif
bool CLinksManager::Send( const CNodeAddress &dst, CMemoryStream &pkt ) const
{
#ifdef NET_TEST_APPLICATION
	static vector<SPacket> pktQueue;
	if ( bEmulateWeakNetwork )
	{
		if ( rand() <= RAND_MAX * fLostRate )
			return true;
		pktQueue.emplace_back();
		pktQueue.back().addr = dst;
		pktQueue.back().pkt = pkt;
		while ( pktQueue.size() > 3 )
		{
			int nPkt = rand() % pktQueue.size();
			SPacket &p = pktQueue[nPkt];
			int nSize = p.pkt.GetSize();
			int nRv = sendto( s, (const char*)p.pkt.GetBuffer(), nSize, 0, &p.addr.addr, sizeof( p.addr.addr ) );
			pktQueue.erase( pktQueue.begin() + nPkt );
		}
		return true;
	}
#endif
	int nSize = pkt.GetSize();
	int nRv = sendto( s, (const char*)pkt.GetBuffer(), nSize, 0, &dst.addr, sizeof( dst.addr ) );

	// CRAP{
	if ( nRv >= 0 )
	{
		++nTrafficPackets;
		nTrafficTotalSize += nRv;
	}
	// CRAP}
//	printf( "send to %s\n", dst.GetFastName().c_str() );

	return nRv == nSize;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::Recv( CNodeAddress *pSrc, CMemoryStream *pPkt ) const
{
	ASSERT( pSrc );
	ASSERT( pPkt );
	int nAddrSize;
	pPkt->Seek( 2048 );
	nAddrSize = sizeof( pSrc->addr );
	int nRes = recvfrom( s, (char*)pPkt->GetBufferForWrite(), 2048, 0, &pSrc->addr, &nAddrSize );
	if ( nRes >= 0 )
	{
		pSrc->addr.sa_family = AF_INET;       // somehow this gets spoiled on win2k
		memset( pSrc->addr.sa_data + 6, 0, 8 );
		pPkt->SetSize( nRes );
	}

	// CRAP{
	if ( nRes >=0 )
	{
		++nTrafficPackets;
		nTrafficTotalSize += nRes;
	}
	// CRAP}
//	if ( nRes >= 0 )
//		printf( "rect from %s\n", pSrc->GetFastName().c_str() );

	return nRes >= 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CLinksManager::GetSelfAddress( CNodeAddressSet *pRes ) const
{
	pRes->Clear();
	sockaddr_in addr;
	int nBufLeng = sizeof(sockaddr_in);
	if ( getsockname( s, (sockaddr*)&addr, &nBufLeng ) != 0 )
		return false;
	pRes->nPort = addr.sin_port;
	char szHostName[10000];
	gethostname( szHostName, 9999 );
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo *pInfo = 0;
	if ( getaddrinfo( szHostName, 0, &hints, &pInfo ) != 0 || pInfo == 0 )
		return false;
	int k = 0;
	for ( addrinfo *p = pInfo; p != 0 && k < N_MAX_HOST_HOMES; p = p->ai_next )
	{
		if ( p->ai_family == AF_INET && p->ai_addrlen >= sizeof(sockaddr_in) )
			pRes->ips[k++] = reinterpret_cast<sockaddr_in*>( p->ai_addr )->sin_addr.S_un.S_addr;
	}
	freeaddrinfo( pInfo );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
SOCKET CLinksManager::GetSocket() const
{	
	return s;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}
