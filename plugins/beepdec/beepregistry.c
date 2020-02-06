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
 * Copyright (c) 2011-2016, Freescale Semiconductor, Inc. All rights reserved.
 * Copyright 2017 NXP
 *
 */


/*
 * Module Name:    beepregistry.c
 *
 * Description:    Implementation of utils functions for registry for
 *                 unified audio decoder core functions
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

#include "beepregistry.h"

#define CORE_QUERY_INTERFACE_API_NAME "UniACodecQueryInterface"

#ifdef _ARM11
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry_1.0.arm11.cf"
#elif defined (_ARM9)
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry_1.0.arm9.cf"
#elif defined (_ARM12)
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry_1.0.arm12.cf"
#else
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry_1.0.arm.cf"
#endif


#define BEEP_REGISTRY_FILE_ENV_NAME "BEEP_REGISTRY"

static GstsutilsEntry *g_beep_caps_entry = NULL;

/* id table for all core apis, the same order with BeepCoreInterface */
static uint32 beep_core_interface_id_table[] = {
  ACODEC_API_GET_VERSION_INFO,
  ACODEC_API_CREATE_CODEC,
  ACODEC_API_CREATE_CODEC_PLUS,
  ACODEC_API_DELETE_CODEC,
  ACODEC_API_RESET_CODEC,
  ACODEC_API_SET_PARAMETER,
  ACODEC_API_GET_PARAMETER,
  ACODEC_API_DEC_FRAME,
};


static BeepCoreInterface *
_beep_core_create_interface_from_entry (gchar * dl_name)
{
  BeepCoreInterface *inf = NULL;
  void *dl_handle = NULL;
  int i;
  int32 err;
  int total = G_N_ELEMENTS (beep_core_interface_id_table);
  void **papi;
  tUniACodecQueryInterface query_interface;

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
  inf = g_new0 (BeepCoreInterface, 1);

  if (inf == NULL)
    goto fail;

  papi = (void **) inf;

  for (i = 0; i < total; i++) {
    err = query_interface (beep_core_interface_id_table[i], papi);
    if (err) {
      *papi = NULL;
    }
    papi++;
  }

  inf->dl_handle = dl_handle;

  if (inf->getVersionInfo) {
    inf->coreid = (inf->getVersionInfo) ();
    if (inf->coreid) {
      g_print ("\n====== BEEP: %s build on %s %s. ======\n\tCore: %s\n file: %s\n",
              VERSION, __DATE__,__TIME__, inf->coreid, dl_name);
    }
  }

  return inf;
fail:
  if (dl_handle) {
    dlclose (dl_handle);
  }
  return inf;
}

static GstCaps * beep_get_caps_from_entry(GstsutilsEntry * entry)
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
      if (!gstsutils_get_value_by_key(group,FSL_KEY_DSP_LIB,&libname)) {
        g_free(mime);
        continue;
      }
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
static GstsutilsGroup * beep_core_find_caps_group(GstsutilsEntry *entry,GstCaps * caps)
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
beep_core_get_caps ()
{
  GstCaps *caps = NULL;

  if(g_beep_caps_entry == NULL){
    char *beepenv = getenv (BEEP_REGISTRY_FILE_ENV_NAME);
    if (beepenv == NULL) {
      beepenv = BEEP_REGISTRY_FILE_DEFAULT;
    }
    g_beep_caps_entry = gstsutils_init_entry(beepenv);
  }

  caps = beep_get_caps_from_entry(g_beep_caps_entry);

  return caps;
}


BeepCoreInterface *
beep_core_create_interface_from_caps_dsp (GstCaps * caps)
{
  BeepCoreInterface *inf = NULL;
  GstsutilsGroup * group;
  gchar * libname = NULL;
  void *dlhandle = NULL;

  group = beep_core_find_caps_group(g_beep_caps_entry,caps);
  if (group) {
    if (gstsutils_get_value_by_key(group,FSL_KEY_DSP_LIB, &libname)){
       dlhandle = dlopen(libname, RTLD_LAZY);
      if (dlhandle) {
        dlclose (dlhandle);
        inf = _beep_core_create_interface_from_entry (libname);
        inf->name = gstsutils_get_group_name(group);
      }
    }
  }
  if (libname)
    g_free(libname);
  return inf;
}



