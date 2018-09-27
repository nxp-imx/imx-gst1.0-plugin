/*
 * Copyright (c) 2013, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2017-2018 NXP
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
#include <libdrm/drm_fourcc.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideohdr10meta.h>
#include "gstimxcommon.h"
#include <gst/allocators/gstphymemmeta.h>
#include "gstvpuallocator.h"
#include "gstvpudecobject.h"

GST_DEBUG_CATEGORY_STATIC(vpu_dec_object_debug);
#define GST_CAT_DEFAULT vpu_dec_object_debug

#define VPUDEC_TS_BUFFER_LENGTH_DEFAULT (1024)
#define MAX_BUFFERED_DURATION_IN_VPU (3*1000000000ll)
#define MAX_BUFFERED_COUNT_IN_VPU (100)
#define MASAIC_THRESHOLD (30)
//FIXME: relate with frame plus?
#define DROP_RESUME (200 * GST_MSECOND)
#define MAX_RATE_FOR_NORMAL_PLAYBACK (2)
#define MIN_RATE_FOR_NORMAL_PLAYBACK (0)
#define VPU_FIRMWARE_CODE_DIVX_FLAG (1<<18)
#define VPU_FIRMWARE_CODE_RV_FLAG (1<<19)

enum
{
  AUTO = 0,
  NV12,
  I420,
  YV12,
  Y42B,
  NV16,
  Y444,
  NV24,
  OUTPUT_FORMAT_MAX
};

GType
gst_vpu_dec_output_format_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      {AUTO, "enable chroma interleave. (default)",
          "auto"},
      {NV12, "NV12 format",
          "NV12"},
      {I420, "I420 format",
          "I420"},
      {YV12, "YV12 format",
          "YV12"},
      {Y42B, "Y42B format",
          "Y42B"},
      {NV16, "NV16 format",
          "NV16"},
      {Y444, "Y444 format",
          "Y444"},
      {NV24, "NV24 format",
          "NV24"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstVpuDecOutputFormat", values);
  }
  return gtype;
}

G_DEFINE_TYPE(GstVpuDecObject, gst_vpu_dec_object, GST_TYPE_OBJECT)

static void gst_vpu_dec_object_finalize(GObject *object);

gint gst_vpu_dec_object_get_vpu_fwcode (void)
{
  VpuDecRetCode ret;
  VpuVersionInfo version;

  ret = VPU_DecLoad();
  if (ret != VPU_DEC_RET_SUCCESS) {
    return 0;
  }

  ret = VPU_DecGetVersionInfo(&version);
  if (ret != VPU_DEC_RET_SUCCESS) {
    version.nFwCode = 0;
  }

  ret = VPU_DecUnLoad();
  if (ret != VPU_DEC_RET_SUCCESS) {
    return 0;
  }

  return version.nFwCode;
}

GstCaps *
gst_vpu_dec_object_get_sink_caps (void)
{
  static GstCaps *caps = NULL;
  gint vpu_fwcode = gst_vpu_dec_object_get_vpu_fwcode ();

  if (caps == NULL) {
    VPUMapper *map = vpu_mappers;
    while ((map) && (map->mime)) {
      if ((map->std != VPU_V_RV && map->std != VPU_V_DIVX3
            && map->std != VPU_V_DIVX4 && map->std != VPU_V_DIVX56
            && map->std != VPU_V_AVS && map->std != VPU_V_VP6
            && map->std != VPU_V_SORENSON && map->std != VPU_V_WEBP
            && map->std != VPU_V_VP9 && map->std != VPU_V_HEVC)
          || ((vpu_fwcode & VPU_FIRMWARE_CODE_RV_FLAG) && map->std == VPU_V_RV)
          || ((vpu_fwcode & VPU_FIRMWARE_CODE_DIVX_FLAG) 
            && (map->std == VPU_V_DIVX3 || map->std == VPU_V_DIVX4
              || map->std == VPU_V_DIVX56)) || (IS_HANTRO()
              && (map->std == VPU_V_VP9 || map->std == VPU_V_HEVC
                || map->std == VPU_V_RV || map->std == VPU_V_DIVX3
                || map->std == VPU_V_DIVX4 || map->std == VPU_V_DIVX56
                || map->std == VPU_V_AVS || map->std == VPU_V_VP6
                || map->std == VPU_V_SORENSON || map->std == VPU_V_WEBP))
          || (IS_AMPHION() && (map->std == VPU_V_HEVC))) {
        if (IS_AMPHION() && (map->std == VPU_V_VP8 || map->std == VPU_V_H263
              || map->std == VPU_V_XVID || map->std == VPU_V_VC1
              || map->std == VPU_V_MJPG || map->std == VPU_V_VC1_AP)) {
          map++;
          continue;
        }
        if (IS_IMX8MM() && (map->std != VPU_V_HEVC && map->std != VPU_V_VP9
                    && map->std != VPU_V_AVC && map->std != VPU_V_VP8)) {
            map++;
            continue;
        }
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
    caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("{ NV12, I420, YV12, Y42B, \
          NV16, Y444, NV24, NV12_10LE}"));
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
  vpu_dec_object->handle = NULL;
  vpu_dec_object->vpuframebuffers = NULL;
  vpu_dec_object->new_segment = TRUE;
  vpu_dec_object->mosaic_cnt = 0;
  vpu_dec_object->tsm_mode = MODE_AI;
  vpu_dec_object->last_valid_ts = GST_CLOCK_TIME_NONE;
  vpu_dec_object->last_received_ts = GST_CLOCK_TIME_NONE;
  vpu_dec_object->vpu_internal_mem.internal_virt_mem = NULL;
  vpu_dec_object->vpu_internal_mem.internal_phy_mem = NULL;
  vpu_dec_object->mv_mem = NULL;
  vpu_dec_object->gstbuffer_in_vpudec = NULL;
  vpu_dec_object->gstbuffer_in_vpudec2 = NULL;
  vpu_dec_object->system_frame_number_in_vpu = NULL;
  vpu_dec_object->dropping = FALSE;
  vpu_dec_object->vpu_report_resolution_change = FALSE; 
  vpu_dec_object->vpu_need_reconfig = FALSE;
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

  g_print("====== VPUDEC: %s build on %s %s. ======\n",  (VERSION),__DATE__,__TIME__);;
  g_print("\twrapper: %d.%d.%d (%s)\n", wrapper_version.nMajor, wrapper_version.nMinor, 
    wrapper_version.nRelease, (wrapper_version.pBinary? wrapper_version.pBinary:"unknow"));
  g_print("\tvpulib: %d.%d.%d\n", version.nLibMajor, version.nLibMinor, version.nLibRelease);
  g_print("\tfirmware: %d.%d.%d.%d\n", version.nFwMajor, version.nFwMinor, version.nFwRelease, version.nFwCode);

  /* mem_info contains information about how to set up memory blocks
   * the VPU uses as temporary storage (they are "work buffers") */
  memset(&(vpu_dec_object->vpu_internal_mem.mem_info), 0, sizeof(VpuMemInfo));
  ret = VPU_DecQueryMem(&(vpu_dec_object->vpu_internal_mem.mem_info));
  if (ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "could not get VPU memory information: %s", \
        gst_vpu_dec_object_strerror(ret));
    return FALSE;
  }

  if (!gst_vpu_allocate_internal_mem (&(vpu_dec_object->vpu_internal_mem))) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_allocate_internal_mem fail");
    return FALSE;
  }

  vpu_dec_object->tsm = createTSManager (VPUDEC_TS_BUFFER_LENGTH_DEFAULT);

  if (!gst_vpu_dec_object_init_qos(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_init_qos fail");
    return FALSE;
  }

  vpu_dec_object->frame2gstbuffer_table = g_hash_table_new(NULL, NULL);
  vpu_dec_object->gstbuffer2frame_table = g_hash_table_new(NULL, NULL);
  vpu_dec_object->total_frames = 0;
  vpu_dec_object->total_time = 0;
  vpu_dec_object->vpu_hold_buffer = 0;

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

  if (!IS_HANTRO()) {
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
  }

  return TRUE;
}

