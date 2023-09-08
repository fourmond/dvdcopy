/**
    \file dvdreader.cc
    Implementation of the DVDReader class
    Copyright 2011 by Vincent Fourmond

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
#include "dvdreader.hh"
#include "dvdoutfile.hh"

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

bool DVDFileData::isBackup() const
{
  return domain == DVD_READ_INFO_BACKUP_FILE;
}

bool DVDFileData::isIFO() const
{
  return (domain == DVD_READ_INFO_BACKUP_FILE) ||
    (domain == DVD_READ_INFO_FILE);
}


std::string DVDFileData::fileName(int title, dvd_read_domain_t domain,
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


#define MAX_FILE_SIZE (512*1024)

std::string DVDFileData::fileName(bool stripInitialSlash, 
                                  int blocks) const
{
  std::string s(stripInitialSlash ? "VIDEO_TS/": "/VIDEO_TS/");
  int nb = number;
  if(domain == DVD_READ_TITLE_VOBS && blocks >= 0) {
    nb = 1 + blocks/MAX_FILE_SIZE; // Great !
  }
  return s + DVDFileData::fileName(title, domain, nb);
}


DVDFileData * DVDFileData::findBase(const std::vector<DVDFileData *> & files,
                                    const DVDFileData * file)
{
  if(file->number <= 1)
    return NULL;
  for(auto it = files.begin(); it != files.end(); ++it) {
    DVDFileData * fl = *it;
    if((fl->title == file->title) &&
       (fl->domain == file->domain) && (fl->number == 1))
      return fl;
  }
  return NULL;
}


//////////////////////////////////////////////////////////////////////

DVDReader::DVDReader() : reader(NULL), isDir(false)
{
}

DVDReader::DVDReader(const char * device)
{
  open(device);
}

dvd_reader_t * DVDReader::handle() const
{
  return reader;
}

void DVDReader::open(const char * device)
{
  if(reader)
    throw std::logic_error("Opening an already opened device");
  source = device;
  isDir = false;
  reader = DVDOpen(device);
  if(! reader) {
    std::string err("Error opening device ");
    err += device;
    throw std::runtime_error(err);
  }
  struct stat sb;
  if(! stat(device, &sb)) {
    if(S_ISDIR(sb.st_mode))
      isDir = true;
  }
}



DVDFileData * DVDReader::getFileInfo(int title, 
                                     dvd_read_domain_t domain, 
                                     int number)
{
  if(! reader)
    throw std::logic_error("Device not opened");
  DVDFileData * data = new DVDFileData(title, domain, number);
  if(isDir) {
    struct stat sb;
    std::string file = source + data->fileName();
    if(! stat(file.c_str(), &sb)) {
      data->fileID = sb.st_ino;
      data->size = sb.st_size;
      return data;
    }
  }
  else {
    uint32_t start, size;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", data->fileName().c_str());
    start = UDFFindFile(reader, buf, &size);
    if(start) {
      data->fileID = start;
      data->size = size;
      return data;
    }
  }
  delete data;
  return NULL;
}

std::vector<DVDFileData *> DVDReader::listFiles()
{
  std::vector<DVDFileData *> retval;
  int title = 0;
  while(1) {
    DVDFileData * dat;
    dat = getFileInfo(title, DVD_READ_INFO_FILE, 0);
    if(! dat)
      break;
    retval.push_back(dat);
    
    dat = getFileInfo(title, DVD_READ_MENU_VOBS, 0);
    if(dat) 
      retval.push_back(dat);

    dat = getFileInfo(title, DVD_READ_INFO_BACKUP_FILE, 0);
    if(dat) 
      retval.push_back(dat);

    if(title) {
      int nb = 1;
      while(1) {
        dat = getFileInfo(title, DVD_READ_TITLE_VOBS, nb);
        if( ! dat)
          break;
        retval.push_back(dat);
        nb++;
      }
    }
    title ++;
  }

  // Now browse through retval to find about duplicates
  std::map<unsigned long, DVDFileData *> dups;
  for(std::vector<DVDFileData *>::iterator i = retval.begin(); 
      i != retval.end(); i++) {
    DVDFileData * dat = *i;
    std::map<unsigned long, DVDFileData *>::iterator j = 
      dups.find(dat->fileID);
    if(j != dups.end())
      dat->dup = j->second;
    else 
      dups[dat->fileID] = dat;
  }

  return retval;
}



void DVDReader::displayFiles()
{
  std::vector<DVDFileData *> files = listFiles();
  for(std::vector<DVDFileData *>::iterator i = files.begin();
      i != files.end(); i++) {
    DVDFileData * f = *i;
    char buffer[30];
    snprintf(buffer, sizeof(buffer), "%12u", f->size);
    std::cout << f->fileName() << ": " << buffer 
              << "\t" << f->fileID << std::endl;
    if(f->dup)
      std::cout << "\t-> duplicate of " << f->dup->fileName()
                << std::endl;
  }
}

DVDReader::~DVDReader()
{
  DVDClose(reader);
}
