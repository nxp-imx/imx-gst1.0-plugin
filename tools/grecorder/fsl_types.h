/*
 * Copyright (c) 2014-2015, Freescale Semiconductor, Inc. All rights reserved.
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

typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef int int32;
typedef short int16;
typedef char int8;

#endif /* _FSL_TYPES_H */

