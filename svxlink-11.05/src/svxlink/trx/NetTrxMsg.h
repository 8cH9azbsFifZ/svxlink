/**
@file	 NetTrxMsg.h
@brief   Network messages for remote transceivers
@author  Tobias Blomberg / SM0SVX
@date	 2006-04-14

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


#ifndef NET_TRX_MSG_INCLUDED
#define NET_TRX_MSG_INCLUDED


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <utility>

#include <gcrypt.h>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <Tx.h>


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

namespace NetTrxMsg
{


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

#define NET_TRX_DEFAULT_TCP_PORT   "5210"
#define NET_TRX_DEFAULT_UDP_PORT   NET_TRX_DEFAULT_TCP_PORT


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

#pragma pack(push, 1)

/**
@brief	Base class for remote transceiver network messages
@author Tobias Blomberg / SM0SVX
@date   2006-04-14
*/
class Msg
{
  public:
    /**
     * @brief 	Constuctor
     * @param 	type The message type
     * @param 	size The message size
     */
    Msg(unsigned type, unsigned size) : m_type(type), m_size(size) {}
  
    /**
     * @brief 	Destructor
     */
    ~Msg(void) {}
  
    /**
     * @brief 	Get the message type
     * @return	Returns the message type
     */
     unsigned type(void) const { return m_type; }
  
    /**
     * @brief 	Get the message size
     * @return	Returns the message size
     */
     unsigned size(void) const { return m_size; }
    
  protected:
  
    /**
     * @brief 	Set the message size
     * @param 	size The new size of the message in bytes
     */
    void setSize(unsigned size) { m_size = size; }
        
  private:
    unsigned m_type;
    unsigned m_size;
    
    Msg(const Msg&);
    Msg& operator=(const Msg&);
    
};  /* class Msg */



/************************** Administrative Messages **************************/

class MsgProtoVer : public Msg
{
  public:
    static const unsigned TYPE  = 0;
    static const uint16_t MAJOR = 2;
    static const uint16_t MINOR = 2;
    MsgProtoVer(void)
      : Msg(TYPE, sizeof(MsgProtoVer)), m_major(MAJOR),
        m_minor(MINOR) {}
    uint16_t major(void) const { return m_major; }
    uint16_t minor(void) const { return m_minor; }
  
  private:
    uint16_t m_major;
    uint16_t m_minor;
    
}; /* MsgProtoVer */


class MsgHeartbeat : public Msg
{
  public:
    static const unsigned TYPE = 1;
    MsgHeartbeat(void) : Msg(TYPE, sizeof(MsgHeartbeat)) {}
    
};  /* MsgHeartbeat */


class MsgAuthChallenge : public Msg
{
  public:
    static const unsigned TYPE      = 10;
    static const int CHALLENGE_LEN  = 20;
    MsgAuthChallenge(void)
      : Msg(TYPE, sizeof(MsgAuthChallenge))
    {
      gcry_create_nonce(m_challenge, CHALLENGE_LEN);
    }
    
    const unsigned char *challenge(void) const { return m_challenge; }
  
  private:
    unsigned char m_challenge[CHALLENGE_LEN];
    
}; /* MsgAuthChallenge */


class MsgAuthResponse : public Msg
{
  public:
    static const unsigned TYPE        = 11;
    static const int      ALGO        = GCRY_MD_SHA1;
    static const int      DIGEST_LEN  = 20;
    MsgAuthResponse(const std::string &key, const unsigned char *challenge)
      : Msg(TYPE, sizeof(MsgAuthResponse))
    {
      if (!calcDigest(m_digest, key.c_str(), key.size(), challenge))
      {
        exit(1);
      }
    }
    
    const unsigned char *digest(void) const { return m_digest; }
    
    bool verify(const std::string &key, const unsigned char *challenge) const
    {
      unsigned char digest[DIGEST_LEN];
      bool ok = calcDigest(digest, key.c_str(), key.size(), challenge);
      return ok && (memcmp(m_digest, digest, DIGEST_LEN) == 0);
    }
  
  private:
    unsigned char m_digest[DIGEST_LEN];
    
