
/***********************************************************************
 * Copyright (c) 2009-2013, Freescale Semiconductor, Inc.
 * All modifications are confidential and proprietary information
 * of Freescale Semiconductor, Inc. ALL RIGHTS RESERVED.
 ***********************************************************************/

/*
 *
 *  History :
 *  Date             Author              Version    Description
 *
 *  Oct, 2009        Amanda              1.0        Initial Version
 *  Jan, 2010        Amanda              1.1        Extend user data ID.
 *  Apr, 2010        Amanda              2.0        Further unify API, add entry point "FslParserInit" for DLL loading.
 *  Mar, 2010        Larry               2.1        Add API for getting Meta data
 *  Sep, 2011        Fanghui             2.2        Add USER_DATA_TRACKNUMBER, USER_DATA_TOTALTRACKNUMBER
 *  May, 2012        Fanghui             2.3		Change FslParserGetProgramTracks definition
 *  May, 2012        Fanghui             2.4		Add USER_DATA_LOCATION
 *  Jun, 2012        Fanghui             2.5		Add USER_DATA_PROGRAMINFO
 *  Sep, 2012        Fanghui             2.6		Add USER_DATA_PMT
 */

#ifndef _FSL_PARSER_COMMON_H
#define _FSL_PARSER_COMMON_H

#include "fsl_media_types.h"
#include "fsl_types.h"

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif


/* Warning:
 * It's not recommended to use any enum types as API arguments or return value!
 * Please use data types that can explicitly tell the data length and asign them the listed enum values.
 * It's because different compilers can treat enum as different types such as integer or unsinged char.
 * If the parser library and plug-in(filter) are built by different compliers, the data length mismatch
 * will cause error.
 */


#ifdef __WINCE
	#define DEBUGMSG
    //#define PARSERMSG(cond, fmt, ...) DEBUGMSG(cond, _T(fmt), __VA_ARGS__))
    //#define PARSERMSG(fmt, ...) DEBUGMSG(1, (_T(fmt),  __VA_ARGS__))
    #define PARSERMSG(fmt, ...)
#elif WIN32
	#define DEBUGMSG(cond, fmt) printf fmt
	#define PARSERMSG(fmt, ...) printf(fmt, __VA_ARGS__)
#else /* linux platform */
    #ifdef DEBUG
        #define PARSERMSG printf
    #else
        #define PARSERMSG(fmt...)
    #endif
#endif


typedef void * FslParserHandle;

#define PARSER_INVALID_TRACK_NUMBER   (-1)

#define PARSER_UNKNOWN_DURATION 0    /* Unknown movie, track or sample duration.
                                       In some broadcasting sources (eg. MMS of broadcasting)
                                       or recording clips, the movie or track's duration is set to 0.
                                       It means the duration is unknown, not an empty clip or track.
                                       The plug-in and the core parser shall just try to read as many sample as possible until EOF.*/

#define PARSER_UNKNOWN_TIME_STAMP (-1) /* The time stamp is unknown. Usually used when the exact audio samples
                                        are not known until after decoding. And so only audio decoder can give
                                        a valid time stamp for each decoded audio frame.
                                        However, for the 1st sample after seeking,
                                        the parser MUST NOT use this value but shall give a valid time stamp.*/

#define PARSER_UNKNOWN_BITRATE  0    /* unknown bitrate */


/*
 * Common error codes of parsers,
 * within the range [-100 , +100].
 * Different parsers can extend the format specific errors OUTSIDE this range,
 * in their own API header files.
 */

enum
{
    PARSER_SUCCESS  = 0,
    PARSER_EOS = 1,    /* reach the end of the track/movie */
    PARSER_BOS = 2,    /* reach the beginning of the track/movie */
    PARSER_NEED_MORE_DATA = 3,  /* No longer used. Shall use "PARSER_INSUFFICIENT_DATA" */

    /* errors */
    PARSER_ERR_UNKNOWN = -1, /* Unknown error, not captured by parser logic */

    PARSER_ERR_INVALID_API = -2, /* Some common API is not implemented properly */