BeepCoreInterface *
beep_core_create_interface_from_caps (GstCaps * caps)
{
  BeepCoreInterface *inf = NULL;
  GstsutilsGroup * group;
  gchar * libname = NULL;
  gchar * libname2 = NULL;
  gchar * temp_name;
  gboolean find = TRUE;
  void *dlhandle = NULL;

  group = beep_core_find_caps_group(g_beep_caps_entry,caps);
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

    if (find )
      inf = _beep_core_create_interface_from_entry (libname);
    else
      return inf;

    if(libname)
      g_free(libname);

    inf->name = gstsutils_get_group_name(group);
  }
  return inf;
}


void
beep_core_destroy_interface (BeepCoreInterface * inf)
{
  if (inf == NULL)
    return;

  if (inf->dl_handle) {
    dlclose (inf->dl_handle);
  }

  g_free(inf->name);

  g_free (inf);
}


void __attribute__ ((destructor)) beep_free_dll_entry (void);


void
beep_free_dll_entry ()
{
  gstsutils_deinit_entry(g_beep_caps_entry);

}

#if 0
#define CORE_QUERY_INTERFACE_API_NAME "UniACodecQueryInterface"

#ifdef _ARM11
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry.arm11.cf"
#elif defined (_ARM9)
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry.arm9.cf"
#else
#define BEEP_REGISTRY_FILE_DEFAULT "/usr/share/beep_registry.arm12.cf"
#endif



#define BEEP_REGISTRY_FILE_ENV_NAME "BEEP_REGISTRY"

#define KEY_LIB "library"
#define KEY_MIME "mime"
#define KEY_RANK "rank"
#define KEY_LONGNAME "longname"
#define KEY_DESCRIPTION "description"


#define BEEP_DEFAULT_RANK FSL_GST_DEFAULT_DECODER_RANK


static BeepCoreDlEntry *g_beep_core_entry = NULL;

/* id table for all core apis, the same order with BeepCoreInterface */
uint32 beep_core_interface_id_table[] = {
  ACODEC_API_GET_VERSION_INFO,
  ACODEC_API_CREATE_CODEC,
  ACODEC_API_DELETE_CODEC,
  ACODEC_API_RESET_CODEC,
  ACODEC_API_SET_PARAMETER,
  ACODEC_API_GET_PARAMETER,
  ACODEC_API_DEC_FRAME,
};

static gchar *
beep_strip_blank (gchar * str)
{
  gchar *ret = NULL;
  if (str) {
    while ((*str == ' ') && (*str != '\0')) {
      str++;
    }
    if (*str != '\0') {
      ret = str;
    }
  }
  return ret;
}

