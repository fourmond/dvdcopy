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



DVDReader::DVDReader(const char * device) : source(device), isDir(false)
{
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

std::string DVDReader::FileData::fileName() const
{
  std::string s("/VIDEO_TS/");
  return s + DVDOutFile::fileName(title, domain, number);
}

DVDReader::FileData * DVDReader::getFileInfo(int title, 
                                             dvd_read_domain_t domain, 
                                             int number)
{
  FileData * data = new FileData(title, domain, number);
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

std::vector<DVDReader::FileData *> DVDReader::listFiles()
{
  std::vector<FileData *> retval;
  int title = 0;
  while(1) {
    FileData * dat;
    dat = getFileInfo(title, DVD_READ_INFO_FILE, 0);
    if(! dat)
      break;
    retval.push_back(dat);
    
    dat = getFileInfo(title, DVD_READ_MENU_VOBS, 0);
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
  std::map<unsigned long, FileData *> dups;
  for(std::vector<FileData *>::iterator i = retval.begin(); 
      i != retval.end(); i++) {
    FileData * dat = *i;
    std::map<unsigned long, FileData *>::iterator j = 
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
  std::vector<FileData *> files = listFiles();
  for(std::vector<FileData *>::iterator i = files.begin();
      i != files.end(); i++) {
    FileData * f = *i;
    std::cout << f->fileName() << ": " << f->size 
              << "\t" << f->fileID << std::endl;
    if(f->dup)
      std::cout << "\t-> duplicate of " << f->dup->fileName()
                << std::endl;
  }
}
