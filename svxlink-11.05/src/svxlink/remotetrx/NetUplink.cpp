/**
@file	 NetUplink.cpp
@brief   Contains a class that implements a remote transceiver uplink via IP
@author  Tobias Blomberg / SM0SVX
@date	 2006-04-14

\verbatim
RemoteTrx - A remote receiver for the SvxLink server
Copyright (C) 2003-2010 Tobias Blomberg / SM0SVX

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
#include <cstring>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncTcpServer.h>
#include <AsyncAudioFifo.h>
#include <AsyncTimer.h>
#include <AsyncAudioEncoder.h>
#include <AsyncAudioDecoder.h>
#include <AsyncAudioSplitter.h>
#include <AsyncAudioSelector.h>
#include <AsyncAudioPassthrough.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "NetUplink.h"
#include "Rx.h"


/****************************************************************************
 *
 * Namespaces to use
 *
 ****************************************************************************/

using namespace std;
using namespace Async;
using namespace NetTrxMsg;



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

NetUplink::NetUplink(Config &cfg, const string &name, Rx *rx, Tx *tx,
      	      	     const string& port_str)
  : server(0), con(0), recv_cnt(0), recv_exp(0), rx(rx), tx(tx), fifo(0),
    cfg(cfg), name(name), heartbeat_timer(0), audio_enc(0), audio_dec(0),
    loopback_con(0), rx_splitter(0), tx_selector(0), state(STATE_DISC),
    mute_tx_timer(0), tx_muted(false), fallback_enabled(false)
{
  heartbeat_timer = new Timer(10000);
  heartbeat_timer->setEnable(false);
  heartbeat_timer->expired.connect(slot(*this, &NetUplink::heartbeat));
} /* NetUplink::NetUplink */


NetUplink::~NetUplink(void)
{
  delete audio_enc;
  delete audio_dec;
  delete fifo;
  delete tx_selector;
  delete rx_splitter;
  delete loopback_con;
  delete server;
  delete heartbeat_timer;
  delete mute_tx_timer;
} /* NetUplink::~NetUplink */


bool NetUplink::initialize(void)
{
  string listen_port;
  if (!cfg.getValue(name, "LISTEN_PORT", listen_port))
  {
    cerr << "*** ERROR: Configuration variable " << name
      	 << "/LISTEN_PORT is missing.\n";
    return false;
  }
  
  cfg.getValue(name, "FALLBACK_REPEATER", fallback_enabled, true);
  cfg.getValue(name, "AUTH_KEY", auth_key, true);
  
  int mute_tx_on_rx = -1;
  cfg.getValue(name, "MUTE_TX_ON_RX", mute_tx_on_rx, true);
  if (mute_tx_on_rx >= 0)
  {
    mute_tx_timer = new Timer(mute_tx_on_rx);
    mute_tx_timer->setEnable(false);
    mute_tx_timer->expired.connect(slot(*this, &NetUplink::unmuteTx));
  }
  
  server = new TcpServer(listen_port);
  server->clientConnected.connect(slot(*this, &NetUplink::clientConnected));
  server->clientDisconnected.connect(
      slot(*this, &NetUplink::clientDisconnected));
  
  rx->reset();
  rx->squelchOpen.connect(slot(*this, &NetUplink::squelchOpen));
  rx->dtmfDigitDetected.connect(slot(*this, &NetUplink::dtmfDigitDetected));
  rx->toneDetected.connect(slot(*this, &NetUplink::toneDetected));
  rx->selcallSequenceDetected.connect(
      slot(*this, &NetUplink::selcallSequenceDetected));
  
  tx->txTimeout.connect(slot(*this, &NetUplink::txTimeout));
  tx->transmitterStateChange.connect(
      slot(*this, &NetUplink::transmitterStateChange));
  
  rx_splitter = new AudioSplitter;
  rx->registerSink(rx_splitter);

  loopback_con = new AudioPassthrough;
  
  rx_splitter->addSink(loopback_con);

  tx_selector = new AudioSelector;
  tx_selector->addSource(loopback_con);

  fifo = new AudioFifo(16000);
  tx_selector->addSource(fifo);
  tx_selector->selectSource(fifo);

  tx_selector->registerSink(tx);
  
  if (fallback_enabled)
  {
    setFallbackActive(true);
  }

  return true;
  
} /* NetUplink::initialize */



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


