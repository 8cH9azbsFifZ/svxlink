/**
@file	 AprsUdpClient.cpp
@brief   Contains an implementation of APRS updates via UDP
@author  Adi/DL1HRC and Steve/DH1DM
@date	 2009-03-12

\verbatim
SvxLink - A Multi Purpose Voice Services System for Ham Radio Use
Copyright (C) 2003-2009 Tobias Blomberg / SM0SVX

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

#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <ctime>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncTimer.h>
#include <rtp.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "version/SVXLINK.h"
#include "AprsUdpClient.h"


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

#define HASH_KEY		0x73e2


/****************************************************************************
 *
 * Public member functions
 *
 ****************************************************************************/

AprsUdpClient::AprsUdpClient(LocationInfo::Cfg &loc_cfg,
            const std::string &server, int port)
  : loc_cfg(loc_cfg), server(server), port(port), dns(0), beacon_timer(0),
    curr_status(StationData::STAT_UNKNOWN), num_connected(0)
{
   beacon_timer = new Timer(loc_cfg.interval, Timer::TYPE_PERIODIC);
   beacon_timer->setEnable(false);
   beacon_timer->expired.connect(slot(*this, &AprsUdpClient::sendLocationInfo));
} /* AprsUdpClient::AprsUdpClient */


AprsUdpClient::~AprsUdpClient(void)
{
  updateDirectoryStatus(StationData::STAT_OFFLINE);
  delete beacon_timer;
} /* AprsUdpClient::~AprsUdpClient */


void AprsUdpClient::updateDirectoryStatus(StationData::Status status)
{
    // Check if system is properly initialized
  if (!beacon_timer)
  {
    return;
  }

    // Stop automatic beacon timer
  beacon_timer->reset();

    // Update status
  curr_status = status;

    // Build and send the packet
  sendLocationInfo();

    // Re-enable timer
  beacon_timer->setEnable(true);

} /* AprsUdpClient::updateDirectoryStatus */


void AprsUdpClient::updateQsoStatus(int action, const string& call,
  const string& info, list<string>& call_list)
{
    // Check if system is properly initialized
  if (!beacon_timer)
  {
    return;
  }

    // Stop automatic beacon timer
  beacon_timer->reset();

    // Update QSO connection status
  num_connected = call_list.size();
  curr_call = num_connected ? call_list.back() : "";

    // Build and send the packet
  sendLocationInfo();

    // Re-enable timer
  beacon_timer->setEnable(true);

} /* AprsUdpClient::updateQsoStatus */


void AprsUdpClient::update3rdState(const string& call, const string& info)
{
   // do nothing
} /* AprsUdpClient::update3rdState */



/****************************************************************************
 *
 * Protected member functions
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Private member functions
 *
 ****************************************************************************/

void AprsUdpClient::sendLocationInfo(Timer *t)
{
  if (ip_addr.ip4Addr().s_addr == INADDR_NONE)
  {
    if (!dns)
    {
      dns = new DnsLookup(server);
      dns->resultsReady.connect(slot(*this, &AprsUdpClient::dnsResultsReady));
    }
    return;
  }

  if (sock.initOk())
  {
    char sdes_packet[256];
    int sdes_len = buildSdesPacket(sdes_packet);

    sock.write(ip_addr, port, sdes_packet, sdes_len);
  }
} /* AprsUdpClient::sendLocationInfo */


void AprsUdpClient::dnsResultsReady(DnsLookup& dns_lookup)
{
  vector<IpAddress> result = dns->addresses();

  delete dns;
  dns = 0;

  if (result.empty() || (result[0].ip4Addr().s_addr == INADDR_NONE))
  {
    ip_addr.ip4Addr().s_addr = INADDR_NONE;
    return;
  }

  ip_addr = result[0];
  sendLocationInfo();

} /* AprsUdpClient::dnsResultsReady */


#define addText(block, text) \
  do { \
    int sl = strlen(text); \
    *block++ = sl; \
    memcpy(block, text, sl); \
    block += sl; \
  } while (0)


