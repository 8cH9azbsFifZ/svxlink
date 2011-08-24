/**
@file	 SigLevDet.h
@brief   The base class for a signal level detector
@author  Tobias Blomberg / SM0SVX
@date	 2009-05-23

\verbatim
SvxLink - A Multi Purpose Voice Services System for Ham Radio Use
Copyright (C) 2003-2008 Tobias Blomberg / SM0SVX

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

#ifndef SIG_LEV_DET_INCLUDED
#define SIG_LEV_DET_INCLUDED


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <sigc++/sigc++.h>
#include <string>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncAudioSink.h>


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

namespace Async
{
  class AudioFilter;
  class SigCAudioSink;
  class Config;
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

/**
@brief	A simple signal level detector
@author Tobias Blomberg / SM0SVX
@date   2006-05-07
*/
class SigLevDet : public SigC::Object, public Async::AudioSink
{
  public:
    /**
     * @brief 	Default constuctor
     */
    SigLevDet(void) {}
  
    /**
     * @brief 	Destructor
     */
    virtual ~SigLevDet(void) {}
    
    /**
     * @brief 	Initialize the signal detector
     * @return 	Return \em true on success, or \em false on failure
     */
    virtual bool initialize(Async::Config &cfg, const std::string& name)
    {
      return true;
    }
    
    /**
     * @brief 	Read the latest measured signal level
     * @return	Returns the latest measured signal level
     */
    virtual float lastSiglev(void) const = 0;
    
    /**
     * @brief   Reset the signal level detector
     */
    virtual void reset(void) = 0;
     
    
  protected:
    
  private:
    SigLevDet(const SigLevDet&);
    SigLevDet& operator=(const SigLevDet&);
    
};  /* class SigLevDet */


//} /* namespace */

#endif /* SIG_LEV_DET_INCLUDED */



/*
 * This file has not been truncated
 */

