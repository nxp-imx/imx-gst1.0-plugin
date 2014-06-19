/*
 * Copyright (c) 2014, Freescale Semiconductor, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_AVB_SRC_H__
#define __GST_AVB_SRC_H__
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gio/gio.h>
#include "gstimxcommon.h"
#include "cip.h"
#include "avtp.h"
#include "ethernet.h"
#include "gstavbclock.h"

G_BEGIN_DECLS

typedef struct _GstAvbSrc GstAvbSrc;
typedef struct _GstAvbSrcClass GstAvbSrcClass;


#define GST_TYPE_AVBSRC \
  (gst_avbsrc_get_type())
#define GST_AVBSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVBSRC,GstAvbSrc))
#define GST_AVBSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVBSRC,GstAvbSrcClass))
#define GST_AVBSRC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AVBSRC, GstAvbSrcClass))
#define GST_IS_AVBSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVBSRC))
#define GST_IS_AVBSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVBSRC))

#define AVB_HEADER_SIZE (sizeof(ETHERNET_HEADER) + sizeof(AVTPDU_DATA_HEADER) + sizeof(CIP_HEADER))

struct _GstAvbSrc {
  GstPushSrc element;
  GstCaps *caps;

  guint32 buffer_size;
  guint64 timeout;

  struct sockaddr_ll * avb_sll;
  GstPollFD  sock;
  GstPoll   *fdset;

  guint32 output_buffer_size;

  guint8 last_sequence_num;

  GstClock *provided_clock;
  gboolean init_segment;
};

struct _GstAvbSrcClass {
  GstPushSrcClass parent_class;

  /**
   * function to check if this package is valid for the src
   *
   * @param src [in] avbsrc object.
   * @param header [in] cip header pointer.
   * @return 0 success, others, fail.
   */
  int (*isValid_cip_header)(GstAvbSrc *src,CIP_HEADER * header);

  /**
   * function to get caps from the first buffer
   *
   * @param src [in] avbsrc object.
   * @param readData [in] pointer to buffer read from socket.
   * @param readsize [in] length of buffer that read from socket
   * @return GstCaps if success.
   */
  GstCaps *(*parse_and_get_caps)(GstAvbSrc *src,guint8 * readData,gint readsize);

  /**
   * function to get output buffer size.
   *
   * @param src [in] avbsrc object.
   * @param avtpDataLen [in] buffer len of cip package data.
   * @return len of output buffer size.
   */
  guint32 (*get_output_buffer_size)(GstAvbSrc *src,guint32 avtpDataLen);

  /**
   * function to write stream data into output buffer.
   *
   * @param src [in] avbsrc object.
   * @param srcBuffer [in] buffer pointer to the whole input buffer.
   * @param srcSize [in] source buffer size which contains length of etherner, avtp & cip header.
   * @param descBuffer [in] pointer to the destination buffer.
   * @param outSize [out] buffer size written, this should be equal to the desc buffer size. 
   * @return TRUE if success
   */
  gboolean (*process_buffer)(GstAvbSrc * src, guint8 *srcBuffer,
    guint32 srcSize, GstBuffer * descBuffer, guint32 * outSize);

  /**
   * function that called when package lost detected.
   *
   * @param src [in] avbsrc object.
   */
  void (*reset_ts)(GstAvbSrc * src);
};

GType gst_avbsrc_get_type(void);

G_END_DECLS


#endif /* __GST_AVB_SRC_H__ */
