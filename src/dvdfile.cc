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
#include "dvdfile.hh"
#include "dvdreader.hh"

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
  virtual int readBlocks(int offset, int blocks, unsigned char * dest) {
    if(offset != blockOffset) {
      /// @todo error handling here.
      DVDFileSeek(file, offset * SECTOR_SIZE);
    }

    /// @todo By construction, this function doesn't work with
    /// sub-sector granularity.
    int nbread = DVDReadBytes(file, dest, blocks * SECTOR_SIZE);
    if(nbread < 0)
      return -1;
    nbread /= SECTOR_SIZE;
    blockOffset = offset + nbread;
    return nbread;              // Great !
  }

  DVDByteFile(dvd_file_t * f) : 
  DVDFile(f), blockOffset(0)
  {
    ;
  }

};

/// Files that work sectors by sectors
class DVDBlockFile : public DVDFile {
public:
  virtual int readBlocks(int offset, int blocks, unsigned char * dest) {
    return DVDReadBlocks(file, offset, blocks, dest);
  }

  DVDBlockFile(dvd_file_t * f) : 
  DVDFile(f)
  {
    ;
  }

};


//////////////////////////////////////////////////////////////////////

DVDFile::DVDFile(dvd_file_t * f) :
  file(f)
{
  // file shouldn't be 0 !
}

DVDFile::~DVDFile()
{
  DVDCloseFile(file);
}


int DVDFile::fileSize()
{
  return DVDFileSize(file);
}

DVDFile * DVDFile::openFile(dvd_reader_t * reader, const DVDFileData * dat)
{
  dvd_file_t * file = DVDOpenFile(reader, dat->title, dat->domain);
  if(! file)
    return NULL;

  switch(dat->domain) {
  case DVD_READ_INFO_FILE:
  case DVD_READ_INFO_BACKUP_FILE:
    return new DVDByteFile(file);
  case DVD_READ_MENU_VOBS:
  case DVD_READ_TITLE_VOBS:
    return new DVDBlockFile(file);
  default:
    ;
  }
  DVDCloseFile(file);           // Because it has been opened !
  return NULL;
}
