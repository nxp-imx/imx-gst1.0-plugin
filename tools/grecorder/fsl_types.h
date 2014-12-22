/*
***********************************************************************
* Copyright 2014 by Freescale Semiconductor, Inc.
* All modifications are confidential and proprietary information
* of Freescale Semiconductor, Inc. ALL RIGHTS RESERVED.
***********************************************************************
*/

#ifndef _FSL_TYPES_H
#define _FSL_TYPES_H

#include <stdio.h>

#ifndef uint64
#ifdef WIN32
	typedef  unsigned __int64 uint64;	
#else
	typedef  unsigned long long uint64;	
#endif
#endif /*uint64*/

#ifndef int64
#ifdef WIN32	
	typedef  __int64 int64;
#else	
	typedef  long long int64;
#endif
#endif /*int64*/

typedef unsigned long uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef long int32;
typedef short int16;
typedef char int8;

#endif /* _FSL_TYPES_H */

