/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
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

#include <string.h>
#include <gst/video/gstvideometa.h>
#include "gstvpuallocator.h"
#include "gstvpudecobject.h"

GST_DEBUG_CATEGORY_STATIC(vpu_dec_object_debug);
#define GST_CAT_DEFAULT vpu_dec_object_debug

#define VPUDEC_TS_BUFFER_LENGTH_DEFAULT (1024)
#define DEFAULT_FRAME_BUFFER_ALIGNMENT_H 16
#define DEFAULT_FRAME_BUFFER_ALIGNMENT_V 16
#define MASAIC_THRESHOLD (30)
//FIXME: relate with frame plus?
#define DROP_RESUME (200 * GST_MSECOND)
#define MAX_RATE_FOR_NORMAL_PLAYBACK (2)
#define MIN_RATE_FOR_NORMAL_PLAYBACK (0)

#define ALIGN(ptr,align)	((align) ? ((((guint32)(ptr))+(align)-1)/(align)*(align)) : ((guint32)(ptr)))

typedef struct
{
  VpuCodStd std;
  const gchar *mime;
} VPUMapper;

static VPUMapper vpu_mappers[] = {
  {VPU_V_MPEG4, "video/mpeg, mpegversion=(int)4"},
  {VPU_V_XVID, "video/x-xvid"},
  {VPU_V_H263, "video/x-h263"},
  {VPU_V_AVC, "video/x-h264"},
  {VPU_V_VC1, "video/x-wmv, wmvversion=(int)3, format=(string)WMV3"},
  {VPU_V_VC1_AP, "video/x-wmv, wmvversion=(int)3, format=(string)WVC1"},
  {VPU_V_MPEG2, "video/mpeg, systemstream=(boolean)false, mpegversion=(int){1,2}"},
  {VPU_V_MJPG, "image/jpeg"},
  {VPU_V_VP8, "video/x-vp8"},
  {-1, NULL}
};

enum
{
  AUTO = 0,
  NV12,
  I420,
  YV12,
  TILED,
  TILED_FIELD,
  OUTPUT_FORMAT_MAX
};

GType
gst_vpu_dec_output_format_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      {AUTO, "Decide output frame based on vpudec priority and downstream caps (default)",
          "auto"},
      {NV12, "NV12 format",
          "NV12"},
      {I420, "I420 format",
          "I420"},
      {YV12, "YV12 format",
          "YV12"},
      {TILED, "Tiled format",
          "tiled"},
      {TILED_FIELD, "Tiled field format",
          "tiledfield"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstVpuDecOutputFormat", values);
  }
  return gtype;
}

G_DEFINE_TYPE(GstVpuDecObject, gst_vpu_dec_object, GST_TYPE_OBJECT)

static void gst_vpu_dec_object_finalize(GObject *object);

GstCaps *
gst_vpu_dec_object_get_sink_caps (void)
{
  static GstCaps *caps = NULL;

  if (caps == NULL) {
    VPUMapper *map = vpu_mappers;
    while ((map) && (map->mime)) {
      if (caps) {
        GstCaps *newcaps = gst_caps_from_string (map->mime);
        if (newcaps) {
          if (!gst_caps_is_subset (newcaps, caps)) {
            gst_caps_append (caps, newcaps);
          } else {
            gst_caps_unref (newcaps);
          }
        }
      } else {
        caps = gst_caps_from_string (map->mime);
      }
      map++;
    }
  }

  return gst_caps_ref (caps);
}

GstCaps *
gst_vpu_dec_object_get_src_caps (void)
{
  static GstCaps *caps = NULL;

  if (caps == NULL) {
    caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("{ NV12, I420, YV12, TNVP, TNVF }"));
  }

  return gst_caps_ref (caps);
}

GstVpuDecObject * 
gst_vpu_dec_object_new(void)
{
	GstVpuDecObject *vpu_dec_object;
	vpu_dec_object = g_object_new(gst_vpu_dec_object_get_type(), NULL);
	return vpu_dec_object;
}

void
gst_vpu_dec_object_destroy (GstVpuDecObject * vpu_dec_object)
{
  g_return_if_fail (vpu_dec_object != NULL);

  gst_object_unref (vpu_dec_object);
}

void 
gst_vpu_dec_object_class_init(GstVpuDecObjectClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = GST_DEBUG_FUNCPTR(gst_vpu_dec_object_finalize);

	GST_DEBUG_CATEGORY_INIT(vpu_dec_object_debug, "vpu_dec_object", 0, "VPU object");
}

void 
gst_vpu_dec_object_init(GstVpuDecObject *vpu_dec_object)
{
  vpu_dec_object->state = STATE_NULL;
  vpu_dec_object->input_state = NULL;
  vpu_dec_object->output_state = NULL;
  vpu_dec_object->vpuframebuffers = NULL;
  vpu_dec_object->new_segment = TRUE;
  vpu_dec_object->mosaic_cnt = 0;
  vpu_dec_object->tsm_mode = MODE_AI;
  vpu_dec_object->internal_virt_mem = NULL;
  vpu_dec_object->internal_phy_mem = NULL;
  vpu_dec_object->mv_mem = NULL;
  vpu_dec_object->gstbuffer_in_vpudec = NULL;
  vpu_dec_object->system_frame_number_in_vpu = NULL;
  vpu_dec_object->dropping = FALSE;
}

static void 
gst_vpu_dec_object_finalize(GObject *object)
{
	GstVpuDecObject *dec_object = GST_VPU_DEC_OBJECT (object);

	GST_DEBUG_OBJECT(dec_object, "freeing memory");

	G_OBJECT_CLASS(gst_vpu_dec_object_parent_class)->finalize(object);
}

static gchar const *
gst_vpu_dec_object_strerror(VpuDecRetCode code)
{
  switch (code) {
    case VPU_DEC_RET_SUCCESS: return "success";
    case VPU_DEC_RET_FAILURE: return "failure";
    case VPU_DEC_RET_INVALID_PARAM: return "invalid param";
    case VPU_DEC_RET_INVALID_HANDLE: return "invalid handle";
    case VPU_DEC_RET_INVALID_FRAME_BUFFER: return "invalid frame buffer";
    case VPU_DEC_RET_INSUFFICIENT_FRAME_BUFFERS: return "insufficient frame buffers";
    case VPU_DEC_RET_INVALID_STRIDE: return "invalid stride";
    case VPU_DEC_RET_WRONG_CALL_SEQUENCE: return "wrong call sequence";
    case VPU_DEC_RET_FAILURE_TIMEOUT: return "failure timeout";
    default: return NULL;
  }
}

