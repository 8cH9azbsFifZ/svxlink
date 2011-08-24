/**
@file	 MsgHandler.cpp
@brief   Handling of playback of audio clips
@author  Tobias Blomberg / SM0SVX
@date	 2005-10-22

This file contains an object that handles the playback of audio clips.

\verbatim
<A brief description of the program or library this file belongs to>
Copyright (C) 2004-2005  Tobias Blomberg / SM0SVX

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



#ifndef MSG_HANDLER_INCLUDED
#define MSG_HANDLER_INCLUDED


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <string>
#include <list>
#include <map>

#include <sigc++/sigc++.h>



/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncAudioSource.h>


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

class QueueItem;



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
@brief	Handling of playback of audio clips
@author Tobias Blomberg / SM0SVX
@date   2005-10-22

This class handles the playback of audio clips.
*/
class MsgHandler : public SigC::Object, public Async::AudioSource
{
  public:
    /**
     * @brief 	Default constuctor
     * @param	sample_rate The sample rate of the playback system
     */
    explicit MsgHandler(int sample_rate);

    /**
     * @brief 	Destructor
     */
    ~MsgHandler(void);
    
    /**
     * @brief 	Play a file
     * @param 	path The full path to the file to play
     */
    void playFile(const std::string& path, bool idle_marked);
    
    /**
     * @brief 	Play the given number of milliseconds of silence
     * @param 	length The length in milliseconds of the silence
     */
    void playSilence(int length, bool idle_marked);
    
    /**
     * @brief 	Play a sinus tone
     * @param 	fq The frequency of the tone to play
     * @param 	amp The amplitude of the tone to play
     * @param 	length The length in milliseconds of the tone to play
     */
    void playTone(int fq, int amp, int length, bool idle_marked);
    
    /**
     * @brief 	Check if a message is beeing written
     * @return	Return \em true if a message is beeing written
     */
    bool isWritingMessage(void) const { return is_writing_message; }
    
    /**
     * @brief 	Check if the message writer is idle (ignoring idle marked items)
     * @return	Returns \em true if the idle or else \em false
     */
    bool isIdle(void) const { return non_idle_cnt == 0; }
    
    /**
     * @brief 	Clear all messages
     */
    void clear(void);
    
    /**
     * @brief 	Mark the beginning of a block of messages
     *
     * All the playXxx functions will be queued until the end() method is
     * called. Multiple begin/end can be nested.
     */
    void begin(void);
    
    /**
     * @brief 	Mark the end of a block of messages
     *
     * All the playXxx functions that have been previously queued will be
     * executed.
     */
    void end(void);    
    
    /**
     * @brief 	A signal that is emitted when all messages has been written
     */
    SigC::Signal0<void>       	    allMsgsWritten;
    
    /**
     * @brief Resume audio output to the sink
     * 
     * This function will be called when the registered audio sink is ready
     * to accept more samples.
     * This function is normally only called from a connected sink object.
     */
    virtual void resumeOutput(void);
    
  protected:
    /**
     * @brief The registered sink has flushed all samples
     *
     * This function will be called when all samples have been flushed in the
     * registered sink.
     * This function is normally only called from a connected sink object.
     */
    virtual void allSamplesFlushed(void);
    
  private:
    std::list<QueueItem*>   msg_queue;
    int			    sample_rate;
    int      	      	    nesting_level;
    bool      	      	    pending_play_next;
    QueueItem 	      	    *current;
    bool      	      	    is_writing_message;
    int       	      	    non_idle_cnt;
    
    MsgHandler(const MsgHandler&);
    MsgHandler& operator=(const MsgHandler&);
    void addItemToQueue(QueueItem *item);
    void playMsg(void);
    void writeSamples(void);
    void deleteQueueItem(QueueItem *item);

}; /* class MsgHandler */


//} /* namespace */

#endif /* MSG_HANDLER_INCLUDED */



/*
 * This file has not been truncated
 */
