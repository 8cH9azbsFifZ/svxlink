/**
@file	 AsyncAudioIO.cpp
@brief   Contains a class for handling audio input/output to an audio device
@author  Tobias Blomberg
@date	 2003-03-23

This file contains a class for handling audio input and output to an audio
device. See usage instruction in the class documentation.

\verbatim
Async - A library for programming event driven applications
Copyright (C) 2003  Tobias Blomberg

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

#include <sigc++/sigc++.h>
#include <stdint.h>

#include <cstdio>
#include <cassert>
#include <cerrno>
#include <cmath>


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

#include "AsyncAudioDevice.h"
#include "AsyncFdWatch.h"
#include "AsyncAudioReader.h"
#include "AsyncAudioFifo.h"
#include "AsyncAudioValve.h"
#include "AsyncAudioIO.h"
#include "AsyncAudioDebugger.h"



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

class Async::AudioIO::InputFifo : public AudioFifo
{
  public:
    InputFifo(int size, AudioDevice *audio_dev)
      : AudioFifo(size), audio_dev(audio_dev), do_flush(false)
    {
    }
    
    virtual int writeSamples(const float *samples, int count)
    {
      do_flush = false;
      if ((audio_dev->mode() != AudioDevice::MODE_WR) &&
          (audio_dev->mode() != AudioDevice::MODE_RDWR))
      {
        return count;
      }
      audio_dev->audioToWriteAvailable();
      return AudioFifo::writeSamples(samples, count);
    }
    
    virtual void flushSamples(void)
    {
      if ((audio_dev->mode() != AudioDevice::MODE_WR) &&
          (audio_dev->mode() != AudioDevice::MODE_RDWR))
      {
        do_flush = false;
        sourceAllSamplesFlushed();
        return;
      }
      do_flush = true;
      if (!empty())
      {
        audio_dev->audioToWriteAvailable();
      }
      AudioFifo::flushSamples();
    }

    virtual void allSamplesFlushed(void)
    {
      do_flush = false;
      AudioFifo::allSamplesFlushed();
    }
    
    bool doFlush(void) const { return do_flush; }
  
  private:
    AudioDevice *audio_dev;
    bool do_flush;
    
}; /* Async::AudioIO::InputFifo */


class Async::AudioIO::DelayedFlushAudioReader
  : public AudioReader, public SigC::Object
{
  public:
    DelayedFlushAudioReader(AudioDevice *audio_dev)
      : audio_dev(audio_dev), flush_timer(0), is_idle(true)
    {
      
    }
    
    ~DelayedFlushAudioReader(void)
    {
      delete flush_timer;
    }

    bool isIdle(void) const { return is_idle; }
    
    virtual int writeSamples(const float *samples, int count)
    {
      //printf("DelayedFlushAudioReader[%s]::writeSamples: count=%d\n", audio_dev->devName().c_str(), count);

      is_idle = false;

      if (flush_timer != 0)
      {
	delete flush_timer;
	flush_timer = 0;
      }
      
      return AudioReader::writeSamples(samples, count);
    }
    
    virtual void flushSamples(void)
    {
      //printf("DelayedFlushAudioReader[%s]::flushSamples\n", audio_dev->devName().c_str());

      is_idle = true;

      audio_dev->flushSamples();
      long flushtime =
              1000 * audio_dev->samplesToWrite() / audio_dev->sampleRate();
      delete flush_timer;
      flush_timer = new Timer(flushtime);
      flush_timer->expired.connect(
      	  slot(*this, &DelayedFlushAudioReader::flushDone));
    }

  private:
    AudioDevice *audio_dev;
    Timer     	*flush_timer;
    bool        is_idle;
  
    void flushDone(Timer *timer)
    {
      //printf("DelayedFlushAudioReader[%s]::flushDone\n", audio_dev->devName().c_str());

      delete flush_timer;
      flush_timer = 0;
      AudioReader::flushSamples();
    } /* AudioIO::flushDone */

}; /* class Async::AudioIO::DelayedFlushAudioReader */



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


void AudioIO::setSampleRate(int rate)
{
  AudioDevice::setSampleRate(rate);
} /* AudioIO::setSampleRate */


void AudioIO::setBlocksize(int size)
{
  AudioDevice::setBlocksize(size);
} /* AudioIO::setBlocksize */


int AudioIO::blocksize(void)
{
  return audio_dev->blocksize();
} /* AudioIO::blocksize */


