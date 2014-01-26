/*
 * Copyright (c) 2013-2014, Freescale Semiconductor, Inc. All rights reserved.
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

#ifndef __AIURCONTENT_H__
#define __AIURCONTENT_H__
#include <gst/gst.h>
#include "aiurstreamcache.h"
#include "fsl_parser.h"

#define AIURDEMUX_MIN_OUTPUT_BUFFER_SIZE 8

typedef struct _AiurContent AiurContent;

int aiurcontent_new(AiurContent **pContent);
void aiurcontent_release(AiurContent *pContent);

int aiurcontent_get_pullfile_callback(AiurContent * pContent,FslFileStream *file_cbks);
int aiurcontent_get_pushfile_callback(AiurContent * pContent,FslFileStream *file_cbks);

int aiurcontent_get_memory_callback(AiurContent * pContent,ParserMemoryOps *mem_cbks);
int aiurcontent_get_buffer_callback(AiurContent * pContent,ParserOutputBufferOps *file_cbks);

int aiurcontent_init(AiurContent * pContent,GstPad *sinkpad,GstAiurStreamCache *stream_cache);

gboolean aiurcontent_is_live(AiurContent * pContent);
gboolean aiurcontent_is_seelable(AiurContent * pContent);
gboolean aiurcontent_is_random_access(AiurContent * pContent);
gchar* aiurcontent_get_url(AiurContent * pContent);
gchar* aiurcontent_get_index_file(AiurContent * pContent);






#endif