gboolean
gst_vpu_dec_object_stop (GstVpuDecObject * vpu_dec_object)
{
  VpuDecRetCode dec_ret;

  GST_INFO_OBJECT(vpu_dec_object, "Video decoder frames: %lld time: %lld fps: (%.3f).\n",
      vpu_dec_object->total_frames, vpu_dec_object->total_time, (gfloat)1000000
      * vpu_dec_object->total_frames / vpu_dec_object->total_time);
  if (vpu_dec_object->gstbuffer_in_vpudec != NULL) {
    g_list_foreach (vpu_dec_object->gstbuffer_in_vpudec, (GFunc) gst_buffer_unref, NULL);
    g_list_free (vpu_dec_object->gstbuffer_in_vpudec);
    vpu_dec_object->gstbuffer_in_vpudec = NULL;
  }

  if (vpu_dec_object->gstbuffer_in_vpudec2 != NULL) {
    g_list_free (vpu_dec_object->gstbuffer_in_vpudec2);
    vpu_dec_object->gstbuffer_in_vpudec2 = NULL;
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

  if (vpu_dec_object->handle) {
    dec_ret = VPU_DecClose(vpu_dec_object->handle);
    if (dec_ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "closing decoder failed: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return FALSE;
    }
    vpu_dec_object->handle = NULL;
  }

  if (!gst_vpu_dec_object_free_mv_buffer(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_free_mv_buffer fail");
    return FALSE;
  }

  if (!gst_vpu_free_internal_mem (&(vpu_dec_object->vpu_internal_mem))) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_free_internal_mem fail");
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
      case Y42B: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_Y42B; break;
      case NV16: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_NV16; break;
      case Y444: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_Y444; break;
      case NV24: vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_NV24; break;
      default: GST_WARNING_OBJECT(vpu_dec_object, "unknown output format"); break;
    }
  }
}

