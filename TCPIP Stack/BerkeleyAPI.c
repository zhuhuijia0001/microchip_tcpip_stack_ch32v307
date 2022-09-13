/*********************************************************************
 *
 *     Berekely Socket Distribution API Source File
 *
 *********************************************************************
 * FileName:        BerkeleyAPI.c
 * Description:     Berkeley Socket Distribution(BSD) APIs for Microchip TCPIP Stack
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32
 * Compiler:        Microchip C32 v1.00 or higher
 *					Microchip C30 v3.01 or higher
 *					Microchip C18 v3.20 or higher
 *					HI-TECH PICC-18 STD 9.50PL3 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * Copyright (C) 2008 Microchip Technology Inc.  All rights 
 * reserved.
 *
 * Microchip licenses to you the right to use, modify, copy, and 
 * distribute: 
 * (i)  the Software when embedded on a Microchip microcontroller or 
 *      digital signal controller product ("Device") which is 
 *      integrated into Licensee's product; or
 * (ii) ONLY the Software driver source files ENC28J60.c and 
 *      ENC28J60.h ported to a non-Microchip device used in 
 *      conjunction with a Microchip ethernet controller for the 
 *      sole purpose of interfacing with the ethernet controller. 
 *
 * You should refer to the license agreement accompanying this 
 * Software for additional information regarding your rights and 
 * obligations.
 *
 * THE SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT 
 * WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT 
 * LIMITATION, ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A 
 * PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL 
 * MICROCHIP BE LIABLE FOR ANY INCIDENTAL, SPECIAL, INDIRECT OR 
 * CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF 
 * PROCUREMENT OF SUBSTITUTE GOODS, TECHNOLOGY OR SERVICES, ANY CLAIMS 
 * BY THIRD PARTIES (INCLUDING BUT NOT LIMITED TO ANY DEFENSE 
 * THEREOF), ANY CLAIMS FOR INDEMNITY OR CONTRIBUTION, OR OTHER 
 * SIMILAR COSTS, WHETHER ASSERTED ON THE BASIS OF CONTRACT, TORT 
 * (INCLUDING NEGLIGENCE), BREACH OF WARRANTY, OR OTHERWISE.
 *
 * Author               Date    	Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Aseem Swalah         4/3/08  	Original
 ********************************************************************/

#include "TCPIP Stack/TCPIP.h"

#if defined(STACK_USE_BERKELEY_API)


#pragma udata BSD_uRAM
static struct BSDSocket  BSDSocketArray[BSD_SOCKET_COUNT];
#pragma udata

static WORD gAutoPortNumber = 1024;
static char sockCount = BSD_SOCKET_COUNT;

/*****************************************************************************
  Function:
	void BerkeleySocketInit(void)

  Summary:
	Initializes the Berkeley socket structure array.

  Description:
	This function initializes the Berkeley socket array. This function should
	be called before any BSD socket call.

  Precondition:
	None.

  Parameters:
	None.

  Returns:
	None

  Remarks:
	None.
  ***************************************************************************/
void BerkeleySocketInit(void)
{
    BYTE s;
    struct BSDSocket        *pSocket;
    
    for ( s = 0; s < BSD_SOCKET_COUNT; s++ )
    {
        pSocket             = (struct BSDSocket *)&BSDSocketArray[s];
        pSocket->SocketType = SOCK_DGRAM;
        pSocket->bsdState   = SKT_CLOSED;
        pSocket->backlog    = 5;
        pSocket->SocketID   = INVALID_SOCKET;
        pSocket->localPort  = 0;
    }
}

/*****************************************************************************
  Function:
	SOCKET socket( int af, int type, int protocol )

  Summary:
	This function creates a new Berkeley socket.

  Description:
	This function creates a new BSD socket for the microchip
	TCPIP stack. The return socket descriptor is used for the subsequent
	BSD operations.

  Precondition:
	BerkeleySocketInit function should be called.

  Parameters:
	af - address family - AF_INET.
	type - socket type SOCK_DGRAM or SOCK_STREAM.
	protocol - IP protocol IPPROTO_UDP or IPPROTO_TCP.

  Returns:
	New socket descriptor. INVALID_SOCKET in case of error.

  Remarks:
	None.
  ***************************************************************************/