BeepCoreDlEntry *
beep_config_to_dllentry (GKeyFile * keyfile, gchar * group)
{
  BeepCoreDlEntry *entry = MM_MALLOC (sizeof (BeepCoreDlEntry));

  if (entry) {
    char *value;
    gint num;
    memset (entry, 0, sizeof (BeepCoreDlEntry));
    entry->name = g_strdup (group);
    entry->dl_names =
        g_key_file_get_string_list (keyfile, group, KEY_LIB, &num, NULL);
    entry->mime = g_key_file_get_string (keyfile, group, KEY_MIME, NULL);

    if (g_key_file_has_key (keyfile, group, KEY_LONGNAME, NULL)) {
      entry->longname =
          g_key_file_get_string (keyfile, group, KEY_LONGNAME, NULL);
    }
    if (g_key_file_has_key (keyfile, group, KEY_DESCRIPTION, NULL)) {
      entry->description =
          g_key_file_get_string (keyfile, group, KEY_DESCRIPTION, NULL);
    }
    if (g_key_file_has_key (keyfile, group, KEY_RANK, NULL)) {
      entry->rank = g_key_file_get_integer (keyfile, group, KEY_RANK, NULL);
    } else {
      entry->rank = BEEP_DEFAULT_RANK;
    }
    entry->next = NULL;


    if ((!entry->name) || (!entry->dl_names) || (!entry->mime)) {
      GST_ERROR ("beep config file corrupt, please check %s",
          BEEP_REGISTRY_FILE_DEFAULT);
      goto fail;
    }

  }
  return entry;

fail:
  if (entry) {
    if (entry->dl_names)
      g_strfreev (entry->dl_names);
    if (entry->mime)
      g_free (entry->mime);
    if (entry->name)
      g_free (entry->name);
    if (entry->longname)
      g_free (entry->longname);
    if (entry->description)
      g_free (entry->description);
    MM_FREE (entry);
    entry = NULL;
  }
  return entry;

}


BeepCoreDlEntry *
beep_get_dll_entry_from_file (char *filename)
{
  BeepCoreDlEntry *dlentry = NULL, *entry;

  GKeyFile *keyfile = g_key_file_new ();
  gchar **groups, **group;

  if ((keyfile == NULL)
      || (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE,
              NULL)))
    goto fail;

  group = groups = g_key_file_get_groups (keyfile, NULL);


  while (*group) {

    entry = beep_config_to_dllentry (keyfile, *group);

    if (entry) {
      entry->next = dlentry;
      dlentry = entry;
    }

    group++;
  };

  g_strfreev (groups);


fail:
  if (keyfile) {
    g_key_file_free (keyfile);
  }
  return dlentry;

}

void
_free_dll_entry (BeepCoreDlEntry * entry)
{
  BeepCoreDlEntry *next;
  while (entry) {
    next = entry->next;
    if (entry->dl_names)
      g_strfreev (entry->dl_names);
    if (entry->mime)
      g_free (entry->mime);
    if (entry->name)
      g_free (entry->name);
    if (entry->longname)
      g_free (entry->longname);
    if (entry->description)
      g_free (entry->description);
    MM_FREE (entry);
    entry = next;
  }
}

BeepCoreDlEntry *
beep_get_core_entry ()
{
  if (g_beep_core_entry == NULL) {
    char *beepenv = getenv (BEEP_REGISTRY_FILE_ENV_NAME);
    if (beepenv == NULL) {
      beepenv = BEEP_REGISTRY_FILE_DEFAULT;
    }
    g_beep_core_entry = beep_get_dll_entry_from_file (beepenv);
  }
  return g_beep_core_entry;
}


