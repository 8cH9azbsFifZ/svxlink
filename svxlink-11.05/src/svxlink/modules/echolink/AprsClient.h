/**
@file	 AprsClient.h
@brief   Contains an interface class for APRS communication
@author  Tobias Blomberg / SM0SVX
@date	 2009-03-25

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


#ifndef APRS_CLIENT
#define APRS_CLIENT


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <string>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <EchoLinkStationData.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Forward declarations
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Namespace
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Forward declarations of classes inside of the declared namespace
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Defines & typedefs
 *
 ****************************************************************************/


/****************************************************************************
 *
 * Exported Global Variables
 *
 ****************************************************************************/


/****************************************************************************
 *
 * Class definitions
 *
 ****************************************************************************/

class AprsClient
{
  public:
    virtual ~AprsClient(void) {};
    
    virtual void updateDirectoryStatus(EchoLink::StationData::Status status) = 0;
    virtual void updateQsoStatus(int action, const std::string& call,
      const std::string& info, std::list<std::string>& call_list) = 0;
};  /* class AprsClient */


#endif /* APRS_CLIENT */

/*
 * This file has not been truncated
 */