    PARSER_NOT_IMPLEMENTED = -5, /* No support for some feature. */
    PARSER_ERR_INVALID_PARAMETER = -6, /* parameters are invalid */

    PARSER_INSUFFICIENT_MEMORY = -7, /* memory not enough, causing general memory allocation failure */
    PARSER_INSUFFICIENT_DATA = -8, /* data not enough, parser need more data to go ahead */

    PARSER_ERR_NO_OUTPUT_BUFFER = -9, /* can not get sample buffer for output */

    PARSER_FILE_OPEN_ERROR = -10,
    PARSER_READ_ERROR = -11, /* file read error, no need for further error concealment */
    PARSER_WRITE_ERROR = -12,
    PARSER_SEEK_ERROR = -13, /* file system seeking error */
    PARSER_ILLEAGAL_FILE_SIZE = -14, /* file size is wrong or exceeds parser's capacity.
                                       (some parser can not handle file larger than 2GB)*/
    PARSER_ILLEAGAL_OPERATION =-15, /* the parser is being used improperly */

    PARSER_ERR_INVALID_MEDIA = -20, /* invalid or unsupported media format */

    PARSER_ERR_NOT_SEEKABLE = -21, /* This file is not seekable and does not support trick mode */

    /* error concealment */
    PARSER_ERR_CONCEAL_FAIL = -22, /* Error in bitstream and no sample can be found by error concealment.
                                If the file is seekable, it's better to perform a seeking than further
                                searching the bit stream for the next sample. */

    PARSER_ERR_MEMORY_ACCESS_VIOLATION = -25, /* internal memory access error */

    PARSER_ERR_TRACK_DISABLED = -30, /* The track is disabled and no media samples can be read from it. Only enabled track can output samples.*/
    PARSER_ERR_INVALID_READ_MODE = -32, /* The reading mode is invalid, or some operation is illegal under current reading mode */


};


/*********************************************************************
 * Reading mode. There are two options:
 * a. File-based sample reading.
 *      The reading order is same as that of track interleaving in the file.
 *      Mainly for streaming application.
 *
 * b. Track-based sample reading.
 *      Each track can be read independently from each other.
 *
 * Note:
 * A parser may support only one reading mode. Setting it to a not-supported mode will fail.
 * And it usually has a default reading mode.
 ********************************************************************/
enum
{
    PARSER_READ_MODE_FILE_BASED = 0, /* File-based sample reading.*/
    PARSER_READ_MODE_TRACK_BASED /* Track-based sample reading.*/
};




/*********************************************************************
 * sample flags :
 * 32-bit long, properties of a sample read.
 * The low 16 bits is reserved for common flag.
 * Parsers can use high 16 bits to define their own flags.
 ********************************************************************/
#define FLAG_SYNC_SAMPLE 0X01  /* This is a sync sample */

#define FLAG_SAMPLE_ERR_CONCEALED 0X02 /* This sample is got by error concealment, such as searching the bitstream.*/

#define FLAG_SAMPLE_SUGGEST_SEEK 0X04   /* A seeking is suggested. Although sample is got by error concealment,
                                        A/V sync may be impacted.
                                        If the file is seekable, a seeking on all tracks can save the A/V sync.*/
#define FLAG_SAMPLE_NOT_FINISHED 0X08   /* Sample is NOT finished at this call, large samples can be output in several calls. */

#define FLAG_UNCOMPRESSED_SAMPLE 0X10 /* This is a uncompressed sample.
                                        Warning:
                                        A track may have both compressed & uncompressed samples.
                                        But some AVI clips seem to abuse this flag, sync samples are mark as uncompressed,
                                        although they are actually compressed ones.
                                        Now suggest not care this flag.*/

#define FLAG_SAMPLE_NEWSEG       0x20 /* A new segment of new sample */

/*********************************************************************
 * seeking flags :
 when to seek, must set one of the following flags
 ********************************************************************/
#define SEEK_FLAG_NEAREST 0X01  /* Default flag. The actual seeked time shall be nearest one to the given time (can be later or earlier)*/
#define SEEK_FLAG_NO_LATER  0X02    /* The actual seeked time shall be no later than the given time */
#define SEEK_FLAG_NO_EARLIER 0X03    /* The actual seeked time shall be no earlier than the given time */
#define SEEK_FLAG_FUZZ 0X04 /* Reserved. No accurate request on time but request quick response.
                               Parsers shall optimize performances with other flags.*/


