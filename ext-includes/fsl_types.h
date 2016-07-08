
/*
* Copyright (c) 2009-2010, 2013,2016 Freescale Semiconductor, Inc. 
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

/***********************************************************************
*
*  History :
*  Date             Author              Version    Description
*
*  Aug,2009         B06543              0.1        Initial Version
*
*/

#ifndef _FSL_MMLAYER_TYPES_H
#define _FSL_MMLAYER_TYPES_H

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

#ifndef bool
    #define bool int
#endif

#ifndef TRUE
    #define TRUE 1    
#endif

#ifndef FALSE
    #define FALSE 0    
#endif

#ifndef NULL
    #define NULL (void *)0
#endif
#endif /* _FSL_MMLAYER_TYPES_H */

