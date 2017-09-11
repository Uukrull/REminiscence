/* REminiscence - Flashback interpreter
 * Copyright (C) 2005 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __SYS_H__
#define __SYS_H__

typedef unsigned char uint8;
typedef signed char int8;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned long uint32;
typedef signed long int32;

#if defined SYS_LITTLE_ENDIAN

inline uint16 READ_LE_UINT16(const void *ptr) {
#if defined SYS_NEED_ALIGNMENT
	uint16 r;
	memcpy(&r, ptr, 2);
	return r;
#else
	return *(const uint16 *)ptr;
#endif
}

inline uint32 READ_LE_UINT32(const void *ptr) {
#if defined SYS_NEED_ALIGNMENT
	uint32 r;
	memcpy(&r, ptr, 4);
	return r;
#else
	return *(const uint32 *)ptr;
#endif
}

inline uint16 READ_BE_UINT16(const void *ptr) {
	const uint8 *b = (const uint8 *)ptr;
	return (b[0] << 8) | b[1];
}

inline uint32 READ_BE_UINT32(const void *ptr) {
	const uint8 *b = (const uint8 *)ptr;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

#elif defined SYS_BIG_ENDIAN

inline uint16 READ_LE_UINT16(const void *ptr) {
	const uint8 *b = (const uint8 *)ptr;
	return (b[1] << 8) | b[0];
}

inline uint32 READ_LE_UINT32(const void *ptr) {
	const uint8 *b = (const uint8 *)ptr;
	return (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
}

inline uint16 READ_BE_UINT16(const void *ptr) {
#if defined SYS_NEED_ALIGNMENT
	uint16 r;
	memcpy(&r, ptr, 2);
	return r;
#else
	return *(const uint16 *)ptr;
#endif
}

inline uint32 READ_BE_UINT32(const void *ptr) {
#if defined SYS_NEED_ALIGNMENT
	uint32 r;
	memcpy(&r, ptr, 4);
	return r;
#else
	return *(const uint32 *)ptr;
#endif
}

#else

#error No endianness defined

#endif

#endif // __SYS_H__