SOCKET socket( int af, int type, int protocol )
{
    struct BSDSocket *socket = BSDSocketArray;
    SOCKET s;

    if( af != AF_INET )
        return INVALID_SOCKET;

    for( s = 0; s < BSD_SOCKET_COUNT; s++,socket++ )
    {
        if( socket->bsdState != SKT_CLOSED ) //socket in use
            continue;

        socket->SocketType = type;
      
        if( type == SOCK_DGRAM && protocol == IPPROTO_UDP )
        {
            socket->bsdState = SKT_CREATED;
            return s;
        }
        else if( type == SOCK_STREAM && protocol == IPPROTO_TCP )
        {
            socket->bsdState = SKT_CREATED;
            return s;
        }
        else
            return INVALID_SOCKET;
    }

    return INVALID_SOCKET;
}

/*****************************************************************************
  Function:
	int bind( SOCKET s, const struct sockaddr* name, int namelen )

  Summary:
	This function assigns a name to the socket descriptor.

  Description:
	The bind function assigns a name to an unnamed socket. The
    name represents the local address of the communication
    endpoint. For sockets of type SOCK_STREAM, the name of the
    remote endpoint is assigned when a connect or accept function
    is executed.

  Precondition:
	socket function should be called.

  Parameters:
	s - Socket descriptor returned from a previous call to socket.
	name - pointer to the the sockaddr structure containing the
	local address of the socket.
	namelen - length of the sockaddr structure.

  Returns:
	If bind is successful, a value of 0 is returned. A return
    value of SOCKET_ERROR indicates an error.

  Remarks:
	None.
  ***************************************************************************/
int bind( SOCKET s, const struct sockaddr* name, int namelen )
{
    struct BSDSocket *socket;
    struct sockaddr_in *local_addr;
    WORD lPort;
        
    if( s >= BSD_SOCKET_COUNT )
        return SOCKET_ERROR;

    socket = &BSDSocketArray[s];

    if( socket->bsdState != SKT_CREATED ) //only work with recently created socket
        return SOCKET_ERROR;

    if( (unsigned int)namelen < sizeof(struct sockaddr_in) )
        return SOCKET_ERROR;

    local_addr = (struct sockaddr_in *)name;

    lPort = local_addr->sin_port;
    if( lPort == 0 ) //pick a port
    {
        lPort = gAutoPortNumber++;
        if(gAutoPortNumber == 5000) // reset the port numbers
            gAutoPortNumber = 1024;
    }

    socket->localPort = lPort;
    socket->bsdState  = SKT_BOUND;
    return 0; //success
}

/*****************************************************************************
  Function:
	int listen( SOCKET s, int backlog )

  Summary:
	The listen function sets the specified socket in a listen mode

  Description:
	This function sets the specified socket in a listen
    mode. Calling the listen function indicates that the
    application is ready to accept connection requests arriving
    at a socket of type SOCK_STREAM. The connection request is
    queued (if possible) until accepted with an accept function.
    The backlog parameter defines the maximum number of pending
    connections that may be queued.

  Precondition:
	bind function should be called.

  Parameters:
	s - Socket identifier.
	backlog - maximum number of connection requests that can be queued.
	
  Returns:
	returns 0 on success, else return SOCKET_ERROR.

  Remarks:
	None.
  ***************************************************************************/
int listen( SOCKET s, int backlog )
{
    struct BSDSocket *ps;

    if( s >= BSD_SOCKET_COUNT )
        return SOCKET_ERROR;

    ps = &BSDSocketArray[s];

    if ((ps->bsdState == SKT_BOUND) && (ps->SocketType == SOCK_STREAM))
    {
      // put this socket in listen mode
       ps->bsdState = SKT_LISTEN;
       ps->backlog   = backlog;
       return 0;
    }

   return SOCKET_ERROR; 
}


/*****************************************************************************
  Function:
	SOCKET accept( SOCKET s, struct sockaddr* addr, int* addrlen )

  Summary:
	This function accepts connection requests queued for a listening socket.

  Description:
	The accept function is used to accept connection requests
	queued for a listening socket. If a connection request is
    pending, accept removes the request from the queue, and a new
    socket is created for the connection. The original listening
    socket remains open and continues to queue new connection
    requests. The socket must be a SOCK_STREAM type socket.

  Precondition:
	listen function should be called.

  Parameters:
	s - Socket descriptor returned from a previous call to
    socket. must be bound to a local name and in listening mode.
    addr - Optional pointer to a buffer that receives the address
    of the connecting entity.
    addrlen - Optional pointer to an integer that contains the
    length of the address addr
    
  Returns:
	If the accept function succeeds, it returns a non-negative
    integer that is a descriptor for the accepted socket.
    Otherwise, the value INVALID_SOCKET is returned.

  Remarks:
	None.
  ***************************************************************************/