/*********************************************************************
 * direction for trick mode/sync sample reading
 ********************************************************************/
#define FLAG_BACKWARD   0X00
#define FLAG_FORWARD    0x01

/*********************************************************************
 * file flags:
 * return flags of GetFlag() in FslFileStream
 * Return value for some cases:
 *   0 local playback.(default)
 *   1 illegal
 *   2 http streaming, which source is seekable, but seek may affect the fluency of playback.
 *   3 live streaming such as rtp or udp streaming.
 ********************************************************************/
#define FILE_FLAG_NON_SEEKABLE   0X01
//file source should be read in sequence. You should not read from random position even if the file is seekable
#define FILE_FLAG_READ_IN_SEQUENCE   0X02

/*********************************************************************
 * User data ID
 * Some File level metadata
*********************************************************************/
typedef enum FSL_PARSER_USER_DATA_TYPE
{
    USER_DATA_TITLE = 0,      /* title of the content */
    USER_DATA_LANGUAGE,       /* user data may tell the language of the movie as a string */
    USER_DATA_GENRE,          /* genre of the content, mainly music */
    USER_DATA_ARTIST,         /* main artist and performer */
    USER_DATA_COPYRIGHT,      /* copyright statement */
    USER_DATA_COMMENTS,       /* comments of the content */
    USER_DATA_CREATION_DATE,  /* date the movie content was created */
    USER_DATA_RATING,         /* ? */
    USER_DATA_ALBUM,          /* album name of music content */
    USER_DATA_VCODECNAME,     /* video codec name */
    USER_DATA_ACODECNAME,     /* audio codec name */
    USER_DATA_ARTWORK,        /* artwork of movie or music */
    USER_DATA_COMPOSER,       /* name of composer */
    USER_DATA_DIRECTOR,       /* name of movie's director */
    USER_DATA_INFORMATION,    /* information about the movie */
    USER_DATA_CREATOR,        /* name of the file creator or maker */
    USER_DATA_PRODUCER,       /* name of producer */
    USER_DATA_PERFORMER,      /* name of performer */
    USER_DATA_REQUIREMENTS,   /* special hardware and software requirements */
    USER_DATA_SONGWRITER,     /* name of songwriter */
    USER_DATA_MOVIEWRITER,    /* name of movie's writer */
    USER_DATA_TOOL,           /* writing application */
    USER_DATA_DESCRIPTION,    /* movie description */
    USER_DATA_TRACKNUMBER,    /* track number */
    USER_DATA_TOTALTRACKNUMBER,    /* total track number */
    USER_DATA_LOCATION,       /* geographic location */  

    /* add more? */
    USER_DATA_CHAPTER_MENU,     /* Chapter Menu information */
    USER_DATA_FORMATVERSION,   /* container format version */
    USER_DATA_PROFILENAME,     /* Profile Name (DivX)*/

    USER_DATA_PROGRAMINFO,     /* program info for ts */
    USER_DATA_PMT,             /* program map table */

    USER_DATA_AUD_ENC_DELAY,   /* audio encoding delay */
    USER_DATA_AUD_ENC_PADDING, /* audio encoding padding */
    USER_DATA_DISCNUMBER,      /* disc number */

    USER_DATA_MAX
} UserDataID;

/*********************************************************************
 * User data format
*********************************************************************/
typedef enum FSL_PARSER_USER_DATA_FORMAT
{
    USER_DATA_FORMAT_UTF8,         /* all strings in stream should be convert to UTF-8 and output */

    USER_DATA_FORMAT_INT_BE,
    USER_DATA_FORMAT_UINT_BE,
    USER_DATA_FORMAT_FLOAT32_BE,
    USER_DATA_FORMAT_FLOAT64_BE,

    USER_DATA_FORMAT_JPEG,
    USER_DATA_FORMAT_PNG,
    USER_DATA_FORMAT_BMP,
    USER_DATA_FORMAT_GIF,

    USER_DATA_FORMAT_CHAPTER_MENU,
    /* add more? */

    USER_DATA_FORMAT_PROGRAM_INFO,
    USER_DATA_FORMAT_PMT_INFO,

    USER_DATA_FORMAT_INT_LE,
    USER_DATA_FORMAT_UINT_LE,

    USER_DATA_FORMAT_MAX
} UserDataFormat;