gboolean
gst_vpu_dec_object_open (GstVpuDecObject * vpu_dec_object)
{
	VpuDecRetCode ret;

	ret = VPU_DecLoad();
	if (ret != VPU_DEC_RET_SUCCESS) {
		GST_ERROR_OBJECT(vpu_dec_object, "VPU_DecLoad fail: %s", \
                gst_vpu_dec_object_strerror(ret));
		return FALSE;
	}

  vpu_dec_object->state = STATE_LOADED;

  return TRUE;
}

gboolean
gst_vpu_dec_object_close (GstVpuDecObject * vpu_dec_object)
{
	VpuDecRetCode ret;

	ret = VPU_DecUnLoad();
	if (ret != VPU_DEC_RET_SUCCESS) {
		GST_ERROR_OBJECT(vpu_dec_object, "VPU_DecUnLoad fail: %s", \
                gst_vpu_dec_object_strerror(ret));
		return FALSE;
	}

  vpu_dec_object->state = STATE_NULL;

  return TRUE;
}

static gboolean
gst_vpu_dec_object_free_internal_buffer (GstVpuDecObject * vpu_dec_object)
{
  g_list_foreach (vpu_dec_object->internal_virt_mem, (GFunc) g_free, NULL);
  g_list_free (vpu_dec_object->internal_virt_mem);
  vpu_dec_object->internal_virt_mem = NULL;
  g_list_foreach (vpu_dec_object->internal_phy_mem, (GFunc) gst_memory_unref, NULL);
  g_list_free (vpu_dec_object->internal_phy_mem);
  vpu_dec_object->internal_phy_mem = NULL;

  return TRUE;
}
 
static gboolean
gst_vpu_dec_object_allocate_internal_buffer (GstVpuDecObject * vpu_dec_object)
{
  GstAllocationParams params;
  GstMemory * gst_memory;
  PhyMemBlock *memory;
	gint size;
	guint8 *ptr;
        gint i;

	memset(&params, 0, sizeof(GstAllocationParams));
	for (i = 0; i < vpu_dec_object->mem_info.nSubBlockNum; ++i) {
		size = vpu_dec_object->mem_info.MemSubBlock[i].nAlignment \
           + vpu_dec_object->mem_info.MemSubBlock[i].nSize;
		GST_DEBUG_OBJECT(vpu_dec_object, "sub block %d  type: %s  size: %d", i, \
        (vpu_dec_object->mem_info.MemSubBlock[i].MemType == VPU_MEM_VIRT) ? \
        "virtual" : "phys", size);
 
		if (vpu_dec_object->mem_info.MemSubBlock[i].MemType == VPU_MEM_VIRT) {
      ptr = g_malloc(size);
      if (ptr == NULL) {
        GST_ERROR_OBJECT (vpu_dec_object, "Could not allocate memory");
        return FALSE;
      }

			vpu_dec_object->mem_info.MemSubBlock[i].pVirtAddr = (unsigned char*)ALIGN( \
          ptr, vpu_dec_object->mem_info.MemSubBlock[i].nAlignment);
      vpu_dec_object->internal_virt_mem = g_list_append (vpu_dec_object->internal_virt_mem, ptr);
		} else if (vpu_dec_object->mem_info.MemSubBlock[i].MemType == VPU_MEM_PHY) {
      params.align = vpu_dec_object->mem_info.MemSubBlock[i].nAlignment - 1;
      gst_memory = gst_allocator_alloc (gst_vpu_allocator_obtain(), size, &params);
      memory = gst_memory_query_phymem_block (gst_memory);
      if (memory == NULL) {
        GST_ERROR_OBJECT (vpu_dec_object, "Could not allocate memory using VPU allocator");
        return FALSE;
      }

			vpu_dec_object->mem_info.MemSubBlock[i].pVirtAddr = memory->vaddr;
			vpu_dec_object->mem_info.MemSubBlock[i].pPhyAddr = memory->paddr;
      vpu_dec_object->internal_phy_mem = g_list_append (vpu_dec_object->internal_phy_mem, gst_memory);
		} else {
			GST_WARNING_OBJECT(vpu_dec_object, "sub block %d type is unknown - skipping", i);
		}
 	}

  return TRUE;
}

static gboolean
gst_vpu_dec_object_init_qos (GstVpuDecObject * vpu_dec_object)
{

  return TRUE;
}