SOCKET accept( SOCKET s,
               struct sockaddr* addr,
               int* addrlen )
{
    struct BSDSocket *pListenSock;
    struct BSDSocket *clientSocket; 
    SOCKET clientSockID;
    SOCKET_INFO *remoteSockInfo;
    struct sockaddr_in *addrRemote;
   
    if( s >= BSD_SOCKET_COUNT )
        return INVALID_SOCKET;

    pListenSock = &BSDSocketArray[s]; /* Get the pointer to listening socket */

    if ( pListenSock->bsdState < SKT_LISTEN )
        return INVALID_SOCKET;

    if(pListenSock->SocketID == INVALID_SOCKET)
    {
        clientSockID = TCPOpen(0, TCP_OPEN_SERVER, pListenSock->localPort, TCP_PURPOSE_BERKELEY_SERVER);
        if(clientSockID == INVALID_SOCKET)
        {
            return INVALID_SOCKET;
        }
        pListenSock->SocketID = s;
        pListenSock->backlog--;
        remoteSockInfo = TCPGetRemoteInfo(clientSockID);
        addrRemote = (struct sockaddr_in *)addr;
        addrRemote->sin_addr.S_un.S_addr = remoteSockInfo->remote.IPAddr.Val;
        addrRemote->sin_port = remoteSockInfo->remotePort.Val;
      
        for(sockCount = 0; sockCount < BSD_SOCKET_COUNT; sockCount++)
        {
            clientSocket = &BSDSocketArray[(BYTE)sockCount]; /* Get the pointer to client socket */
            if( clientSocket->bsdState != SKT_CLOSED ) //socket in use
                continue;
         
            clientSocket->SocketID   = clientSockID;
            clientSocket->bsdState   = SKT_EST;
            clientSocket->SocketType = pListenSock->SocketType;
            clientSocket->localPort  = pListenSock->localPort;
            clientSocket->remotePort = addrRemote->sin_port;
            clientSocket->remoteIP   = addrRemote->sin_addr.S_un.S_addr;
            break;
        }
    }

    if(sockCount == BSD_SOCKET_COUNT) // number of sockets exceeds the allotted number of sockets.
        return INVALID_SOCKET;
    
    if(!TCPIsConnected(BSDSocketArray[(BYTE)sockCount].SocketID))
    {
        pListenSock->bsdState = SKT_IN_PROGRESS;
        return INVALID_SOCKET;
    }
   
    pListenSock->bsdState = SKT_EST;
    if(pListenSock->backlog)//restrict the number of connection to the number specified while listening for the sockets
        pListenSock->SocketID = INVALID_SOCKET;
    return sockCount;
}

/*****************************************************************************
  Function:
	int connect( SOCKET s, struct sockaddr* name, int namelen )

  Summary:
	This function connects to the peer communications end point.

  Description:
	The connect function assigns the address of the peer
    communications endpoint. For stream sockets, connection is
    established between the endpoints. For datagram sockets, an
    address filter is established between the endpoints until
    changed with another connect() function.

  Precondition:
	socket function should be called.

  Parameters:
	s - Socket descriptor returned from a previous call to socket.
    name - pointer to the sockaddr structure containing the
    peer address and port number.
    namelen - length of the sockaddr structure.
    
  Returns:
	If the connect() function succeeds, it returns 0. Otherwise,
    the value SOCKET_ERROR is returned to indicate an error
    condition. For stream based socket, if the connection is not
    established yet, connect returns SOCKET_CNXN_IN_PROGRESS.

  Remarks:
	None.
  ***************************************************************************/
