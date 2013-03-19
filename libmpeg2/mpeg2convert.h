/*
 * mpeg2convert.h
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef LIBMPEG2_MPEG2CONVERT_H
#define LIBMPEG2_MPEG2CONVERT_H

#ifdef LIBMPEG2_DLL
#define _EXPORT __declspec(dllexport)
#else
#ifndef LIBMPEG2_NO_IMPORT
#define _EXPORT __declspec(dllimport)
#else
#define _EXPORT
#endif
#endif

_EXPORT mpeg2_convert_t mpeg2convert_rgb32;
_EXPORT mpeg2_convert_t mpeg2convert_rgb24;
_EXPORT mpeg2_convert_t mpeg2convert_rgb16;
_EXPORT mpeg2_convert_t mpeg2convert_rgb15;
_EXPORT mpeg2_convert_t mpeg2convert_rgb8;
_EXPORT mpeg2_convert_t mpeg2convert_bgr32;
_EXPORT mpeg2_convert_t mpeg2convert_bgr24;
_EXPORT mpeg2_convert_t mpeg2convert_bgr16;
_EXPORT mpeg2_convert_t mpeg2convert_bgr15;
_EXPORT mpeg2_convert_t mpeg2convert_bgr8;

typedef enum {
    MPEG2CONVERT_RGB = 0,
    MPEG2CONVERT_BGR = 1
} mpeg2convert_rgb_order_t;

_EXPORT mpeg2_convert_t * mpeg2convert_rgb (mpeg2convert_rgb_order_t order,
				    unsigned int bpp);

_EXPORT mpeg2_convert_t mpeg2convert_uyvy;

#endif /* LIBMPEG2_MPEG2CONVERT_H */