void NetUplink::clientConnected(TcpConnection *incoming_con)
{
  cout << "Client connected: " << incoming_con->remoteHost() << ":"
       << incoming_con->remotePort() << endl;
  
  if (con == 0)
  {
    if (fallback_enabled) // Deactivate fallback repeater mode
    {
      setFallbackActive(false);
    }
    
    if (audio_enc != 0)
    {
      rx_splitter->removeSink(audio_enc);
      delete audio_enc;
      audio_enc = 0;
    }
    
    delete audio_dec;
    audio_dec = 0;
    
    con = incoming_con;
    con->dataReceived.connect(slot(*this, &NetUplink::tcpDataReceived));
    recv_exp = sizeof(Msg);
    recv_cnt = 0;
    heartbeat_timer->setEnable(true);
    gettimeofday(&last_msg_timestamp, NULL);
    
    MsgProtoVer *ver_msg = new MsgProtoVer;
    sendMsg(ver_msg);
    
    if (auth_key.empty())
    {
      MsgAuthOk *auth_msg = new MsgAuthOk;
      sendMsg(auth_msg);
      state = STATE_READY;
    }
    else
    {
      MsgAuthChallenge *auth_msg = new MsgAuthChallenge;
      memcpy(auth_challenge, auth_msg->challenge(),
             MsgAuthChallenge::CHALLENGE_LEN);
      sendMsg(auth_msg);
      state = STATE_AUTH_WAIT;
    }
  }
  else
  {
    cout << "Only one client allowed. Disconnecting...\n";
    incoming_con->disconnect();
  }
} /* NetUplink::clientConnected */


void NetUplink::clientDisconnected(TcpConnection *the_con,
      	      	      	      	   TcpConnection::DisconnectReason reason)
{
  cout << "Client disconnected: " << con->remoteHost() << ":"
       << con->remotePort() << endl;

  assert(the_con == con);
  con = 0;
  recv_exp = 0;
  state = STATE_DISC;

  rx->reset();
  tx->enableCtcss(false);
  fifo->clear();
  if (audio_dec != 0)
  {
    audio_dec->flushEncodedSamples();
  }
  tx->setTxCtrlMode(Tx::TX_OFF);
  heartbeat_timer->setEnable(false);
  
  if (fallback_enabled)
  {
    setFallbackActive(true);
  }
} /* NetUplink::clientDisconnected */


int NetUplink::tcpDataReceived(TcpConnection *con, void *data, int size)
{
  //cout << "NetRx::tcpDataReceived: size=" << size << endl;
  
  //Msg *msg = reinterpret_cast<Msg*>(data);
  //cout << "Received a TCP message with type " << msg->type()
  //     << " and size " << msg->size() << endl;
  
  if (recv_exp == 0)
  {
    cerr << "*** ERROR: Unexpected TCP data received. Throwing it away...\n";
    return size;
  }
  
  int orig_size = size;
  
  char *buf = static_cast<char*>(data);
  while (size > 0)
  {
    unsigned read_cnt = min(static_cast<unsigned>(size), recv_exp-recv_cnt);
    if (recv_cnt+read_cnt > sizeof(recv_buf))
    {
      cerr << "*** ERROR: TCP receive buffer overflow. Disconnecting...\n";
      con->disconnect();
      clientDisconnected(con, TcpConnection::DR_ORDERED_DISCONNECT);
      return orig_size;
    }
    memcpy(recv_buf+recv_cnt, buf, read_cnt);
    size -= read_cnt;
    recv_cnt += read_cnt;
    buf += read_cnt;
    
    if (recv_cnt == recv_exp)
    {
      if (recv_exp == sizeof(Msg))
      {
      	Msg *msg = reinterpret_cast<Msg*>(recv_buf);
	if (msg->size() == sizeof(Msg))
	{
	  handleMsg(msg);
	  recv_cnt = 0;
	  recv_exp = sizeof(Msg);
	}
	else if (msg->size() > sizeof(Msg))
	{
      	  recv_exp = msg->size();
	}
	else
	{
	  cerr << "*** ERROR: Illegal message header received. Header length "
	       << "too small (" << msg->size() << ")\n";
	  con->disconnect();
	  clientDisconnected(con, TcpConnection::DR_ORDERED_DISCONNECT);
	  return orig_size;
	}
      }
      else
      {
      	Msg *msg = reinterpret_cast<Msg*>(recv_buf);
      	handleMsg(msg);
	recv_cnt = 0;
	recv_exp = sizeof(Msg);
      }
    }
  }
  
  return orig_size;
  
} /* NetUplink::tcpDataReceived */


