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
