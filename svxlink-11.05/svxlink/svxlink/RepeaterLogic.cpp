/**
@file	 RepeaterLogic.cpp
@brief   Contains a class that implements a repeater controller
@author  Tobias Blomberg / SM0SVX
@date	 2004-04-24

\verbatim
SvxLink - A Multi Purpose Voice Services System for Ham Radio Use
Copyright (C) 2003-20089 Tobias Blomberg / SM0SVX

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

#include <sys/time.h>

#include <cstdio>
#include <string>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <sstream>
#include <algorithm>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncTimer.h>
#include <AsyncConfig.h>

#include <Rx.h>
#include <Tx.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "RepeaterLogic.h"



/****************************************************************************
 *
 * Namespaces to use
 *
 ****************************************************************************/

using namespace std;
using namespace Async;



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



/****************************************************************************
 *
 * Public member functions
 *
 ****************************************************************************/


RepeaterLogic::RepeaterLogic(Async::Config& cfg, const std::string& name)
  : Logic(cfg, name), repeater_is_up(false), up_timer(0), idle_timeout(30000),
    idle_sound_timer(0), idle_sound_interval(0),
    required_sql_open_duration(-1), open_on_sql_after_rpt_close(0),
    open_on_dtmf('?'), activate_on_sql_close(false), no_repeat(false),
    open_on_sql_timer(0), open_sql_flank(SQL_FLANK_CLOSE),
    short_sql_open_cnt(0), sql_flap_sup_min_time(1000),
    sql_flap_sup_max_cnt(0), rgr_enable(true), open_reason("?"),
    ident_nag_timeout(0), ident_nag_min_time(2000), ident_nag_timer(0)
{
  timerclear(&rpt_close_timestamp);
  timerclear(&sql_up_timestamp);
} /* RepeaterLogic::RepeaterLogic */


RepeaterLogic::~RepeaterLogic(void)
{
  delete open_on_sql_timer;
  delete idle_sound_timer;
  delete up_timer;
  delete ident_nag_timer;
} /* RepeaterLogic::~RepeaterLogic */


bool RepeaterLogic::initialize(void)
{
  if (!Logic::initialize())
  {
    return false;
  }
  
  float open_on_ctcss_fq = 0;
  int open_on_ctcss_duration = 0;
  int required_1750_duration = 0;
  
  string str;
  if (cfg().getValue(name(), "IDLE_TIMEOUT", str))
  {
    idle_timeout = atoi(str.c_str()) * 1000;
  }
  
  if (cfg().getValue(name(), "OPEN_ON_1750", str))
  {
    required_1750_duration = atoi(str.c_str());
  }
  
  if (cfg().getValue(name(), "OPEN_ON_CTCSS", str))
  {
    string::iterator it;
    it = find(str.begin(), str.end(), ':');
    if (it == str.end())
    {
      cerr << "*** ERROR: Illegal format for config variable " << name()
      	   << "/OPEN_ON_CTCSS. Should be <fq>:<required duration>.\n";
    }
    string fq_str(str.begin(), it);
    string dur_str(it+1, str.end());
    open_on_ctcss_fq = atof(fq_str.c_str());
    open_on_ctcss_duration = atoi(dur_str.c_str());
  }

  if (cfg().getValue(name(), "OPEN_ON_SQL", str))
  {
    required_sql_open_duration = atoi(str.c_str());
  }
  
  if (cfg().getValue(name(), "OPEN_ON_SQL_AFTER_RPT_CLOSE", str))
  {
    open_on_sql_after_rpt_close = atoi(str.c_str());
  }
  
  if (cfg().getValue(name(), "OPEN_ON_DTMF", str))
  {
    open_on_dtmf = str.c_str()[0];
  }
  
  if (cfg().getValue(name(), "OPEN_ON_SEL5", str))
  {
    if (str.length() > 25 || str.length() < 4)
    {
      cerr << "*** WARNING: Sel5 sequence to long/short, valid range "
            "from 4 to 25 digits, ignoring\n";
    }
    else
    {
      open_on_sel5 = str;
    }
  }

  if (cfg().getValue(name(), "OPEN_SQL_FLANK", str))
  {
    if (str == "OPEN")
    {
      open_sql_flank = SQL_FLANK_OPEN;
    }
    else if (str == "CLOSE")
    {
      open_sql_flank = SQL_FLANK_CLOSE;
    }
    else
    {
      cerr << "*** ERROR: Valid values for configuration variable "
      	   << "OPEN_SQL_FLANK are OPEN and CLOSE.\n";
    }
  }
  
  if (cfg().getValue(name(), "IDLE_SOUND_INTERVAL", str))
  {
    idle_sound_interval = atoi(str.c_str());
  }
  
  if (cfg().getValue(name(), "NO_REPEAT", str))
  {
    no_repeat = atoi(str.c_str()) != 0;
  }
  
  if (cfg().getValue(name(), "SQL_FLAP_SUP_MIN_TIME", str))
  {
    sql_flap_sup_min_time = atoi(str.c_str());
  }
  
  if (cfg().getValue(name(), "SQL_FLAP_SUP_MAX_COUNT", str))
  {
    sql_flap_sup_max_cnt = atoi(str.c_str());
  }
  
  if (cfg().getValue(name(), "IDENT_NAG_TIMEOUT", str))
  {
    ident_nag_timeout = 1000 * atoi(str.c_str());
  }
  
  if (cfg().getValue(name(), "IDENT_NAG_MIN_TIME", str))
  {
    ident_nag_min_time = atoi(str.c_str());
  }
  
  rx().toneDetected.connect(slot(*this, &RepeaterLogic::detectedTone));
  
  if (required_1750_duration > 0)
  {
    if (!rx().addToneDetector(1750, 50, 10, required_1750_duration))
    {
      cerr << "*** WARNING: Could not setup 1750 detection\n";
    }
  }
  
  if ((open_on_ctcss_fq > 0) && (open_on_ctcss_duration > 0))
  {
    if (!rx().addToneDetector(open_on_ctcss_fq, 4, 10, open_on_ctcss_duration))
    {
      cerr << "*** WARNING: Could not setup CTCSS tone detection\n";
    }
  }
  
  rptValveSetOpen(!no_repeat);
  
  idleStateChanged.connect(slot(*this, &RepeaterLogic::setIdle));
  
  tx().setTxCtrlMode(Tx::TX_AUTO);
  
  processEvent("startup");
  
  return true;
  
} /* RepeaterLogic::initialize */