int connect( SOCKET s, struct sockaddr* name, int namelen )
{
    struct BSDSocket *pSock;
    struct sockaddr_in *addr;
    DWORD remoteIP;
    WORD remotePort;
 
    if( s >= BSD_SOCKET_COUNT )
        return SOCKET_ERROR;

    pSock = &BSDSocketArray[s];

    if( pSock->bsdState < SKT_CREATED )
        return SOCKET_ERROR;

    if( (unsigned int)namelen < sizeof(struct sockaddr_in))
        return SOCKET_ERROR;

    addr = (struct sockaddr_in *)name;
    remotePort 	= addr->sin_port;		
	remoteIP 	= addr->sin_addr.S_un.S_addr;
   
    if( remoteIP == 0 || remotePort == 0 )
        return SOCKET_ERROR;
        
    if( pSock->SocketType == SOCK_STREAM )
    {
       switch(pSock->bsdState)
       {
            case SKT_EST:
                return 0; // already established

            case SKT_IN_PROGRESS:
                if(!TCPIsConnected(pSock->SocketID))
                {
                    pSock->bsdState = SKT_IN_PROGRESS;
                    return SOCKET_CNXN_IN_PROGRESS;
                }

                if(pSock->SocketID != INVALID_SOCKET)
                {
                    pSock->bsdState = SKT_EST;
                    return 0; //success
                }

                break;
      
            case SKT_CREATED:
            case SKT_BOUND:            
                pSock->SocketID = TCPOpen(remoteIP, TCP_OPEN_IP_ADDRESS, remotePort, TCP_PURPOSE_BERKELEY_CLIENT);
                if(pSock->SocketID == INVALID_SOCKET)
                    return SOCKET_ERROR;
            
                if(!TCPIsConnected(pSock->SocketID))
                {
                    pSock->bsdState = SKT_IN_PROGRESS;
                    return SOCKET_CNXN_IN_PROGRESS;
                }
            
                if(pSock->SocketID != INVALID_SOCKET)
                {
                    pSock->bsdState = SKT_EST;
                    return 0; //success
                }

            default:
                return SOCKET_ERROR;
        }
    }
    else
    {   
        // UDP: remote port is used as a filter only. Need to call connect when using send/recv
        // calls. no need to call 'connect' if using sendto/recvfrom calls.
        pSock->remotePort = remotePort;
        pSock->remoteIP = remoteIP;
        pSock->bsdState = SKT_READY;
        return 0; //success
    }
   return SOCKET_ERROR;
}

/*****************************************************************************
  Function:
	int send( SOCKET s, const char* buf, int len, int flags )
	
  Summary:
	The send function is used to send outgoing data on an already
    connected socket.

  Description:
	The send function is used to send outgoing data on an already
    connected socket. This function is used to send a reliable,
    ordered stream of data bytes on a socket of type SOCK_STREAM
    but can also be used to send datagrams on a socket of type SOCK_DGRAM.

  Precondition:
	connect function should be called for TCP and UDP sockets.
	Server side, accept function should be called.

  Parameters:
	s - Socket descriptor returned from a previous call to socket.
    buf - application data buffer containing data to transmit.
    len - length of data in bytes.
    flags - message flags. Currently this field is not supported.
  
  Returns:
	On success, send returns number of bytes sent. In case of
    error, returns SOCKET_ERROR. a zero indicates no data send.

  Remarks:
	None.
  ***************************************************************************/
int send( SOCKET s, const char* buf, int len, int flags )
{
    struct BSDSocket *socket;
    WORD size;
    NODE_INFO remoteInfo;
    static DWORD startTick;

    if( s >= BSD_SOCKET_COUNT )
        return SOCKET_ERROR;

    socket = &BSDSocketArray[s];

    if( socket->bsdState < SKT_BOUND )
        return SOCKET_ERROR;

    if(!TCPIsConnected(socket->SocketID))
        return SOCKET_ERROR; //Indicates the socket is disconnected.
      
    if(socket->SocketType == SOCK_STREAM) //TCP
    {
        // Make certain the socket can be written to
        if(TCPIsPutReady(socket->SocketID) > 0)
        {
            return TCPPutArray(socket->SocketID, (BYTE*)buf, len);
        }
        else
        {
            return SOCKET_ERROR;
        }
    }
    else if(socket->SocketType == SOCK_DGRAM) //UDP
    {
        if((socket->bsdState >= SKT_READY) && (socket->bsdState != SKT_EST))//making sure that connect function is called
        {
            if(socket->bsdState != SKT_ARP_VERIFY)
            {
                remoteInfo.IPAddr.Val = socket->remoteIP;
                ARPResolve(&remoteInfo.IPAddr);
                startTick = TickGet();
                socket->bsdState = SKT_ARP_VERIFY;
            }
            else if(socket->bsdState == SKT_ARP_VERIFY)
            {
                // Wait for the MAC address to finish being obtained
                remoteInfo.IPAddr.Val = socket->remoteIP;
                if(!ARPIsResolved(&remoteInfo.IPAddr, &remoteInfo.MACAddr))
                {
                    // Time out if too much time is spent in this state
    			    if(TickGet()- startTick > 1*TICK_SECOND)
    			    {
                        // Retransmit ARP request
                        socket->bsdState = SKT_ARP_RESOLVE;
    			    }
                 }
                 socket->SocketID = UDPOpen(socket->localPort, &remoteInfo, socket->remotePort);
                 socket->bsdState = SKT_EST;
            } 
        }
        
        if(socket->bsdState == SKT_EST)
        {
            if(UDPIsPutReady(socket->SocketID) > 0)
            {
                size = UDPPutArray((BYTE*)buf, len);
                UDPFlush();
                return size;
            }
        }
        else
        {
            return SOCKET_ERROR;
        }
    }
    return SOCKET_ERROR;
}

