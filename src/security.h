/*
 * security.h: supplies correct macros to security routines
 *  
 * Copyright (C), 1994, 1995, Graeme W. Wilford. (Wilf.)
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with this
 * distribution.
 *
 * Sat Nov 12 13:58:54 GMT 1994  Wilf. (G.Wilford@ee.surrey.ac.uk) 
 */

#ifndef SECURITY_H
#define SECURITY_H

#ifdef SECURE_MAN_UID

   /*
    * This is the name of the user that the preformatted man pages belong to.
    * If you are running man as a setuid program, you should make sure
    * that all of the preformatted man pages and the directories that
    * they live in are readable and writeable by this user.
    */

extern __inline__ void drop_effective_privs(void);
extern __inline__ void regain_effective_privs(void);

#  ifdef HAVE_UNISTD_H

#    include <unistd.h> 			/* for _POSIX_SAVED_IDS */

#    if defined(_POSIX_SAVED_IDS)
#      if defined(__linux__) || defined(__hpux)
         /* Linux pre 1.1.37 has broken saved uids */
#        define BROKEN_LINUX_SAVED_IDS
#      elif defined(__ultrix__)
         /* Ultrix pretends to have saved uids, but hasn't unless: */
#        if defined(POSIX) || defined(SYSTEM_FIVE)
#          define POSIX_SAVED_IDS
#        endif /* POSIX || SYSTEM_FIVE */
#      else /* !linux && !ultrix */
#        define POSIX_SAVED_IDS
#      endif /* linux || ultrix */
#    endif /* _POSIX_SAVED_IDS */
#  endif /* HAVE_UNISTD_H */

#  if !defined(BROKEN_LINUX_SAVED_IDS) && \
      !defined(POSIX_SAVED_IDS) && \
      !defined (HAVE_SETREUID)
#    error Cannot install man setuid as no SAVED IDS or setreuid()
#  endif

#endif /* SECURE_MAN_UID */
#endif /* SECURITY_H */