void NetUplink::handleMsg(Msg *msg)
{
  switch (state)
  {
    case STATE_DISC:
      return;
      
    case STATE_AUTH_WAIT:
      if (msg->type() == MsgAuthResponse::TYPE &&
          msg->size() == sizeof(MsgAuthResponse))
      {
        MsgAuthResponse *resp_msg = reinterpret_cast<MsgAuthResponse *>(msg);
        if (!resp_msg->verify(auth_key, auth_challenge))
        {
          cerr << "*** ERROR: Authentication error.\n";
          con->disconnect();
          clientDisconnected(con, TcpConnection::DR_ORDERED_DISCONNECT);
          return;
        }
        else
        {
          MsgAuthOk *ok_msg = new MsgAuthOk;
          sendMsg(ok_msg);
        }
        state = STATE_READY;
      }
      else
      {
        cerr << "*** ERROR: Protocol error.\n";
        con->disconnect();
        clientDisconnected(con, TcpConnection::DR_ORDERED_DISCONNECT);
      }
      return;
    
    case STATE_READY:
      break;
  }
  
  gettimeofday(&last_msg_timestamp, NULL);
  
  switch (msg->type())
  {
    case MsgHeartbeat::TYPE:
    {
      break;
    }
    
    case MsgReset::TYPE:
    {
      rx->reset();
      break;
    }
    
    case MsgMute::TYPE:
    {
      MsgMute *mute_msg = reinterpret_cast<MsgMute*>(msg);
      cout << rx->name() << ": Mute(" << (mute_msg->doMute() ? "true" : "false")
      	   << ")\n";
      rx->mute(mute_msg->doMute());
      break;
    }
    
    case MsgAddToneDetector::TYPE:
    {
      MsgAddToneDetector *atd = reinterpret_cast<MsgAddToneDetector*>(msg);
      cout << rx->name() << ": AddToneDetector(" << atd->fq()
      	   << ", " << atd->bw()
	   << ", " << atd->requiredDuration() << ")\n";
      rx->addToneDetector(atd->fq(), atd->bw(), atd->thresh(),
      	      	      	  atd->requiredDuration());
      break;
    }
    
    case MsgSetTxCtrlMode::TYPE:
    {
      MsgSetTxCtrlMode *mode_msg = reinterpret_cast<MsgSetTxCtrlMode *>(msg);
      tx->setTxCtrlMode(mode_msg->mode());
      break;
    }
     
    case MsgEnableCtcss::TYPE:
    {
      MsgEnableCtcss *ctcss_msg = reinterpret_cast<MsgEnableCtcss *>(msg);
      tx->enableCtcss(ctcss_msg->enable());
      break;
    }
     
    case MsgSendDtmf::TYPE:
    {
      MsgSendDtmf *dtmf_msg = reinterpret_cast<MsgSendDtmf *>(msg);
      tx->sendDtmf(dtmf_msg->digits());
      break;
    }
    
    case MsgRxAudioCodecSelect::TYPE:
    {
      MsgRxAudioCodecSelect *codec_msg = 
          reinterpret_cast<MsgRxAudioCodecSelect *>(msg);
      if (audio_enc != 0)
      {
	rx_splitter->removeSink(audio_enc);
	delete audio_enc;
      }
      audio_enc = AudioEncoder::create(codec_msg->name());
      if (audio_enc != 0)
      {
        audio_enc->writeEncodedSamples.connect(
                slot(*this, &NetUplink::writeEncodedSamples));
        audio_enc->flushEncodedSamples.connect(
                slot(*audio_enc, &AudioEncoder::allEncodedSamplesFlushed));
        //audio_enc->registerSource(rx);
	rx_splitter->addSink(audio_enc);
        cout << name << ": Using CODEC \"" << audio_enc->name()
             << "\" to encode RX audio\n";
	
	MsgRxAudioCodecSelect::Opts opts;
	codec_msg->options(opts);
	MsgRxAudioCodecSelect::Opts::const_iterator it;
	for (it=opts.begin(); it!=opts.end(); ++it)
	{
	  audio_enc->setOption((*it).first, (*it).second);
	}
	audio_enc->printCodecParams();
      }
      else
      {
        cerr << "*** ERROR: Received request for unknown RX audio codec ("
             << codec_msg->name() << ")\n";
      }
      break;
    }
    
    case MsgTxAudioCodecSelect::TYPE:
    {
      MsgTxAudioCodecSelect *codec_msg = 
          reinterpret_cast<MsgTxAudioCodecSelect *>(msg);
      delete audio_dec;
      audio_dec = AudioDecoder::create(codec_msg->name());
      if (audio_dec != 0)
      {
        audio_dec->registerSink(fifo);
        audio_dec->allEncodedSamplesFlushed.connect(
            slot(*this, &NetUplink::allEncodedSamplesFlushed));
        cout << name << ": Using CODEC \"" << audio_dec->name()
             << "\" to decode TX audio\n";
	
	MsgRxAudioCodecSelect::Opts opts;
	codec_msg->options(opts);
	MsgTxAudioCodecSelect::Opts::const_iterator it;
	for (it=opts.begin(); it!=opts.end(); ++it)
	{
	  audio_dec->setOption((*it).first, (*it).second);
	}
	audio_dec->printCodecParams();
      }
      else
      {
        cerr << "*** ERROR: Received request for unknown TX audio codec ("
             << codec_msg->name() << ")\n";
      }
      break;
    }
    
    case MsgAudio::TYPE:
    {
      //cout << "NetUplink [MsgAudio]\n";
      if (!tx_muted && (audio_dec != 0))
      {
        MsgAudio *audio_msg = reinterpret_cast<MsgAudio*>(msg);
        audio_dec->writeEncodedSamples(audio_msg->buf(), audio_msg->size());
      }
      break;
    }
    
    case MsgFlush::TYPE:
    {
      if (audio_dec != 0)
      {
        audio_dec->flushEncodedSamples();
      }
      break;
    } 
    
    default:
      cerr << "*** ERROR: Unknown TCP message received. type="
      	   << msg->type() << ", tize=" << msg->size() << endl;
      break;
  }
  
} /* NetUplink::handleMsg */