/*********************************************************************
 * file I/O interface on a file or live source.

open
    Open a local file or URL.
    Arguments:
        fileName [in] File name or url to open.
                      To open the movie source file, just set file name to NULL.
                      To open another external file for some track (eg. MP4), set the url.

        mode [in] Open mode, same as libc. Such as "rb".

    Return value:
        Handle of the opened file. NULL for failure.

read
    Read data from the file.
    Arguments:
        handle [in] Handle of the file.
        buffer [in] Pointer to a block of memory, to receive the data.
        size[in] Data size to read, in bytes.

    Return value:
        The total number of bytes successfully read.
        If this number differs from the size parameter, either an error occurred or the EOF was reached.


seek
    Seek the stream.
    Arguments:
        handle [in] Handle of the file.
        offset [in] The offset.
                    To move to a position before the end-of-file, you need to pass a negative value in offset and set whence to SEEK_END.

        whence in]  The new position, measured in bytes from the beginning of the file,
                    is obtained by adding offset to the position specified by whence.
                    SEEK_SET - Set position equal to offset bytes.
                    SEEK_CUR - Set position to current location plus offset .
                    SEEK_END - Set position to end-of-file plus offset.
    Return value:
        Upon success, returns 0; otherwise, returns -1.

tell
    Tell the position of the file pointer
    Arguments:
        handle [in] Handle of the file.

    Return value:
        Returns the position of the file pointer in bytes; i.e., its offset into the file stream.
        If error occurs or this feature can not be supported (eg. broadcast application), returns -1.

size
    Tell the size of the entire file.
    Arguments:
        handle [in] Handle of the file.
    Return value:
        Returns the file size in bytes.
        If error occurs or this feature can not be supported (eg. broadcast application), returns -1.

check_available_bytes
    Tell the availble bytes of the file. Especially useful for a live source file (streaming).
    The parser can decide not to read if cached data is not enough and so avoid reading failure in unexpected context.
    For a local file, any bytes request from the parser can be met as long as it's within the file range.

    Arguments:
        handle [in] Handle of the file.
        bytes_requested [in]    Bytes requested for further parsing. This information can help the application
                                to cache enough data before calling parser API next time.
                                If the parser can not know the exact data size needed, set it to 0.

    Return value:
        If the file source can always meet the data reading request unless EOF (eg. a local file or a pull-mode live source),
        returns the data size from the current file pointer to the file end.

        Otherwise (eg. a push-mode live source), returns the cached data size.
        If error occurs or this feature can not be supported (eg. broadcast application), returns -1.

close
    Close the file.
    Arguments:
        handle [in] Handle of the file.

    Return value:
        Upon success, returns 0; otherwise, returns -1.
*/

typedef void * FslFileHandle;

/* Seek origin, position from where offset is added, same as libc */
#ifndef FSL_SEEK_SET
#define FSL_SEEK_SET 0 /* SEEK_SET, Beginning of file */
#endif
#ifndef FSL_SEEK_CUR
#define FSL_SEEK_CUR 1  /* SEEK_CUR, Current position of file pointer */
#endif
#ifndef FSL_SEEK_END
#define FSL_SEEK_END 2  /* SEEK_END, End of file */
#endif

