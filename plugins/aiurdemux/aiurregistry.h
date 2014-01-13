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
 * Module Name:    aiurregistry.h
 *
 * Description:    Head file of unified parser core functions
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 */

#ifndef __AIURREGISTRY_H__
#define __AIURREGISTRY_H__
#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/tag/tag.h>
#include "fsl_parser.h"
#include "../../libs/gstsutils/gstsutils.h"

typedef struct
{
  /* creation & deletion */
  FslParserVersionInfo getVersionInfo;
  FslCreateParser createParser;
  FslDeleteParser deleteParser;
  FslCreateParser2 createParser2;

  /* index export/import */
  FslParserInitializeIndex initializeIndex;
  FslParserImportIndex importIndex;
  FslParserExportIndex exportIndex;

  /* movie properties */
  FslParserIsSeekable isSeekable;
  FslParserGetMovieDuration getMovieDuration;
  FslParserGetUserData getUserData;
  FslParserGetMetaData getMetaData;

  FslParserGetNumTracks getNumTracks;

  FslParserGetNumPrograms getNumPrograms;
  FslParserGetProgramTracks getProgramTracks;

  /* generic track properties */
  FslParserGetTrackType getTrackType;
  FslParserGetDecSpecificInfo getDecoderSpecificInfo;
  FslParserGetTrackDuration getTrackDuration;
  FslParserGetLanguage getLanguage;
  FslParserGetBitRate getBitRate;

  /* video properties */
  FslParserGetVideoFrameWidth getVideoFrameWidth;
  FslParserGetVideoFrameHeight getVideoFrameHeight;
  FslParserGetVideoFrameRate getVideoFrameRate;
  FslParserGetVideoFrameRotation getVideoFrameRotation;

  /* audio properties */
  FslParserGetAudioNumChannels getAudioNumChannels;
  FslParserGetAudioSampleRate getAudioSampleRate;
  FslParserGetAudioBitsPerSample getAudioBitsPerSample;
  FslParserGetAudioBlockAlign getAudioBlockAlign;
  FslParserGetAudioChannelMask getAudioChannelMask;
  FslParserGetAudioBitsPerFrame getAudioBitsPerFrame;

  /* text/subtitle properties */
  FslParserGetTextTrackWidth getTextTrackWidth;
  FslParserGetTextTrackHeight getTextTrackHeight;

  /* sample reading, seek & trick mode */
  FslParserGetReadMode getReadMode;
  FslParserSetReadMode setReadMode;

  FslParserEnableTrack enableTrack;

  FslParserGetNextSample getNextSample;
  FslParserGetNextSyncSample getNextSyncSample;

  FslParserGetFileNextSample getFileNextSample;
  FslParserGetFileNextSyncSample getFileNextSyncSample;

  FslParserSeek seek;

  /* add new interface here */

  void *dl_handle;              /* must be last, for dl handle */
  //GstsutilsEntry *dlentry;
  gchar * name;
  
  const char *coreid;

} AiurCoreInterface;

GstCaps *aiur_core_get_caps ();
AiurCoreInterface *aiur_core_create_interface_from_caps (GstCaps * caps);

void
aiur_core_destroy_interface (AiurCoreInterface * inf);


#endif /* __AIURREGISTRY_H__ */