/*****************************************************************************
  Function:
	int sendto(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)

  Summary:
	This function used to send the data for both connection oriented and connection-less
	sockets.

  Description:
	The sendto function is used to send outgoing data on a socket.
    The destination address is given by to and tolen. Both 
    Datagram and stream sockets are supported.

  Precondition:
	socket function should be called.

  Parameters:
	s - Socket descriptor returned from a previous call to socket.
    buf - application data buffer containing data to transmit.
    len - length of data in bytes.
    flags - message flags. Currently this field is not supported.
    to - pointer to the the sockaddr structure containing the
    destination address.
    tolen - length of the sockaddr structure.
  
  Returns:
	On success, sendto returns number of bytes sent. In case of
    error returns SOCKET_ERROR

  Remarks:
	None.
  ***************************************************************************/
int sendto( SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen )
{
    struct BSDSocket *socket;
    struct sockaddr_in *addr;
    int size=0;
    NODE_INFO remoteInfo;
    static DWORD startTick;

    socket = &BSDSocketArray[s];
    addr = (struct sockaddr_in *)to;
    socket->remotePort = addr->sin_port;
    socket->remoteIP = addr->sin_addr.s_addr;
    
    if(socket->SocketType == SOCK_DGRAM) //UDP
    {
        if(socket->bsdState != SKT_EST)
        {
            if((addr == NULL) || (addr->sin_addr.s_addr == IP_ADDR_ANY) || (tolen == 0)) //broadcast
            {
                socket->SocketID = UDPOpen(socket->localPort, NULL, socket->remotePort);
            }
            else
            {
                if(socket->bsdState != SKT_ARP_VERIFY)
                {
                    remoteInfo.IPAddr.Val = socket->remoteIP;
                    ARPResolve(&remoteInfo.IPAddr);
                    startTick = TickGet();
                    socket->bsdState = SKT_ARP_VERIFY;
                }
                else if(socket->bsdState == SKT_ARP_VERIFY)
                {
                    // Wait for the MAC address to finish being obtained
                    remoteInfo.IPAddr.Val = socket->remoteIP;
                    if(!ARPIsResolved(&remoteInfo.IPAddr, &remoteInfo.MACAddr))
                    {
                        // Time out if too much time is spent in this state
    			        if(TickGet()- startTick > 1*TICK_SECOND)
    			        {
                            // Retransmit ARP request
                            socket->bsdState = SKT_ARP_RESOLVE;
    			        }
                    }
                    socket->SocketID = UDPOpen(socket->localPort, &remoteInfo, socket->remotePort);
                    socket->bsdState = SKT_EST;
                }
            }
        }
        
        if(socket->bsdState == SKT_EST)
        {
            if(UDPIsPutReady(socket->SocketID) > 0)
            {
                size = UDPPutArray((BYTE*)buf, len);
                UDPFlush();
                return size;
            }
        }
    }
    else if(socket->SocketType == SOCK_STREAM) //TCP will send to the already established socket.
    {
        return send(s, buf, len, 0);
    }
    return SOCKET_ERROR;
}

