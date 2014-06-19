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
#ifndef __GST_AVB_SINK_H__
#define __GST_AVB_SINK_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "fsl_types.h"

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gio/gio.h>
#include "gstimxcommon.h"
#include "../../libs/gstsutils/gstsutils.h"
#include <net/if.h>
#include "gstavbclock.h"


G_BEGIN_DECLS

typedef struct _GstAvbSink GstAvbSink;
typedef struct _GstAvbSinkClass GstAvbSinkClass;

#define GST_TYPE_AVBSINK \
  (gst_avbsink_get_type())

#define GST_AVBSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVBSINK,GstAvbSink))
#define GST_AVBSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVBSINK,GstAvbSinkClass))
#define GST_AVBSINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AVBSINK, GstAvbSinkClass))
#define GST_IS_AVBSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVBSINK))
#define GST_IS_AVBSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVBSINK))

struct _GstAvbSink
{
  GstBaseSink element;
  GstClock *provided_clock;

  guint32 latency;
  guint32 package_count;
  gboolean use_ptp_time;

  guint8 * header;//pointer to ethernet,avtp & cip header

  //socket
  int avb_fd;
  struct sockaddr_ll * avb_sll;
  int ptp_fd;
  gchar net_name[IF_NAMESIZE];

  gboolean prepared;

  guint64 ts;
  guint64 start_ts;

  guint8 sph;

  guint64 sequence_num;
  guint64 data_block_count;


};

struct _GstAvbSinkClass
{
  GstBaseSinkClass parent_class;

  //mandatory function implemented by subclass
  guint8 (*get_dbs)(GstAvbSink *sink);
  guint8 (*get_fn)(GstAvbSink *sink);
  guint8 (*get_sph)(GstAvbSink *sink);
  guint8 (*get_dbc)(GstAvbSink *sink,guint32 payloadLen);
  guint8 (*get_fmt)(GstAvbSink *sink);
  guint8 (*get_fdf)(GstAvbSink *sink);
  guint16 (*get_syt)(GstAvbSink *sink);

  /**
   * function to write stream data buffer.
   *
   * @param sink [in] avbsink object.
   * @param leftsize [in] buffer len waiting to send.
   * @return len of stream data.
   */
  guint32 (*get_packet_len)(GstAvbSink *sink,guint32 leftsize);

  /**
   * function to write stream data buffer.
   *
   * @param sink [in] avbsink object.
   * @param srcBuffer [in] buffer pointer to the whole input buffer.
   * @param consumedLen [in&out] len of buffer already sent by avb socket.
   * @param descSize [in] destination buffer size, should be equal to the payload size got from subclass.
   * @param descBuffer [in] pointer to the payload buffer.
   * @param outDuration [out] duration of payload buffer.
   * @return TRUE if success
   */
  gboolean (*process_buffer)(GstAvbSink *sink, guint8 *srcBuffer, guint32 *consumedLen, guint32 descSize,
    guint8 *descBuffer, guint64 *outDuration);

  //optional function implemented by subclass
  //write timestamp into cip package for IEC 61883-4.
  /**
   * function to write avtp time to stream data header.
   *
   * @param sink [in] avbsink object.
   * @param srcBuffer [in] buffer pointer to stream data buffer.
   * @param bufferSize [in] len of stream data buffer
   * @param inTime [in] timestamp to write.
   * @return TRUE if success
   */
  gboolean (*rewrite_time)(GstAvbSink *sink, guint8 *inBuffer, guint32 bufferSize, guint64 ptp_ts, guint64 ts);


};

GType gst_avbsink_get_type(void);

//set & get property functions

void gst_avbsink_set_latency(GstAvbSink *avbsink, guint32 latency);
guint32 gst_avbsink_get_latency(GstAvbSink *avbsink);

void gst_avbsink_set_package_count(GstAvbSink *avbsink, guint32 count);
guint32 gst_avbsink_get_package_count(GstAvbSink *avbsink);

void gst_avbsink_set_use_ptp_time(GstAvbSink *avbsink, gboolean enabled);
gboolean gst_avbsink_get_use_ptp_time(GstAvbSink *avbsink);


G_END_DECLS

#endif /* __GST_AVB_SINK_H__ */
