/**
    \file dvdcopy.cc
    Implementation of the DVDCopy class
    Copyright 2006, 2008, 2011 by Vincent Fourmond

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
#include "dvdcopy.hh"

#include "dvdoutfile.hh"

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/time.h>



#define BUF_BLOCK_SIZE 128
#define BUF_SIZE (BUF_BLOCK_SIZE * 2048)

int debug = 1;


DVDCopy::DVDCopy()
{
  readBuffer = new char[BUF_SIZE];
  reader = NULL;
}


void DVDCopy::copyFile(int title, dvd_read_domain_t d)
{
  dvd_file_t * file = DVDOpenFile(reader, title, d);
  DVDOutFile outfile(targetDirectory.c_str(), title, d);

  /* Data structures necessary for progress report */
  struct timeval init;
  struct timeval current;

  double elapsed_seconds;
  double estimated_seconds;
  double rate;
  const char * rate_suffix;
  if(file) {
    int size = DVDFileSize(file);
    int current_size = outfile.fileSize();
    int read;
    int blk = 0;
    unsigned nb;		/* The number of blocks we're about to read */
    int skipped = 0;
    if(current_size == size) {
      printf("File already fully read: not reading again\n");
      return;
    }
    /* Now, if current_size > 0:
       - seek the input file, if necessary
       - seek the output file...
    */
    if(current_size > 0) {
      if(d == DVD_READ_INFO_FILE || d == DVD_READ_INFO_BACKUP_FILE)
	DVDFileSeek(file, current_size * 2048);
      else
	blk = current_size;

      outfile.seek(current_size);
      size -= current_size;
      printf("File already partially read: using %d sectors\n",
	     current_size);
    }
    switch(d) {
    case DVD_READ_INFO_FILE:
    case DVD_READ_INFO_BACKUP_FILE:
      size *= 2048;		/* The number of bytes to read ! */
      while(size > 0) {
	read = DVDReadBytes(file, readBuffer, 
                            (size > BUF_SIZE) ? BUF_SIZE : size);
	if(read < 0) {
	  fprintf(stderr, "Error reading file\n");
	  size = -1;
	  break;
	}
	outfile.writeSectors(readBuffer, read/2048);
	size -= read;
      }
      break;
    case DVD_READ_MENU_VOBS:
    case DVD_READ_TITLE_VOBS:
      /* TODO: error handling */
      gettimeofday(&init, NULL);
      while(size > 0) {
	/* First, we determine the number of blocks to be read */
	if(size > BUF_BLOCK_SIZE)
	  nb = BUF_BLOCK_SIZE;
	else
	  nb = size;
	  
        std::string fileName = outfile.currentOutputName();
	printf("\rReading block %7d/%d (%s)", 
	       blk, blk + size, fileName.c_str());
	read = DVDReadBlocks(file, blk, nb, (unsigned char*) readBuffer);

	if(read < 0) {
	  /* There was an error reading the file. */
	  printf("\rError while reading block %d of file %s, skipping\n",
		 blk, fileName.c_str());
	  outfile.skipSectors(nb);
	  read = nb;
	  skipped += nb;
	}
	else {
	  outfile.writeSectors(readBuffer, read);
	}
	size -= read;
	blk += read;
	gettimeofday(&current, NULL);
	elapsed_seconds = current.tv_sec - init.tv_sec + 
	  1e-6 * (current.tv_usec - init.tv_usec);
	estimated_seconds = elapsed_seconds * (blk + size)/(blk);
	rate = (blk * 2048.)/(elapsed_seconds);
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
      outfile.closeFile(); 
      DVDCloseFile(file);
      if(skipped) {
	printf("\nThere were %d sectors skipped in this title set\n",
	       skipped);
      }
    }
  }
  else {
    std::string fileName = outfile.currentOutputName();
    printf("\nSkipping file %s (not found)\n", fileName.c_str());
  }
}

void DVDCopy::copy(const char *device, const char * target)
{
  dvd_file_t * file;
  int title = 0;
  char buf[1024];
  targetDirectory = target;

  reader = DVDOpen(device);
  if(! reader) {
    std::string err("Error opening device ");
    err += device;
    throw std::runtime_error(err);
  }

  struct stat dummy;
  if(stat(target,&dummy)) {
    fprintf(stderr,"Creating directory %s\n", target);
    mkdir(target, 0755);
  }

  /* Then, create the VIDEO_TS subdir if necessary */
  snprintf(buf, sizeof(buf), "%s/VIDEO_TS", target);
  if(stat(buf, &dummy))  {
    fprintf(stderr,"Creating directory %s\n", buf);
    mkdir(buf, 0755);
  }

  while(file = DVDOpenFile(reader, title, DVD_READ_INFO_FILE)) {
    DVDCloseFile(file);
    
    printf("\nReading title set %02d\n", title);
    printf("Reading info file\n", title);
    copyFile(title, DVD_READ_INFO_FILE);
    if(! title) {
      printf("Reading backup info file\n", title);
      copyFile(title, DVD_READ_INFO_BACKUP_FILE);
    }
    printf("Reading menu file\n", title);
    copyFile(title, DVD_READ_MENU_VOBS);
    if(title) {
      /* It does not make sense for the main title */
      printf("\nReading title set files\n", title);
      copyFile(title, DVD_READ_TITLE_VOBS);
    }
    title ++;
  }
}

DVDCopy::~DVDCopy()
{
  delete readBuffer;
  if(reader)
    DVDClose(reader);
        
}