/*****************************************************************************
  Function:
	int recv( SOCKET s, char* buf, int len, int flags )

  Summary:
	The recv() function is used to receive incoming data that has
    been queued for a socket.

  Description:
	The recv() function is used to receive incoming data that has
    been queued for a socket. This function can be used with both 
    datagram and stream socket. If the available data is too large
    to fit in the supplied application buffer buf, the data is
    buffered internally so the application can retreive all data
    by multiple calls of recv.

  Precondition:
	connect function should be called for TCP and UDP sockets.
	Server side, accept function should be called.

  Parameters:
	s - Socket descriptor returned from a previous call to socket.
    buf - application data receive buffer.
    len - buffer length in bytes.
    flags - message flags. value zero gives the data and one
    gives the count of RX bytes waiting. value 2 gives the status 

  Returns:
	If recv is successful, the number of bytes copied to
    application buffer buf is returned. A value of zero indicates
    no data available. A return value of SOCKET_ERROR (-1)
    indicates an error condition. A return value of SOCKET_DISCONNECTED
    indicates the connection no longer exists.

  Remarks:
	None.
  ***************************************************************************/
int recv( SOCKET s, char* buf, int len, int flags )
{
    struct BSDSocket *socket;
    NODE_INFO remoteInfo;
    static DWORD startTick;
    
    if( s >= BSD_SOCKET_COUNT )
        return SOCKET_ERROR;

    socket = &BSDSocketArray[s];

    if( socket->bsdState < SKT_BOUND )
            return SOCKET_ERROR;
   
    if(socket->SocketType == SOCK_STREAM) //TCP
    {
        if(flags == 0)
        {
            if(TCPIsGetReady(socket->SocketID))
            {
                return TCPGetArray(socket->SocketID, (BYTE*)buf, len);
            }
        }
        else if(flags == 1) // get the count of RX bytes waiting.
        {
            return TCPIsGetReady(socket->SocketID);
        }
        else if(flags == 2) // get the status.
        {
            if(!TCPIsConnected(socket->SocketID))
            {
                return SOCKET_DISCONNECTED;
            }
        } 
    }
    else if(socket->SocketType == SOCK_DGRAM) //UDP
    {
        if((socket->bsdState >= SKT_READY) && (socket->bsdState != SKT_EST))//making sure that connect function is called
        {
            if(socket->bsdState != SKT_ARP_VERIFY)
            {
                remoteInfo.IPAddr.Val = socket->remoteIP;
                ARPResolve(&remoteInfo.IPAddr);
                startTick = TickGet();
                socket->bsdState = SKT_ARP_VERIFY;
            }
            else if(socket->bsdState == SKT_ARP_VERIFY)
            {
                // Wait for the MAC address to finish being obtained
                remoteInfo.IPAddr.Val = socket->remoteIP;
                if(!ARPIsResolved(&remoteInfo.IPAddr, &remoteInfo.MACAddr))
                {
                    // Time out if too much time is spent in this state
    			    if(TickGet()- startTick > 1*TICK_SECOND)
    			    {
                        // Retransmit ARP request
                        socket->bsdState = SKT_ARP_RESOLVE;
    			    }
                 }
                 socket->SocketID = UDPOpen(socket->localPort, &remoteInfo, socket->remotePort);
                 socket->bsdState = SKT_EST;
            }
        }
        
        if(socket->bsdState == SKT_EST)
        {
            if(UDPIsGetReady(socket->SocketID) > 0)
            {
                return UDPGetArray((BYTE*)buf, len);
            }
        }
    }
    
    return 0;
}

/*****************************************************************************
  Function:
	int recvfrom(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen)

  Summary:
	The recvfrom() function is used to receive incoming data that
    has been queued for a socket.

  Description:
	The recvfrom() function is used to receive incoming data that
    has been queued for a socket. This function can be used with
    both datagram and stream type sockets. If the available data
    is too large to fit in the supplied application buffer buf,
    excess bytes are discarded in case of SOCK_DGRAM type
    sockets. For SOCK_STREAM types, the data is buffered
    internally so the application can retreive all data by
    multiple calls of recvfrom.

  Precondition:
	socket function should be called.

  Parameters:
	s - Socket descriptor returned from a previous call to socket.
    buf - application data receive buffer.
    len - buffer length in bytes.
    flags - message flags. Currently this is not supported.
    from - pointer to the sockaddr structure that will be
    filled in with the destination address.
    fromlen - size of buffer pointed by from.

  Returns:
	If recvfrom is successful, the number of bytes copied to
    application buffer buf is returned. A value of zero indicates
    no data available. A return value of SOCKET_ERROR (-1)
    indicates an error condition.

  Remarks:
	None.
  ***************************************************************************/
