/* Copyright (C) 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/* Operations for the `flock' call.  */
#ifndef LOCK_SH
#  define LOCK_SH       1    /* Shared lock.  */
#endif

#ifndef LOCK_EX
#  define LOCK_EX       2    /* Exclusive lock.  */
#endif

#ifndef LOCK_UN
#  define LOCK_UN       8    /* Unlock.  */
#endif

/* Can be OR'd in to one of the above.  */
#ifndef LOCK_NB
#  define LOCK_NB       4    /* Don't block when locking.  */
#endif
