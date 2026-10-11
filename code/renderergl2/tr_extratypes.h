/*
===========================================================================
Copyright (C) 2009-2011 Andrei Drexler, Richard Allen, James Canete

This file is part of Reaction source code.

Reaction source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Reaction source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Reaction source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#ifndef __TR_EXTRATYPES_H__
#define __TR_EXTRATYPES_H__

#define RDF_NOFOG 0x0008
#define RDF_EXTRA 0x0010
#define RDF_SUNLIGHT 0x0020

typedef struct {
    float blurFactor;
    float sunDir[3];
    float sunCol[3];
    float sunAmbCol[3];
} refdefex_t;

#endif