typedef struct _FslFileStream
{
    FslFileHandle (*Open)(const uint8 * fileName, const uint8 * mode, void * context); /* Open a file or URL */
    int32 	(*Close)(FslFileHandle handle, void * context); /* Close the stream */
    uint32  (*Read)(FslFileHandle handle, void * buffer, uint32 size, void * context); /* Read data from the stream */
    int32 	(*Seek)(FslFileHandle handle, int64 offset, int32 whence, void * context);  /* Seek the stream */
    int64  	(*Tell)(FslFileHandle handle, void * context); /* Tell the current position from start of the stream */
    int64 	(*Size)(FslFileHandle handle, void * context); /* Get the size of the entire stream */
    int64   (*CheckAvailableBytes)(FslFileHandle handle, int64 bytesRequested, void * context); /* How many bytes cached but not read yet */
    uint32  (*GetFlag)(FslFileHandle handle, void * context);
    void * reserved[1];

} FslFileStream;


/*********************************************************************
 * Core parser memory callback function pointer table.
 *********************************************************************/
typedef struct
{
    void* (*Calloc) (uint32 numElements, uint32 size);
    void* (*Malloc) (uint32 size);
    void  (*Free) (void * ptr);
    void* (*ReAlloc)(void * ptr, uint32 size); /* necessary for index scanning!*/

}ParserMemoryOps; /* callback operation callback table */


/********************************************************************************************************
Callback functions to request/release an output buffer.
Usually, the core parser requests an output buffer, fill the media data and return it to the application
on GetNextSample(). But, on flushing (eg. on seek or deletion), the core parser need explicitly release
all buffers not returned yet.

RequestBuffer
    Request an output buffer.

    Arguments:
        streamNum [in] Track number, 0-based.

        size [in,out] The requested buffer size as input, and the size actually got as output, both in bytes.
                      The actually got size can be larger than the requested size, and the parser can
                      make full use of the buffer.

        bufContext [out] A buffer context from the application. The parser shall not modify it.

        parserContext [in] The parser context from the application, got on parser creation.

    Return value:
        Buffer pointer. NULL for failure.


ReleaseBuffer
    Release an output buffer explicitly.

    Arguments:
        streamNum [in] Track number, 0-based.
        pBuffer [in] Buffer to release.
        bufContext [in] The buffer context from the application, got on requestBuffer().
        parserContext [in] The parser context from the application, got on parser creation.

    Return value: none.

********************************************************************************************************/

typedef struct
{
    uint8* (*RequestBuffer) (uint32 streamNum, uint32 *size, void ** bufContext, void * parserContext);
    void (*ReleaseBuffer) (uint32 streamNum, uint8 * pBuffer, void * bufContext, void * parserContext);

}ParserOutputBufferOps;

typedef struct _ChapterInfo
{
    uint32 ChapterUID;      /* UID for chapter */
    uint32 dwStartTime;     /* Chapter's Start time in ms unit */
    uint32 dwStopTime;      /* Chapter's Stop time in ms unit */
    uint32 dwTitleSize;     /* Chapter Title size in byte */
    char * Title;           /* The string for Title */
}ChapterInfo;

typedef struct _strChapterMenu
{
    uint32 EditionUID;      /* UID for Movie/Edition */
    uint32 EdtionFlags;     /* Flags for Movie/Edition */
    uint32 dwChapterNum;    /* Total number of chapters in this Edition */
    ChapterInfo * pChapterList; /* the pointer to Chapter information list */
}ChapterMenu;

typedef struct _ProgramInfo
{
    uint32 m_dwChannel; //program_number in spec, for a broadcast channel.
    uint32 m_dwPID; //program PID(packet ID)
}ProgramInfo;

typedef struct _ProgramInfoMenu
{
    uint32 m_dwProgramNum;
    ProgramInfo m_atProgramInfo[0];
}ProgramInfoMenu;


#define INVALID_PID         (uint32)(-1)
#define INVALID_CHANNEL     (uint32)(-1)


typedef struct _TrackInfo
{
    uint32 m_dwTrackNo; //global track No
    uint32 m_dwPID;     //PID(packet ID)
    uint8 m_byLan[3];   //language
    uint32 m_dwReserved[16];    
}TrackInfo;

//single program map table
typedef struct _PMTInfo
{
    uint32 m_dwChannel; //program_number in spec, for a broadcast channel.
    uint32 m_dwPID;     //PID(packet ID)
    uint32 m_dwReserved[8];
    uint32 m_dwTrackNum;    
    TrackInfo *m_ptTrackInfo;
}PMTInfo;

