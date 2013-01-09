/**
    \file dvdcopy.cc
    Implementation of the DVDCopy class
    Copyright 2006, 2008, 2011, 2012, 2013 by Vincent Fourmond

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

#include "dvdfile.hh"
#include "dvdoutfile.hh"

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/time.h>

// use of regular expressions !
#include <regex.h>



#define BUF_BLOCK_SIZE 128
#define BUF_SIZE (BUF_BLOCK_SIZE * 2048)

int debug = 1;


DVDCopy::DVDCopy() : badSectors(NULL)
{
  reader = NULL;
}


int DVDCopy::copyFile(const DVDFileData * dat, int firstBlock, 
                      int blockNumber, int readNumber)
{
  /// @todo This function shouldn't mix calls to printf and std::cout
  /// ? (hmmm, if all calls finish by std::endl, flushes should be
  /// fine)

  if(readNumber < 0)
    readNumber = BUF_BLOCK_SIZE;

  // First, looking for duplicates:
  if(dat->dup) {
    // We do hard links
    struct stat st;
    std::string source = targetDirectory + dat->dup->fileName();
    std::string target = targetDirectory + dat->fileName();
    if(stat(target.c_str(), &st)) {
      std::cout << "Hardlinking " 
                << target << " to " << source << std::endl;
      link(source.c_str(), target.c_str());
    }
    else {
      struct stat stold;
      if(stat(source.c_str(), &stold)) {
        std::string error = "Must link ";
        error += target + " to " + source + ", but the latter doesnt exist !";
        throw std::runtime_error(error);
      }
      // Both target file and source file exists, we check the inode
      // numbers are the same.
      if(stold.st_ino != st.st_ino) {
        std::string error = "Must link ";
        error += target + " to " + source + ", but " + target +
          " exists and isn't a hard link to " + source + "\n" +
          "You must remove it to proceed";
        throw std::runtime_error(error);
      }
      std::cout << "Not hardlinking " 
                << target << " to " << source 
                << ", already done" << std::endl;
      return 0;
    }
    return 0;
  }

  // Files where the number is greater than 1 (ie part of a track VOB)
  // have already been copied along with the number 1, no need to do
  // anything.
  if(dat->number > 1)
    return 0;


  std::unique_ptr<DVDFile> file(DVDFile::openFile(reader, dat));
  if(! file) {
    std::string fileName = dat->fileName(true);
    printf("\nSkipping file %s (not found)\n", fileName.c_str());
    return 0;
  }
  DVDOutFile outfile(targetDirectory.c_str(), dat->title, dat->domain);


  int skipped = 0;
  auto success = [&outfile](int offset, int nb, 
                            unsigned char * buffer,
                            const DVDFileData * dat) {
    outfile.writeSectors(reinterpret_cast<char*>(buffer), nb);
  };

  auto failure = [&outfile, &skipped, this](int blk, int nb, 
                                            const DVDFileData * dat) {
    outfile.skipSectors(nb);
    registerBadSectors(dat, blk, nb);
    skipped += nb;
  };

  int size = file->fileSize();
  int current_size = outfile.fileSize();
  if(firstBlock > 0)
    current_size = firstBlock; 

  if(current_size == size) {
    printf("File already fully read: not reading again\n");
    return 0;
  }
  if(blockNumber < 0)
    blockNumber = size - current_size;

  file->walkFile(current_size, blockNumber, readNumber, 
                 success, failure);

  outfile.closeFile(); 
  if(skipped) {
    printf("\nThere were %d sectors skipped in this title set\n",
           skipped);
  }
  return skipped;
}

void DVDCopy::setup(const char *device, const char * target)
{
  DVDReader r(device);
  files = r.listFiles();

  reader = DVDOpen(device);
  if(! reader) {
    std::string err("Error opening device ");
    err += device;
    throw std::runtime_error(err);
  }

  if(target) {
    char buf[1024];
    targetDirectory = target;
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
  }

}

void DVDCopy::copy(const char *device, const char * target)
{
  setup(device, target);

  /// Methodically copies all listed files
  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++)
    copyFile(*i);
}

void DVDCopy::secondPass(const char *device, const char * target)
{
  setup(device, target);
  readBadSectors();
  int totalMissing = 0;

  for(int i = 0; i < badSectorsList.size(); i++) {
    BadSectors & bs = badSectorsList[i];
    printf("Trying to read %d bad sectors from file %s at %d:\n",
           bs.number,
           bs.file->fileName().c_str(),
           bs.start);
    int nb = copyFile(bs.file, bs.start, bs.number, 1);
    if(nb > 0)
      printf("\n -> still got %d bad sectors (out of %d)\n",
             nb, bs.number);
    else
      printf("\n -> apparently successfully read missing sectors\n");
    totalMissing += nb;
  }
  printf("\nAltogether, there are still %d missing sectors\n", 
         totalMissing);
  
}

void DVDCopy::scanForBadSectors(const char *device, 
                                const char * badSectorsFile)
{
  setup(device, NULL);

  badSectors = fopen(badSectorsFile, "w");

  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++) {
    DVDFileData * dat = *i;
    if(dat->dup || dat->number > 1)
      continue;
    if(dat->domain == DVD_READ_INFO_FILE ||
       dat->domain == DVD_READ_INFO_BACKUP_FILE)
      continue;
    std::unique_ptr<DVDFile> file(DVDFile::openFile(reader, dat));
    int sz = file->fileSize();

    auto success = [this](int blk, int nb, 
                      unsigned char * buf,
                      const DVDFileData * dat) {
      for(int i = 0; i < nb; i++) {
        unsigned char * buffer = buf + i * 2048;
        int first_pes_offset = 13 + (buffer[13] & 0x7);
        if(buffer[2] == 1 && buffer[first_pes_offset + 3] == 1)
          continue;
        else
          registerBadSectors(dat, blk + i, 1);
      }
    };

    auto failure = [this](int blk, int nb, 
                      const DVDFileData * dat) {
      registerBadSectors(dat, blk, nb);
    };

    file->walkFile(0, sz, 128, 
                   success, failure);
    /// @todo Compact the bad sectors file (this should be done at the
    /// very end, rewriting the whole file)
  }
}

DVDCopy::~DVDCopy()
{
  if(reader)
    DVDClose(reader);
  if(badSectors)
    fclose(badSectors);
  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++)
    delete *i;                  // Keep it clean;
}

void DVDCopy::openBadSectorsFile(const char * mode)
{
  if(! badSectors) {
    std::string bsf = targetDirectory + ".bad";
    badSectors = fopen(bsf.c_str(), mode);
  }
}

void DVDCopy::registerBadSectors(const DVDFileData * dat, 
                                 int beg, int size)
{
  openBadSectorsFile("a");
  fprintf(badSectors, "%s: %d,%d,%d  %d (%d)\n",
          dat->fileName().c_str(),
          dat->title,
          dat->domain,
          dat->number,
          beg, size);
  fflush(badSectors);

  badSectorsList.push_back(BadSectors(dat, beg, size));
}

int DVDCopy::findFile(int title, dvd_read_domain_t domain, int number)
{
  for(int i = 0; i < files.size(); i++) {
    const DVDFileData * dat = files[i];
    if( 
       (dat->title == title) && 
       (dat->domain == domain) && 
       (dat->number == number)
        )
      return i;
  }
  return -1;
}

void DVDCopy::readBadSectors()
{
  openBadSectorsFile("r");
  if(! badSectors) {
    fprintf(stderr, "%s", "No bad sectors file found, "
            "which is probably good news !\n");
    return;
  }

  char buffer[1024];
  regex_t re;
  regmatch_t matches[6];
  { 
    int er = regcomp(&re, "[^:]+: *([0-9]+),([0-9]+),([0-9]+) *"
                     "([0-9]+) *\\(([0-9]+)\\)",
                     REG_EXTENDED);
    if(er) {
      regerror(er, &re, buffer, sizeof(buffer));
      fprintf(stderr, "Error building the line regexp: %s", buffer);
      return;
    }
  }
    
  while(! feof(badSectors)) {
    fgets(buffer, sizeof(buffer), badSectors);
    int status = regexec(&re, buffer, sizeof(matches)/sizeof(regmatch_t),
                         matches, 0);
    if(status) {
      fprintf(stderr, "error parsing line: %s", buffer);
    }
    else {
      // Make all substrings NULL-terminated:
      for(int i = 1; i < sizeof(matches)/sizeof(regmatch_t); i++) {
        if(matches[i].rm_so >= 0)
          buffer[matches[i].rm_eo] = 0;
      }
      
      // No validation whatsoever, but all groups should be here anyway !
      int title = atoi(buffer + matches[1].rm_so);
      dvd_read_domain_t domain = 
        (dvd_read_domain_t) atoi(buffer + matches[2].rm_so);
      int number = atoi(buffer + matches[3].rm_so);

      int beg = atoi(buffer + matches[4].rm_so);
      int size = atoi(buffer + matches[5].rm_so);

      int idx = findFile(title, domain, number);
      if(idx < 0) {
        fprintf(stderr, "Found no match for file %d,%d,%d\n",
                title, domain, number);
      }
      else
        badSectorsList.push_back(BadSectors(files[idx], beg, size));
    }
  }
  
}
