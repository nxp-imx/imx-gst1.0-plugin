/*
 * Copyright (c) 2010-2012,2014-2016 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * Module Name:    aiurregistry.c
 *
 * Description:    Implementation of utils functions for registry for
 *                 unified parser core functions
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "aiurregistry.h"


#define CORE_QUERY_INTERFACE_API_NAME "FslParserQueryInterface"

#ifdef _ARM11
#define AIUR_REGISTRY_FILE_DEFAULT "/usr/share/aiur_registry_1.0.arm11.cf"
#elif defined (_ARM9)
#define AIUR_REGISTRY_FILE_DEFAULT "/usr/share/aiur_registry1.0.arm9.cf"
#else
#define AIUR_REGISTRY_FILE_DEFAULT "/usr/share/aiur_registry_1.0.arm.cf"
#endif

#define AIUR_REGISTRY_FILE_ENV_NAME "AIUR_REGISTRY"


static GstsutilsEntry *g_aiur_caps_entry = NULL;

/* id table for all core apis, the same order with AiurCoreInterface */
uint32 aiur_core_interface_id_table[] = {
  PARSER_API_GET_VERSION_INFO,
  PARSER_API_CREATE_PARSER,
  PARSER_API_DELETE_PARSER,
  PARSER_API_CREATE_PARSER2,

  PARSER_API_INITIALIZE_INDEX,
  PARSER_API_IMPORT_INDEX,
  PARSER_API_EXPORT_INDEX,

  PARSER_API_IS_MOVIE_SEEKABLE,
  PARSER_API_GET_MOVIE_DURATION,
  PARSER_API_GET_USER_DATA,
  PARSER_API_GET_META_DATA,

  PARSER_API_GET_NUM_TRACKS,

  PARSER_API_GET_NUM_PROGRAMS,
  PARSER_API_GET_PROGRAM_TRACKS,

  PARSER_API_GET_TRACK_TYPE,
  PARSER_API_GET_DECODER_SPECIFIC_INFO,
  PARSER_API_GET_TRACK_DURATION,
  PARSER_API_GET_LANGUAGE,
  PARSER_API_GET_BITRATE,

  PARSER_API_GET_VIDEO_FRAME_WIDTH,
  PARSER_API_GET_VIDEO_FRAME_HEIGHT,
  PARSER_API_GET_VIDEO_FRAME_RATE,
  PARSER_API_GET_VIDEO_FRAME_ROTATION,

  PARSER_API_GET_AUDIO_NUM_CHANNELS,
  PARSER_API_GET_AUDIO_SAMPLE_RATE,
  PARSER_API_GET_AUDIO_BITS_PER_SAMPLE,

  PARSER_API_GET_AUDIO_BLOCK_ALIGN,
  PARSER_API_GET_AUDIO_CHANNEL_MASK,
  PARSER_API_GET_AUDIO_BITS_PER_FRAME,

  PARSER_API_GET_TEXT_TRACK_WIDTH,
  PARSER_API_GET_TEXT_TRACK_HEIGHT,

  PARSER_API_GET_READ_MODE,
  PARSER_API_SET_READ_MODE,

  PARSER_API_ENABLE_TRACK,

  PARSER_API_GET_NEXT_SAMPLE,
  PARSER_API_GET_NEXT_SYNC_SAMPLE,

  PARSER_API_GET_FILE_NEXT_SAMPLE,
  PARSER_API_GET_FILE_NEXT_SYNC_SAMPLE,

  PARSER_API_SEEK,
};


static AiurCoreInterface *
_aiur_core_create_interface_from_entry (gchar * dl_name)
{
  AiurCoreInterface *inf = NULL;
  void *dl_handle = NULL;
  int i;
  int32 err;
  int total = G_N_ELEMENTS (aiur_core_interface_id_table);
  void **papi;
  tFslParserQueryInterface query_interface;

  dl_handle = dlopen (dl_name, RTLD_LAZY);

  if (!dl_handle) {
    g_print ("Demux core %s error or missed! \n(Err: %s)\n",
            dl_name, dlerror ());
    goto fail;
  }

  query_interface = dlsym (dl_handle, CORE_QUERY_INTERFACE_API_NAME);

  if (query_interface == NULL) {
    g_print ("can not find symbol %s\n", CORE_QUERY_INTERFACE_API_NAME);
    goto fail;
  }
  inf = g_new0 (AiurCoreInterface, 1);

  if (inf == NULL)
    goto fail;

  papi = (void **) inf;

  for (i = 0; i < total; i++) {
    err = query_interface (aiur_core_interface_id_table[i], papi);
    if (err) {
      *papi = NULL;
    }
    papi++;
  }

  inf->dl_handle = dl_handle;

  if (inf->getVersionInfo) {
    inf->coreid = (inf->getVersionInfo) ();
    if (inf->coreid) {
      g_print ("\n====== AIUR: %s build on %s %s. ======\n\tCore: %s\n file: %s\n",
              VERSION, __DATE__,__TIME__, inf->coreid, dl_name);
    }
  }
  //inf->dlentry = g_aiur_caps_entry;

  return inf;
fail:
  if (dl_handle) {
    dlclose (dl_handle);
  }
  return inf;
}

