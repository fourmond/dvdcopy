/**
    \file dvddrive.cc
    Implementation of the DVDFile class and subclasses
    Copyright 2013 by Vincent Fourmond

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "headers.hh"
#include "dvddrive.hh"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#ifdef HAVE_LINUX_CDROM_H
#include <linux/cdrom.h>
#endif

void DVDDrive::eject(const char * drive)
{
#ifndef HAVE_LINUX_CDROM_H
  fprintf(stderr, "Ejecting a CDROM is not supported on this platform\n");
#else
  int fd = open(drive, O_RDWR|O_NONBLOCK);
  if(fd < 0) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "Could not open drive %s", drive);
    perror(buffer);
    return;
  }
  if(ioctl(fd, CDROMEJECT) < 0) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "Could not eject drive %s", drive);
    perror(buffer);
    return;
  }
  // The bad luck is that apparently, at least on my machine, only the
  // SCSI approach ejects the drive, not the IDE one... But the SCSI
  // one is much more complex.
  close(fd);
#endif
}
