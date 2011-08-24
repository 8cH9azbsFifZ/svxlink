/**
@file	 Logic.h
@brief   The logic core of the SvxLink Server application
@author  Tobias Blomberg / SM0SVX
@date	 2004-03-23

This is the logic core of the SvxLink Server application. This is where
everything is tied together. It is also the base class for implementing
specific logic core classes (e.g. SimplexLogic and RepeaterLogic).

\verbatim
SvxLink - A Multi Purpose Voice Services System for Ham Radio Use
Copyright (C) 2004-2010  Tobias Blomberg / SM0SVX

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


#ifndef LOGIC_INCLUDED
#define LOGIC_INCLUDED


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdint.h>

#include <sigc++/sigc++.h>


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

#include "AudioSwitchMatrix.h"
#include "CmdParser.h"



/****************************************************************************
 *
 * Forward declarations
 *
 ****************************************************************************/

namespace Async
{
  class Config;
  class Timer;
  class AudioMixer;
  class AudioAmp;
  class AudioSelector;
  class AudioSplitter;
  class AudioValve;
  class AudioStreamStateDetector;
  class AudioRecorder;
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

class Rx;
class Tx;
class MsgHandler;
class Module;
class EventHandler;
class Command;
class QsoRecorder;


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
@brief	This class implements the core logic of SvxLink
@author Tobias Blomberg
@date   2004-03-23
*/
class Logic : public SigC::Object
{
  public:
    static bool connectLogics(const std::vector<std::string> &link_list,
      	    int timeout=0);
    static bool disconnectLogics(const std::vector<std::string> &link_list);
    static bool logicsAreConnected(const std::string& l1,
      	    const std::string& l2);

    /**
     * @brief 	Default constuctor
     */
    Logic(Async::Config& cfg, const std::string& name);

    /**
     * @brief 	Destructor
     */
    virtual ~Logic(void);

    /**
     * @brief 	A_brief_member_function_description
     * @param 	param1 Description_of_param1
     * @return	Return_value_of_this_member_function
     */

    virtual bool initialize(void);

    const std::string& name(void) const { return m_name; }

    virtual void processEvent(const std::string& event, const Module *module=0);
    void setEventVariable(const std::string& name, const std::string& value);
    virtual void playFile(const std::string& path);
    virtual void playSilence(int length);
    virtual void playTone(int fq, int amp, int len);
    void recordStart(const std::string& filename, unsigned max_time);
    void recordStop(void);

    virtual bool activateModule(Module *module);
    virtual void deactivateModule(Module *module);
    Module *activeModule(void) const { return active_module; }
    Module *findModule(int id);
    Module *findModule(const std::string& name);
    std::list<Module*> moduleList(void) const { return modules; }

    const std::string& callsign(void) const { return m_callsign; }

    Async::Config &cfg(void) const { return m_cfg; }
    Rx &rx(void) const { return *m_rx; }
    Tx &tx(void) const { return *m_tx; }

    void disconnectAllLogics(void);

    void sendDtmf(const std::string& digits);

    void injectDtmfDigit(char digit, int duration_ms)
    {
      dtmfDigitDetectedP(digit, duration_ms);
    }

    bool isIdle(void) const { return is_idle; }

    void setReportEventsAsIdle(bool idle) { report_events_as_idle = idle; }

    bool isWritingMessage(void);

    SigC::Signal1<void, bool> idleStateChanged;


  protected:
    virtual void squelchOpen(bool is_open);
    virtual void allMsgsWritten(void);
    virtual void dtmfDigitDetected(char digit, int duration);
    virtual void audioStreamStateChange(bool is_active, bool is_idle);
    virtual bool getIdleState(void) const;
    virtual void transmitterStateChange(bool is_transmitting);
    virtual void selcallSequenceDetected(std::string sequence);

    void clearPendingSamples(void);
    void enableRgrSoundTimer(bool enable);
    void rxValveSetOpen(bool do_open);
    void rptValveSetOpen(bool do_open);
    void checkIdle(void);


  private:
    static AudioSwitchMatrix  	audio_switch_matrix;

    typedef enum
    {
      TX_CTCSS_ALWAYS=1, TX_CTCSS_SQL_OPEN=2, TX_CTCSS_LOGIC=4,
      TX_CTCSS_MODULE=8, TX_CTCSS_ANNOUNCEMENT=16
    } TxCtcssType;

    Async::Config     	      	    &m_cfg;
    std::string       	      	    m_name;
    Rx	      	      	      	    *m_rx;
    Tx	      	      	      	    *m_tx;
    MsgHandler	      	      	    *msg_handler;
    Module    	      	      	    *active_module;
    std::list<Module*>	      	    modules;
    std::string       	      	    received_digits;
    std::string       	      	    m_callsign;
    Async::Timer      	      	    *cmd_tmo_timer;
    std::list<std::string>    	    cmd_queue;
    bool      	      	      	    anti_flutter;
    char      	      	      	    prev_digit;
    int      	      	      	    exec_cmd_on_sql_close;
    Async::Timer      	      	    *exec_cmd_on_sql_close_timer;
    Async::Timer      	      	    *rgr_sound_timer;
    int       	      	      	    rgr_sound_delay;
    float       	      	    report_ctcss;
    std::map<int, std::string>	    macros;
    EventHandler      	      	    *event_handler;
    Async::AudioSelector      	    *logic_con_out;
    Async::AudioSplitter	    *logic_con_in;
    CmdParser 	      	      	    cmd_parser;
    Async::Timer      	      	    *every_minute_timer;
    Async::AudioRecorder  	    *recorder;
    Async::AudioMixer	      	    *tx_audio_mixer;
    Async::AudioAmp   	      	    *fx_gain_ctrl;
    Async::AudioSelector      	    *tx_audio_selector;
    Async::AudioSplitter      	    *rx_splitter;
    Async::AudioValve 	      	    *rx_valve;
    Async::AudioValve 	      	    *rpt_valve;
    Async::AudioSelector      	    *audio_from_module_selector;
    Async::AudioSplitter      	    *audio_to_module_splitter;
    Async::AudioSelector      	    *audio_to_module_selector;
    Async::AudioStreamStateDetector *state_det;
    bool      	      	      	    is_idle;
    int                             fx_gain_normal;
    int                             fx_gain_low;
    unsigned       	      	    long_cmd_digits;
    std::string       	      	    long_cmd_module;
    bool      	      	      	    report_events_as_idle;
    QsoRecorder                     *qso_recorder;
    uint8_t			    tx_ctcss;
    uint8_t			    tx_ctcss_mask;
    std::string                     sel5_from;
    std::string                     sel5_to;

    void loadModules(void);
    void loadModule(const std::string& module_name);
    void unloadModules(void);
    void cmdTimeout(Async::Timer *t);
    void processCommandQueue(void);
    void processCommand(const std::string &cmd, bool force_core_cmd=false);
    void processMacroCmd(const std::string &macro_cmd);
    void putCmdOnQueue(Async::Timer *t=0);
    void sendRgrSound(Async::Timer *t=0);
    void everyMinute(Async::Timer *t);
    void dtmfDigitDetectedP(char digit, int duration);
    void cleanup(void);
    void updateTxCtcss(bool do_set, TxCtcssType type);
    void logicConInStreamStateChanged(bool is_active, bool is_idle);
    void audioFromModuleStreamStateChanged(bool is_active, bool is_idle);

};  /* class Logic */


//} /* namespace */

#endif /* LOGIC_INCLUDED */



/*
 * This file has not been truncated
 */

