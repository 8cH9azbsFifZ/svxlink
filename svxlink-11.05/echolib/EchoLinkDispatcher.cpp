/**
@file	 EchoLinkDispatcher.cpp
@brief   Contains a class for handling incoming/active EchoLink Qso connections
@author  Tobias Blomberg
@date	 2003-03-30

This file contains a class that listens to incoming packets on the two
UDP-ports. The incoming packets are dispatched to the associated connection
object, if it exists. If the connection does not exist, a signal will be emitted
to indicate that an incoming connection is on its way.

\verbatim
EchoLib - A library for EchoLink communication
Copyright (C) 2003  Tobias Blomberg / SM0SVX

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\endverbatim
*/




/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <iomanip>
#include <cassert>
#include <cstring>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "rtp.h"
#include "rtpacket.h"
#include "EchoLinkQso.h"
#include "EchoLinkDispatcher.h"



/****************************************************************************
 *
 * Namespaces to use
 *
 ****************************************************************************/

using namespace std;
using namespace Async;
using namespace EchoLink;



/****************************************************************************
 *
 * Defines & typedefs
 *
 ****************************************************************************/

#define AUDIO_PORT  port_base
#define CTRL_PORT   (port_base+1)


/****************************************************************************
 *
 * Local class definitions
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Prototypes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Exported Global Variables
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Local Global Variables
 *
 ****************************************************************************/

int Dispatcher::port_base = Dispatcher::DEFAULT_PORT_BASE;
Dispatcher *Dispatcher::the_instance = 0;


/****************************************************************************
 *
 * Public member functions
 *
 ****************************************************************************/

void Dispatcher::setPortBase(int base)
{
  assert(the_instance == 0);
  port_base = base;
} /* Dispatcher::setPortBase */


/*
 *------------------------------------------------------------------------
 * Method:    
 * Purpose:   
 * Input:     
 * Output:    
 * Author:    
 * Created:   
 * Remarks:   
 * Bugs:      
 *------------------------------------------------------------------------
 */
Dispatcher *Dispatcher::instance(void)
{
  if (the_instance == 0)
  {
    the_instance = new Dispatcher;
    if (the_instance->ctrl_sock == 0)
    {
      delete the_instance;
    }
  }
  
  return the_instance;
  
} /* Dispatcher::instance */


Dispatcher::~Dispatcher(void)
{
  delete ctrl_sock;
  delete audio_sock;
  the_instance = 0;
} /* Dispatcher::~Dispatcher */


bool Dispatcher::registerConnection(Qso *con, CtrlInputHandler cih,
	AudioInputHandler aih)
{
  if (con_map.find(con->remoteIp()) != con_map.end())
  {
    return false;
  }
  
  ConData con_data;
  con_data.con = con;
  con_data.cih = cih;
  con_data.aih = aih;
  con_map[con->remoteIp()] = con_data;
  
  return true;
  
} /* Dispatcher::registerConnection */


void Dispatcher::unregisterConnection(Qso *con)
{
  ConMap::iterator iter;
  iter = con_map.find(con->remoteIp());
  assert(iter != con_map.end());
  con_map.erase(iter);
} /* Dispatcher::unregisterConnection */


bool Dispatcher::sendCtrlMsg(const IpAddress& to, const void *buf, int len)
{
  return ctrl_sock->write(to, CTRL_PORT, buf, len);
} /* Dispatcher::sendCtrlMsg */


bool Dispatcher::sendAudioMsg(const IpAddress& to, const void *buf, int len)
{
  return audio_sock->write(to, AUDIO_PORT, buf, len);
} /* Dispatcher::sendCtrlMsg */



/****************************************************************************
 *
 * Protected member functions
 *
 ****************************************************************************/


/*
 *------------------------------------------------------------------------
 * Method:    
 * Purpose:   
 * Input:     
 * Output:    
 * Author:    
 * Created:   
 * Remarks:   
 * Bugs:      
 *------------------------------------------------------------------------
 */






