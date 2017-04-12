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

#include "dvddrive.hh"

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/time.h>

// use of regular expressions !
#include <regex.h>


std::string BadSectors::toString() const
{
  char buffer[1024];
  snprintf(buffer, sizeof(buffer),
           "%s: %d,%d,%d  %d (%d)",
           file->fileName().c_str(),
           file->title,
           file->domain,
           file->number,
           start, number);
  return std::string(buffer);
}

bool BadSectors::tryMerge(const BadSectors & follower)
{
  if(follower.file != file)
    return false;
  if(start + number != follower.start) 
    return false;
  number += follower.number;
  return true;
}


//////////////////////////////////////////////////////////////////////

void Progress::setupForCopying(const std::vector<DVDFileData * > & files)
{
  totalSectors = 0;
  totalSkipped = 0;
  sectorsDone = 0;
  for(auto it = files.begin(); it != files.end(); it++) {
    const DVDFileData * file = *it;
    FileProgress pg;
    pg.totalSectors = (file->dup ? 0 : file->size/2048);
    totalSectors += pg.totalSectors;
    pg.sectorsDone = 0;
    pg.skippedSectors = 0;
    progresses.insert(std::pair<const DVDFileData *, FileProgress>(file, pg));
  }

  /// @todo Use std::chrono
  gettimeofday(&startTime, NULL);
}

void Progress::successfulRead(const DVDFileData * file, int nb)
{
  auto it = progresses.find(file);
  if(it == progresses.end())
    throw std::runtime_error("Could not find the file for the progress");
  FileProgress & progress = it->second;
  progress.sectorsDone += nb;
  sectorsDone += nb;
}

void Progress::finishedFile(const DVDFileData * file)
{
  auto it = progresses.find(file);
  if(it == progresses.end())
    throw std::runtime_error("Could not find the file for the progress");
  FileProgress & progress = it->second;
  sectorsDone += progress.totalSectors - progress.sectorsDone;
  progress.sectorsDone = progress.totalSectors;
}

void Progress::failedRead(const DVDFileData * file, int nb)
{
  auto it = progresses.find(file);
  if(it == progresses.end())
    throw std::runtime_error("Could not find the file for the progress");
  FileProgress & progress = it->second;
  progress.sectorsDone += nb;
  progress.skippedSectors += nb;
  sectorsDone += nb;
  totalSkipped += nb;
}


void Progress::writeCurrentProgress(const DVDFileData * file) const
{
  struct timeval current;

  double elapsed_seconds;
  double estimated_seconds;
  double rate;
  const char * rate_suffix;

  auto it = progresses.find(file);
  if(it == progresses.end())
    throw std::runtime_error("Could not find the file for the progress");
  const FileProgress & progress = it->second;

  // Progress report
  gettimeofday(&current, NULL);

  int remaining = totalSectors - sectorsDone;

  elapsed_seconds = (current.tv_sec - startTime.tv_sec)*1.0 + 
    1e-6 * (current.tv_usec - startTime.tv_usec);
  estimated_seconds = (elapsed_seconds * (totalSectors))/
    sectorsDone;
  rate = ((sectorsDone) * 2048.)/(elapsed_seconds);
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
  printf(" %d skipped, total: %7d/%d blocks, %d skipped "
         "(%02d:%02d out of %02d:%02d, %5.1f%s)",
         progress.skippedSectors,
         sectorsDone, totalSectors, totalSkipped,
         ((int) elapsed_seconds) / 60, ((int) elapsed_seconds) % 60, 
         ((int) estimated_seconds) / 60, ((int) estimated_seconds) % 60,
         rate, rate_suffix);
  fflush(stdout);
}



//////////////////////////////////////////////////////////////////////


DVDCopy::DVDCopy() : badSectors(NULL), sectorsRead(-1), skipBUP(false)
{
  reader = NULL;
}

#define STANDARD_READ 128


