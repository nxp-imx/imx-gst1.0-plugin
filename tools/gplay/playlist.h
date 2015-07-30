/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc. All rights reserved.
 *
 */
 
/*
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

/*
 * =====================================================================================
 *
 *       Filename:  playlist.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/06/2009 02:17:14 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Dr. Fritz Mehner (mn), mehner@fh-swf.de
 *        Company:  FH Südwestfalen, Iserlohn
 *
 *       Modified:  Haihua Hu
 *           Date:  08/03/2015
 *        Company:  Freescale Semiconductor
 * =====================================================================================
 */
#ifndef __PLAYLIST_H__
#define __PLAYLIST_H__

#include <gst/gst.h>
#include "playengine.h"

typedef void * PlayListHandle;

PlayListHandle    createPlayList();
void              destroyPlayList (PlayListHandle handle);

PlayEngineResult  isPlayListEmpty (PlayListHandle handle, gboolean *empty);
PlayEngineResult  isLastItem      (PlayListHandle handle, gboolean *islast);
PlayEngineResult  isFirstItem     (PlayListHandle handle, gboolean *isfirst);
PlayEngineResult  addItemAtTail   (PlayListHandle handle, gchar *iName);

const gchar *     getFirstItem    (PlayListHandle handle);
const gchar *     getLastItem     (PlayListHandle handle);
const gchar *     getCurItem      (PlayListHandle handle);
const gchar *     getPrevItem     (PlayListHandle handle);
const gchar *     getNextItem     (PlayListHandle handle);
 
#endif