void RepeaterLogic::processEvent(const string& event, const Module *module)
{
  rgr_enable = true;
  
  if ((event == "every_minute") && isIdle())
  {
    rgr_enable = false;
  }
  
  if ((event == "repeater_idle") || (event == "send_rgr_sound") /* ||
      (event.find("repeater_down") == 0) */ )
  {
    setReportEventsAsIdle(true);
    Logic::processEvent(event, module);
    setReportEventsAsIdle(false);
  }
  else
  {
    Logic::processEvent(event, module);
  }
} /* RepeaterLogic::processEvent */


bool RepeaterLogic::activateModule(Module *module)
{
  open_reason = "MODULE";
  setUp(true, open_reason);
  return Logic::activateModule(module);
} /* RepeaterLogic::activateModule */


void RepeaterLogic::dtmfDigitDetected(char digit, int duration)
{
  if (repeater_is_up)
  {
    Logic::dtmfDigitDetected(digit, duration);
  }
  else
  {
    if (digit == open_on_dtmf)
    {
      cout << "DTMF digit \"" << digit << "\" detected. "
      	      "Activating repeater...\n";
      open_reason = "DTMF";
      activateOnOpenOrClose(SQL_FLANK_CLOSE);
    }
    else
    {
      cout << "Ignoring DTMF digit \"" << digit
           << "\" since the repeater is not up\n";
    }
  }
} /* RepeaterLogic::dtmfDigitDetected */


void RepeaterLogic::selcallSequenceDetected(std::string sequence)
{
  if (repeater_is_up)
  {
     Logic::selcallSequenceDetected(sequence);
  }
  else
  {
    if (sequence == open_on_sel5)
    {
      cout << "Sel5 digits \"" << sequence << "\" detected. "
              "Activating repeater...\n";
      open_reason = "SEL5";
      activateOnOpenOrClose(SQL_FLANK_CLOSE);
    }
    else
    {
      cout << "Ignoring Sel5 sequence \"" << sequence
           << "\" since the repeater is not up\n";
    }
  }
} /* RepeaterLogic::selcallSequenceDetected */


/****************************************************************************
 *
 * Protected member functions
 *
 ****************************************************************************/