//total program map table
typedef struct _PMTInfoList
{
    uint32 m_dwProgramNum;
    PMTInfo *m_ptPMTInfo;
}PMTInfoList;

/*********************************************************************************************************
 *                  API Function Prototypes List
 *
 * There are mandatory and optional APIs.
 * A core parser must implement the mandatory APIs while need not implement the optional one.
 * And in its DLL entry point "FslParserInit", it shall set the not-implemented function pointers to NULL.
 *
 *********************************************************************************************************/

/***************************************************************************************
 *
 *                Creation & Deletion
 *
 ***************************************************************************************/
/* mandatory */
typedef const char * (*FslParserVersionInfo)();

typedef int32  (*FslCreateParser)(  bool isLive,
                                    FslFileStream * streamOps,
                                    ParserMemoryOps * memOps,
                                    ParserOutputBufferOps * outputBufferOps,
                                    void * context,
                                    FslParserHandle * parserHandle);

typedef int32 (*FslDeleteParser)(FslParserHandle parserHandle);


/***************************************************************************************
 *
 *                 Index Table Loading, Export & Import
 *
 ***************************************************************************************/
/* optional */
typedef int32 (*FslParserInitializeIndex)(FslParserHandle parserHandle); /*Loading index from the movie file */

typedef int32 (*FslParserImportIndex)(  FslParserHandle parserHandle, /* Import index from outside */
                                        uint8 * buffer,
                                        uint32 size);

typedef int32 (*FslParserExportIndex)(  FslParserHandle parserHandle,
                                        uint8 * buffer,
                                        uint32 *size);


/************************************************************************************************************
 *
 *               Movie Properties
 *
 ************************************************************************************************************/
/* mandatory */
typedef int32 (*FslParserIsSeekable)(FslParserHandle parserHandle, bool * seekable);

typedef int32 (*FslParserGetMovieDuration)(FslParserHandle parserHandle,  uint64 * usDuration);

typedef int32 (*FslParserGetUserData)(  FslParserHandle parserHandle,
                                        uint32 userDataId,
                                        uint16 ** unicodeString,
                                        uint32 * stringLength);

typedef int32 (*FslParserGetMetaData)(  FslParserHandle parserHandle,
                                        UserDataID userDataId,
                                        UserDataFormat * userDataFormat,
                                        uint8 ** userData,
                                        uint32 * userDataLength);

typedef int32 (*FslParserGetNumTracks)(FslParserHandle parserHandle, uint32 * numTracks); /* single program interface */

typedef int32 (*FslParserGetNumPrograms)(FslParserHandle parserHandle, uint32 * numPrograms); /* multiple program interface */
typedef int32 (*FslParserGetProgramTracks)( FslParserHandle parserHandle,
                                            uint32 programNum,
                                            uint32 * numTracks,
                                            uint32 ** ppTrackNumList);

/************************************************************************************************************
 *
 *              General Track Properties
 *
 ************************************************************************************************************/
typedef int32 (*FslParserGetTrackType)( FslParserHandle parserHandle,
                                        uint32 trackNum,
                                        uint32 * mediaType,
                                        uint32 * decoderType,
                                        uint32 * decoderSubtype);

typedef int32 (*FslParserGetTrackDuration)( FslParserHandle parserHandle,
                                            uint32 trackNum,
                                            uint64 * usDuration); /* Duration 0 means an empty track */

typedef int32 (*FslParserGetLanguage)(  FslParserHandle parserHandle,
                                        uint32 trackNum,
                                        uint8 * threeCharCode);

/* optional */
typedef int32 (*FslParserGetBitRate)(   FslParserHandle parserHandle,
                                        uint32 trackNum,
                                        uint32 * bitrate);

typedef int32 (*FslParserGetDecSpecificInfo)(   FslParserHandle parserHandle,
                                                uint32 trackNum,
                                                uint8 ** data,
                                                uint32 * size);


/************************************************************************************************************
 *
 *               Video Properties, only for video media
 *
 ************************************************************************************************************/
/* mandatory */
typedef    int32 (*FslParserGetVideoFrameWidth)(FslParserHandle parserHandle, uint32 trackNum, uint32 *width);

