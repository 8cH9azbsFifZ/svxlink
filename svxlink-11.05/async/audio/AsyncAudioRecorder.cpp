/**
@file	 AudioRecorder.cpp
@brief   Contains a class for recording raw audio to a file
@author  Tobias Blomberg / SM0SVX
@date	 2005-08-29

\verbatim
Async - A library for programming event driven applications
Copyright (C) 2004-2009 Tobias Blomberg / SM0SVX

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

#include <cassert>
#include <cstdio>
#include <cstring>
#include <algorithm>


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

#include "AsyncAudioRecorder.h"



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

#define WAVE_HEADER_SIZE  44


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

AudioRecorder::AudioRecorder(const string& filename,
      	      	      	     AudioRecorder::Format fmt,
			     int sample_rate)
  : filename(filename), file(NULL), samples_written(0), format(fmt),
    sample_rate(sample_rate), max_samples(0)
{
  if (format == FMT_AUTO)
  {
    format = FMT_RAW;
    
    string::size_type dot_pos = filename.rfind('.');
    if (dot_pos > 0)
    {
      string ext(filename.substr(dot_pos+1));
      if (ext == "wav")
      {
        format = FMT_WAV;
      }
    }
  }
} /* AudioRecorder::AudioRecorder */


AudioRecorder::~AudioRecorder(void)
{
  if (file != NULL)
  {
    if (format == FMT_WAV)
    {
      writeWaveHeader();
    }
    fclose(file);
  }
} /* AudioRecorder::~AudioRecorder */


bool AudioRecorder::initialize(void)
{
  assert(file == NULL);
  
  file = fopen(filename.c_str(), "w");
  if (file == NULL)
  {
    perror("*** ERROR fopen");
    return false;
  }
  
  if (format == FMT_WAV)
  {
      // Leave room for the wave file header
    if (fseek(file, WAVE_HEADER_SIZE, SEEK_SET) != 0)
    {
      perror("fseek");
    }
  }
  
  return true;
  
} /* AudioRecorder::initialize */


void AudioRecorder::setMaxRecordingTime(unsigned time_ms)
{
  max_samples = time_ms * (sample_rate / 1000);
} /* AudioRecorder::setMaxRecordingTime */


int AudioRecorder::writeSamples(const float *samples, int count)
{
  if (file == NULL)
  {
    return count;
  }
  
  if (max_samples > 0)
  {
    if (samples_written >= max_samples)
    {
      return count;
    }
    count = min(static_cast<unsigned>(count), max_samples - samples_written);
  }
  
  short buf[count];
  for (int i=0; i<count; ++i)
  {
    float sample = samples[i];
    if (sample > 1)
    {
      buf[i] = 32767;
    }
    else if (sample < -1)
    {
      buf[i] = -32767;
    }
    else
    {
      buf[i] = static_cast<short>(32767.0 * sample);
    }
  }
  
  int written = fwrite(buf, sizeof(*buf), count, file);
  if ((written == 0) && ferror(file))
  {
    fclose(file);
    file = NULL;
  }
  
  samples_written += written;
  
  return written;

} /* AudioRecorder::writeSamples */


void AudioRecorder::flushSamples(void)
{
  sourceAllSamplesFlushed();
} /* AudioRecorder::flushSamples */




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

void AudioRecorder::writeWaveHeader(void)
{
  rewind(file);
 
  char buf[WAVE_HEADER_SIZE];
  char *ptr = buf;
  
    // ChunkID
  memcpy(ptr, "RIFF", 4);
  ptr += 4;
  
    // ChunkSize
  ptr += store32bitValue(ptr, 36 + samples_written * sizeof(short));
  
    // Format
  memcpy(ptr, "WAVE", 4);
  ptr += 4;
    
    // Subchunk1ID
  memcpy(ptr, "fmt ", 4);
  ptr += 4;
  
    // Subchunk1Size
  ptr += store32bitValue(ptr, 16);
  
    // AudioFormat (PCM)
  ptr += store16bitValue(ptr, 1);
  
    // NumChannels
  ptr += store16bitValue(ptr, 1);
  
    // SampleRate
  ptr += store32bitValue(ptr, sample_rate);
  
    // ByteRate (sample rate * num channels * bytes per sample)
  ptr += store32bitValue(ptr, sample_rate * 1 * sizeof(short));
  
    // BlockAlign (num channels * bytes per sample)
  ptr += store16bitValue(ptr, 1 * sizeof(short));
  
    // BitsPerSample
  ptr += store16bitValue(ptr, 16);
  
    // Subchunk2ID
  memcpy(ptr, "data", 4);
  ptr += 4;
  
    // Subchunk2Size (num samples * num channels * bytes per sample)
  ptr += store32bitValue(ptr, samples_written * 1 * sizeof(short));
  
  assert(ptr - buf == WAVE_HEADER_SIZE);

  if (fwrite(buf, 1, WAVE_HEADER_SIZE, file) != WAVE_HEADER_SIZE)
  {
    perror("fwrite");
  }
} /* AudioRecorder::writeWaveHeader */


int AudioRecorder::store32bitValue(char *ptr, uint32_t val)
{
  *ptr++ = val & 0xff;
  val >>= 8;
  *ptr++ = val & 0xff;
  val >>= 8;
  *ptr++ = val & 0xff;
  val >>= 8;
  *ptr++ = val & 0xff;
  return 4;
} /* AudioRecorder::store32bitValue */


int AudioRecorder::store16bitValue(char *ptr, uint16_t val)
{
  *ptr++ = val & 0xff;
  val >>= 8;
  *ptr++ = val & 0xff;
  return 2;
} /* AudioRecorder::store32bitValue */



/*
 * This file has not been truncated
 */