void AudioIO::setBlockCount(int count)
{
  AudioDevice::setBlockCount(count);
} /* AudioIO::setBlockCount */


void AudioIO::setChannels(int channels)
{
  return AudioDevice::setChannels(channels);
} /* AudioIO::setBufferCount */



AudioIO::AudioIO(const string& dev_name, int channel)
  : io_mode(MODE_NONE), audio_dev(0),
    /* lead_in_pos(0), */ m_gain(1.0), sample_rate(-1),
    m_channel(channel), input_valve(0), input_fifo(0), audio_reader(0)
{
  audio_dev = AudioDevice::registerAudioIO(dev_name, this);
  if (audio_dev == 0)
  {
    return;
  }
  
  sample_rate = audio_dev->sampleRate();
  
  input_valve = new AudioValve;
  input_valve->setOpen(false);
  AudioSink::setHandler(input_valve);
  AudioSource *prev_src = input_valve;
  
  input_fifo = new InputFifo(1, audio_dev);
  input_fifo->setOverwrite(false);
  prev_src->registerSink(input_fifo, true);
  prev_src = input_fifo;

  audio_reader = new DelayedFlushAudioReader(audio_dev);
  prev_src->registerSink(audio_reader, true);
  prev_src = 0;
  
  //new AudioDebugger(input_fifo);

} /* AudioIO::AudioIO */


AudioIO::~AudioIO(void)
{
  close();
  AudioSink::clearHandler();
  delete input_valve;
  AudioDevice::unregisterAudioIO(this);
} /* AudioIO::~AudioIO */


bool AudioIO::isFullDuplexCapable(void)
{
  return audio_dev->isFullDuplexCapable();
} /* AudioIO::isFullDuplexCapable */


bool AudioIO::open(Mode mode)
{
  if (audio_dev == 0)
  {
    return false;
  }
  
  if (mode == io_mode)
  {
    return true;
  }
  
  close();
  
  if (mode == MODE_NONE)
  {
    return true;
  }
  
    /* FIXME: Yes, I knwow... Ugly cast... */
  bool open_ok = audio_dev->open((AudioDevice::Mode)mode);
  if (open_ok)
  {
    io_mode = mode;
    input_fifo->setSize(audio_dev->blocksize() * 2 + 1);
    input_fifo->setPrebufSamples(audio_dev->blocksize() * 2 + 1);
  }
  
  input_valve->setOpen(true);

  return open_ok;
  
} /* AudioIO::open */


void AudioIO::close(void)
{
  if (io_mode == MODE_NONE)
  {
    return;
  }
  
  /*
  if ((io_mode == MODE_RD) || (io_mode == MODE_RDWR))
  {
    read_con.disconnect();
  }
  */
  
  io_mode = MODE_NONE;
  
  input_valve->setOpen(false);
  input_fifo->clear();

  audio_dev->close(); 
  
} /* AudioIO::close */



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




/****************************************************************************
 *
 * Private member functions used by friend class AudioDevice
 *
 ****************************************************************************/

int AudioIO::readSamples(float *samples, int count)
{
  //printf("AudioIO[%s:%d]::readSamples\n", audio_dev->devName().c_str(), m_channel);

  int samples_read = audio_reader->readSamples(samples, count);
  
  //printf("AudioIO[%s:%d]::readSamples: count=%d  sample_read=%d\n", audio_dev->devName().c_str(), m_channel, count, samples_read);

  if (m_gain != 1.0)
  {
    for (int i=0; i<samples_read; ++i)
    {
      samples[i] = m_gain * samples[i];
    }
  }
  
  return samples_read;
  
} /* AudioIO::readSamples */


bool AudioIO::doFlush(void) const
{
  //printf("AudioIO::doFlush\n");
  return input_fifo->doFlush();
} /* AudioIO::doFlush */


bool AudioIO::isIdle(void) const
{
  //printf("AudioIO::doFlush\n");
  return audio_reader->isIdle();
} /* AudioIO::isIdle */


int AudioIO::audioRead(float *samples, int count)
{
  return sinkWriteSamples(samples, count);
} /* AudioIO::audioRead */


unsigned AudioIO::samplesAvailable(void)
{
  //printf("AudioIO[%s:%d]::samplesAvailable: %d\n", audio_dev->devName().c_str(), m_channel, input_fifo->samplesInFifo(true));
  return input_fifo->samplesInFifo();
} /* AudioIO::samplesAvailable */




/*
 * This file has not been truncated
 */

