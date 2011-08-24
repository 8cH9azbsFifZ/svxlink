/**
@file	 S54sDtmfDecoder.h
@brief   This file contains a class that add support for the S54S interface
@author  Tobias Blomberg / SM0SVX
@date	 2008-02-04

\verbatim
SvxLink - A Multi Purpose Voice Services System for Ham Radio Use
Copyright (C) 2004-2008  Tobias Blomberg / SM0SVX

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


#ifndef S54S_DTMF_DECODER_INCLUDED
#define S54S_DTMF_DECODER_INCLUDED


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/



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

#include "HwDtmfDecoder.h"


/****************************************************************************
 *
 * Forward declarations
 *
 ****************************************************************************/

namespace Async
{
  class Serial;
};


/****************************************************************************
 *
 * Namespace
 *
 ****************************************************************************/

//namespace MyNameSpace
//{

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

/**
 * @brief   This class add support for the S54S interface board
 * @author  Tobias Blomberg, SM0SVX
 * @date    2008-02-04
 *
 * This class implements support for the hardware DTMF decoder on the
 * S54S SvxLink interface board.
 */   
class S54sDtmfDecoder : public HwDtmfDecoder
{
  public:
    /**
     * @brief 	Constructor
     * @param 	cfg A previously initialised configuration object
     * @param 	name The name of the receiver configuration section
     */
    S54sDtmfDecoder(Async::Config &cfg, const std::string &name);
    
    /**
     * @brief 	Destructor
     */
    virtual ~S54sDtmfDecoder(void);

    /**
     * @brief 	Initialize the DTMF decoder
     * @returns Returns \em true if the initialization was successful or
     *          else \em false.
     *
     * Call this function to initialize the DTMF decoder. It must be called
     * before using it.
     */
    virtual bool initialize(void);
    
  protected:
    
  private:
    Async::Serial   *serial;
    
    void charactersReceived(char *buf, int len);

};  /* class S54sDtmfDecoder */


//} /* namespace */

#endif /* S54S_DTMF_DECODER_INCLUDED */



/*
 * This file has not been truncated
 */