    bool calcDigest(unsigned char *digest, const char *key,
                    int keylen, const unsigned char *challenge) const
    {
      unsigned char *digest_ptr;
      /*
      printf("key=%s\n", key);
      printf("Challenge=");
      int i;
      for (i=0; i<MsgAuthChallenge::CHALLENGE_LEN; ++i)
      {
        printf("%02x", challenge[i]);
      }
      printf("\n");
      */
        // Must call gcry_check_verion to initialize the gcypt library
      gcry_check_version(NULL);
      gcry_error_t err;
      //err = gcry_control(GCRYCTL_INIT_SECMEM, 16384, 0);
      err = gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
      if (err) goto error;
      gcry_md_hd_t hd;
      //printf("gcry_md_open\n");
      err = gcry_md_open(&hd, ALGO, GCRY_MD_FLAG_HMAC);
      if (err) goto error;
      //printf("gcry_md_setkey\n");
      err = gcry_md_setkey(hd, key, keylen);
      if (err) goto error;
      //printf("gcry_md_write\n");
      gcry_md_write(hd, challenge, MsgAuthChallenge::CHALLENGE_LEN);
      //printf("gcry_md_read\n");
      digest_ptr = gcry_md_read(hd, 0);
      memcpy(digest, digest_ptr, DIGEST_LEN);
      /*
      printf("Digest=");
      for (i=0; i<DIGEST_LEN; ++i)
      {
        printf("%02x", digest[i]);
      }
      printf("\n");
      printf("gcry_md_close\n");
      */
      gcry_md_close(hd);
      return true;
      
      error:
        std::cerr << "*** ERROR: gcrypt error: " << gcry_strerror(err)
                  << std::endl;
        gcry_md_close(hd);
        return false;
    }
    
}; /* MsgAuthResponse */


class MsgAuthOk : public Msg
{
  public:
    static const unsigned TYPE = 12;
    MsgAuthOk(void) : Msg(TYPE, sizeof(MsgAuthOk)) {}
    
};  /* MsgAuthOk */





/****************************** Common Messages *****************************/

class MsgAudioCodecSelect : public Msg
{
  public:
    typedef std::vector<std::pair<std::string, std::string> > Opts;
    
    MsgAudioCodecSelect(const char *codec_name, unsigned msg_type)
      : Msg(msg_type, sizeof(MsgAudioCodecSelect)), m_option_cnt(0)
    {
      memset(m_codec_name, 0, sizeof(m_codec_name));
      memset(m_options, 0, sizeof(m_options));
      strncpy(m_codec_name, codec_name, sizeof(m_codec_name));
      m_codec_name[sizeof(m_codec_name)-1] = 0;
    }
    
    void addOption(const std::string &name, const std::string &value)
    {
      char *ptr = m_options;
      for (int i=0; i<m_option_cnt; ++i)
      {
      	uint8_t len = static_cast<uint8_t>(*ptr++);
	ptr += len;
	if (ptr >= m_options + sizeof(m_options))
	{
	  std::cerr << "*** ERROR: Malformed option field in "
	      	    << "MsgAudioCodecSelect message\n";
	  return;
	}
      	len = static_cast<uint8_t>(*ptr++);
	ptr += len;
	if (ptr >= m_options + sizeof(m_options))
	{
	  std::cerr << "*** ERROR: Malformed option field in "
	      	    << "MsgAudioCodecSelect message\n";
	  return;
	}
      }
      if (ptr + name.size() + value.size() + 2 >= m_options + sizeof(m_options))
      {
      	std::cerr << "*** ERROR: No room for option " << name << " in"
	      	  << "MsgAudioCodecSelect message\n";
	return;
      }
      *ptr++ = name.size();
      memcpy(ptr, name.c_str(), name.size());
      ptr += name.size();
      *ptr++ = value.size();
      memcpy(ptr, value.c_str(), value.size());
      
      m_option_cnt += 1;
    }
    