static gboolean 
gst_vpu_dec_object_set_vpu_param (GstVpuDecObject * vpu_dec_object, \
    GstVideoDecoder * bdec, GstVideoCodecState *state, VpuDecOpenParam *open_param)
{
  GstVideoInfo *info = &state->info;

  open_param->CodecFormat = gst_vpu_find_std (state->caps);
  if (open_param->CodecFormat < 0) {
    GST_ERROR_OBJECT(vpu_dec_object, "can't find VPU supported format");
    return FALSE;
  }

  GST_INFO_OBJECT (vpu_dec_object, "Get codec std %d", open_param->CodecFormat);
  vpu_dec_object->framerate_n = GST_VIDEO_INFO_FPS_N (info);
  vpu_dec_object->framerate_d = GST_VIDEO_INFO_FPS_D (info);

  open_param->nChromaInterleave = 0;
  open_param->nMapType = 0;
  vpu_dec_object->implement_config = FALSE;
  vpu_dec_object->force_linear = FALSE;
  if ((IS_HANTRO() && (open_param->CodecFormat == VPU_V_HEVC
        || open_param->CodecFormat == VPU_V_VP9
        || open_param->CodecFormat == VPU_V_AVC))
      || IS_AMPHION()) {
    open_param->nTiled2LinearEnable = 1;
    if (IS_IMX8MM())
        open_param->nTiled2LinearEnable = 0;
    vpu_dec_object->implement_config = TRUE;
    if (open_param->CodecFormat == VPU_V_HEVC
        || open_param->CodecFormat == VPU_V_VP9)
      vpu_dec_object->drm_modifier_pre = DRM_FORMAT_MOD_VSI_G2_TILED_COMPRESSED;
    else
      vpu_dec_object->drm_modifier_pre = DRM_FORMAT_MOD_VSI_G1_TILED;
  } else {
    open_param->nTiled2LinearEnable = 0;
  }
  open_param->nEnableVideoCompressor = 1;
  if (IS_IMX8MM()) {
    open_param->nEnableVideoCompressor = 0;
    open_param->nPixelFormat = 1;
  }
  vpu_dec_object->output_format_decided = GST_VIDEO_FORMAT_NV12;
  if (open_param->CodecFormat == VPU_V_MJPG) {
    vpu_dec_object->is_mjpeg = TRUE;
  } else {
    vpu_dec_object->is_mjpeg = FALSE;
  }
  if (IS_HANTRO() && (open_param->CodecFormat == VPU_V_HEVC
        || open_param->CodecFormat == VPU_V_VP9)) {
    vpu_dec_object->is_g2 = TRUE;
  } else {
    vpu_dec_object->is_g2 = FALSE;
  }
  gst_vpu_dec_object_decide_output_format(vpu_dec_object, bdec);
  if (vpu_dec_object->is_mjpeg 
      && (vpu_dec_object->output_format_decided == GST_VIDEO_FORMAT_NV12
        || vpu_dec_object->output_format_decided == GST_VIDEO_FORMAT_NV16
        || vpu_dec_object->output_format_decided == GST_VIDEO_FORMAT_NV24)) {
    open_param->nChromaInterleave = 1;
    vpu_dec_object->chroma_interleaved = TRUE;
  }else if (vpu_dec_object->output_format_decided == GST_VIDEO_FORMAT_NV12) {
    open_param->nChromaInterleave = 1;
    vpu_dec_object->chroma_interleaved = TRUE;
  }
  open_param->nAdaptiveMode = 1;
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

  ret = VPU_DecOpen(&(vpu_dec_object->handle), &open_param, \
      &(vpu_dec_object->vpu_internal_mem.mem_info));
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

  if (vpu_dec_object->vpu_report_resolution_change == FALSE \
      && vpu_dec_object->state >= STATE_REGISTRIED_FRAME_BUFFER) {
    /* drain output */
    gst_vpu_dec_object_decode (vpu_dec_object, bdec, NULL);
    dec_ret = VPU_DecClose(vpu_dec_object->handle);
    if (dec_ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "closing decoder failed: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return FALSE;
    }
    vpu_dec_object->handle = NULL;

    vpu_dec_object->new_segment = TRUE;
    g_list_free (vpu_dec_object->system_frame_number_in_vpu);
    vpu_dec_object->system_frame_number_in_vpu = NULL;
    GST_DEBUG_OBJECT (vpu_dec_object, "system_frame_number_in_vpu list free\n");

    if (!gst_vpu_dec_object_free_mv_buffer(vpu_dec_object)) {
      GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_free_mv_buffer fail");
      return GST_FLOW_ERROR;
    }

    vpu_dec_object->state = STATE_ALLOCATED_INTERNAL_BUFFER;
  }

  g_list_foreach (vpu_dec_object->gstbuffer_in_vpudec, (GFunc) gst_buffer_unref, NULL);
  g_list_free (vpu_dec_object->gstbuffer_in_vpudec);
  g_list_free (vpu_dec_object->gstbuffer_in_vpudec2);
  vpu_dec_object->gstbuffer_in_vpudec = NULL;
  vpu_dec_object->gstbuffer_in_vpudec2 = NULL;
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
  GstBuffer *buffer;
  guint i;

  g_hash_table_remove_all (vpu_dec_object->frame2gstbuffer_table);
  g_hash_table_remove_all (vpu_dec_object->gstbuffer2frame_table);

  if (!gst_vpu_register_frame_buffer (vpu_dec_object->gstbuffer_in_vpudec, \
    &vpu_dec_object->output_state->info, vpu_dec_object->vpuframebuffers)) {
      GST_ERROR_OBJECT (vpu_dec_object, "gst_vpu_register_frame_buffer fail.\n");
      return FALSE;
  }

  for (i=0; i<vpu_dec_object->actual_buf_cnt; i++) {
    buffer = g_list_nth_data (vpu_dec_object->gstbuffer_in_vpudec, i);

    g_hash_table_replace(vpu_dec_object->frame2gstbuffer_table, \
        (gpointer)(vpu_dec_object->vpuframebuffers[i].pbufVirtY), (gpointer)(buffer));
    g_hash_table_replace(vpu_dec_object->gstbuffer2frame_table, \
        (gpointer)(buffer), \
        (gpointer)(&(vpu_dec_object->vpuframebuffers[i])));
    GST_DEBUG_OBJECT (vpu_dec_object, "VpuFrameBuffer: 0x%x VpuFrameBuffer pbufVirtY: 0x%x GstBuffer: 0x%x\n", \
        vpu_dec_object->vpuframebuffers[i], vpu_dec_object->vpuframebuffers[i].pbufVirtY, buffer);
  }

  if (!IS_AMPHION()) {
    dec_ret = VPU_DecRegisterFrameBuffer (vpu_dec_object->handle, \
        vpu_dec_object->vpuframebuffers, vpu_dec_object->actual_buf_cnt);
    if (dec_ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "registering framebuffers failed: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return FALSE;
    }
  } else
    vpu_dec_object->vpu_hold_buffer = vpu_dec_object->actual_buf_cnt;

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
  gint width_align;
  GstBuffer *buffer;
  guint i;

  dec_ret = VPU_DecGetInitialInfo(vpu_dec_object->handle, &(vpu_dec_object->init_info));
  if (dec_ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "could not get init info: %s", \
        gst_vpu_dec_object_strerror(dec_ret));
    return GST_FLOW_ERROR;
  }

  if (vpu_dec_object->init_info.nPicWidth <= 0 || vpu_dec_object->init_info.nPicHeight <= 0) {
    GST_ERROR_OBJECT(vpu_dec_object, "VPU get init info error.");
    return GST_FLOW_ERROR;
  }

  if (vpu_dec_object->is_mjpeg) {
    switch (vpu_dec_object->init_info.nMjpgSourceFormat) {
      case VPU_COLOR_420: fmt = vpu_dec_object->chroma_interleaved ? \
              GST_VIDEO_FORMAT_NV12 : GST_VIDEO_FORMAT_I420; break;
      case VPU_COLOR_422H: fmt = vpu_dec_object->chroma_interleaved ? \
              GST_VIDEO_FORMAT_NV16 : GST_VIDEO_FORMAT_Y42B; break;
      case VPU_COLOR_444: fmt = vpu_dec_object->chroma_interleaved ? \
              GST_VIDEO_FORMAT_NV24 : GST_VIDEO_FORMAT_Y444; break;
      default:
              GST_ERROR_OBJECT(vpu_dec_object, "unsupported MJPEG output format %d", \
                  vpu_dec_object->init_info.nMjpgSourceFormat);
              return GST_FLOW_ERROR;
    }
  }
  else
    fmt = vpu_dec_object->output_format_decided;

  if (fmt ==  GST_VIDEO_FORMAT_NV12 && vpu_dec_object->init_info.nBitDepth == 10){
    fmt = GST_VIDEO_FORMAT_NV12_10LE;
  }
  if (IS_HANTRO() && vpu_dec_object->init_info.nInterlace
      && vpu_dec_object->implement_config) {
    vpu_dec_object->force_linear = TRUE;
  }

  GST_INFO_OBJECT(vpu_dec_object, "using %s as video output format", gst_video_format_to_string(fmt));

  /* Create the output state */
  //FIXME: set max resolution to avoid buffer reallocate when resolution change.
  vpu_dec_object->output_state = state =
    gst_video_decoder_set_output_state (bdec, fmt, vpu_dec_object->init_info.nPicWidth, \
        vpu_dec_object->init_info.nPicHeight, vpu_dec_object->input_state);

  vpu_dec_object->min_buf_cnt = vpu_dec_object->init_info.nMinFrameBufferCount;
  vpu_dec_object->frame_size = vpu_dec_object->init_info.nFrameSize;
  vpu_dec_object->init_info.nBitDepth;
  GST_INFO_OBJECT(vpu_dec_object, "video bit depth: %d", vpu_dec_object->init_info.nBitDepth);
  GST_VIDEO_INFO_WIDTH (&(state->info)) = vpu_dec_object->init_info.nPicWidth;
  GST_VIDEO_INFO_HEIGHT (&(state->info)) = vpu_dec_object->init_info.nPicHeight;
  GST_VIDEO_INFO_INTERLACE_MODE(&(state->info)) = \
    vpu_dec_object->init_info.nInterlace ? GST_VIDEO_INTERLACE_MODE_INTERLEAVED \
    : GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  vpu_dec_object->buf_align = vpu_dec_object->init_info.nAddressAlignment;
  memset(&(vpu_dec_object->video_align), 0, sizeof(GstVideoAlignment));

  if (IS_AMPHION())
    width_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_H_AMPHION;
  else if (IS_HANTRO() && vpu_dec_object->implement_config)
    width_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_H_HANTRO_TILE;
  else
    width_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_H;
  if (vpu_dec_object->init_info.nPicWidth % width_align)
    vpu_dec_object->video_align.padding_right = width_align \
      - vpu_dec_object->init_info.nPicWidth % width_align;
  if (IS_HANTRO() && vpu_dec_object->is_g2 == TRUE)
    height_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_V_HANTRO;
  else if (IS_AMPHION())
    height_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_V_AMPHION;
  else
    height_align = DEFAULT_FRAME_BUFFER_ALIGNMENT_V;
  if (!IS_HANTRO() && vpu_dec_object->init_info.nInterlace)
    height_align <<= 1;
  if (vpu_dec_object->init_info.nPicHeight % height_align)
    vpu_dec_object->video_align.padding_bottom = height_align \
      - vpu_dec_object->init_info.nPicHeight % height_align;

  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++)
    vpu_dec_object->video_align.stride_align[i] = width_align - 1;

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
    vpu_dec_object->gstbuffer_in_vpudec2 = g_list_append ( \
        vpu_dec_object->gstbuffer_in_vpudec2, buffer);
    GST_DEBUG_OBJECT (vpu_dec_object, "gst_video_decoder_allocate_output_buffer end");
    GST_DEBUG_OBJECT (vpu_dec_object, "gstbuffer get from buffer pool: %x\n", buffer);
    GST_DEBUG_OBJECT (vpu_dec_object, "gstbuffer_in_vpudec list length: %d actual_buf_cnt: %d \n", \
        g_list_length (vpu_dec_object->gstbuffer_in_vpudec), vpu_dec_object->actual_buf_cnt);
  }

  if (!gst_vpu_dec_object_free_mv_buffer(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_free_mv_buffer fail");
    return GST_FLOW_ERROR;
  }

  if (!gst_vpu_dec_object_allocate_mv_buffer(vpu_dec_object)) {
    GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_allocate_mv_buffer fail");
    return GST_FLOW_ERROR;
  }

  if (IS_HANTRO() && vpu_dec_object->implement_config) {
    VpuBufferNode in_data = {0};
    int buf_ret;
    dec_ret = VPU_DecDecodeBuf(vpu_dec_object->handle, &in_data, &buf_ret);
    if (dec_ret == VPU_DEC_RET_FAILURE) {
      GST_ERROR_OBJECT(vpu_dec_object, "VPU_DecDecodeBuf fail: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return GST_FLOW_ERROR;
    }
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
  GST_DEBUG_OBJECT (vpu_dec_object, "gstbuffer_in_vpudec list length: %d\n", \
      g_list_length (vpu_dec_object->gstbuffer_in_vpudec));

  GST_LOG_OBJECT (vpu_dec_object, "GstBuffer: 0x%x VpuFrameBuffer: 0x%x\n", \
      buffer, frame_buffer);
  dec_ret = VPU_DecOutFrameDisplayed(vpu_dec_object->handle, frame_buffer);
  if (dec_ret != VPU_DEC_RET_SUCCESS) {
    GST_ERROR_OBJECT(vpu_dec_object, "clearing display framebuffer failed: %s", \
        gst_vpu_dec_object_strerror(dec_ret));
    return FALSE;
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
      GST_WARNING_OBJECT(vpu_dec_object, "decoder can catch up. needn't drop frame. diff: %lld\n", \
          diff);
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
  GstPhyMemMeta *pmeta;
  GstBuffer *output_buffer = NULL;
  GstClockTime output_pts = 0;
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
  GST_LOG_OBJECT (vpu_dec_object, "gst_video_decoder_get_frame: 0x%x\n", \
      out_frame);
  if (out_frame && vpu_dec_object->frame_drop)
    gst_vpu_dec_object_process_qos (vpu_dec_object, bdec, out_frame);
 
  if (drop != TRUE) {
    dec_ret = VPU_DecGetOutputFrame(vpu_dec_object->handle, &out_frame_info);
    if (dec_ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "could not get decoded output frame: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return GST_FLOW_ERROR;
    }

    GST_LOG_OBJECT(vpu_dec_object, "vpu display buffer: 0x%x pbufVirtY: 0x%x\n", \
        out_frame_info.pDisplayFrameBuf, out_frame_info.pDisplayFrameBuf->pbufVirtY);
    output_pts = TSManagerSend2 (vpu_dec_object->tsm, \
        out_frame_info.pDisplayFrameBuf);
    output_buffer = g_hash_table_lookup( \
        vpu_dec_object->frame2gstbuffer_table, \
        out_frame_info.pDisplayFrameBuf->pbufVirtY);
    g_hash_table_replace(vpu_dec_object->gstbuffer2frame_table, \
        (gpointer)(output_buffer), \
        (gpointer)(out_frame_info.pDisplayFrameBuf));
    vpu_dec_object->gstbuffer_in_vpudec = g_list_remove ( \
        vpu_dec_object->gstbuffer_in_vpudec, output_buffer);
  } else {
    output_pts = TSManagerSend (vpu_dec_object->tsm);
  }

  if (out_frame == NULL) {
    //FIXME: workaround for VPU will output more video frame if drop B enabled
    // for Xvid.
    GST_WARNING_OBJECT(vpu_dec_object, "gst_video_decoder_get_frame failed.");
    if (output_buffer) {
      if (!gst_vpu_dec_object_release_frame_buffer_to_vpu (vpu_dec_object, output_buffer)) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_release_frame_buffer_to_vpu fail.");
        return FALSE;
      }
    }
    return GST_FLOW_OK;
  }

  if (((vpu_dec_object->mosaic_cnt != 0)
      && (vpu_dec_object->mosaic_cnt < MASAIC_THRESHOLD))
      || drop == TRUE) {
    GST_INFO_OBJECT(vpu_dec_object, "drop frame.");
    if (output_buffer) {
      if (!gst_vpu_dec_object_release_frame_buffer_to_vpu (vpu_dec_object, output_buffer)) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_release_frame_buffer_to_vpu fail.");
        return FALSE;
      }
    }
    if (output_pts)
      out_frame->pts = output_pts;
    return gst_video_decoder_drop_frame (bdec, out_frame);
  }

  if (output_pts)
    out_frame->pts = output_pts;
  if (output_buffer)
    out_frame->output_buffer = output_buffer;

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
    case VPU_FIELD_TOP: vmeta->flags = GST_VIDEO_FRAME_FLAG_ONEFIELD | GST_VIDEO_FRAME_FLAG_TFF; break;
    case VPU_FIELD_BOTTOM: vmeta->flags = GST_VIDEO_FRAME_FLAG_ONEFIELD; break;
    case VPU_FIELD_TB: vmeta->flags = GST_VIDEO_FRAME_FLAG_INTERLACED | GST_VIDEO_FRAME_FLAG_TFF; break;
    case VPU_FIELD_BT: vmeta->flags = GST_VIDEO_FRAME_FLAG_INTERLACED; break;
    default: GST_WARNING_OBJECT(vpu_dec_object, "unknown field type"); break;
  }
  GST_DEBUG_OBJECT(vpu_dec_object, "field type: %d\n", out_frame_info.eFieldType);

  /* set crop info */
  cmeta = gst_buffer_add_video_crop_meta (out_frame->output_buffer);
  cmeta->x = out_frame_info.pExtInfo->FrmCropRect.nLeft;
  cmeta->y = out_frame_info.pExtInfo->FrmCropRect.nTop;
  cmeta->width = out_frame_info.pExtInfo->FrmCropRect.nRight-out_frame_info.pExtInfo->FrmCropRect.nLeft;
  cmeta->height = out_frame_info.pExtInfo->FrmCropRect.nBottom-out_frame_info.pExtInfo->FrmCropRect.nTop;

  if (vpu_dec_object->drm_modifier) {
    gst_buffer_add_dmabuf_meta(out_frame->output_buffer, vpu_dec_object->drm_modifier);
    GST_DEBUG_OBJECT(vpu_dec_object, "add drm modifier: %lld\n", vpu_dec_object->drm_modifier);
  }

  /* set physical memory padding info */
  if (vpu_dec_object->use_my_pool && !vpu_dec_object->pool_alignment_checked) {
    GstStructure *config;
    GstBufferPool *pool = gst_video_decoder_get_buffer_pool (bdec);
    config = gst_buffer_pool_get_config (pool);

    // check if has alignment option setted.
    memset (&vpu_dec_object->video_align, 0, sizeof(GstVideoAlignment));
    if (gst_buffer_pool_config_has_option (config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT)) {
      gst_buffer_pool_config_get_video_alignment (config, &vpu_dec_object->video_align);

      GST_DEBUG_OBJECT (vpu_dec_object, "pool has alignment (%d, %d) , (%d, %d)", 
          vpu_dec_object->video_align.padding_left, vpu_dec_object->video_align.padding_top,
          vpu_dec_object->video_align.padding_right, vpu_dec_object->video_align.padding_bottom);
    }
    vpu_dec_object->pool_alignment_checked = TRUE;
    gst_structure_free (config);
    gst_object_unref (pool);
  }

  if (IS_HANTRO() || vpu_dec_object->use_my_pool) {
    pmeta = GST_PHY_MEM_META_ADD (out_frame->output_buffer);
    pmeta->x_padding = vpu_dec_object->video_align.padding_right;
    pmeta->y_padding = vpu_dec_object->video_align.padding_bottom;
    pmeta->rfc_luma_offset = out_frame_info.pExtInfo->rfc_luma_offset;
    pmeta->rfc_chroma_offset = out_frame_info.pExtInfo->rfc_chroma_offset;
  }
  
  if (vpu_dec_object->init_info.hasHdr10Meta || vpu_dec_object->init_info.hasColorDesc) {
    GstVideoHdr10Meta *meta = gst_buffer_add_video_hdr10_meta (out_frame->output_buffer);
    meta->hdr10meta.redPrimary[0] = vpu_dec_object->init_info.Hdr10Meta.redPrimary[0];
    meta->hdr10meta.redPrimary[1] = vpu_dec_object->init_info.Hdr10Meta.redPrimary[1];
    meta->hdr10meta.greenPrimary[0] = vpu_dec_object->init_info.Hdr10Meta.greenPrimary[0];
    meta->hdr10meta.greenPrimary[1] = vpu_dec_object->init_info.Hdr10Meta.greenPrimary[1];
    meta->hdr10meta.bluePrimary[0] = vpu_dec_object->init_info.Hdr10Meta.bluePrimary[0];
    meta->hdr10meta.bluePrimary[1] = vpu_dec_object->init_info.Hdr10Meta.bluePrimary[1];
    meta->hdr10meta.whitePoint[0] = vpu_dec_object->init_info.Hdr10Meta.whitePoint[0];
    meta->hdr10meta.whitePoint[1] = vpu_dec_object->init_info.Hdr10Meta.whitePoint[1];
    meta->hdr10meta.maxMasteringLuminance = vpu_dec_object->init_info.Hdr10Meta.maxMasteringLuminance;
    meta->hdr10meta.minMasteringLuminance = vpu_dec_object->init_info.Hdr10Meta.minMasteringLuminance;
    meta->hdr10meta.maxContentLightLevel = vpu_dec_object->init_info.Hdr10Meta.maxContentLightLevel;
    meta->hdr10meta.maxFrameAverageLightLevel = vpu_dec_object->init_info.Hdr10Meta.maxFrameAverageLightLevel;
    meta->hdr10meta.colourPrimaries = vpu_dec_object->init_info.ColourDesc.colourPrimaries;
    meta->hdr10meta.transferCharacteristics = vpu_dec_object->init_info.ColourDesc.transferCharacteristics;
    meta->hdr10meta.matrixCoeffs = vpu_dec_object->init_info.ColourDesc.matrixCoeffs;
    meta->hdr10meta.fullRange = vpu_dec_object->init_info.ColourDesc.fullRange;
    meta->hdr10meta.chromaSampleLocTypeTopField = vpu_dec_object->init_info.ChromaLocInfo.chromaSampleLocTypeTopField;
    meta->hdr10meta.chromaSampleLocTypeBottomField = vpu_dec_object->init_info.ChromaLocInfo.chromaSampleLocTypeTopField;
  }

  if (vpu_dec_object->tsm_mode == MODE_FIFO) {
    if (!GST_CLOCK_TIME_IS_VALID(out_frame->pts))
      out_frame->pts = vpu_dec_object->last_valid_ts;
    else
      vpu_dec_object->last_valid_ts = out_frame->pts;
  }

  vpu_dec_object->total_frames ++;
  GST_DEBUG_OBJECT (vpu_dec_object, "vpu dec output frame time stamp: %" \
      GST_TIME_FORMAT, GST_TIME_ARGS (out_frame->pts));

  ret = gst_video_decoder_finish_frame (bdec, out_frame);

  return ret;
}