void RepeaterLogic::allMsgsWritten(void)
{
  Logic::allMsgsWritten();
  if (!repeater_is_up)
  {
    tx().setTxCtrlMode(Tx::TX_AUTO);
  }
} /* RepeaterLogic::allMsgsWritten */


void RepeaterLogic::audioStreamStateChange(bool is_active, bool is_idle)
{
  rgr_enable = true;

  if (!repeater_is_up && !is_idle)
  {
    open_reason = "AUDIO";
    setUp(true, open_reason);
  }

  Logic::audioStreamStateChange(is_active, is_idle);
  
} /* Logic::audioStreamStateChange */


#if 0
bool RepeaterLogic::getIdleState(void) const
{
  /*
  if (preserve_idle_state)
  {
    return isIdle();
  }
  */

  return Logic::getIdleState();

} /* RepeaterLogic::isIdle */
#endif



/****************************************************************************
 *
 * Private member functions
 *
 ****************************************************************************/

void RepeaterLogic::idleTimeout(Timer *t)
{
  //printf("RepeaterLogic::idleTimeout\n");
  setUp(false, "IDLE");
} /* RepeaterLogic::idleTimeout */


void RepeaterLogic::setIdle(bool idle)
{
  //printf("RepeaterLogic::setIdle: idle=%s\n", idle ? "true" : "false");

  if (!repeater_is_up)
  {
    return;
  }
  
  if ((idle && (up_timer != 0)) || (!idle && (up_timer == 0)))
  {
    return;
  }
  
  delete up_timer;
  up_timer = 0;
  delete idle_sound_timer;
  idle_sound_timer = 0;
  if (idle)
  {
    up_timer = new Timer(idle_timeout);
    up_timer->expired.connect(slot(*this, &RepeaterLogic::idleTimeout));
    
    if (idle_sound_interval > 0)
    {
      idle_sound_timer = new Timer(idle_sound_interval, Timer::TYPE_PERIODIC);
      idle_sound_timer->expired.connect(
      	  slot(*this, &RepeaterLogic::playIdleSound));
    }
  }

  enableRgrSoundTimer(idle && rgr_enable);
  
} /* RepeaterLogic::setIdle */


void RepeaterLogic::setUp(bool up, string reason)
{
  //printf("RepeaterLogic::setUp: up=%s  reason=%s\n",
  //    	 up ? "true" : "false", reason.c_str());
  if (up == repeater_is_up)
  {
    return;
  }
  
  if (up)
  {
    short_sql_open_cnt = 0;
    repeater_is_up = true;

    stringstream ss;
    //ss << "repeater_up " << (ident ? "1" : "0");
    ss << "repeater_up " << reason;
    processEvent(ss.str());
    
    rxValveSetOpen(true);
    tx().setTxCtrlMode(Tx::TX_ON);
    
    setIdle(false);
    checkIdle();
    setIdle(isIdle());
    
    if ((ident_nag_timeout > 0) && (reason != "MODULE") &&
        (reason != "AUDIO") && (reason != "SQL_RPT_REOPEN"))
    {
      delete ident_nag_timer; // Just to be sure...
      ident_nag_timer = new Timer(ident_nag_timeout);
      ident_nag_timer->expired.connect(slot(*this, &RepeaterLogic::identNag));
    }
  }
  else
  {
    if (reason != "SQL_FLAP_SUP")
    {
      gettimeofday(&rpt_close_timestamp, NULL);
    }
    else
    {
      timerclear(&rpt_close_timestamp);
    }
    open_reason = "?";
    rxValveSetOpen(false);
    repeater_is_up = false;
    delete up_timer;
    up_timer = 0;
    delete idle_sound_timer;
    idle_sound_timer = 0;
    delete ident_nag_timer;
    ident_nag_timer = 0;
    disconnectAllLogics();
    stringstream ss;
    ss << "repeater_down " << reason;
    processEvent(ss.str());
    if (!isWritingMessage())
    {
      tx().setTxCtrlMode(Tx::TX_AUTO);
    }
  }
  
} /* RepeaterLogic::setUp */


