/*
 * Copyright (c) 2011-2012, Freescale Semiconductor, Inc. 
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
 * Module Name:    aiuridxtab.h
 *
 * Description:    Head file of utils for import/export index table
 *                 for unified parser gstreamer plugin
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */


#ifndef __AIURIDXTAB_H__
#define __AIURIDXTAB_H__

#define AIUR_IDX_TABLE_MAX_SIZE 1000000


typedef struct
{
  guint magic;
  guint version;
} AiurIdxTabHead;

typedef struct
{
  unsigned int readmode;
  unsigned int size;
} AiurIdxTabInfo;


typedef struct
{
  AiurIdxTabHead head;
  AiurIdxTabInfo info;
  gint coreid_len;
  gchar *coreid;
  unsigned char *idx;
  unsigned int crc;
} AiurIndexTable;


void
aiurdemux_destroy_idx_table (AiurIndexTable * idxtable);

AiurIndexTable *aiurdemux_create_idx_table (int size, const char *coreid);
AiurIndexTable *aiurdemux_import_idx_table (gchar * filename);
int
aiurdemux_export_idx_table (const char *filename, AiurIndexTable * itab);



#endif /* __AIURIDXTAB_H__ */