static GstFlowReturn
gst_vpu_dec_object_get_gst_buffer (GstVideoDecoder * bdec, GstVpuDecObject * vpu_dec_object)
{
  GstBuffer *buffer;

  GST_DEBUG_OBJECT (vpu_dec_object, "min_buf_cnt: %d frame_plus: %d actual_buf_cnt: %d",
      vpu_dec_object->min_buf_cnt, vpu_dec_object->frame_plus, vpu_dec_object->actual_buf_cnt);
  if (g_list_length (vpu_dec_object->gstbuffer_in_vpudec) \
      < (vpu_dec_object->min_buf_cnt + vpu_dec_object->frame_plus)
      || vpu_dec_object->vpu_hold_buffer > 0) {
    if (vpu_dec_object->vpu_hold_buffer > 0) {
      buffer = g_list_nth_data (vpu_dec_object->gstbuffer_in_vpudec2, 0);
      vpu_dec_object->gstbuffer_in_vpudec2 = g_list_remove ( \
          vpu_dec_object->gstbuffer_in_vpudec2, buffer);
      vpu_dec_object->gstbuffer_in_vpudec = g_list_remove ( \
          vpu_dec_object->gstbuffer_in_vpudec, buffer);
      vpu_dec_object->vpu_hold_buffer --;
    }
    else
      buffer = gst_video_decoder_allocate_output_buffer(bdec);
    if (G_UNLIKELY (buffer == NULL)) {
      GST_DEBUG_OBJECT (vpu_dec_object, "could not get buffer.");
      return GST_FLOW_FLUSHING;
    }
    if (!(gst_buffer_is_phymem (buffer)
        || gst_is_phys_memory (gst_buffer_peek_memory (buffer, 0)))) {
      gst_buffer_unref (buffer);
      GST_DEBUG_OBJECT(vpu_dec_object, "gstbuffer isn't physical buffer.");
      return GST_FLOW_FLUSHING;
    }
    if (vpu_dec_object->state < STATE_REGISTRIED_FRAME_BUFFER) {
      gst_buffer_unref (buffer);
      GST_DEBUG_OBJECT(vpu_dec_object, "should set buffer to VPU in wrong state when down stream send reconfigure.");
      return GST_FLOW_OK;
    }
    if (!gst_vpu_dec_object_release_frame_buffer_to_vpu (vpu_dec_object, buffer)) {
      GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_release_frame_buffer_to_vpu fail.");
      return GST_FLOW_ERROR;
    }
  } else
    GST_WARNING_OBJECT(vpu_dec_object, "no more gstbuffer.\n");

  return GST_FLOW_OK;
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
    GST_DEBUG_OBJECT (vpu_dec_object, "Chain in with size = %d", minfo.size);

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

    GST_DEBUG_OBJECT (vpu_dec_object, "vpu dec input time stamp: %" \
        GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

    if (vpu_dec_object->use_new_tsm) {
      TSManagerReceive2 (vpu_dec_object->tsm, GST_BUFFER_TIMESTAMP (buffer),
          minfo.size);
    } else {
      TSManagerReceive (vpu_dec_object->tsm, GST_BUFFER_TIMESTAMP (buffer));
    }
    vpu_dec_object->last_received_ts = GST_BUFFER_TIMESTAMP (buffer);
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

  /* Hantro video decoder can output video frame even if only input one frame.
   * Needn't send EOS to drain it.
   */
  if (IS_HANTRO() && vpu_dec_object->tsm_mode == MODE_FIFO && frame == NULL) {
    vpu_buffer_node->nSize = 0;
    vpu_buffer_node->pVirAddr = (unsigned char *) NULL;

    return TRUE;
  }

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
  if (vpu_dec_object->input_state && vpu_dec_object->input_state->codec_data) {
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

static gboolean
gst_vpu_dec_object_clear_decoded_frame_ts (GstVpuDecObject * vpu_dec_object)
{
  int nBlkCnt;
  GstClockTime ts = 0;

  if (vpu_dec_object->use_new_tsm) {
    nBlkCnt = getTSManagerPreBufferCnt(vpu_dec_object->tsm);
    GST_DEBUG_OBJECT (vpu_dec_object, "nBlkCnt: %d", nBlkCnt);
    while (nBlkCnt > 0) {
      nBlkCnt--;
      ts = TSManagerSend2(vpu_dec_object->tsm, NULL);
      GST_DEBUG_OBJECT (vpu_dec_object, "drop ts: %" \
          GST_TIME_FORMAT, GST_TIME_ARGS (ts));
      if(!GST_CLOCK_TIME_IS_VALID(ts)){
        break;
      }
    }
  }

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

  if (!IS_AMPHION() && in_data.nSize == 0 && (frame || vpu_dec_object->state == STATE_OPENED)) {
    return GST_FLOW_OK;
  }

  while (1) {
    gint64 start_time;

    GST_DEBUG_OBJECT (vpu_dec_object, "in data: %d \n", in_data.nSize);

    start_time = g_get_monotonic_time ();

    dec_ret = VPU_DecDecodeBuf(vpu_dec_object->handle, &in_data, &buf_ret);
    if (dec_ret != VPU_DEC_RET_SUCCESS) {
      GST_ERROR_OBJECT(vpu_dec_object, "failed to decode frame: %s", \
          gst_vpu_dec_object_strerror(dec_ret));
      return GST_FLOW_ERROR;
    }

    GST_DEBUG_OBJECT (vpu_dec_object, "buf status: 0x%x time: %lld\n", \
        buf_ret, g_get_monotonic_time () - start_time);
    vpu_dec_object->total_time += g_get_monotonic_time () - start_time;

    if ((vpu_dec_object->use_new_tsm) && (buf_ret & VPU_DEC_ONE_FRM_CONSUMED)) {
      if (!gst_vpu_dec_object_set_tsm_consumed_len (vpu_dec_object)) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_set_tsm_consumed_len fail.");
        return GST_FLOW_ERROR;
      }
    }

    if (buf_ret & VPU_DEC_INIT_OK \
        || buf_ret & VPU_DEC_RESOLUTION_CHANGED) {
      if (buf_ret & VPU_DEC_RESOLUTION_CHANGED)
        vpu_dec_object->vpu_report_resolution_change = TRUE; 
      vpu_dec_object->vpu_need_reconfig = TRUE;
      ret = gst_vpu_dec_object_handle_reconfig(vpu_dec_object, bdec);
      /* workaround for VPU will discard decoded video frame when resolution change. */
      if (!IS_HANTRO() && !IS_AMPHION())
        gst_vpu_dec_object_clear_decoded_frame_ts (vpu_dec_object);
      vpu_dec_object->vpu_report_resolution_change = FALSE; 
      vpu_dec_object->vpu_need_reconfig = FALSE;
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
        GST_WARNING_OBJECT(vpu_dec_object, "gst_vpu_dec_object_send_output fail: %s\n", \
            gst_flow_get_name (ret));
        return ret;
      }
    }
    if (buf_ret & VPU_DEC_NO_ENOUGH_BUF) {
      ret = gst_vpu_dec_object_get_gst_buffer(bdec, vpu_dec_object);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_get_gst_buffer fail: %s\n", \
            gst_flow_get_name (ret));
        return ret;
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
    if (buf_ret & VPU_DEC_OUTPUT_DROPPED \
        || buf_ret & VPU_DEC_SKIP \
        || buf_ret & VPU_DEC_OUTPUT_REPEAT) {
      GST_INFO_OBJECT (vpu_dec_object, "Got drop information!!");
      ret = gst_vpu_dec_object_send_output (vpu_dec_object, bdec, TRUE);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_send_output fail: %s\n", \
            gst_flow_get_name (ret));
        return ret;
      }
    }
    if (buf_ret & VPU_DEC_OUTPUT_EOS) {
      GST_INFO_OBJECT (vpu_dec_object, "Got EOS message!!");
      /* flush VPU after received EOS. non-key frame will be dropped by VPU when
       * rewind. non-key frame will buffered by video decoder base which will
       * cause VPU can't get buffer to decode
       */
      gst_vpu_dec_object_flush (bdec, vpu_dec_object);
      break;
    }
    /* send EOS to VPU to force VPU output all video frame for rewind as videodecoder
     * need it. only can support skip I frame rewind.
     * only can output key frame when rewind as video decoder base will buffer output
     * between key frame, so VPU can't get output buffer to decode and then blocked. 
     */

    if (vpu_dec_object->tsm_mode == MODE_FIFO) {
      GST_DEBUG_OBJECT (vpu_dec_object, "send eos to VPU.\n");
      frame = NULL;
      if (!(buf_ret & VPU_DEC_INPUT_USED))
        GST_WARNING_OBJECT (vpu_dec_object, "VPU hasn't consumed input data, Shouldn't be here!");
      in_data.nSize = 0;
      in_data.pVirAddr = (unsigned char *) 0x1;
      continue;
    }
    if (buf_ret & VPU_DEC_NO_ENOUGH_INBUF) {
      GST_LOG_OBJECT (vpu_dec_object, "Got not enough input message!!");
      if (!IS_AMPHION() && vpu_dec_object->state < STATE_REGISTRIED_FRAME_BUFFER) {
        GST_WARNING_OBJECT (vpu_dec_object, "Dropped video frame before VPU init ok!");
        ret = gst_vpu_dec_object_send_output (vpu_dec_object, bdec, TRUE);
        if (ret != GST_FLOW_OK) {
          GST_ERROR_OBJECT(vpu_dec_object, "gst_vpu_dec_object_send_output fail: %s\n", \
              gst_flow_get_name (ret));
          return ret;
        }
      }
      break;
    }
    if (((buf_ret & VPU_DEC_INPUT_USED)) && frame) {
      gboolean bRetry = FALSE;
      GST_LOG_OBJECT (vpu_dec_object, "Got VPU_DEC_INPUT_USED!!");
      if (GST_CLOCK_TIME_IS_VALID(vpu_dec_object->last_received_ts)) {
        if (vpu_dec_object->last_received_ts - getTSManagerPosition ( \
              vpu_dec_object->tsm) > MAX_BUFFERED_DURATION_IN_VPU)
          bRetry = TRUE;
      } else {
        if (getTSManagerPreBufferCnt (vpu_dec_object->tsm) > MAX_BUFFERED_COUNT_IN_VPU)
          bRetry = TRUE;
      }
      if (vpu_dec_object->low_latency == FALSE && bRetry == FALSE) {
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
  if (!IS_HANTRO() && !IS_AMPHION() && vpu_dec_object->state >= STATE_REGISTRIED_FRAME_BUFFER) {
    gst_vpu_dec_object_get_gst_buffer(bdec, vpu_dec_object);
  }

  return TRUE;
}

