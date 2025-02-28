/* $Id: spp_stream4.h,v 1.10 2003/04/16 14:50:15 chrisgreen Exp $ */

/*
** Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef __SPP_STREAM4_H__
#define __SPP_STREAM4_H__

#include "decode.h"

/* list of function prototypes for this preprocessor */
void SetupStream4();
int AlertFlushStream(Packet *p);


#endif  /* __SPP_STREAM4_H__ */
