/*
$Id: enc.h,v 1.1 2011/01/08 15:26:50 alexsisson Exp $

(C) Copyright 2010 Alex Sisson (alexsisson@gmail.com)

Functions for handling various character encodings.

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

wchar_t getmbchar_utf8(unsigned char c1, FILE *f, int *width);