void RepeaterLogic::squelchOpen(bool is_open)
{
  //cout << name() << ": The squelch is " << (is_open ? "OPEN" : "CLOSED")
  //     << endl;
  
  rgr_enable = true;
  
  if (is_open)
  {
    gettimeofday(&sql_up_timestamp, NULL);
  }

  if (repeater_is_up)
  {
    if (is_open)
    {
      setIdle(false);
    }
    else
    {
      struct timeval now, diff_tv;
      gettimeofday(&now, NULL);
      timersub(&now, &sql_up_timestamp, &diff_tv);
      int diff_ms = diff_tv.tv_sec * 1000 + diff_tv.tv_usec / 1000;
	
      if (sql_flap_sup_max_cnt > 0)
      {
	if (diff_ms < sql_flap_sup_min_time)
	{
      	  if (++short_sql_open_cnt >= sql_flap_sup_max_cnt)
	  {
	    short_sql_open_cnt = 0;
	    cout << sql_flap_sup_max_cnt << " squelch openings less than "
		 << sql_flap_sup_min_time << "ms detected.\n";
	    setUp(false, "SQL_FLAP_SUP");
	  }
	}
	else
	{
      	  short_sql_open_cnt = 0;
	}
      }
      
      if ((ident_nag_timer != 0) && (diff_ms > ident_nag_min_time))
      {
	delete ident_nag_timer;
	ident_nag_timer = 0;
      }
    }
  
    Logic::squelchOpen(is_open);
  }
  else
  {
    if (is_open)
    {
      if (required_sql_open_duration >= 0)
      {
      	open_on_sql_timer = new Timer(required_sql_open_duration);
	open_on_sql_timer->expired.connect(
	    slot(*this, &RepeaterLogic::openOnSqlTimerExpired));
      }
      
      if (open_on_sql_after_rpt_close > 0)
      {
	struct timeval diff_tv;
	timersub(&sql_up_timestamp, &rpt_close_timestamp, &diff_tv);
	if (diff_tv.tv_sec < open_on_sql_after_rpt_close)
	{
	  open_reason = "SQL_RPT_REOPEN";
	  activateOnOpenOrClose(SQL_FLANK_OPEN);
	}
      }
    }
    else
    {
      if (open_on_sql_timer != 0)
      {
      	delete open_on_sql_timer;
      	open_on_sql_timer = 0;
      }
      
      if (activate_on_sql_close)
      {
      	activate_on_sql_close = false;
      	setUp(true, open_reason);
      }
    }
  }
} /* RepeaterLogic::squelchOpen */


void RepeaterLogic::detectedTone(float fq)
{
  if (!repeater_is_up && !activate_on_sql_close)
  {
    cout << fq << " Hz tone call detected" << endl;
    
    if (fq < 300.0)
    {
      open_reason = "CTCSS";
      activateOnOpenOrClose(open_sql_flank);
    }
    else
    {
      open_reason = "TONE";
      activateOnOpenOrClose(SQL_FLANK_CLOSE);
    }
  }
} /* RepeaterLogic::detectedTone */


void RepeaterLogic::playIdleSound(Timer *t)
{
  processEvent("repeater_idle");
} /* RepeaterLogic::playIdleSound */


void RepeaterLogic::openOnSqlTimerExpired(Timer *t)
{
  delete open_on_sql_timer;
  open_on_sql_timer = 0;
  open_reason = "SQL";
  activateOnOpenOrClose(open_sql_flank);
} /* RepeaterLogic::openOnSqlTimerExpired */


void RepeaterLogic::activateOnOpenOrClose(SqlFlank flank)
{
  if (flank == SQL_FLANK_OPEN)
  {
    if ((open_reason == "SQL") || (open_reason == "CTCSS"))
    {
      open_reason += "_OPEN";
    }
    setUp(true, open_reason);
    if (rx().squelchIsOpen())
    {
      RepeaterLogic::squelchOpen(true);
    }
  }
  else
  {
    if ((open_reason == "SQL") || (open_reason == "CTCSS"))
    {
      open_reason += "_CLOSE";
    }
    if (!rx().squelchIsOpen())
    {
      setUp(true, open_reason);
    }
    else
    {
      activate_on_sql_close = true;
    }
  }
}


void RepeaterLogic::identNag(Timer *t)
{
  delete ident_nag_timer;
  ident_nag_timer = 0;
  
  if (!rx().squelchIsOpen())
  {
    cout << "Nagging user about identifying himself\n";
    processEvent("identify_nag");
  }
} /* RepeaterLogic::identNag */



/*
 * This file has not been truncated
 */