void NetUplink::sendMsg(Msg *msg)
{
  if (con != 0)
  {
    int written = con->write(msg, msg->size());
    if (written == -1)
    {
      cerr << "*** ERROR: TCP transmit error.\n";
    }
    else if (written != static_cast<int>(msg->size()))
    {
      cerr << "*** ERROR: TCP transmit buffer overflow.\n";
      con->disconnect();
      clientDisconnected(con, TcpConnection::DR_ORDERED_DISCONNECT);
    }
  }
  
  delete msg;
  
} /* NetUplink::sendMsg */


void NetUplink::squelchOpen(bool is_open)
{
  if (mute_tx_timer != 0)
  {
    if (is_open)
    {
      tx_muted = true;
    }
    else
    {
      mute_tx_timer->setEnable(true);
    }
  }
  
  MsgSquelch *msg = new MsgSquelch(is_open, rx->signalStrength(),
      	      	      	      	   rx->sqlRxId());
  sendMsg(msg);
} /* NetUplink::squelchOpen */


void NetUplink::dtmfDigitDetected(char digit, int duration)
{
  cout << "DTMF digit detected: " << digit << " with duration " << duration
       << " milliseconds" << endl;
  MsgDtmf *msg = new MsgDtmf(digit, duration);
  sendMsg(msg);
} /* NetUplink::dtmfDigitDetected */