/****************************************************************************
 *
 * Private member functions
 *
 ****************************************************************************/


Dispatcher::Dispatcher(void)
{
  ctrl_sock = new UdpSocket(CTRL_PORT);
  audio_sock = new UdpSocket(AUDIO_PORT);
  if (!ctrl_sock->initOk() || !ctrl_sock->initOk())
  {
    delete ctrl_sock;
    ctrl_sock = 0;
    delete audio_sock;
    audio_sock = 0;
    return;
  }
  
  ctrl_sock->dataReceived.connect(slot(*this, &Dispatcher::ctrlDataReceived));
  audio_sock->dataReceived.connect(slot(*this, &Dispatcher::audioDataReceived));
  
  return;
  
} /* Dispatcher::Dispatcher */


/*
 *----------------------------------------------------------------------------
 * Method:    
 * Purpose:   
 * Input:     
 * Output:    
 * Author:    
 * Created:   
 * Remarks:   
 * Bugs:      
 *----------------------------------------------------------------------------
 */
void Dispatcher::ctrlDataReceived(const IpAddress& ip, void *buf, int len)
{
  unsigned char *recv_buf = static_cast<unsigned char *>(buf);
  
  ConMap::iterator iter;
  iter = con_map.find(ip);
  if (iter != con_map.end())
  {
    ((iter->second.con)->*(iter->second.cih))(recv_buf, len);
  }
  else
  {
    if(isRTCPSdespacket(recv_buf, len)) // May be an incoming connection
    {
      char remote_id[256];
      if(parseSDES(remote_id, recv_buf, RTCP_SDES_NAME))
      {
	//printData(remote_id, strlen(remote_id));
	char strtok_buf[256];
	char *strtok_buf_ptr = strtok_buf;
	char *remote_call = strtok_r(remote_id, " \t\n\r", &strtok_buf_ptr);
	const char *remote_name = strtok_r(NULL, " \t\n\r", &strtok_buf_ptr);
	if ((remote_call != 0) && (remote_call[0] != 0))
	{
	  if (remote_name == 0)
	  {
	    remote_name = "";
	  }
	  char priv[256];
	  parseSDES(priv, recv_buf, RTCP_SDES_PRIV);
	  incomingConnection(ip, remote_call, remote_name, priv);
	}
      }
    }
    else
    {
      cerr << "Spurious ctrl packet received from " << ip << endl;
    }
  }
  
} /* Dispatcher::ctrldDataReceived */


void Dispatcher::audioDataReceived(const IpAddress& ip, void *buf, int len)
{
  unsigned char *recv_buf = static_cast<unsigned char *>(buf);
  
  ConMap::iterator iter;
  iter = con_map.find(ip);
  if (iter != con_map.end())
  {
    ((iter->second.con)->*(iter->second.aih))(recv_buf, len);
  }
  else
  {
    cerr << "Spurious audio packet received from " << ip << endl;
  }
} /* Dispatcher::audioDataReceived */


/*
 *----------------------------------------------------------------------------
 * Method:    Dispatcher::printData
 * Purpose:   Print the contents of "buf". Used for debugging.
 * Input:     buf - The buffer to print
 *    	      len - The length of the buffer to print
 * Output:    None
 * Author:    Tobias Blomberg
 * Created:   2003-03-08
 * Remarks:   
 * Bugs:      
 *----------------------------------------------------------------------------
 */
void Dispatcher::printData(const char *buf, int len)
{
  for(int i=0; i<len; i++)
  {
    if (isprint(buf[i]))
    {
      cerr << buf[i];
    }
    else
    {
      unsigned ch = (unsigned char)buf[i];
      cerr << "<" << hex << setfill('0') << setw(2) << ch << ">";
    }
  }
  cerr << endl;
} /* Dispatcher::printData */




/*
 * This file has not been truncated
 */