    void options(Opts &opts)
    {
      char *ptr = m_options;
      for (int i=0; i<m_option_cnt; ++i)
      {
      	uint8_t len = static_cast<uint8_t>(*ptr++);
	if (ptr + len >= m_options + sizeof(m_options))
	{
	  std::cerr << "*** ERROR: Malformed option field in "
	      	    << "MsgAudioCodecSelect message\n";
	  return;
	}
	std::string name(ptr, ptr+len);
	ptr += len;
	if (ptr >= m_options + sizeof(m_options))
	{
	  std::cerr << "*** ERROR: Malformed option field in "
	      	    << "MsgAudioCodecSelect message\n";
	  return;
	}

      	len = static_cast<uint8_t>(*ptr++);
	if (ptr + len >= m_options + sizeof(m_options))
	{
	  std::cerr << "*** ERROR: Malformed option field in "
	      	    << "MsgAudioCodecSelect message\n";
	  return;
	}
	std::string value(ptr, ptr+len);
	ptr += len;
	if (ptr >= m_options + sizeof(m_options))
	{
	  std::cerr << "*** ERROR: Malformed option field in "
	      	    << "MsgAudioCodecSelect message\n";
	  return;
	}
	
	opts.push_back(std::pair<std::string, std::string>(name, value));
      }
    }
    
    const char *name(void) const { return m_codec_name; }
  
  private:
    char    m_codec_name[32];
    uint8_t m_option_cnt;
    char    m_options[256];
    
};  /* MsgAudioCodecSelect */


class MsgRxAudioCodecSelect : public MsgAudioCodecSelect
{
  public:
    static const unsigned TYPE = 100;
    MsgRxAudioCodecSelect(const char *codec_name)
      : MsgAudioCodecSelect(codec_name, TYPE) {}
  
};  /* MsgRxAudioCodecSelect */


class MsgTxAudioCodecSelect : public MsgAudioCodecSelect
{
  public:
    static const unsigned TYPE = 101;
    MsgTxAudioCodecSelect(const char *codec_name)
      : MsgAudioCodecSelect(codec_name, TYPE) {}
  
};  /* MsgTxAudioCodecSelect */


class MsgAudio : public Msg
{
  public:
    static const unsigned TYPE = 102;
    static const int BUFSIZE = sizeof(float) * 512;
    MsgAudio(const void *buf, int size)
      : Msg(TYPE, sizeof(MsgAudio) - (BUFSIZE - size))
    {
      assert(size <= BUFSIZE);
      memcpy(m_buf, buf, size);
      m_size = size;
    }
    void *buf(void)
    {
      return m_buf;
    }
    int size(void) const { return m_size; }
  
  private:
    int     m_size;
    uint8_t m_buf[BUFSIZE];
    
}; /* MsgAudio */




/******************************** RX Messages ********************************/

class MsgMute : public Msg
{
  public:
    static const unsigned TYPE = 200;
    MsgMute(bool do_mute)
      : Msg(TYPE, sizeof(MsgMute)), m_do_mute(do_mute) {}
    int doMute(void) const { return m_do_mute; }
  
  private:
    char  m_do_mute;
    
}; /* MsgMute */


class MsgAddToneDetector : public Msg
{
  public:
    static const unsigned TYPE = 201;
    MsgAddToneDetector(float fq, int bw, float thresh, int required_duration)
      : Msg(TYPE, sizeof(MsgAddToneDetector)), m_fq(fq), m_bw(bw),
      	m_thresh(thresh), m_required_duration(required_duration) {}
    float fq(void) const { return m_fq; }
    int bw(void) const { return m_bw; }
    float thresh(void) const { return m_thresh; }
    int requiredDuration(void) const { return m_required_duration; }
  
  private:
    float m_fq;
    int   m_bw;
    float m_thresh;
    int   m_required_duration;
    
}; /* MsgAddToneDetector */


class MsgReset : public Msg
{
  public:
    static const unsigned TYPE = 202;
    MsgReset(void) : Msg(TYPE, sizeof(MsgReset)) {}
    
}; /* MsgReset */




class MsgSquelch : public Msg
{
  public:
    static const unsigned TYPE = 250;
    MsgSquelch(bool is_open, float signal_strength, int sql_rx_id)
      : Msg(TYPE, sizeof(MsgSquelch)), m_is_open(is_open),
      	m_signal_strength(signal_strength), m_sql_rx_id(sql_rx_id) {}
    bool isOpen(void) const { return m_is_open; }
    float signalStrength(void) const { return m_signal_strength; }
    int sqlRxId(void) const { return m_sql_rx_id; }
  
  private:
    bool  m_is_open;
    float m_signal_strength;
    int   m_sql_rx_id;
    
}; /* MsgSquelch */


