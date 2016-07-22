/*
 * Copyright (C) 2015-2015 Freescale Semiconductor, Inc. All rights reserved.
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
 *       Filename:  playlist.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/06/2009 02:17:20 PM
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "playlist.h"

#define MEM_ALLOC(size) malloc((size))
#define MEM_FREE(ptr) free((ptr))
#define MEM_ZERO(ptr,size) memset((ptr),0,(size))
#define PL_ERR printf


typedef struct _PlayItemCtl{
    gchar * iName;
    struct _PlayItemCtl * prev;
    struct _PlayItemCtl * next;
}PlayItemCtl;


typedef struct _PlayList{
    PlayItemCtl * head;
    PlayItemCtl * tail;
    PlayItemCtl * cur;
}PlayList;

static void 
destroyPlayItemCtl(PlayItemCtl * item)
{
    if (item->iName)
      MEM_FREE(item->iName);
    MEM_FREE(item);
}

PlayListHandle 
createPlayList()
{
    PlayList * pl = MEM_ALLOC(sizeof(PlayList));
    if (pl==NULL){
        PL_ERR("%s failed, no memory!\n", __FUNCTION__);
        goto err;
    }

    MEM_ZERO(pl, sizeof(PlayList));

    return (PlayListHandle)pl;

err:
    if (pl){
        MEM_FREE(pl);
        pl=NULL;
    }

    return (PlayListHandle)pl;
}

PlayEngineResult
isPlayListEmpty(PlayListHandle handle,
                gboolean *empty)
{
  PlayList * pl = (PlayList *)handle;
  if(pl == NULL || empty == NULL)
  {
      PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
      return PLAYENGINE_ERROR_BAD_PARAM;
  }
  if(!pl->head)
    *empty = TRUE;
  else
    *empty = FALSE;

  return PLAYENGINE_SUCCESS;
}

PlayEngineResult
addItemAtTail(PlayListHandle handle,
              gchar * iName)
{
    PlayItemCtl * item = NULL;
    PlayList * pl = (PlayList *)handle;
    
    if ((pl==NULL)||(iName==NULL)){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        goto err;
    }

    item = MEM_ALLOC(sizeof(PlayItemCtl));
    if (item==NULL){
        PL_ERR("%s failed, no memory!\n", __FUNCTION__);
        goto err;
    }

    MEM_ZERO(item, sizeof(PlayItemCtl));

    item->iName = (gchar *)MEM_ALLOC(strlen(iName)+1);
    if (item->iName == NULL)
    {
        PL_ERR("%s failed, no memory!\n", __FUNCTION__);
        goto err;
    }
    strcpy(item->iName, iName);

    item->prev = pl->tail;
    
    if (pl->head){
        pl->tail->next = item;
        pl->tail= item;
    }else{
        pl->head = item;
        pl->tail = item;
        pl->cur = item;
    }
    return PLAYENGINE_SUCCESS;
err:
    if (item){
        MEM_FREE(item);
        item=NULL;
    }
    return PLAYENGINE_FAILURE;
}

const gchar *
getFirstItem(PlayListHandle handle)
{
    PlayList * pl = (PlayList *)handle;
    if (pl==NULL){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        return NULL;
    }
    if(!pl->head)
    {
        PL_ERR("Play list is empty\n");
        return NULL;
    }
    pl->cur = pl->head;
    return pl->head->iName;
}

const gchar * 
getLastItem(PlayListHandle handle)
{
    PlayList * pl = (PlayList *)handle;
    if (pl==NULL){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        return NULL;
    }
    if(!pl->head)
    {
        PL_ERR("Play list is empty\n");
        return NULL;
    }
    pl->cur = pl->tail;
    return pl->tail->iName;
}

const gchar *
getCurItem(PlayListHandle handle)
{
    PlayList * pl = (PlayList *)handle;
    if (pl==NULL){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        return NULL;
    }
    if(!pl->head)
    {
        PL_ERR("Play list is empty\n");
        return NULL;
    }
    return pl->cur->iName;
}

const gchar *
getPrevItem(PlayListHandle handle)
{
    PlayList * pl = (PlayList *)handle;
    if (pl==NULL){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        return NULL;
    }
    if(!pl->head)
    {
        PL_ERR("Play list is empty\n");
        return NULL;
    }
    if(!pl->cur->prev)
    {
        PL_ERR("%s No previous item!\n", __FUNCTION__);
        return NULL;
    }
    pl->cur = pl->cur->prev;
    return pl->cur->iName;
}

const gchar * 
getNextItem(PlayListHandle handle)
{
    PlayList * pl = (PlayList *)handle;
    if (pl==NULL){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        return NULL;
    }
    if(!pl->head)
    {
        PL_ERR("Play list is empty\n");
        return NULL;
    }
    if(!pl->cur->next)
    {
        PL_ERR("%s No next item!\n", __FUNCTION__);
        return NULL;
    }
    pl->cur = pl->cur->next;
    return pl->cur->iName;
}

PlayEngineResult
isLastItem(PlayListHandle handle,
            gboolean *islast)
{
    PlayList * pl = (PlayList *)handle;
    if (pl==NULL || islast==NULL){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        return PLAYENGINE_ERROR_BAD_PARAM;
    }
    if(!pl->head)
    {
        PL_ERR("Play list is empty\n");
        return PLAYENGINE_FAILURE;
    }
    if(pl->cur == pl->tail)
    {
        *islast = TRUE;
    }else
    {
        *islast = FALSE;
    }
    return PLAYENGINE_SUCCESS;
}

PlayEngineResult
isFirstItem(PlayListHandle handle,
            gboolean *isfirst)
{
    PlayList * pl = (PlayList *)handle;
    if (pl==NULL || isfirst==NULL){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        return PLAYENGINE_ERROR_BAD_PARAM;
    }
    if(!pl->head)
    {
        PL_ERR("Play list is empty\n");
        return PLAYENGINE_FAILURE;
    }
    if(pl->cur == pl->head)
    {
        *isfirst = TRUE;
    }else
    {
        *isfirst = FALSE;
    }
    return PLAYENGINE_SUCCESS;
}

void 
destroyPlayList(PlayListHandle handle)
{
    PlayList * pl = (PlayList *)handle;
    PlayItemCtl * item, *itemnext;

    if (pl==NULL){
        PL_ERR("%s failed, parameters error!\n", __FUNCTION__);
        return;
    }
    item = pl->head;
    while(item){
        itemnext = item->next;
        destroyPlayItemCtl(item);
        item=itemnext;
    }
    MEM_FREE(pl);
    pl = NULL;
}