typedef    int32 (*FslParserGetVideoFrameHeight)(FslParserHandle parserHandle, uint32 trackNum, uint32 *height);

/* optional */
typedef    int32 (*FslParserGetVideoFrameRate)( FslParserHandle parserHandle,
                                                uint32 trackNum,
                                                uint32 * rate,
                                                uint32 * scale);

typedef    int32 (*FslParserGetVideoFrameRotation)(FslParserHandle parserHandle, uint32 trackNum, uint32 *rotation);

/************************************************************************************************************
 *
 *               Audio Properties
 *
 ************************************************************************************************************/
/* mandatory */
typedef int32 (*FslParserGetAudioNumChannels)(  FslParserHandle parserHandle,
                                                uint32 trackNum,
                                                uint32 * numchannels);

typedef int32 (*FslParserGetAudioSampleRate)(   FslParserHandle parserHandle,
                                                uint32 trackNum,
                                                uint32 * sampleRate);

typedef int32 (*FslParserGetAudioBitsPerSample)(FslParserHandle parserHandle,
                                                uint32 trackNum,
                                                uint32 * bitsPerSample); /* bit depth */

/* optional */
typedef int32 (*FslParserGetAudioBlockAlign)(FslParserHandle parserHandle,
                                             uint32 trackNum,
                                             uint32 * blockAlign);

typedef int32 (*FslParserGetAudioBitsPerFrame)( FslParserHandle parserHandle,
                                                uint32 trackNum,
                                                uint32 *bits_per_frame); /* for Real audio */

typedef int32 (*FslParserGetAudioChannelMask)(  FslParserHandle parserHandle,
                                                uint32 trackNum,
                                                uint32 * channelMask); /* for WMA audio */


/************************************************************************************************************
 *
 *               Text/Subtitle Properties
 *
 ************************************************************************************************************/
/* mandatory */
typedef int32 (*FslParserGetTextTrackWidth)(FslParserHandle parserHandle,
                                            uint32 trackNum,
                                            uint32 * width);

typedef int32 (*FslParserGetTextTrackHeight)(   FslParserHandle parserHandle,
                                                uint32 trackNum,
                                                uint32 * height);


/************************************************************************************************************
 *
 *               Sample Reading, Seek & Trick Mode
 *
 * NOTE: if the core parser can not give a valid sample duration,
 *       setting the sample duration to ZERO is a good choice.
 *
 ************************************************************************************************************/
/* mandatory */
typedef int32 (*FslParserGetReadMode)(FslParserHandle parserHandle, uint32 * readMode);
typedef int32 (*FslParserSetReadMode)(FslParserHandle parserHandle, uint32 readMode);

typedef int32 (*FslParserEnableTrack)(  FslParserHandle parserHandle,
                                        uint32 trackNum,
                                        bool enable);

typedef int32 (*FslParserGetNextSample)(FslParserHandle parserHandle,
                                        uint32 trackNum,
                                        uint8 ** sampleBuffer,
                                        void  ** bufferContext,
                                        uint32 * dataSize,
                                        uint64 * usStartTime,
                                        uint64 * usDuration,
                                        uint32 * sampleFlags); /* Only for track-based sample reading. The application tell which track to read.*/


/* optional */
typedef int32 (*FslParserGetNextSyncSample)(FslParserHandle parserHandle,
                                            uint32 direction,
                                            uint32 trackNum,
                                            uint8 ** sampleBuffer,
                                            void  ** bufferContext,
                                            uint32 * dataSize,
                                            uint64 * usStartTime,
                                            uint64 * usDuration,
                                            uint32 * flags); /* only for trick mode on video track */


typedef int32 (*FslParserGetFileNextSample)(FslParserHandle parserHandle,
                                            uint32 * trackNum,
                                            uint8 ** sampleBuffer,
                                            void  ** bufferContext,
                                            uint32 * dataSize,
                                            uint64 * usStartTime,
                                            uint64 * usDuration,
                                            uint32 * sampleFlags); /* Only for file-based sample reading. The parser tell which track is being read.*/