static GstCaps * aiur_get_caps_from_entry(GstsutilsEntry * entry)
{
  int group_count=0;
  int index = 0;
  char* mime = NULL;
  char* libname = NULL;
  GstsutilsGroup *group=NULL;
  void *dlhandle = NULL;
  GstCaps * caps = NULL;
  group_count = gstsutils_get_group_count(entry);
  for(index = 1; index <= group_count; index++){
    if(!gstsutils_get_group_by_index(entry,index,&group))
      continue;
    if(!gstsutils_get_value_by_key(group,FSL_KEY_MIME,&mime)){
      continue;
    }
    if(!gstsutils_get_value_by_key(group,FSL_KEY_LIB,&libname)){
      g_free(mime);
      continue;
    }
    dlhandle = dlopen (libname, RTLD_LAZY);
    if (!dlhandle) {
      g_free(mime);
      g_free(libname);
      continue;
    }
    if (caps) {
      GstCaps *newcaps = gst_caps_from_string (mime);
      if (newcaps) {
        if (!gst_caps_is_subset (newcaps, caps)) {
          gst_caps_append (caps, newcaps);
        } else {
          gst_caps_unref (newcaps);
        }
      }
    } else {
      caps = gst_caps_from_string (mime);
    }
    dlclose (dlhandle);
    g_free(mime);
    g_free(libname);
  }
  return caps;
}
static GstsutilsGroup * aiur_core_find_caps_group(GstsutilsEntry *entry,GstCaps * caps)
{
  int group_count=0;
  int index = 0;
  char* mime = NULL;
  GstsutilsGroup *group=NULL;
  void *dlhandle = NULL;
  GstCaps * super_caps = NULL;
  gboolean found = FALSE;
  group_count = gstsutils_get_group_count(entry);
  for(index = 1; index <= group_count; index++){
    if(found)
      break;
    if(!gstsutils_get_group_by_index(entry,index,&group))
      continue;
    if(!gstsutils_get_value_by_key(group,FSL_KEY_MIME,&mime)){
      continue;
    }
    super_caps = gst_caps_from_string (mime);
    if ((super_caps) && (gst_caps_is_subset (caps, super_caps))) {
      found = TRUE;
    }
    if(super_caps)
      gst_caps_unref (super_caps);
    g_free(mime);
  }
  return group;
}

GstCaps *
aiur_core_get_caps ()
{
  GstCaps *caps = NULL;

  if(g_aiur_caps_entry == NULL){
    char *aiurenv = getenv (AIUR_REGISTRY_FILE_ENV_NAME);
    if (aiurenv == NULL) {
      aiurenv = AIUR_REGISTRY_FILE_DEFAULT;
    }
    g_aiur_caps_entry = gstsutils_init_entry(aiurenv);
  }

  caps = aiur_get_caps_from_entry(g_aiur_caps_entry);

  return caps;
}


AiurCoreInterface *
aiur_core_create_interface_from_caps (GstCaps * caps)
{
  AiurCoreInterface *inf = NULL;
  GstsutilsGroup * group;
  gchar * libname = NULL;
  gchar * libname2 = NULL;
  gchar * temp_name;
  gboolean find = TRUE;
  void *dlhandle = NULL;

  group = aiur_core_find_caps_group(g_aiur_caps_entry,caps);
  if (group) {

    if(gstsutils_get_value_by_key(group,FSL_KEY_LIB2,&libname2)){
      dlhandle = dlopen (libname2, RTLD_LAZY);
      if(dlhandle){
        libname = libname2;
        dlclose (dlhandle);
      }else{
        g_free(libname2);
      }
    }
    if(libname == NULL)
      find = gstsutils_get_value_by_key(group,FSL_KEY_LIB,&libname);

    if (find)
      inf = _aiur_core_create_interface_from_entry (libname);
    else
      return inf;

    if(libname)
      g_free(libname);

    inf->name = gstsutils_get_group_name(group);
  }
  return inf;
}


void
aiur_core_destroy_interface (AiurCoreInterface * inf)
{
  if (inf == NULL)
    return;

  if (inf->dl_handle) {
    dlclose (inf->dl_handle);
  }

  g_free(inf->name);
  g_free (inf);
}


void __attribute__ ((destructor)) aiur_free_dll_entry (void);


void
aiur_free_dll_entry ()
{
  gstsutils_deinit_entry(g_aiur_caps_entry);

}