class MsgDtmf : public Msg
{
  public:
    static const unsigned TYPE = 251;
    MsgDtmf(char digit, int duration)
      : Msg(TYPE, sizeof(MsgDtmf)), m_digit(digit), m_duration(duration) {}
    char digit(void) const { return m_digit; }
    int duration(void) const { return m_duration; }
  
  private:
    char  m_digit;
    int   m_duration;
    
}; /* MsgDtmf */


class MsgTone : public Msg
{
  public:
    static const unsigned TYPE = 252;
    MsgTone(float tone_fq)
      : Msg(TYPE, sizeof(MsgTone)), m_tone_fq(tone_fq) {}
    float toneFq(void) const { return m_tone_fq; }
  
  private:
    float  m_tone_fq;
    
}; /* MsgTone */


class MsgSel5 : public Msg
{
  public:
    static const unsigned TYPE = 253;
    static const int MAX_DIGITS = 25;
    MsgSel5(std::string digits)
      : Msg(TYPE, sizeof(MsgSel5))
    {
      strncpy(m_digits, digits.c_str(), MAX_DIGITS);
      m_digits[MAX_DIGITS] = 0;
      setSize(size() - MAX_DIGITS + strlen(m_digits));
    }
    std::string digits(void) const { return m_digits; }

  private:
    char m_digits[MAX_DIGITS + 1];
}; /* MsgSel5 */



/******************************** TX Messages ********************************/

class MsgSetTxCtrlMode : public Msg
{
  public:
    static const unsigned TYPE = 300;
    MsgSetTxCtrlMode(Tx::TxCtrlMode mode)
      : Msg(TYPE, sizeof(MsgSetTxCtrlMode)), m_mode(mode) {}
    Tx::TxCtrlMode mode(void) const { return m_mode; }
  
  private:
    Tx::TxCtrlMode m_mode;
    
}; /* MsgSetTxCtrlMode */


class MsgEnableCtcss : public Msg
{
  public:
    static const unsigned TYPE = 301;
    MsgEnableCtcss(bool enable)
      : Msg(TYPE, sizeof(MsgEnableCtcss)), m_enable(enable) {}
    bool enable(void) const { return m_enable; }
  
  private:
    bool m_enable;
        
}; /* MsgEnableCtcss */


class MsgSendDtmf : public Msg
{
  public:
    static const unsigned TYPE  = 302;
    static const int MAX_DIGITS = 256;
    MsgSendDtmf(const std::string &digits)
      : Msg(TYPE, sizeof(MsgSendDtmf))
    {
      strncpy(m_digits, digits.c_str(), MAX_DIGITS);
      m_digits[MAX_DIGITS] = 0;
      setSize(size() - MAX_DIGITS + strlen(m_digits));
    }
    std::string digits(void) const { return m_digits; }
  
  private:
    char  m_digits[MAX_DIGITS+1];
    
}; /* MsgSendDtmf */


class MsgFlush : public Msg
{
  public:
    static const unsigned TYPE = 303;
    MsgFlush(void)
      : Msg(TYPE, sizeof(MsgFlush)) {}
}; /* MsgFlush */




class MsgTxTimeout : public Msg
{
  public:
    static const unsigned TYPE = 350;
    MsgTxTimeout(void)
      : Msg(TYPE, sizeof(MsgTxTimeout)) {}
}; /* MsgTxTimeout */


class MsgTransmitterStateChange : public Msg
{
  public:
    static const unsigned TYPE = 351;
    MsgTransmitterStateChange(bool is_transmitting)
      : Msg(TYPE, sizeof(MsgTransmitterStateChange)),
      	m_is_transmitting(is_transmitting) {}
    bool isTransmitting(void) const { return m_is_transmitting; }
  
  private:
    bool m_is_transmitting;
    
}; /* MsgTxTimeout */


class MsgAllSamplesFlushed : public Msg
{
  public:
    static const unsigned TYPE = 352;
    MsgAllSamplesFlushed(void)
      : Msg(TYPE, sizeof(MsgAllSamplesFlushed)) {}
}; /* MsgTxTimeout */


#pragma pack(pop)



} /* namespace */


#endif /* NET_TRX_MSG_INCLUDED */



/*
 * This file has not been truncated
 */