BeepCoreInterface *
_beep_core_create_interface_from_entry (BeepCoreDlEntry * dlentry)
{
  BeepCoreInterface *inf = NULL;
  void *dl_handle = NULL;
  int i;
  int32 err;
  int total = G_N_ELEMENTS (beep_core_interface_id_table);
  void **papi;
  tUniACodecQueryInterface query_interface;

  i = 0;
  while ((dl_handle == NULL)
      && (dlentry->dl_name = beep_strip_blank (dlentry->dl_names[i++]))) {
    dl_handle = dlopen (dlentry->dl_name, RTLD_LAZY);
  };

  if (!dl_handle) {
    GST_ERROR ("Demux core %s error or missed! \n(Err: %s)\n",
        dlentry->dl_name, dlerror ());
    goto fail;
  }

  query_interface = dlsym (dl_handle, CORE_QUERY_INTERFACE_API_NAME);

  if (query_interface == NULL) {
    GST_ERROR ("can not find symbol %s\n", CORE_QUERY_INTERFACE_API_NAME);
    goto fail;
  }
  inf = g_new0 (BeepCoreInterface, 1);

  if (inf == NULL)
    goto fail;

  papi = (void **) inf;

  for (i = 0; i < total; i++) {
    err = query_interface (beep_core_interface_id_table[i], papi);
    if (err) {
      *papi = NULL;
    }
    papi++;
  }

  inf->dl_handle = dl_handle;

  if (inf->getVersionInfo) {
    const char *version = (inf->getVersionInfo) ();
    if (version) {
      g_print (BLUE_STR
          ("Beep: %s \nCore: %s\n  mime: %s\n  file: %s\n",
              VERSION, version, dlentry->mime, dlentry->dl_name));
    }
  }
  inf->dlentry = dlentry;

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


BeepCoreDlEntry *
_beep_core_find_match_dlentry (GstCaps * caps)
{
  BeepCoreDlEntry *dlentry = NULL;
  GstCaps *super_caps = NULL;
  BeepCoreDlEntry *pentry = beep_get_core_entry ();

  while (pentry) {
    super_caps = gst_caps_from_string (pentry->mime);

    if ((super_caps) && (gst_caps_is_subset (caps, super_caps))) {
      dlentry = pentry;
      gst_caps_unref (super_caps);
      break;

    }
    gst_caps_unref (super_caps);
    pentry = pentry->next;
  }
  return dlentry;
}


GstCaps *
beep_core_get_cap (BeepCoreDlEntry * entry)
{
  GstCaps *caps = NULL;
  void *dlhandle;
  int i;

  if (entry) {
    dlhandle = NULL;
    i = 0;
    while ((dlhandle == NULL)
        && (entry->dl_name = beep_strip_blank (entry->dl_names[i++]))) {
      dlhandle = dlopen (entry->dl_name, RTLD_LAZY);
    };

    if (dlhandle) {
      caps = gst_caps_from_string (entry->mime);
      dlclose (dlhandle);
    }
  }
  return caps;
}



GstCaps *
beep_core_get_caps ()
{
  GstCaps *caps = NULL;
  void *dlhandle;
  int i;
  BeepCoreDlEntry *pentry = beep_get_core_entry ();

  while (pentry) {

    if (caps) {
      GstCaps *newcaps = beep_core_get_cap (pentry);
      if (newcaps) {
        if (!gst_caps_is_subset (newcaps, caps)) {
          gst_caps_append (caps, newcaps);
        } else {
          gst_caps_unref (newcaps);
        }
      }
    } else {
      caps = beep_core_get_cap (pentry);
    }

    pentry = pentry->next;
  }
  return caps;
}


BeepCoreInterface *
beep_core_create_interface_from_caps (GstCaps * caps)
{
  BeepCoreInterface *inf = NULL;
  BeepCoreDlEntry *pentry = _beep_core_find_match_dlentry (caps);

  if (pentry) {
    inf = _beep_core_create_interface_from_entry (pentry);
    if ((inf) && (!inf->createDecoder)) {
      beep_core_destroy_interface (inf);
      inf = NULL;
    }
  }
  return inf;
}


void
beep_core_destroy_interface (BeepCoreInterface * inf)
{
  if (inf == NULL)
    return;

  if (inf->dl_handle) {
    dlclose (inf->dl_handle);
  }

  g_free (inf);
}


void __attribute__ ((destructor)) beepregistry_c_destructor (void);


void
beepregistry_c_destructor ()
{
  if (g_beep_core_entry) {
    _free_dll_entry (g_beep_core_entry);
    g_beep_core_entry = NULL;
  }

  MM_DEINIT_DBG_MEM ();
}

gboolean
beepDLAvialable (BeepCoreDlEntry *entry)
{
  void *dlhandle = NULL;
  int i = 0;
  gboolean ret = FALSE;

  while ((dlhandle == NULL)
      && (entry->dl_name = beep_strip_blank (entry->dl_names[i++]))) {
    dlhandle = dlopen (entry->dl_name, RTLD_LAZY);
  };

  if (dlhandle) {
    dlclose (dlhandle);
    ret = TRUE;
  }

  return ret;
}
#endif
