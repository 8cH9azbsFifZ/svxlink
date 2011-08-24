/**
@file	 AsyncAudioDevice.cpp
@brief   Handle audio devices
@author  Tobias Blomberg
@date	 2004-03-20

This file contains an implementation of a class that handles
hardware audio devices.

\verbatim
Async - A library for programming event driven applications
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



/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <cstring>
#include <algorithm>
#include <iostream>
#include <sstream>


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

#include "AsyncFdWatch.h"
#include "AsyncAudioIO.h"
#include "AsyncAudioDevice.h"
#include "AsyncAudioDeviceFactory.h"


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

map<string, AudioDevice*>  AudioDevice::devices;
int AudioDevice::sample_rate = DEFAULT_SAMPLE_RATE;
int AudioDevice::block_size_hint = DEFAULT_BLOCK_SIZE_HINT;
int AudioDevice::block_count_hint = DEFAULT_BLOCK_COUNT_HINT;
int AudioDevice::channels = DEFAULT_CHANNELS;



/****************************************************************************
 *
 * Public member functions
 *
 ****************************************************************************/

AudioDevice *AudioDevice::registerAudioIO(const string& dev_designator,
      	AudioIO *audio_io)
{
  string::size_type colon_pos = dev_designator.find_first_of(':');
  if (colon_pos == string::npos)
  {
    cerr << "*** ERROR: The audio device name must be given on the form "
            "\"devtype:devname\".\n";
    return 0;
  }

  string dev_type(dev_designator.substr(0, colon_pos));
  string dev_name(dev_designator.substr(colon_pos+1, string::npos));
  
  AudioDevice *dev = 0;
  if (devices.count(dev_designator) == 0)
  {
    dev = AudioDeviceFactory::instance()->create(dev_type, dev_name);
    if (dev == 0)
    {
      cerr << "*** ERROR: Unknown audio device type \"" << dev_type << "\" "
              "given. Valid device types: "
           << AudioDeviceFactory::instance()->validDevTypes() << endl;
      return 0;
    }

    devices[dev_designator] = dev;
  }
  dev = devices[dev_designator];
  ++dev->use_count;
  dev->aios.push_back(audio_io);
  return dev;
  
} /* AudioDevice::registerAudioIO */


void AudioDevice::unregisterAudioIO(AudioIO *audio_io)
{
  AudioDevice *dev = audio_io->device();
  if (dev == 0)
  {
    return;
  }
  
  assert(dev->use_count > 0);
  
  list<AudioIO*>::iterator it =
      	  find(dev->aios.begin(), dev->aios.end(), audio_io);
  assert(it != dev->aios.end());
  dev->aios.erase(it);
  
  if (--dev->use_count == 0)
  {
      // The device designator isn't available here, so we have
      // to iterate through the map to find the device instance.
    map<string, AudioDevice*>::iterator it;
    for (it = devices.begin(); it != devices.end(); ++it)
    {
      if (it->second == dev)
      {
        devices.erase(it);
        break;
      }
    }
    delete dev;
  }
} /* AudioDevice::unregisterAudioIO */


bool AudioDevice::open(Mode mode)
{
  if (mode == current_mode) // Same mode => do nothing
  {
    return true;
  }
  
  if (mode == MODE_NONE)  // Same as calling close
  {
    close();
  }
  
  if (current_mode == MODE_RDWR)  // Already RDWR => don't have to do anything
  {
    return true;
  }
  
     // Is not closed and is either read or write and we want the other
  if ((current_mode != MODE_NONE) && (mode != current_mode))
  {
    mode = MODE_RDWR;
  }
  
  if (openDevice(mode))
  {
    current_mode = mode;
    return true;
  }
  
  return false;
  
} /* AudioDevice::open */


void AudioDevice::close(void)
{
  list<AudioIO*>::iterator it;
  for (it=aios.begin(); it!=aios.end(); ++it)
  {
    if ((*it)->mode() != AudioIO::MODE_NONE)
    {
      return;
    }
  }

  closeDevice();  
  current_mode = MODE_NONE;
  
} /* AudioDevice::close */



/****************************************************************************
 *
 * Protected member functions
 *
 ****************************************************************************/