gboolean
gst_vpu_dec_object_start (GstVpuDecObject * vpu_dec_object)
{
  VpuDecRetCode ret;
  VpuVersionInfo version;
  VpuWrapperVersionInfo wrapper_version;

  ret = VPU_DecGetVersionInfo(&version);
  if (ret != VPU_DEC_RET_SUCCESS) {
    GST_WARNING_OBJECT(vpu_dec_object, "VPU_DecGetVersionInfo fail: %s", \
        gst_vpu_dec_object_strerror(ret));
  }

  ret = VPU_DecGetWrapperVersionInfo(&wrapper_version);
  if (ret != VPU_DEC_RET_SUCCESS) {
    GST_WARNING_OBJECT(vpu_dec_object, "VPU_DecGetWrapperVersionInfo fail: %s", \
        gst_vpu_dec_object_strerror(ret));
  }

  GST_INFO_OBJECT(vpu_dec_object, "VPU firmware version %d.%d.%d_r%d", \
      version.nFwMajor, version.nFwMinor, version.nFwRelease, version.nFwCode);
  GST_INFO_OBJECT(vpu_dec_object, "VPU library version %d.%d.%d", version.nLibMajor, \
      version.nLibMinor, version.nLibRelease);
  GST_INFO_OBJECT(vpu_dec_object, "VPU wrapper version %d.%d.%d %s", \
      wrapper_version.nMajor, wrapper_version.nMinor, wrapper_version.nRelease, \
      wrapper_version.pBinary);

  /* mem_info contains information about how to set up memory blocks
   * the VPU uses as temporary storage (they are "work buffers") */
  memset(&(vpu_dec_object->mem_info), 0, sizeof(VpuMemInfo));
  ret = VPU_DecQueryMem(&(vpu_dec_object->mem_info));
  if (ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "could not get VPU memory information: %s", \
        gst_vpu_dec_object_strerror(ret));
    return FALSE;
  }

  if (!gst_vpu_dec_object_allocate_internal_buffer(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_allocate_internal_buffer fail");
    return FALSE;
  }

  vpu_dec_object->tsm = createTSManager (VPUDEC_TS_BUFFER_LENGTH_DEFAULT);

  if (!gst_vpu_dec_object_init_qos(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_init_qos fail");
    return FALSE;
  }

  vpu_dec_object->frame2gstbuffer_table = g_hash_table_new(NULL, NULL);
  vpu_dec_object->gstbuffer2frame_table = g_hash_table_new(NULL, NULL);

  vpu_dec_object->state = STATE_ALLOCATED_INTERNAL_BUFFER;

  return TRUE;
}

static gboolean
gst_vpu_dec_object_free_mv_buffer (GstVpuDecObject * vpu_dec_object)
{
  g_list_foreach (vpu_dec_object->mv_mem, (GFunc) gst_memory_unref, NULL);
  g_list_free (vpu_dec_object->mv_mem);
  vpu_dec_object->mv_mem = NULL;

  if (vpu_dec_object->vpuframebuffers != NULL) {
    g_free(vpu_dec_object->vpuframebuffers);
    vpu_dec_object->vpuframebuffers = NULL;
  }

  return TRUE;
}

static gboolean
gst_vpu_dec_object_allocate_mv_buffer (GstVpuDecObject * vpu_dec_object)
{
  VpuFrameBuffer *vpu_frame;
  GstMemory * gst_memory;
  PhyMemBlock *memory;
  gint size;
  guint i;

  vpu_dec_object->vpuframebuffers = (VpuFrameBuffer *)g_malloc ( \
      sizeof (VpuFrameBuffer) * vpu_dec_object->actual_buf_cnt);
  if (vpu_dec_object->vpuframebuffers == NULL) {
    GST_ERROR_OBJECT (vpu_dec_object, "Could not allocate memory");
    return FALSE;
  }
  memset (vpu_dec_object->vpuframebuffers, 0, sizeof (VpuFrameBuffer) \
      * vpu_dec_object->actual_buf_cnt);

  for (i=0; i<vpu_dec_object->actual_buf_cnt; i++) {
    vpu_frame = &vpu_dec_object->vpuframebuffers[i];
    size = vpu_dec_object->width_paded * vpu_dec_object->height_paded / 4;
    gst_memory = gst_allocator_alloc (gst_vpu_allocator_obtain(), size, NULL);
    memory = gst_memory_query_phymem_block (gst_memory);
    if (memory == NULL) {
      GST_ERROR_OBJECT (vpu_dec_object, "Could not allocate memory using VPU allocator");
      return FALSE;
    }

    vpu_frame->pbufMvCol = memory->paddr;
    vpu_frame->pbufVirtMvCol = memory->vaddr;
    vpu_dec_object->mv_mem = g_list_append (vpu_dec_object->mv_mem, gst_memory);
  }

  return TRUE;
}

gboolean
gst_vpu_dec_object_stop (GstVpuDecObject * vpu_dec_object)
{
  VpuDecRetCode dec_ret;

  if (vpu_dec_object->gstbuffer_in_vpudec != NULL) {
    g_list_foreach (vpu_dec_object->gstbuffer_in_vpudec, (GFunc) gst_memory_unref, NULL);
    g_list_free (vpu_dec_object->gstbuffer_in_vpudec);
    vpu_dec_object->gstbuffer_in_vpudec = NULL;
  }

  if (vpu_dec_object->system_frame_number_in_vpu != NULL) {
    g_list_free (vpu_dec_object->system_frame_number_in_vpu);
    vpu_dec_object->system_frame_number_in_vpu = NULL;
  }

	if (vpu_dec_object->frame2gstbuffer_table != NULL) {
		g_hash_table_destroy(vpu_dec_object->frame2gstbuffer_table);
		vpu_dec_object->frame2gstbuffer_table = NULL;
	}

	if (vpu_dec_object->gstbuffer2frame_table != NULL) {
		g_hash_table_destroy(vpu_dec_object->gstbuffer2frame_table);
		vpu_dec_object->gstbuffer2frame_table = NULL;
	}

  if (vpu_dec_object->tsm) {
    destroyTSManager (vpu_dec_object->tsm);
    vpu_dec_object->tsm = NULL;
  }

  dec_ret = VPU_DecClose(vpu_dec_object->handle);
  if (dec_ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "closing decoder failed: %s", \
        gst_vpu_dec_object_strerror(dec_ret));
    return FALSE;
  }

  if (!gst_vpu_dec_object_free_mv_buffer(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_free_mv_buffer fail");
    return FALSE;
  }

  if (!gst_vpu_dec_object_free_internal_buffer(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_free_internal_buffer fail");
    return FALSE;
  }

  if (vpu_dec_object->input_state) {
    gst_video_codec_state_unref (vpu_dec_object->input_state);
    vpu_dec_object->input_state = NULL;
  }
  if (vpu_dec_object->output_state) {
    gst_video_codec_state_unref (vpu_dec_object->output_state);
    vpu_dec_object->output_state = NULL;
  }

  vpu_dec_object->state = STATE_LOADED;

  return TRUE;
}

static gint
gst_vpu_dec_object_find_std (GstCaps * caps)
{
  VPUMapper *mapper = vpu_mappers;

  while (mapper->mime) {
    GstCaps *scaps = gst_caps_from_string (mapper->mime);
    if (scaps) {
      if (gst_caps_is_subset (caps, scaps)) {
        gst_caps_unref (scaps);
        return mapper->std;
      }
      gst_caps_unref (scaps);
    }
    mapper++;
  }

  return -1;
}

static void
gst_vpu_dec_object_decide_output_format (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec)
{
  GstCaps *peer_caps;
  
  if (vpu_dec_object->output_format != AUTO) {
    switch (vpu_dec_object->output_format) {
      case NV12: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_NV12; break;
      case I420: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_I420; break;
      case YV12: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_YV12; break;
                 //case TNVP: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_TNVP; break;
                 //case TNVF: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_TNVF; break;
      default: GST_WARNING_OBJECT(vpu_dec_object, "unknown output format"); break;
    }

    return;
  }

  peer_caps = gst_pad_get_allowed_caps (GST_VIDEO_DECODER_SRC_PAD(bdec));
  if (G_LIKELY (peer_caps)) {
    guint i = 0, j = 0, n = 0;

    n = gst_caps_get_size (peer_caps);
    GST_DEBUG_OBJECT (vpu_dec_object, "peer allowed caps (%u structure(s)) are %"
        GST_PTR_FORMAT, n, peer_caps);

    for (i = 0; i < n; i++) {
      GstStructure *s;
      const gchar *fmt_str;
      const GValue *format, *fmt_val;

      s = gst_caps_get_structure (peer_caps, i);
      if (!gst_structure_has_name (s, "video/x-raw"))
        continue;

      format = gst_structure_get_value (s, "format");
      if (format == NULL)
        continue;

      if (GST_VALUE_HOLDS_LIST (format)) {
        for (j = 0; j < gst_value_list_get_size (format); ++j) {
          fmt_val = gst_value_list_get_value (format, j);
          fmt_str = g_value_get_string (fmt_val);
          GST_DEBUG_OBJECT (vpu_dec_object, "format string: '%s'", fmt_str);
          vpu_dec_object->output_format_decided = gst_video_format_from_string (fmt_str);
          gst_caps_unref (peer_caps);
          return;
        }
      } else if (G_VALUE_HOLDS_STRING (format)) {
        fmt_str = g_value_get_string (format);
        GST_DEBUG_OBJECT (vpu_dec_object, "format string: '%s'", fmt_str);
        vpu_dec_object->output_format_decided = gst_video_format_from_string (fmt_str);
        gst_caps_unref (peer_caps);
        return;
      }
    }

    gst_caps_unref (peer_caps);
  }
}

static gboolean 
gst_vpu_dec_object_set_vpu_param (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec, GstVideoCodecState *state, VpuDecOpenParam *open_param)
{
  GstVideoInfo *info = &state->info;

  open_param->CodecFormat = gst_vpu_dec_object_find_std (state->caps);
  if (open_param->CodecFormat < 0) {
    GST_ERROR_OBJECT(vpu_dec_object, "can't find VPU supported format");
    return FALSE;
  }

  GST_INFO_OBJECT (vpu_dec_object, "Get codec std %d", open_param->CodecFormat);
  vpu_dec_object->framerate_n = GST_VIDEO_INFO_FPS_N (info);
  vpu_dec_object->framerate_d = GST_VIDEO_INFO_FPS_D (info);

  open_param->nChromaInterleave = 0;
  open_param->nMapType = 0;
  open_param->nTiled2LinearEnable = 0;
  vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_I420;
  vpu_dec_object->is_mjpeg = FALSE;
  if (open_param->CodecFormat != VPU_V_MJPG) {
    gst_vpu_dec_object_decide_output_format(vpu_dec_object, bdec);
    if (vpu_dec_object->output_format_decided == GST_VIDEO_FORMAT_NV12) {
      open_param->nChromaInterleave = 1;
    } 
    /*else if (!strcmp (format, "TNVP")) {
      //FIXME:vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_TNVP;
      open_param->nChromaInterleave = 1;
      open_param->nMapType = 1;
    } else if (!strcmp (format, "TNVF")) {
      //FIXME:vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_TNVF;
      open_param->nChromaInterleave = 1;
      open_param->nMapType = 2;
    }*/
  } else {
    vpu_dec_object->is_mjpeg = TRUE;
  }

  open_param->nReorderEnable = 1;
  open_param->nEnableFileMode = 0;
  open_param->nPicWidth = GST_VIDEO_INFO_WIDTH(info);
  open_param->nPicHeight = GST_VIDEO_INFO_HEIGHT(info);

  return TRUE;
}

static gboolean
gst_vpu_dec_object_open_vpu (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec, GstVideoCodecState * state)
{
  VpuDecRetCode ret;
  VpuDecOpenParam open_param;
  int config_param;
  int capability=0;

  memset(&open_param, 0, sizeof(open_param));
  if (!gst_vpu_dec_object_set_vpu_param(vpu_dec_object, bdec, state, &open_param)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_set_vpu_param fail");
    return FALSE;
  }

  ret = VPU_DecOpen(&(vpu_dec_object->handle), &open_param, &(vpu_dec_object->mem_info));
  if (ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "opening new VPU handle failed: %s", \
        gst_vpu_dec_object_strerror(ret));
    return FALSE;
  }

  vpu_dec_object->use_new_tsm = FALSE;
  ret=VPU_DecGetCapability(vpu_dec_object->handle, VPU_DEC_CAP_FRAMESIZE, &capability);
  if((ret==VPU_DEC_RET_SUCCESS)&&capability) {
    vpu_dec_object->use_new_tsm = TRUE;
  }

  config_param = VPU_DEC_SKIPNONE;
	ret = VPU_DecConfig(vpu_dec_object->handle, VPU_DEC_CONF_SKIPMODE, &config_param);
	if (ret != VPU_DEC_RET_SUCCESS) {
		GST_ERROR_OBJECT(vpu_dec_object, "could not configure skip mode: %s", \
        gst_vpu_dec_object_strerror(ret));
		return FALSE;
	}

	config_param = 0;
	ret = VPU_DecConfig(vpu_dec_object->handle, VPU_DEC_CONF_BUFDELAY, &config_param);
	if (ret != VPU_DEC_RET_SUCCESS) {
		GST_ERROR_OBJECT(vpu_dec_object, "could not configure buffer delay: %s", \
        gst_vpu_dec_object_strerror(ret));
		return FALSE;
	}

  vpu_dec_object->state = STATE_OPENED;

  return TRUE;
}

gboolean
gst_vpu_dec_object_config (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec, GstVideoCodecState * state)
{
  VpuDecRetCode dec_ret;

  if (state) {
    /* Keep a copy of the input state */
    if (vpu_dec_object->input_state) {
      gst_video_codec_state_unref (vpu_dec_object->input_state);
    }
    vpu_dec_object->input_state = gst_video_codec_state_ref (state);
  } else {
    state = vpu_dec_object->input_state;
  }

  if (vpu_dec_object->state >= STATE_REGISTRIED_FRAME_BUFFER) {
    dec_ret = VPU_DecClose(vpu_dec_object->handle);
    if (dec_ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "closing decoder failed: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return FALSE;
    }

    if (!gst_vpu_dec_object_free_mv_buffer(vpu_dec_object)) {
      GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_free_mv_buffer fail");
      return GST_FLOW_ERROR;
    }

    vpu_dec_object->state = STATE_ALLOCATED_INTERNAL_BUFFER;
  }

  g_list_foreach (vpu_dec_object->gstbuffer_in_vpudec, (GFunc) gst_memory_unref, NULL);
  g_list_free (vpu_dec_object->gstbuffer_in_vpudec);
  vpu_dec_object->gstbuffer_in_vpudec = NULL;
  GST_DEBUG_OBJECT (vpu_dec_object, "gstbuffer_in_vpudec list free\n");

  if (vpu_dec_object->state < STATE_OPENED) {
    if (!gst_vpu_dec_object_open_vpu(vpu_dec_object, bdec, state)) {
      GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_open_vpu fail");
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_vpu_dec_object_register_frame_buffer (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec)
{
	VpuDecRetCode dec_ret;
  VpuFrameBuffer *vpu_frame;
  GstVideoFrame frame;
  PhyMemBlock * mem_block;
  GstBuffer *buffer;
  GstVideoInfo *info = &vpu_dec_object->output_state->info;
  guint i;

  g_hash_table_remove_all (vpu_dec_object->frame2gstbuffer_table);
  g_hash_table_remove_all (vpu_dec_object->gstbuffer2frame_table);
 
  for (i=0; i<vpu_dec_object->actual_buf_cnt; i++) {
    buffer = g_list_nth_data (vpu_dec_object->gstbuffer_in_vpudec, i);
    GST_DEBUG_OBJECT (vpu_dec_object, "gstbuffer index: %d get from list: %x\n", \
        i, buffer);
    vpu_frame = &vpu_dec_object->vpuframebuffers[i];

    if (!gst_video_frame_map (&frame, info, buffer, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (vpu_dec_object, "Could not map video buffer");
      return FALSE;
    }
    vpu_frame->nStrideY = GST_VIDEO_FRAME_COMP_STRIDE (&frame, 0);
    vpu_frame->nStrideC = GST_VIDEO_FRAME_COMP_STRIDE (&frame, 1);

    if (!gst_buffer_is_phymem (buffer)) {
      GST_ERROR_OBJECT (vpu_dec_object, "isn't physical memory allocator");
      gst_video_frame_unmap (&frame);
      return FALSE;
    }

    mem_block = gst_buffer_query_phymem_block (buffer);
    //FIXME: for tiled format.
    vpu_frame->pbufY = mem_block->paddr;
    vpu_frame->pbufCb = vpu_frame->pbufY + \
      (GST_VIDEO_FRAME_COMP_DATA (&frame, 1) - GST_VIDEO_FRAME_COMP_DATA (&frame, 0));
    vpu_frame->pbufCr = vpu_frame->pbufCb + \
      (GST_VIDEO_FRAME_COMP_DATA (&frame, 2) - GST_VIDEO_FRAME_COMP_DATA (&frame, 1));
    //unsigned char* pbufY_tilebot;	//for field tile: luma bottom pointer
    //unsigned char* pbufCb_tilebot;	//for field tile: chroma bottom pointer

    vpu_frame->pbufVirtY = GST_VIDEO_FRAME_COMP_DATA (&frame, 0);
    vpu_frame->pbufVirtCb = GST_VIDEO_FRAME_COMP_DATA (&frame, 1);
    vpu_frame->pbufVirtCr = GST_VIDEO_FRAME_COMP_DATA (&frame, 2);
    //unsigned char* pbufVirtY_tilebot;	//for field tile: luma bottom pointer
    //unsigned char* pbufVirtCb_tilebot;	//for field tile: chroma bottom pointer

    gst_video_frame_unmap (&frame);

    g_hash_table_replace(vpu_dec_object->frame2gstbuffer_table, \
        (gpointer)(vpu_frame->pbufVirtY), (gpointer)(buffer));
    GST_DEBUG_OBJECT (vpu_dec_object, "VpuFrameBuffer: 0x%x VpuFrameBuffer pbufVirtY: 0x%x GstBuffer: 0x%x\n", \
        vpu_frame, vpu_frame->pbufVirtY, buffer);
  }

	dec_ret = VPU_DecRegisterFrameBuffer (vpu_dec_object->handle, \
      vpu_dec_object->vpuframebuffers, vpu_dec_object->actual_buf_cnt);
	if (dec_ret != VPU_DEC_RET_SUCCESS) {
		GST_ERROR_OBJECT(vpu_dec_object, "registering framebuffers failed: %s", \
        gst_vpu_dec_object_strerror(dec_ret));
		return FALSE;
	}

  vpu_dec_object->state = STATE_REGISTRIED_FRAME_BUFFER;

  return TRUE;
}

static GstFlowReturn
gst_vpu_dec_object_handle_reconfig(GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec)
{
  VpuDecRetCode dec_ret;
  GstVideoCodecState *state;
  GstVideoFormat fmt;
  gint height_align;
  GstBuffer *buffer;
  guint i;

  dec_ret = VPU_DecGetInitialInfo(vpu_dec_object->handle, &(vpu_dec_object->init_info));
  if (dec_ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "could not get init info: %s", gst_vpu_dec_object_strerror(dec_ret));
    return GST_FLOW_ERROR;
  }

  if (vpu_dec_object->is_mjpeg) {
    switch (vpu_dec_object->init_info.nMjpgSourceFormat) {
      case 0: fmt = GST_VIDEO_FORMAT_I420; break;
      case 1: fmt = GST_VIDEO_FORMAT_Y42B; break;
      case 3: fmt = GST_VIDEO_FORMAT_Y444; break;
      default:
              GST_ERROR_OBJECT(vpu_dec_object, "unsupported MJPEG output format %d", \
                  vpu_dec_object->init_info.nMjpgSourceFormat);
              return GST_FLOW_ERROR;
    }
  }
  else
    fmt = vpu_dec_object->output_format_decided;

  GST_INFO_OBJECT(vpu_dec_object, "using %s as video output format", gst_video_format_to_string(fmt));

  /* Create the output state */
  //FIXME: set max resolution to avoid buffer reallocate when resolution change.
  vpu_dec_object->output_state = state =
    gst_video_decoder_set_output_state (bdec, fmt, vpu_dec_object->init_info.nPicWidth, \
        vpu_dec_object->init_info.nPicHeight, vpu_dec_object->input_state);

  vpu_dec_object->min_buf_cnt = vpu_dec_object->init_info.nMinFrameBufferCount;
  GST_VIDEO_INFO_WIDTH (&(state->info)) = vpu_dec_object->init_info.nPicWidth;
  GST_VIDEO_INFO_HEIGHT (&(state->info)) = vpu_dec_object->init_info.nPicHeight;
  GST_VIDEO_INFO_INTERLACE_MODE(&(state->info)) = \
    vpu_dec_object->init_info.nInterlace ? GST_VIDEO_INTERLACE_MODE_INTERLEAVED \
    : GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  vpu_dec_object->buf_align = vpu_dec_object->init_info.nAddressAlignment;
  memset(&(vpu_dec_object->video_align), 0, sizeof(GstVideoAlignment));
  if (vpu_dec_object->init_info.nPicWidth % DEFAULT_FRAME_BUFFER_ALIGNMENT_H)
    vpu_dec_object->video_align.padding_right = DEFAULT_FRAME_BUFFER_ALIGNMENT_H \
      - vpu_dec_object->init_info.nPicWidth % DEFAULT_FRAME_BUFFER_ALIGNMENT_H;
  height_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_V;
  if (vpu_dec_object->init_info.nInterlace)
    height_align <<= 1;
  if (vpu_dec_object->init_info.nPicHeight % height_align)
    vpu_dec_object->video_align.padding_bottom = height_align \
      - vpu_dec_object->init_info.nPicHeight % height_align;

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++)
    vpu_dec_object->video_align.stride_align[i] = DEFAULT_FRAME_BUFFER_ALIGNMENT_H - 1;

  vpu_dec_object->width_paded = vpu_dec_object->init_info.nPicWidth \
                                + vpu_dec_object->video_align.padding_right;
  vpu_dec_object->height_paded = vpu_dec_object->init_info.nPicHeight \
                              + vpu_dec_object->video_align.padding_bottom;
 
  GST_DEBUG_OBJECT (vpu_dec_object, "width: %d height: %d paded width: %d paded height: %d\n", \
      vpu_dec_object->init_info.nPicWidth, vpu_dec_object->init_info.nPicHeight, \
      vpu_dec_object->width_paded, vpu_dec_object->height_paded);

  gst_video_decoder_negotiate (bdec);

  while (g_list_length (vpu_dec_object->gstbuffer_in_vpudec) \
          < vpu_dec_object->actual_buf_cnt) {
    GST_DEBUG_OBJECT (vpu_dec_object, "gst_video_decoder_allocate_output_buffer before");
    buffer = gst_video_decoder_allocate_output_buffer(bdec);
    vpu_dec_object->gstbuffer_in_vpudec = g_list_append ( \
        vpu_dec_object->gstbuffer_in_vpudec, buffer);
    GST_DEBUG_OBJECT (vpu_dec_object, "gst_video_decoder_allocate_output_buffer end");
    GST_DEBUG_OBJECT (vpu_dec_object, "gstbuffer get from buffer pool: %x\n", buffer);
    GST_DEBUG_OBJECT (vpu_dec_object, "gstbuffer_in_vpudec list length: %d\n", \
        g_list_length (vpu_dec_object->gstbuffer_in_vpudec));
  }

  if (!gst_vpu_dec_object_allocate_mv_buffer(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_allocate_mv_buffer fail");
    return GST_FLOW_ERROR;
  }

  if (!gst_vpu_dec_object_register_frame_buffer (vpu_dec_object, bdec)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_register_frame_buffer fail");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_vpu_dec_object_release_frame_buffer_to_vpu (GstVpuDecObject * vpu_dec_object, \
    GstBuffer *buffer)
{
  VpuDecRetCode dec_ret;
  VpuFrameBuffer * frame_buffer;

  vpu_dec_object->gstbuffer_in_vpudec = g_list_append ( \
      vpu_dec_object->gstbuffer_in_vpudec, buffer);
  frame_buffer = g_hash_table_lookup(vpu_dec_object->gstbuffer2frame_table, buffer);

  GST_LOG_OBJECT (vpu_dec_object, "GstBuffer: 0x%x VpuFrameBuffer: 0x%x\n", \
      buffer, frame_buffer);
  dec_ret = VPU_DecOutFrameDisplayed(vpu_dec_object->handle, frame_buffer);
  if (dec_ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "clearing display framebuffer failed: %s", \
        gst_vpu_dec_object_strerror(dec_ret));
    return GST_FLOW_ERROR;
  }

  return TRUE;
}

static gboolean
gst_vpu_dec_object_process_qos (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec, GstVideoCodecFrame * frame)
{
  int config_param;
  VpuDecRetCode ret;

  if (frame) {
    GstClockTimeDiff diff = gst_video_decoder_get_max_decode_time (bdec, frame);
    GST_DEBUG_OBJECT(vpu_dec_object, "diff: %lld\n", diff);
    if (diff < 0) {
      if (vpu_dec_object->dropping == FALSE) { 
        GST_WARNING_OBJECT(vpu_dec_object, "decoder can't catch up. need drop frame.\n");
        config_param = VPU_DEC_SKIPB;
        ret = VPU_DecConfig(vpu_dec_object->handle, VPU_DEC_CONF_SKIPMODE, &config_param);
        if (ret != VPU_DEC_RET_SUCCESS) {
          GST_ERROR_OBJECT(vpu_dec_object, "could not configure skip mode: %s", \
              gst_vpu_dec_object_strerror(ret));
          return FALSE;
        }
        vpu_dec_object->dropping = TRUE;
      }
    } else if (vpu_dec_object->dropping == TRUE && diff != G_MAXINT64 \
        && diff > DROP_RESUME) {
      config_param = VPU_DEC_SKIPNONE;
      ret = VPU_DecConfig(vpu_dec_object->handle, VPU_DEC_CONF_SKIPMODE, &config_param);
      if (ret != VPU_DEC_RET_SUCCESS) {
        GST_ERROR_OBJECT(vpu_dec_object, "could not configure skip mode: %s", \
            gst_vpu_dec_object_strerror(ret));
        return FALSE;
      }
      GST_WARNING_OBJECT(vpu_dec_object, "decoder can catch up. needn't drop frame. diff: %lld\n", diff);
      vpu_dec_object->dropping = FALSE;
    }
  }

  return TRUE;
}

static GstFlowReturn
gst_vpu_dec_object_send_output (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec, gboolean drop)
{
  GstFlowReturn ret = GST_FLOW_OK;
  VpuDecRetCode dec_ret;
  VpuDecOutFrameInfo out_frame_info;
  GstVideoCodecFrame *out_frame;
  GstVideoMeta *vmeta;
  GstVideoCropMeta *cmeta;
  gint frame_number;
#if 0
  GList *l;

  l = gst_video_decoder_get_frames (bdec);
  if (g_list_length (l) > vpu_dec_object->actual_buf_cnt) {
    GST_DEBUG_OBJECT(vpu_dec_object, "video frame list too long: %d \n", \
        g_list_length (l));
  }

  g_list_foreach (l, (GFunc) gst_video_codec_frame_unref, NULL);
  g_list_free (l);
#endif

  frame_number = g_list_nth_data (vpu_dec_object->system_frame_number_in_vpu, 0);
  GST_DEBUG_OBJECT(vpu_dec_object, "system frame number send out: %d list length: %d \n", \
      frame_number, g_list_length (vpu_dec_object->system_frame_number_in_vpu));
  vpu_dec_object->system_frame_number_in_vpu = g_list_remove ( \
      vpu_dec_object->system_frame_number_in_vpu, frame_number);

  out_frame = gst_video_decoder_get_frame (bdec, frame_number);
  if (out_frame == NULL) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_video_decoder_get_frame failed.");
    return GST_FLOW_ERROR;
  }
  GST_LOG_OBJECT (vpu_dec_object, "gst_video_decoder_get_frame: 0x%x\n", \
      out_frame);
  gst_vpu_dec_object_process_qos (vpu_dec_object, bdec, out_frame);
 
  if (drop == FALSE) {
    dec_ret = VPU_DecGetOutputFrame(vpu_dec_object->handle, &out_frame_info);
    if (dec_ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "could not get decoded output frame: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return GST_FLOW_ERROR;
    }
  }

  if (((vpu_dec_object->mosaic_cnt != 0)
      && (vpu_dec_object->mosaic_cnt < MASAIC_THRESHOLD))
      || drop == TRUE) {
    GST_INFO_OBJECT(vpu_dec_object, "drop frame.");
    TSManagerSend (vpu_dec_object->tsm);
    return gst_video_decoder_drop_frame (bdec, out_frame);
  }

  GST_LOG_OBJECT(vpu_dec_object, "vpu display buffer: 0x%x pbufVirtY: 0x%x\n", \
      out_frame_info.pDisplayFrameBuf, out_frame_info.pDisplayFrameBuf->pbufVirtY);
  out_frame->pts = TSManagerSend2 (vpu_dec_object->tsm, \
      out_frame_info.pDisplayFrameBuf);
  out_frame->output_buffer = g_hash_table_lookup( \
      vpu_dec_object->frame2gstbuffer_table, \
      out_frame_info.pDisplayFrameBuf->pbufVirtY);
  g_hash_table_replace(vpu_dec_object->gstbuffer2frame_table, \
      (gpointer)(out_frame->output_buffer), \
      (gpointer)(out_frame_info.pDisplayFrameBuf));
  vpu_dec_object->gstbuffer_in_vpudec = g_list_remove ( \
      vpu_dec_object->gstbuffer_in_vpudec, out_frame->output_buffer);

  vmeta = gst_buffer_get_video_meta (out_frame->output_buffer);
  /* If the buffer pool didn't add the meta already
   * we add it ourselves here */
  if (!vmeta)
    vmeta = gst_buffer_add_video_meta (out_frame->output_buffer, \
        GST_VIDEO_FRAME_FLAG_NONE, \
        vpu_dec_object->output_state->info.finfo->format, \
        vpu_dec_object->output_state->info.width, \
        vpu_dec_object->output_state->info.height);

  /* set field info */
  switch (out_frame_info.eFieldType) {
    case VPU_FIELD_NONE: vmeta->flags = GST_VIDEO_FRAME_FLAG_NONE; break;
    case VPU_FIELD_TOP: vmeta->flags = GST_VIDEO_FRAME_FLAG_ONEFIELD; break;
    case VPU_FIELD_BOTTOM: vmeta->flags = GST_VIDEO_FRAME_FLAG_ONEFIELD; break;
    case VPU_FIELD_TB: vmeta->flags = GST_VIDEO_FRAME_FLAG_TFF; break;
    case VPU_FIELD_BT: vmeta->flags = GST_VIDEO_FRAME_FLAG_INTERLACED; break;
    default: GST_WARNING_OBJECT(vpu_dec_object, "unknown field type"); break;
  }

  /* set crop info */
  cmeta = gst_buffer_add_video_crop_meta (out_frame->output_buffer);
  cmeta->x = out_frame_info.pExtInfo->FrmCropRect.nLeft;
  cmeta->y = out_frame_info.pExtInfo->FrmCropRect.nTop;
  cmeta->width = out_frame_info.pExtInfo->FrmCropRect.nRight-out_frame_info.pExtInfo->FrmCropRect.nLeft;
  cmeta->height = out_frame_info.pExtInfo->FrmCropRect.nBottom-out_frame_info.pExtInfo->FrmCropRect.nTop;

  GST_DEBUG_OBJECT (vpu_dec_object, "vpu dec output frame time stamp: %" \
      GST_TIME_FORMAT, GST_TIME_ARGS (out_frame->pts));

  ret = gst_video_decoder_finish_frame (bdec, out_frame);

  return ret;
}

static gboolean
gst_vpu_dec_object_set_tsm_consumed_len (GstVpuDecObject * vpu_dec_object)
{
  VpuDecRetCode dec_ret;
  VpuDecFrameLengthInfo dec_framelen_info;

  dec_ret = VPU_DecGetConsumedFrameInfo(vpu_dec_object->handle, &dec_framelen_info);
  if (dec_ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "could not get information about consumed frame: %s", \
        gst_vpu_dec_object_strerror(dec_ret));
    return FALSE;
  }

  TSManagerValid2 (vpu_dec_object->tsm, dec_framelen_info.nFrameLength + \
      dec_framelen_info.nStuffLength, dec_framelen_info.pFrame);

  return TRUE;
}

static gboolean
gst_vpu_dec_object_handle_input_time_stamp (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec, GstVideoCodecFrame * frame)
{
  GstBuffer *buffer;
  GstMapInfo minfo;

  if (frame == NULL) {
    return TRUE;
  }

  buffer = frame->input_buffer;
  gst_buffer_map (buffer, &minfo, GST_MAP_READ);

  if (buffer) {
    GST_LOG ("Chain in with size = %d", minfo.size);

    if (G_UNLIKELY ((vpu_dec_object->new_segment))) {
      gdouble rate = bdec->input_segment.rate;

      if ((rate <= MAX_RATE_FOR_NORMAL_PLAYBACK) && (rate >= MIN_RATE_FOR_NORMAL_PLAYBACK)) {
        vpu_dec_object->tsm_mode = MODE_AI;
      } else {
        vpu_dec_object->tsm_mode = MODE_FIFO;
      }

      if ((buffer) && (GST_BUFFER_TIMESTAMP_IS_VALID (buffer))) {
        resyncTSManager (vpu_dec_object->tsm, GST_BUFFER_TIMESTAMP (buffer),
            vpu_dec_object->tsm_mode);
      }
      vpu_dec_object->new_segment = FALSE;
    }

    if (vpu_dec_object->use_new_tsm) {
      TSManagerReceive2 (vpu_dec_object->tsm, GST_BUFFER_TIMESTAMP (buffer),
          minfo.size);
    } else {
      TSManagerReceive (vpu_dec_object->tsm, GST_BUFFER_TIMESTAMP (buffer));
    }
  }

  gst_buffer_unmap (buffer, &minfo);

  return TRUE;
}

static gboolean
gst_vpu_dec_object_set_vpu_input_buf (GstVpuDecObject * vpu_dec_object, \
    GstVideoCodecFrame * frame, VpuBufferNode *vpu_buffer_node)
{
  GstBuffer * buffer;
  GstMapInfo minfo;

  if (frame == NULL) {
    GST_DEBUG_OBJECT (vpu_dec_object, "vpu_dec_object received eos\n");
    vpu_buffer_node->nSize = 0;
    vpu_buffer_node->pVirAddr = (unsigned char *) 0x1;
 
    return TRUE;
  }

  vpu_dec_object->system_frame_number_in_vpu = g_list_append ( \
      vpu_dec_object->system_frame_number_in_vpu, frame->system_frame_number);
  GST_DEBUG_OBJECT (vpu_dec_object, "vpu_dec_object received system_frame_number: %d\n", \
      frame->system_frame_number);

  buffer = frame->input_buffer;
  gst_buffer_map (buffer, &minfo, GST_MAP_READ);

  vpu_buffer_node->nSize = minfo.size;
  vpu_buffer_node->pPhyAddr = NULL;
  vpu_buffer_node->pVirAddr = minfo.data;
  if (vpu_dec_object->input_state->codec_data) {
    GstBuffer *buffer2 = vpu_dec_object->input_state->codec_data;
    GstMapInfo minfo2;
    gst_buffer_map (buffer2, &minfo2, GST_MAP_READ);
    vpu_buffer_node->sCodecData.nSize = minfo2.size;
    vpu_buffer_node->sCodecData.pData = minfo2.data;
    GST_DEBUG_OBJECT (vpu_dec_object, "codec data size: %d\n", minfo2.size);
    gst_buffer_unmap (buffer2, &minfo2);
  }

  gst_buffer_unmap (buffer, &minfo);

  return TRUE;
}

GstFlowReturn
gst_vpu_dec_object_decode (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec, GstVideoCodecFrame * frame)
{
  GstFlowReturn ret = GST_FLOW_OK;
	VpuDecRetCode dec_ret;
	VpuBufferNode in_data = {0};
	int buf_ret;

  GST_LOG_OBJECT (vpu_dec_object, "GstVideoCodecFrame: 0x%x\n", frame);
  gst_vpu_dec_object_handle_input_time_stamp (vpu_dec_object, bdec, frame);
  gst_vpu_dec_object_set_vpu_input_buf (vpu_dec_object, frame, &in_data);
  if (frame)
    gst_video_codec_frame_unref (frame);

  while (1) {

    GST_DEBUG_OBJECT(vpu_dec_object, "in data: %d \n", in_data.nSize);

    dec_ret = VPU_DecDecodeBuf(vpu_dec_object->handle, &in_data, &buf_ret);
    if (dec_ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "failed to decode frame: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return GST_FLOW_ERROR;
    }

    GST_DEBUG_OBJECT(vpu_dec_object, "buf status: 0x%x \n", buf_ret);

    if ((vpu_dec_object->use_new_tsm) && (buf_ret & VPU_DEC_ONE_FRM_CONSUMED)) {
      if (!gst_vpu_dec_object_set_tsm_consumed_len (vpu_dec_object)) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_set_tsm_consumed_len fail.");
        return GST_FLOW_ERROR;
      }
    }

    if (buf_ret & VPU_DEC_INIT_OK \
        || buf_ret & VPU_DEC_RESOLUTION_CHANGED) {
      ret = gst_vpu_dec_object_handle_reconfig(vpu_dec_object, bdec);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_handle_reconfig fail: %s\n", \
            gst_flow_get_name (ret));
        return ret;
      }
    }
    if (buf_ret & VPU_DEC_OUTPUT_DIS) {
      vpu_dec_object->mosaic_cnt = 0;
      ret = gst_vpu_dec_object_send_output (vpu_dec_object, bdec, FALSE);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_send_output fail: %s\n", \
            gst_flow_get_name (ret));
        return ret;
      }
    }
    if (buf_ret & VPU_DEC_NO_ENOUGH_BUF) {
      GstBuffer *buffer;
      buffer = gst_video_decoder_allocate_output_buffer(bdec);
      if (G_UNLIKELY (buffer == NULL)) {
        GST_DEBUG_OBJECT (vpu_dec_object, "could not get buffer.");
        return GST_FLOW_ERROR;
      }
      if (!gst_vpu_dec_object_release_frame_buffer_to_vpu (vpu_dec_object, buffer)) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_release_frame_buffer_to_vpu fail.");
        return GST_FLOW_ERROR;
      }
    }
    if (buf_ret & VPU_DEC_OUTPUT_MOSAIC_DIS) {
      vpu_dec_object->mosaic_cnt++;
      ret = gst_vpu_dec_object_send_output (vpu_dec_object, bdec, FALSE);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_send_output fail: %s\n", \
            gst_flow_get_name (ret));
        return ret;
      }
    }
    if (buf_ret & VPU_DEC_FLUSH) {
      GST_WARNING ("Got need flush message!!");
      dec_ret = VPU_DecFlushAll(vpu_dec_object->handle);
      if (dec_ret != VPU_DEC_RET_SUCCESS) {
        GST_ERROR_OBJECT(vpu_dec_object, "flushing VPU failed: %s", \
            gst_vpu_dec_object_strerror(ret));
        return GST_FLOW_ERROR;
      }
    }
    if (buf_ret & VPU_DEC_OUTPUT_REPEAT) {
      GST_INFO_OBJECT (vpu_dec_object, "Got repeat information!!");
      TSManagerSend (vpu_dec_object->tsm);
    }
    if (buf_ret & VPU_DEC_OUTPUT_DROPPED) {
      GST_INFO_OBJECT (vpu_dec_object, "Got drop information!!");
      ret = gst_vpu_dec_object_send_output (vpu_dec_object, bdec, TRUE);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_send_output fail: %s\n", \
            gst_flow_get_name (ret));
        return ret;
      }
    }
    if (buf_ret & VPU_DEC_SKIP) {
      GST_INFO_OBJECT (vpu_dec_object, "Got skip message!!");
      TSManagerSend (vpu_dec_object->tsm);
    }
    if (buf_ret & VPU_DEC_OUTPUT_EOS) {
      GST_INFO_OBJECT (vpu_dec_object, "Got EOS message!!");
      break;
    }
    if (buf_ret & VPU_DEC_NO_ENOUGH_INBUF) {
      GST_LOG_OBJECT (vpu_dec_object, "Got not enough input message!!");
      break;
    }
    if (((buf_ret & VPU_DEC_INPUT_USED)) && frame) {
      GST_LOG_OBJECT (vpu_dec_object, "Got VPU_DEC_INPUT_USED!!");
      if (vpu_dec_object->low_latency == FALSE) {
        break;
      } else {
        if (in_data.nSize) {
          in_data.nSize = 0;
          in_data.pVirAddr = 0;
        }
      }
    }
  }

	return GST_FLOW_OK;
}