/* optional */
typedef int32 (*FslParserGetFileNextSyncSample)(FslParserHandle parserHandle,
                                            uint32 direction,
                                            uint32 * trackNum,
                                            uint8 ** sampleBuffer,
                                            void  ** bufferContext,
                                            uint32 * dataSize,
                                            uint64 * usStartTime,
                                            uint64 * usDuration,
                                            uint32 * flags); /* only for trick mode on video track */


/* mandatory */
typedef int32 (*FslParserSeek)( FslParserHandle parserHandle,
                                uint32 trackNum,
                                uint64 * usTime,
                                uint32 flag);




/************************************************************************************************************
 *
 *               DLL entry point (mandatory) - to query parser interface
 *
 ************************************************************************************************************/
enum /* API function ID */
{
    /* creation & deletion */
    PARSER_API_GET_VERSION_INFO  = 0,
    PARSER_API_CREATE_PARSER     = 1,
    PARSER_API_DELETE_PARSER     = 2,

    /* index export/import */
    PARSER_API_INITIALIZE_INDEX  = 10,
    PARSER_API_IMPORT_INDEX      = 11,
    PARSER_API_EXPORT_INDEX      = 12,


    /* movie properties */
    PARSER_API_IS_MOVIE_SEEKABLE    = 20,
    PARSER_API_GET_MOVIE_DURATION   = 21,
    PARSER_API_GET_USER_DATA       = 22,
    PARSER_API_GET_META_DATA       = 23,

    PARSER_API_GET_NUM_TRACKS       = 25,

    PARSER_API_GET_NUM_PROGRAMS     = 26,
    PARSER_API_GET_PROGRAM_TRACKS   = 27,


    /* generic track properties */
    PARSER_API_GET_TRACK_TYPE                   = 30,
    PARSER_API_GET_TRACK_DURATION               = 31,
    PARSER_API_GET_LANGUAGE                     = 32,
    PARSER_API_GET_BITRATE                      = 36,
    PARSER_API_GET_DECODER_SPECIFIC_INFO        = 37,


    /* video properties */
    PARSER_API_GET_VIDEO_FRAME_WIDTH        = 50,
    PARSER_API_GET_VIDEO_FRAME_HEIGHT       = 51,
    PARSER_API_GET_VIDEO_FRAME_RATE         = 52,
    PARSER_API_GET_VIDEO_FRAME_ROTATION     = 53,


    /* audio properties */
    PARSER_API_GET_AUDIO_NUM_CHANNELS       = 60,
    PARSER_API_GET_AUDIO_SAMPLE_RATE        = 61,
    PARSER_API_GET_AUDIO_BITS_PER_SAMPLE    = 62,

    PARSER_API_GET_AUDIO_BLOCK_ALIGN        = 65,
    PARSER_API_GET_AUDIO_CHANNEL_MASK       = 66,
    PARSER_API_GET_AUDIO_BITS_PER_FRAME     = 67,


    /* text/subtitle properties */
    PARSER_API_GET_TEXT_TRACK_WIDTH = 80,
    PARSER_API_GET_TEXT_TRACK_HEIGHT= 81,

    /* sample reading, seek & trick mode */
    PARSER_API_GET_READ_MODE = 100,
    PARSER_API_SET_READ_MODE = 101,

    PARSER_API_ENABLE_TRACK = 105,

    PARSER_API_GET_NEXT_SAMPLE = 110,
    PARSER_API_GET_NEXT_SYNC_SAMPLE = 111,


    PARSER_API_GET_FILE_NEXT_SAMPLE = 115,
    PARSER_API_GET_FILE_NEXT_SYNC_SAMPLE = 116,


    PARSER_API_SEEK  = 120


};

/* prototype of entry point */
typedef int32 (*tFslParserQueryInterface)(uint32 id, void ** func);

/*
Every core parser shall implement this function and tell a specific API function pointer.
If the queried API is not implemented, the parser shall set funtion pointer to NULL and return PARSER_SUCCESS. */

EXTERN int32 FslParserQueryInterface(uint32 id, void ** func);


#endif /* _FSL_PARSER_COMMON_H */