void NetUplink::toneDetected(float tone_fq)
{
  cout << "Tone detected: " << tone_fq << endl;
  MsgTone *msg = new MsgTone(tone_fq);
  sendMsg(msg);
} /* NetUplink::toneDetected */


void NetUplink::selcallSequenceDetected(std::string sequence)
{
  // cout "Sel5 sequence detected: " << sequence << endl;
  MsgSel5 *msg = new MsgSel5(sequence);
  sendMsg(msg);
} /* NetUplink::selcallSequenceDetected */


void NetUplink::writeEncodedSamples(const void *buf, int size)
{
  //cout << "NetUplink::writeEncodedSamples: size=" << size << endl;
  const char *ptr = reinterpret_cast<const char *>(buf);
  while (size > 0)
  {
    const int bufsize = MsgAudio::BUFSIZE;
    int len = min(size, bufsize);
    MsgAudio *msg = new MsgAudio(ptr, len);
    sendMsg(msg);
    size -= len;
    ptr += len;
  }
} /* NetUplink::writeEncodedSamples */


void NetUplink::txTimeout(void)
{
  MsgTxTimeout *msg = new MsgTxTimeout;
  sendMsg(msg);
} /* NetUplink::txTimeout */


void NetUplink::transmitterStateChange(bool is_transmitting)
{
  MsgTransmitterStateChange *msg =
      new MsgTransmitterStateChange(is_transmitting);
  sendMsg(msg);
} /* NetUplink::transmitterStateChange */


void NetUplink::allEncodedSamplesFlushed(void)
{
  MsgAllSamplesFlushed *msg = new MsgAllSamplesFlushed;
  sendMsg(msg);
} /* NetUplink::allEncodedSamplesFlushed */


void NetUplink::heartbeat(Timer *t)
{
  MsgHeartbeat *msg = new MsgHeartbeat;
  sendMsg(msg);
  
  struct timeval diff_tv;
  struct timeval now;
  gettimeofday(&now, NULL);
  timersub(&now, &last_msg_timestamp, &diff_tv);
  int diff_ms = diff_tv.tv_sec * 1000 + diff_tv.tv_usec / 1000;
  
  if (diff_ms > 15000)
  {
    cerr << "*** ERROR: Heartbeat timeout\n";
    con->disconnect();
    clientDisconnected(con, TcpConnection::DR_ORDERED_DISCONNECT);
  }
  
  t->reset();
  
} /* NetTrxTcpClient::heartbeat */


void NetUplink::unmuteTx(Timer *t)
{
  mute_tx_timer->setEnable(false);
  tx_muted = false;
} /* NetUplink::unmuteTx */


void NetUplink::setFallbackActive(bool activate)
{
  if (activate)
  {
    cout << name << ": Activating fallback repeater mode\n";
    rx->reset();
    tx->setTxCtrlMode(Tx::TX_AUTO);
    tx_selector->selectSource(loopback_con);
    rx->mute(false);
  }
  else
  {
    cout << name << ": Deactivating fallback repeater mode\n";
    rx->reset();
    tx->setTxCtrlMode(Tx::TX_OFF);
    tx_selector->selectSource(fifo);
  }
} /* NetUplink::setFallbackActive */



/*
 * This file has not been truncated
 */

