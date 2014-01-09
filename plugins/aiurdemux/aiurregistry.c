/*
 * Copyright (c) 2010-2012, Freescale Semiconductor, Inc. All rights reserved.
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

#include "aiurdemux.h"


#define CORE_QUERY_INTERFACE_API_NAME "FslParserQueryInterface"

#define AIUR_REGISTRY_FILE_DEFAULT "/usr/share/aiur_registry.arm11.cf"

#ifdef _ARM9
#undef AIUR_REGISTRY_FILE_DEFAULT
#define AIUR_REGISTRY_FILE_DEFAULT "/usr/share/aiur_registry.arm9.cf"
#endif

#define AIUR_REGISTRY_FILE_ENV_NAME "AIUR_REGISTRY"

#define KEY_LIB "library"
#define KEY_MIME "mime"
#define VERSION "3.0.9"


static GstsutilsEntry *g_aiur_caps_entry = NULL;

/* id table for all core apis, the same order with AiurCoreInterface */
uint32 aiur_core_interface_id_table[] = {
  PARSER_API_GET_VERSION_INFO,
  PARSER_API_CREATE_PARSER,
  PARSER_API_DELETE_PARSER,

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


AiurCoreInterface *
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
      g_print ("Aiur: %s \nCore: %s\n file: %s\n",
              VERSION, inf->coreid, dl_name);
    }
  }
  //inf->dlentry = g_aiur_caps_entry;

  return inf;
fail:
  if (inf) {
    g_free (inf);
    inf = NULL;
  }

  if (dl_handle) {
    dlclose (dl_handle);
  }
  return inf;
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

  caps = gstsutils_get_caps_from_entry(g_aiur_caps_entry);

  return caps;
}


AiurCoreInterface *
aiur_core_create_interface_from_caps (GstCaps * caps)
{
  AiurCoreInterface *inf = NULL;
  GstsutilsGroup * group;
  gchar * libname = NULL;
  gchar * temp_name;
  gboolean find = FALSE;

  find = gstsutils_get_group_by_value(g_aiur_caps_entry,compare_caps,caps,&group);
  if (find) {

    if(gstsutils_get_value_by_group(group,FSL_KEY_LIB2,&temp_name))
      libname = temp_name;
    else if(gstsutils_get_value_by_group(group,FSL_KEY_LIB,&temp_name))
      libname = temp_name;
    
    inf = _aiur_core_create_interface_from_entry (libname);

    if(libname)
      g_free(libname);

    if(group->name)
      inf->name = group->name;
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

  g_free (inf);
}


void __attribute__ ((destructor)) aiur_free_dll_entry (void);


void
aiur_free_dll_entry ()
{
  gstsutils_deinit_entry(g_aiur_caps_entry);

}
