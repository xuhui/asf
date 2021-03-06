/**
 * \file
 *
 * \brief Bufferization player.
 *
 * Copyright (c) 2009-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "compiler.h"
#include "audio_mixer.h"
#include "buff_player.h"

#include "conf_buff_player.h"

#if !defined(BUFF_PLAYER_MAX_BUF_CNT)
  #define BUFF_PLAYER_MAX_BUF_CNT   64
#endif

typedef struct
{
  void *sample_buffer;
  size_t sample_length;
} buff_player_pcm_buf_t;

typedef struct
{
  volatile unsigned int rd_idx;
  unsigned int wr_idx;
  buff_player_pcm_buf_t pcm_buf[BUFF_PLAYER_MAX_BUF_CNT];
} buff_player_pcm_buf_fifo_t;


static buff_player_pcm_buf_fifo_t *buff_player_pcm_buf_fifo = NULL;

static unsigned int buff_player_get_reload_pcm_buf_idx(void)
{
  return (buff_player_pcm_buf_fifo->rd_idx + 1) % BUFF_PLAYER_MAX_BUF_CNT;
}


static unsigned int buff_player_get_wr_pcm_buf_idx(void)
{
  return buff_player_pcm_buf_fifo->wr_idx % BUFF_PLAYER_MAX_BUF_CNT;
}


static unsigned int buff_player_get_first_pcm_buf_idx(void)
{
  return buff_player_pcm_buf_fifo->rd_idx % BUFF_PLAYER_MAX_BUF_CNT;
}


static unsigned int buff_player_get_last_pcm_buf_idx(void)
{
  return (buff_player_pcm_buf_fifo->wr_idx + BUFF_PLAYER_MAX_BUF_CNT - 1) % BUFF_PLAYER_MAX_BUF_CNT;
}


static buff_player_pcm_buf_t *buff_player_get_reload_pcm_buf(void)
{
  return &buff_player_pcm_buf_fifo->pcm_buf[buff_player_get_reload_pcm_buf_idx()];
}


static buff_player_pcm_buf_t *buff_player_get_wr_pcm_buf(void)
{
  return &buff_player_pcm_buf_fifo->pcm_buf[buff_player_get_wr_pcm_buf_idx()];
}


static buff_player_pcm_buf_t *buff_player_get_first_pcm_buf(void)
{
  return &buff_player_pcm_buf_fifo->pcm_buf[buff_player_get_first_pcm_buf_idx()];
}


static buff_player_pcm_buf_t *buff_player_get_last_pcm_buf(void)
{
  return &buff_player_pcm_buf_fifo->pcm_buf[buff_player_get_last_pcm_buf_idx()];
}


static size_t buff_player_get_pcm_buf_fifo_size(void)
{
  buff_player_pcm_buf_fifo_t *fifo = buff_player_pcm_buf_fifo;

  return (fifo->wr_idx + 2 * BUFF_PLAYER_MAX_BUF_CNT - fifo->rd_idx) % (2 * BUFF_PLAYER_MAX_BUF_CNT);
}


static bool buff_player_is_pcm_buf_fifo_empty(void)
{
  return (buff_player_get_pcm_buf_fifo_size() == 0);
}


static bool buff_player_is_pcm_buf_fifo_reloadable(void)
{
  return (buff_player_get_pcm_buf_fifo_size() > 1);
}


static bool buff_player_is_pcm_buf_fifo_full(void)
{
  return (buff_player_get_pcm_buf_fifo_size() == BUFF_PLAYER_MAX_BUF_CNT);
}


static bool buff_player_put_pcm_buf(void *sample_buffer, size_t sample_length)
{
  buff_player_pcm_buf_fifo_t *fifo = buff_player_pcm_buf_fifo;
  buff_player_pcm_buf_t *wr_pcm_buf;

  if (!fifo ||
      buff_player_is_pcm_buf_fifo_full())
    return false;

  wr_pcm_buf = buff_player_get_wr_pcm_buf();

  wr_pcm_buf->sample_buffer = sample_buffer;
  wr_pcm_buf->sample_length = sample_length;

  fifo->wr_idx = (fifo->wr_idx + 1) % (2 * BUFF_PLAYER_MAX_BUF_CNT);

  return true;
}


static bool buff_player_peek_reload_pcm_buf(buff_player_pcm_buf_t *pcm_buf)
{
  buff_player_pcm_buf_fifo_t *fifo = buff_player_pcm_buf_fifo;

  if (!fifo ||
      !buff_player_is_pcm_buf_fifo_reloadable() ||
      !pcm_buf)
    return false;

  *pcm_buf = *buff_player_get_reload_pcm_buf();

  return true;
}


static bool buff_player_remove_used_pcm_buf(void)
{
  buff_player_pcm_buf_fifo_t *fifo = buff_player_pcm_buf_fifo;

  if (!fifo ||
      buff_player_is_pcm_buf_fifo_empty())
    return false;

  fifo->rd_idx = (fifo->rd_idx + 1) % (2 * BUFF_PLAYER_MAX_BUF_CNT);

  return true;
}


bool buff_player_create(uint8_t nb_buffers, size_t buffer_size)
{
  buff_player_pcm_buf_fifo = calloc(1, sizeof(*buff_player_pcm_buf_fifo));

  if (!buff_player_pcm_buf_fifo)
    return false;

  return true;
}


void *buff_player_malloc(size_t buffer_size)
{
  size_t sample_length = buffer_size;

  buff_player_pcm_buf_fifo_t *fifo = buff_player_pcm_buf_fifo;
  buff_player_pcm_buf_t *first_pcm_buf, *last_pcm_buf;
  void *sample_buffer, *sample_buffer_end;

  if (!buff_player_pcm_buf_fifo)
    return NULL;

  if (!fifo ||
      !sample_length)
    return NULL;

  if (buff_player_is_pcm_buf_fifo_empty())
    sample_buffer = BUFF_PLAYER_ADDRESS_START;
  else if (buff_player_is_pcm_buf_fifo_full())
    return NULL;
  else
  {
    first_pcm_buf = buff_player_get_first_pcm_buf();
    last_pcm_buf = buff_player_get_last_pcm_buf();

    sample_buffer = (uint8_t *)last_pcm_buf->sample_buffer + last_pcm_buf->sample_length * 2 * sizeof(int16_t);
    sample_buffer_end = (uint8_t *)sample_buffer + sample_length * 2 * sizeof(int16_t);
    if (sample_buffer <= first_pcm_buf->sample_buffer)
    {
      if (sample_buffer_end > first_pcm_buf->sample_buffer)
        sample_buffer = NULL;
    }
    else if (sample_buffer_end > (void *)((uint8_t *)BUFF_PLAYER_ADDRESS_START + BUFF_PLAYER_MEMORY_SIZE))
    {
      sample_buffer = BUFF_PLAYER_ADDRESS_START;
      sample_buffer_end = (uint8_t *)sample_buffer + sample_length * 2 * sizeof(int16_t);
      if (sample_buffer_end > first_pcm_buf->sample_buffer)
        sample_buffer = NULL;
    }
  }

  return sample_buffer;
}


void buff_player_dacs_setup(uint32_t sample_rate_hz,
                           uint8_t num_channels,
                           uint8_t bits_per_sample,
                           bool swap_channels)
{
  audio_mixer_dacs_setup_direct(sample_rate_hz,
                                num_channels,
                                bits_per_sample,
                                swap_channels);
}


bool buff_player_dacs_output(void *sample_buffer, size_t sample_length)
{
  if (!buff_player_pcm_buf_fifo)
    return false;

  if (!sample_length)
    return (!buff_player_is_pcm_buf_fifo_full());

  if (audio_mixer_dacs_output_direct(NULL, 0))
    audio_mixer_dacs_output_direct(sample_buffer, sample_length);

  return buff_player_put_pcm_buf(sample_buffer, sample_length);
}


void buff_player_dacs_end_of_pcm_buf(void)
{
  buff_player_pcm_buf_t pcm_buf;

  if (!buff_player_pcm_buf_fifo)
    return;

  buff_player_remove_used_pcm_buf();

  if (!buff_player_peek_reload_pcm_buf(&pcm_buf))
    return;

  audio_mixer_dacs_output_direct(pcm_buf.sample_buffer, pcm_buf.sample_length);
}


void buff_player_dacs_flush(bool force)
{
  if (!buff_player_pcm_buf_fifo)
    return;
  // Wait until all the samples have been played
  if (!force)
    while (!buff_player_is_pcm_buf_fifo_empty());
  // Clean the FIFO
  if (force)
    memset(buff_player_pcm_buf_fifo, 0, sizeof(*buff_player_pcm_buf_fifo));
  audio_mixer_dacs_flush_direct(false);
}


void buff_player_close(void)
{
  if (!buff_player_pcm_buf_fifo)
    return;
  free((void *)buff_player_pcm_buf_fifo);
  buff_player_pcm_buf_fifo = NULL;
}

void buff_player_mute_audio(bool mute)
{
  audio_mixer_mute_audio_direct(mute);
  if (mute)
    buff_player_dacs_flush(true);
}