int recvfrom( SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen )
{
    struct BSDSocket *socket;
    struct sockaddr_in *rem_addr;
    SOCKET_INFO *remoteSockInfo;
    
    socket = &BSDSocketArray[s];
    rem_addr = (struct sockaddr_in *)from;
    
    if(socket->SocketType == SOCK_DGRAM) //UDP
    {
        if(socket->bsdState != SKT_EST)
        {
            socket->SocketID = UDPOpen(socket->localPort, NULL, socket->localPort); //recieve on the same port.
            socket->bsdState = SKT_EST;
        }
        
        if(socket->bsdState == SKT_EST)
        {
            if(UDPIsGetReady(socket->SocketID) > 0)
            {
                return UDPGetArray((BYTE*)buf, len);
            }
        }
    }
    else  //TCP recieve from already connected socket.
    {
        if(from && fromlen && ((unsigned int)*fromlen >= sizeof(struct sockaddr_in)))
        {
            remoteSockInfo = TCPGetRemoteInfo(socket->SocketID);
            rem_addr->sin_addr.S_un.S_addr = remoteSockInfo->remote.IPAddr.Val;
            rem_addr->sin_port = remoteSockInfo->remotePort.Val;
            *fromlen = sizeof(struct sockaddr_in);
        }
        return recv(s, buf, len, 0);
    }
    return 0;
}

/*****************************************************************************
  Function:
	int gethostname(char* name, int namelen )

  Summary:
	Returns the standard host name for the system.

  Description:
	This function returns the standard host name of the system which is 
	calling this function.	The returned name is null-terminated.

  Precondition:
	None.

  Parameters:
	name - Pointer to a buffer that receives the local host name.
    namelen - size of the name array.

  Returns:
	Success will return a value of 0. 
	If name is too short to hold the host name or any other error occurs, 
    SOCKET_ERROR (-1) will be returned.  On error, *name will be unmodified 
    and no null terminator will be generated.

  Remarks:
	None.
  ***************************************************************************/
int gethostname(char* name, int namelen)
{
    WORD wSourceLen;
    WORD w;
    BYTE v;

	wSourceLen = sizeof(AppConfig.NetBIOSName);
	for(w = 0; w < wSourceLen; w++)
	{
		v = AppConfig.NetBIOSName[w];
		if((v == ' ') || (v == 0u))
			break;
	}
	wSourceLen = w;
	if(namelen < wSourceLen + 1)
		return SOCKET_ERROR;

	memcpy((void*)name, (void*)AppConfig.NetBIOSName, wSourceLen);
	name[wSourceLen] = 0;

    return 0;
}

/*****************************************************************************
  Function:
	int closesocket( SOCKET s )
	
  Summary:
	The closesocket function closes an existing socket.

  Description:
	The closesocket function closes an existing socket.  
    This function releases the socket descriptor s.  
    Further references to s fails with SOCKET_ERROR code.  
    Any data buffered at the socket is discarded.  If the 
    socket s is no longer needed, closesocket() must be 
    called in order to release all resources associated with s.

  Precondition:
	None.

  Parameters:
	s - Socket descriptor returned from a previous call to socket

  Returns:
	If closesocket is successful, a value of 0 is returned. 
    A return value of SOCKET_ERROR (-1) indicates an error.

  Remarks:
	None.
  ***************************************************************************/
int closesocket( SOCKET s )
{
    struct BSDSocket *socket;
    
    if( s >= BSD_SOCKET_COUNT )
        return SOCKET_ERROR;

    socket = &BSDSocketArray[s];

    if( socket->SocketType == SOCK_STREAM)
    {
        TCPDisconnect(socket->SocketID);
    }
    else //udp sockets
    {
        UDPClose(socket->SocketID);
    }
    socket->SocketType = SOCK_DGRAM;
    socket->bsdState   = SKT_CLOSED;
    socket->SocketID   = INVALID_SOCKET;
    return 0; //success
}

#endif //STACK_USE_BERKELEY_API

