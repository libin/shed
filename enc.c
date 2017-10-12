/*
$Id: enc.c,v 1.1 2011/01/08 15:26:50 alexsisson Exp $

(C) Copyright 2010 Alex Sisson (alexsisson@gmail.com)

Functions for decoding multi-byte character encodings.

This file is part of shed.

shed is free software; you can redistribute it and/or modify
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

shed is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with shed; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <wchar.h>
#include <stdio.h>

wchar_t getmbchar_utf8(unsigned char c1, FILE *f, int *width) {
  int i,c2,c3,c4;
  //wchar_t r;
  (*width) = 0;

  if((c1 & 0x80)==0)
    return (wchar_t)c1;

  if((c1 & 0x40)==0)
    return 0; // shouldn't occur

  (*width)++;
  c2 = fgetc(f);
  if(c2<0)
    return 0; // file corrupted?

  if((c2 & 0x20)==0)
    return (((wchar_t)(c1&0x1F))<<0x06) | ((wchar_t)(c2&0x3F));

  (*width)++;
  c3 = fgetc(f);
  if(c3<0)
    return (((wchar_t)(c1&0x0F))<<0x0C) | (((wchar_t)(c2&0x3F))<<6) | ((wchar_t)(c3&0x3F));

  return 0;
}