int AprsUdpClient::buildSdesPacket(char *p)
{
  time_t update;
  struct tm *utc;
  char pos[20], info[80], tmp[80];
  char *ap;
  int ver, len;

    // Evaluate directory status
  switch(curr_status)
  {
    case StationData::STAT_OFFLINE:
    case StationData::STAT_UNKNOWN:
      sprintf(info, " Off @");
      break;

    case StationData::STAT_BUSY:
      sprintf(info, " Busy ");
      break;

    case StationData::STAT_ONLINE:
      switch(num_connected)
      {
        case 0:
          sprintf(info, " On  @");
          break;
        case 1:
          sprintf(info, "=%s ", curr_call.c_str());
          break;
        default:
          sprintf(info, "+%s ", curr_call.c_str());
          break;
      }
      break;
  }

    // Read update time
  time(&update);
  utc = gmtime(&update);

    // Geographic position
  sprintf(pos, "%02d%02d.%02d%cE%03d%02d.%02d%c",
               loc_cfg.lat_pos.deg, loc_cfg.lat_pos.min,
               (loc_cfg.lat_pos.sec * 100) / 60, loc_cfg.lat_pos.dir,
               loc_cfg.lon_pos.deg, loc_cfg.lon_pos.min,
               (loc_cfg.lon_pos.sec * 100) / 60, loc_cfg.lon_pos.dir);

    // Set SDES version/misc data
  ver = (RTP_VERSION << 14) | RTCP_SDES | (1 << 8);
  p[0] = ver >> 8;
  p[1] = ver;

    // Set SDES source
  p[4] = 0;
  p[5] = 0;
  p[6] = 0;
  p[7] = 0;

    // At this point ap points to the beginning of the first SDES item
  ap = p + 8;

  *ap++ = RTCP_SDES_CNAME;
  sprintf(tmp, "%s-%s/%d", loc_cfg.mycall.c_str(), loc_cfg.prefix.c_str(),
                           getPasswd(loc_cfg.mycall));
  addText(ap, tmp);

  *ap++ = RTCP_SDES_LOC;
  sprintf(tmp, ")EL-%.6s!%s0PHG%d%d%d%d/%06d/%03d%6s%02d%02d\r\n",
               loc_cfg.mycall.c_str(), pos,
               getPowerParam(), getHeightParam(), getGainParam(),
               getDirectionParam(), loc_cfg.frequency, getToneParam(),
               info, utc->tm_hour, utc->tm_min);
  addText(ap, tmp);

  *ap++ = RTCP_SDES_END;
  *ap++ = 0;

    // Some data padding for alignment
  while ((ap - p) & 3)
  {
    *ap++ = 0;
  }

    // Length of all items
  len = ((ap - p) / 4) - 1;
  p[2] = len >> 8;
  p[3] = len;

    // Size of the entire packet
  return (ap - p);

} /* AprsUdpClient::buildSdesPacket */


// generate passcode for the aprs-servers, copied from xastir-source...
// special tnx to:
// Copyright (C) 1999,2000  Frank Giannandrea
// Copyright (C) 2000-2008  The Xastir Group
short AprsUdpClient::getPasswd(const string& call)
{
  short hash = HASH_KEY;
  string::size_type i = 0;
  string::size_type len = call.length();
  const char *ptr = call.c_str();

  while (i < len)
  {
    hash ^= toupper(*ptr++) << 8;
    hash ^= toupper(*ptr++);
    i += 2;
  }
  return (hash & 0x7fff);
} /* AprsUdpClient::passwd_hash */


int AprsUdpClient::getToneParam()
{
  return (loc_cfg.tone < 1000) ? loc_cfg.tone : 0;
} /* AprsUdpClient::getToneParam */


int AprsUdpClient::getPowerParam()
{
  return lrintf(sqrt((float)loc_cfg.power));
} /* AprsUdpClient::getPowerParam */


int AprsUdpClient::getHeightParam()
{
  return lrintf(log((float)loc_cfg.height / 10.0) / log(2.0));
} /* AprsUdpClient::getHeightParam */


int AprsUdpClient::getGainParam()
{
  return (loc_cfg.gain < 10) ? loc_cfg.gain : 9;
} /* AprsUdpClient::getGainParam */


int AprsUdpClient::getDirectionParam()
{
  if (loc_cfg.beam_dir == 0 || loc_cfg.beam_dir > 360)
  {
    return 8;
  }

  if (loc_cfg.beam_dir < 0)
  {
    return 0;
  }

  return lrintf((float)loc_cfg.beam_dir / 45.0);

} /* AprsUdpClient::getDirectionParam */


/*
 * This file has not been truncated
 */