gboolean
gst_vpu_dec_object_flush (GstVideoDecoder * bdec, GstVpuDecObject * vpu_dec_object)
{
	VpuDecRetCode ret;
  GstBuffer *buffer;

  if (vpu_dec_object->state >= STATE_OPENED) {
    ret = VPU_DecFlushAll(vpu_dec_object->handle);
    if (ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "flushing VPU failed: %s", \
          gst_vpu_dec_object_strerror(ret));
      return FALSE;
    }
  }
  vpu_dec_object->new_segment = TRUE;
  g_list_free (vpu_dec_object->system_frame_number_in_vpu);
  vpu_dec_object->system_frame_number_in_vpu = NULL;
  GST_DEBUG_OBJECT (vpu_dec_object, "system_frame_number_in_vpu list free\n");

  // FIXME: workaround for VP8 seek. VPU will block if VPU need framebuffer
  // before seek.
  if (g_list_length (vpu_dec_object->gstbuffer_in_vpudec) \
      < vpu_dec_object->actual_buf_cnt) {
    buffer = gst_video_decoder_allocate_output_buffer(bdec);
    if (G_UNLIKELY (buffer == NULL)) {
      GST_DEBUG_OBJECT (vpu_dec_object, "could not get buffer.");
      return GST_FLOW_ERROR;
    }
    if (!gst_vpu_dec_object_release_frame_buffer_to_vpu (vpu_dec_object, buffer)) {
      GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_release_frame_buffer_to_vpu fail.");
      return GST_FLOW_ERROR;
    }
  }

  return TRUE;
}

