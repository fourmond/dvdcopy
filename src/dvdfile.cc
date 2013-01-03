/**
    \file dvdfile.cc
    Implementation of the DVDFile class and subclasses
    Copyright 2012 by Vincent Fourmond

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
#include "dvdoutfile.hh"

/* For stat(2), open(2) and comrades... */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <stdlib.h>
#include <stdio.h>

#define SECTOR_SIZE 2048

/// Files that work byte-by-byte
class DVDByteFile : public DVDFile {
  int blockOffset;
public:
  virtual int readBlocks(int blocks, int offset, unsigned char * dest) {
    
  }

  DVDByteFile(dvd_file_t * f) : 
  DVDFile(f), blockOffset(0)
  {
    ;
  }

};