int DVDCopy::copyFile(const DVDFileData * dat, int firstBlock, 
                      int blockNumber, int readNumber)
{
  /// @todo This function shouldn't mix calls to printf and std::cout
  /// ? (hmmm, if all calls finish by std::endl, flushes should be
  /// fine)

  if(readNumber < 0)
    readNumber = STANDARD_READ;

  if(skipBUP && dat->isBackup()) {
    // But, that may be a bad idea ?
    std::cout << std::flush << "\nSkipping backup file: " 
              << dat->fileName() << std::endl;
    return 0;
  }
    

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

  int ifoSectors = -1;
  if(dat->isIFO())
    extractIFOSizes(dat, &ifoSectors);
  

  std::unique_ptr<DVDFile> file(DVDFile::openFile(reader, dat));
  if(! file) {
    std::string fileName = dat->fileName(true);
    printf("\nSkipping file %s (not found)\n", fileName.c_str());
    return 0;
  }
  DVDOutFile outfile(targetDirectory.c_str(), dat->title, dat->domain);

  int skipped = 0;
  auto success = [&outfile, this](int offset, int nb, 
                            unsigned char * buffer,
                            const DVDFileData * dat) {
    outfile.writeSectors(reinterpret_cast<char*>(buffer), nb);
    overallProgress.successfulRead(dat, nb);
    overallProgress.writeCurrentProgress(dat);
  };

  auto failure = [&outfile, &skipped, this](int blk, int nb, 
                                            const DVDFileData * dat) {
    outfile.skipSectors(nb);
    registerBadSectors(dat, blk, nb);
    overallProgress.failedRead(dat, nb);
    overallProgress.writeCurrentProgress(dat);
  };

  int size = file->fileSize();
  /// @todo make that configurable
  if(ifoSectors > 0 && size > ifoSectors) {
    std::string fileName = dat->fileName(true);
    printf("\nIFO headers say read only %d sectors instead of %d for file %s\n",
           ifoSectors, size, fileName.c_str());
    size = ifoSectors;
  }
  int current_size = outfile.fileSize();
  if(firstBlock > 0)
    current_size = firstBlock; 

  if(current_size == size) {
    printf("File already fully read: not reading again\n");
    overallProgress.finishedFile(dat);
    return 0;
  }
  if(blockNumber < 0)
    blockNumber = size - current_size;

  outfile.seek(current_size);

  file->walkFile(current_size, blockNumber, readNumber, 
                 success, failure);

  outfile.closeFile(); 
  if(skipped) {
    printf("\nThere were %d sectors skipped in this title set\n",
           skipped);
  }
  
  overallProgress.finishedFile(dat);
  return skipped;
}

void DVDCopy::setup(const char *device, const char * target)
{
  DVDReader r(device);
  sourceDevice = device;
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
  overallProgress.setupForCopying(files);

  /// Methodically copies all listed files
  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++)
    copyFile(*i);
}

void DVDCopy::secondPass(const char *device, const char * target)
{
  setup(device, target);
  readBadSectors();
  closeBadSectorsFile();
  badSectors = NULL;
  int totalMissing = 0;

  std::vector<BadSectors> oldBadSectors;
  std::swap(oldBadSectors, badSectorsList);

  for(int i = 0; i < oldBadSectors.size(); i++) {
    BadSectors & bs = oldBadSectors[i];
    printf("Trying to read %d bad sectors from file %s at %d:\n",
           bs.number,
           bs.file->fileName().c_str(),
           bs.start);
    int nb = copyFile(bs.file, bs.start, bs.number, 
                      (sectorsRead > 0 ? sectorsRead : 1));
    if(nb > 0)
      printf("\n -> still got %d bad sectors (out of %d)\n",
             nb, bs.number);
    else
      printf("\n -> apparently successfully read missing sectors\n");
    totalMissing += nb;

    // Now, we update the bad sectors list file
    printf("Updating the bad sectors file '%s'\n",
           badSectorsFileName.c_str());
    openBadSectorsFile("w");
    for(int j = 0; j < badSectorsList.size(); j++)
      fprintf(badSectors, "%s\n", badSectorsList[j].toString().c_str());
    for(int j = i+1; j < oldBadSectors.size(); j++)
      fprintf(badSectors, "%s\n", oldBadSectors[j].toString().c_str());
    closeBadSectorsFile();
  }
  printf("\nAltogether, there are still %d missing sectors\n", 
         totalMissing);
  
}

void DVDCopy::scanForBadSectors(const char *device, 
                                const char * badSectorsFile)
{
  setup(device, NULL);

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
          registerBadSectors(dat, blk + i, 1, true);
      }
    };

    auto failure = [this](int blk, int nb, 
                      const DVDFileData * dat) {
      registerBadSectors(dat, blk, nb, true);
    };

    file->walkFile(0, sz, (sectorsRead > 0 ? sectorsRead : -1), 
                   success, failure);
  }

  // OK, now, we have a list of bad sectors. We simplify it
  std::vector<BadSectors> simplifiedBad;
  for(int i = 0; i < badSectorsList.size(); i++) {
    const BadSectors & bs = badSectorsList[i];
    if(!i)
      simplifiedBad.push_back(bs);
    else {
      if(! simplifiedBad.back().tryMerge(bs))
        simplifiedBad.push_back(bs);
    }
  }

  FILE * bad = fopen(badSectorsFile, "w");
  for(int i = 0; i < simplifiedBad.size(); i++)
    fprintf(bad, "%s\n",
            simplifiedBad[i].toString().c_str());
}

