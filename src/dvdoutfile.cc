/**
    \file dvdoutfile.cc
    Implementation of the DVDOutFile class
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
#include "dvdoutfile.hh"

/* For stat(2), open(2) and comrades... */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <stdlib.h>
#include <stdio.h>

/** The maximum size of a file, in sectors */
#define MAX_FILE_SIZE (512*1024)
#define SECTOR_SIZE 2048


std::string DVDOutFile::fileName(int title, dvd_read_domain_t domain,
                                 int number = -1)
{
  char buffer[100];             // Large enough
  const char * format;
  const char * ext;
  if(title)
    format = "VTS_%2$02d_%3$1d.%1$s";
  else
    format = "VIDEO_TS.%1$s";
  switch(domain) {
  case DVD_READ_INFO_FILE:
    ext = "IFO"; 
    number = 0;
    break;
  case DVD_READ_INFO_BACKUP_FILE:
    ext = "BUP"; 
    number = 0;
    break;
  case DVD_READ_MENU_VOBS:
    ext = "VOB"; 
    number = 0;
    break;
  case DVD_READ_TITLE_VOBS:
    ext = "VOB"; 
    if(! number) 		/* Fixing number to at least one */
      number = 1;
  }
  snprintf(buffer, sizeof(buffer), format, ext, title, number);
  return std::string(buffer);
}

std::string DVDOutFile::makeFileName(int number) const
{
  if(number < 0) 
    number = sector / MAX_FILE_SIZE + 1;
  return fileName(title, domain, number);
}


std::string DVDOutFile::outputFileName(int number) const
{
  return outputDirectory + 
    "/VIDEO_TS/" + makeFileName(number);
}


DVDOutFile::DVDOutFile(const char * output_dir, int t, 
                       dvd_read_domain_t d) :
  outputDirectory(output_dir), title(t), domain(d), sector(0), fd(-1)
{
  
}

void DVDOutFile::openFile()
{
  off_t pos;
  std::string name = outputFileName();

  /* Closing to avoid unclosed files */
  if(fd >= 0)
    close(fd);
  fd = open(name.c_str(), O_CREAT|O_WRONLY, 0666);
  if(fd <= 0) {
    std::string err("Failed to open output file '");
    err += name + "': " + strerror(errno);
    throw std::runtime_error(err);
  }
  /* Now, we seek to the position specified by sector */
  pos = SECTOR_SIZE * (sector % MAX_FILE_SIZE);
  lseek(fd, pos, SEEK_SET);
}


void DVDOutFile::writeSectors(const char * data, size_t number)
{
  if(fd < 0)
    openFile();
  int cur_sect_pos = sector % MAX_FILE_SIZE;
  if(cur_sect_pos + number <= MAX_FILE_SIZE) {
    /* Simple case */
    write(fd, data, number * SECTOR_SIZE);
    sector += number;

    /* If we reached the end of file, we switch to the next one. */
    if(sector % MAX_FILE_SIZE == 0)
      openFile();
  }
  else {
    int left = MAX_FILE_SIZE - cur_sect_pos;
    writeSectors(data, left);
    writeSectors(data + SECTOR_SIZE * left, number - left);
  }
}

void DVDOutFile::closeFile()
{
  if(fd >= 0)
    close(fd);
  fd = -1;
}


DVDOutFile::~DVDOutFile()
{
  closeFile();
}

std::string DVDOutFile::currentOutputName() const
{
  std::string name("VIDEO_TS/");
  name += makeFileName();
  return name;
}



static char empty_sector[2048] = {0, 0, 0, 0};

void DVDOutFile::skipSectors(size_t number)
{
  /// @todo Possibly we should fill this with relevant information ?
  while(number--)
    writeSectors(empty_sector, 1);
}

size_t DVDOutFile::fileSize() const
{
  int cur;
  struct stat fs;
  std::string name;
  switch(domain) {
  case DVD_READ_INFO_FILE:
  case DVD_READ_INFO_BACKUP_FILE:
  case DVD_READ_MENU_VOBS:
    /* The simple case:  */
    name = outputFileName(0);
    if(stat(name.c_str(), &fs) == -1)
      return 0;
    return fs.st_size/2048;
    break;
  case DVD_READ_TITLE_VOBS:
    /* The delicate one. */
    cur = 1;
    while(1) {
      name = outputFileName(cur);
      if(stat(name.c_str(), &fs) == -1)
	return (cur - 1)*512*1024;
      if(fs.st_size < (1024 * 1024 * 1024))
	return (cur - 1)*512*1024 + fs.st_size/2048;
      cur += 1;
    }
  }
}

void DVDOutFile::seek(int s)
{
  sector = s;
  openFile();
}