AudioDevice::AudioDevice(const string& dev_name)
  : dev_name(dev_name), current_mode(MODE_NONE), use_count(0)
{
} /* AudioDevice::AudioDevice */


AudioDevice::~AudioDevice(void)
{
} /* AudioDevice::~AudioDevice */


void AudioDevice::putBlocks(int16_t *buf, int frame_cnt)
{
  //printf("putBlocks: frame_cnt=%d\n", frame_cnt);
  float samples[frame_cnt];
  for (int ch=0; ch<channels; ch++)
  {
    for (int i=0; i<frame_cnt; i++)
    {
        // no divisions in index calculation (embedded system performance !!!)
      samples[i] = static_cast<float>(buf[i * channels + ch]) / 32768.0;
    }
    list<AudioIO*>::iterator it;
    for (it=aios.begin(); it!=aios.end(); ++it)
    {
      if ((*it)->channel() == ch)
      {
        (*it)->audioRead(samples, frame_cnt);
      }
    }
  }
} /* AudioDevice::putBlocks */


int AudioDevice::getBlocks(int16_t *buf, int block_cnt)
{
  unsigned block_size = blocksize();
  unsigned frames_to_write = block_cnt * block_size;
  memset(buf, 0, channels * frames_to_write * sizeof(*buf));
  
    // Loop through all AudioIO objects and find out if they have any
    // samples to write and how many. The non-flushing AudioIO object with
    // the least number of samples will decide how many samples can be
    // written in total. If all AudioIO objects are flushing, the AudioIO
    // object with the most number of samples will decide how many samples
    // get written.
  list<AudioIO*>::iterator it;
  bool do_flush = true;
  unsigned int max_samples_in_fifo = 0;
  for (it=aios.begin(); it!=aios.end(); ++it)
  {
    if (!(*it)->isIdle())
    {
      unsigned samples_avail = (*it)->samplesAvailable();
      if (!(*it)->doFlush())
      {
	do_flush = false;
	if (samples_avail < frames_to_write)
	{
	  frames_to_write = samples_avail;
	}
      }

      if (samples_avail > max_samples_in_fifo)
      {
	max_samples_in_fifo = samples_avail;
      }
    }
  }
  do_flush &= (max_samples_in_fifo <= frames_to_write);
  if (max_samples_in_fifo < frames_to_write)
  {
    frames_to_write = max_samples_in_fifo;
  }

  //printf("### frames_to_write=%d  do_flush=%s  fragsize=%u\n",
  //	 frames_to_write, do_flush ? "TRUE" : "FALSE", fragsize);
  
    // If not flushing, make sure the number of frames to write is an even
    // multiple of the frag size.
  if (!do_flush)
  {
    frames_to_write /= block_size;
    frames_to_write *= block_size;
  }
  
    // If there are no frames to write, bail out and wait for an AudioIO
    // object to provide us with some.
  if (frames_to_write == 0)
  {
    return 0;
  }
  
    // Fill the sample buffer with samples from the non-idle AudioIO objects.
  for (it=aios.begin(); it!=aios.end(); ++it)
  {
    if (!(*it)->isIdle())
    {
      int channel = (*it)->channel();
      float tmp[frames_to_write];
      int samples_read = (*it)->readSamples(tmp, frames_to_write);
      for (int i=0; i<samples_read; ++i)
      {
	int buf_pos = i * channels + channel;
	float sample = 32767.0 * tmp[i] + buf[buf_pos];
	if (sample > 32767)
	{
	  buf[buf_pos] = 32767;
	}
	else if (sample < -32767)
	{
	  buf[buf_pos] = -32767;
	}
	else
	{
	  buf[buf_pos] = static_cast<int16_t>(sample);
	}
      }
    }
  }  
      
    // If flushing and the number of frames to write is not an even
    // multiple of the frag size, round the number of frags to write
    // up. The end of the buffer is already zeroed out.
  if (do_flush && (frames_to_write % block_size > 0))
  {
    frames_to_write /= block_size;
    frames_to_write = (frames_to_write + 1) * block_size;
  }
  
  return frames_to_write / block_size;
  
} /* AudioDevice::getBlocks */



/****************************************************************************
 *
 * Private member functions
 *
 ****************************************************************************/


/*
 * This file has not been truncated
 */