void DVDCopy::spliceIFO(const char * device, const char * target, int nb)
{
  setup(device, target);


  DVDFileData * ifoFile = NULL;

  typedef std::vector< std::pair<DVDFileData *, DVDFileData *> > file_pairs;
  
  file_pairs ifoFiles;

  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++) {
    DVDFileData * f = *i;
    if(f->domain == DVD_READ_INFO_FILE)
      ifoFile = f;
    if(ifoFile && f->isBackup() && 
       f->title == ifoFile->title && 
       f->number == ifoFile->number) {
      // Found a correct IFO -> BUP correspondance
      ifoFiles.push_back(std::pair<DVDFileData *, DVDFileData *> (ifoFile, f));
    }
  }

  for(file_pairs::iterator i = ifoFiles.begin(); 
      i != ifoFiles.end(); i++) {
    DVDFileData * ifo = i->first;
    DVDFileData * bup = i->second;
    
    std::cout << "Splicing in information from " << bup->fileName() 
              << " to " << ifo->fileName() << std::endl;

    // Now, the game is to copy the sectors whose number is listed in
    // the IFO file from the BUP file, excepted the first _nb_
    int ifoSectors = 0;
    extractIFOSizes(ifo, &ifoSectors);
    DVDOutFile outfile(targetDirectory.c_str(), 
                       ifo->title, ifo->domain);
    std::unique_ptr<DVDFile> file(DVDFile::openFile(reader, bup));

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

    outfile.seek(nb);
    file->walkFile(nb, ifoSectors - nb, 128, 
                   success, failure);
  }
}


DVDCopy::~DVDCopy()
{
  if(reader)
    DVDClose(reader);
  closeBadSectorsFile();
  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++)
    delete *i;                  // Keep it clean;
}

void DVDCopy::openBadSectorsFile(const char * mode)
{
  if(! badSectors) {
    if(badSectorsFileName.empty())
      badSectorsFileName = targetDirectory + ".bad";
    badSectors = fopen(badSectorsFileName.c_str(), mode);
  }
}

void DVDCopy::closeBadSectorsFile()
{
  if(badSectors)
    fclose(badSectors);
  badSectors = NULL;
}

void DVDCopy::registerBadSectors(const DVDFileData * dat, 
                                 int beg, int size, bool dontWrite)
{
  badSectorsList.push_back(BadSectors(dat, beg, size));
  if(! dontWrite) {
    openBadSectorsFile("a");
    fprintf(badSectors, "%s\n",
            badSectorsList.back().toString().c_str());
    fflush(badSectors);
  }
}

void DVDCopy::setBadSectorsFileName(const char * file)
{
  badSectorsFileName = file;
  std::cout << "Using '" << badSectorsFileName << "' for bad sectors " 
            << std::endl;
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

void DVDCopy::ejectDrive()
{
  if(! sourceDevice.empty())
    DVDDrive::eject(sourceDevice.c_str());
}

void DVDCopy::extractIFOSizes(const DVDFileData * dat, 
                              int * ifoSectors,
                              int * titleSectors)
{
  unsigned char buffer[2048];
  std::unique_ptr<DVDFile> file(DVDFile::openFile(reader, dat));

  // Read the first sector
  file->readBlocks(0, 1, buffer);

  // Information coming from:
  // http://dvd.sourceforge.net/dvdinfo/ifo.html

  unsigned last_sec = ((unsigned)buffer[0x0C] << 24 |
                       (unsigned)buffer[0x0D] << 16 | 
                       (unsigned)buffer[0x0E] << 8 |
                       (unsigned)buffer[0x0F]);
  if(titleSectors)
    *titleSectors = last_sec + 1;
      
  unsigned last_ifo_sec = ((unsigned)buffer[0x1C] << 24 |
                           (unsigned)buffer[0x1D] << 16 | 
                           (unsigned)buffer[0x1E] << 8 |
                           (unsigned)buffer[0x1F]);

  if(ifoSectors)
    *ifoSectors = last_ifo_sec + 1;

}


void DVDCopy::scanIFOs(const char * device)
{
  setup(device, NULL);

  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++) {
    DVDFileData * dat = *i;
    if(dat->domain == DVD_READ_INFO_FILE ||
       dat->domain == DVD_READ_INFO_BACKUP_FILE) {
      // That's what we want !

      int titleSize = 0;
      int ifoSize = 0;
      extractIFOSizes(dat, &ifoSize, &titleSize);
      


      std::cout << "Looking at file " << dat->fileName(true) 
                << " of size " << dat->size/2048 << " sectors\n"
                << "IFO size " << ifoSize << "\n"
                << "Title size " << titleSize << std::endl;
    }
  }  
}
