/*
 * tempnam.c: generate file name for temporary files
 *  
 * Copyright (C), 1994, 1995, Carl Edman
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with this
 * distribution.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#if defined(STDC_HEADERS)
#  include <string.h>
#  include <stdlib.h>
#elif defined(HAVE_STRING_H)
#  include <string.h>
#elif defined(HAVE_STRINGS_H)
#  include <strings.h>
#else /* no string(s) header */
extern char *strchr(), *strcat(), *getenv();
#endif /* STDC_HEADERS */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#ifndef NULL
#define NULL 0
#endif

#define TEMPNAM_PFX	""

static int isokdir(char *name)
   {
   struct stat buf;

   if (name==NULL) return 0;
   if (access(name, R_OK|W_OK|X_OK)==-1) return 0;
   if (stat(name,&buf)==-1) return 0;
   if (buf.st_mode&S_IFDIR==0) return 0;

   return 1;
   }

/* return filename for temporary file */
char *tempnam (char *dir, char *pfx)
   {
   int n;
   struct timeval tp;
   struct timezone tzp;
   char *ret;
   
   if (!isokdir(dir)) dir=NULL;
   
   if (dir==NULL)
      {
      dir=getenv("TMPDIR");
      if (!isokdir(dir)) dir=NULL;
      }
   
#ifdef P_tmpdir
   if (dir==NULL)
      {
      dir=P_tmpdir;
      if (!isokdir(dir)) dir=NULL;
      }
#endif
   
   if (dir==NULL)
      {
      dir="/tmp";
      if (!isokdir(dir)) dir=NULL;
      }
   
   if (dir==NULL) return NULL;

   if (pfx==NULL) pfx=TEMPNAM_PFX;

   ret=malloc(strlen(dir)+1+strlen(pfx)+8+1);
   if (ret==NULL) return NULL;

   n=getpid();
   sprintf(ret,"%s/%s%08d",dir,pfx,n);

   while (access(ret,F_OK)!=-1)
      {
      char number[11];
      gettimeofday(&tp,&tzp);
      sprintf(number,"%d",n^tp.tv_usec^tp.tv_sec);
      sprintf(ret,"%s/%s%08.8s",dir,pfx,number);
      }

   return ret;
   }
