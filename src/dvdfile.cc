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

#include <sys/time.h>


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

  DVDByteFile(dvd_file_t * f, const DVDFileData * d) : 
    DVDFile(f,d), blockOffset(0)
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

  DVDBlockFile(dvd_file_t * f, const DVDFileData * d) : 
    DVDFile(f, d)
  {
    ;
  }

};


//////////////////////////////////////////////////////////////////////

DVDFile::DVDFile(dvd_file_t * f, const DVDFileData * d) :
  file(f), dat(d)
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
    return new DVDByteFile(file, dat);
  case DVD_READ_MENU_VOBS:
  case DVD_READ_TITLE_VOBS:
    return new DVDBlockFile(file, dat);
  default:
    ;
  }
  DVDCloseFile(file);           // Because it has been opened !
  return NULL;
}


void DVDFile::walkFile(int start, int blocks, int steps, 
                       const std::function<void (int offset, int nb, 
                                                 unsigned char * buffer,
                                                 const DVDFileData * dat)> & 
                         successfulRead,
                       const std::function<void (int offset, int nb, 
                                                 const DVDFileData * dat)> & 
                         failedRead)
{
  /* Data structures necessary for progress report */
  struct timeval init;
  struct timeval current;

  double elapsed_seconds;
  double estimated_seconds;
  double rate;
  const char * rate_suffix;

  if(steps < 0)
    steps = 128;                // Decent default ?
  std::unique_ptr<unsigned char[]> 
    readBuffer(new unsigned char[steps * SECTOR_SIZE]); 

  int overallSize = fileSize();
  int remaining = overallSize - start;
  int blk = start;
  int nb;
  int read;
  if(blocks < remaining)
    remaining = blocks;

  /// @todo Correct error handling
  gettimeofday(&init, NULL);
  printf("\nReading %d sectors at a time\n", steps); 
  while(remaining > 0) {
    /* First, we determine the number of blocks to be read */
    if(remaining > steps)
      nb = steps;
    else
      nb = remaining;
	  
    std::string fileName = dat->fileName(true, blk);
    printf("\rReading block %7d/%d (%s)", 
           blk, overallSize, fileName.c_str());
    read = readBlocks(blk, nb, (unsigned char*) readBuffer.get());

    if(read < 0) {
      /* There was an error reading the file. */
      printf("\nError while reading block %d of file %s, skipping\n",
             blk, fileName.c_str());
      failedRead(blk, nb, dat);
      read = nb;
    }
    else 
      successfulRead(blk, read, readBuffer.get(), dat);

    remaining -= read;
    blk += read;

    // Progress report
    gettimeofday(&current, NULL);

    elapsed_seconds = current.tv_sec - init.tv_sec + 
      1e-6 * (current.tv_usec - init.tv_usec);
    estimated_seconds = elapsed_seconds * (remaining + blk - start)/
      (blk - start);
    rate = ((blk - start) * 2048.)/(elapsed_seconds);
    if(rate >= 1e6) {
      rate_suffix = "MB/s";
      rate /= 1e6;
    }
    else if(rate >= 1e3) {
      rate_suffix = "kB/s";
      rate /= 1e3;
    }
    else 
      rate_suffix = "B/s";
    printf(" (%02d:%02d out of %02d:%02d, %5.1f%s)", 
           ((int) elapsed_seconds) / 60, ((int) elapsed_seconds) % 60, 
           ((int) estimated_seconds) / 60, ((int) estimated_seconds) % 60,
           rate, rate_suffix);
    fflush(stdout);
  }
}
