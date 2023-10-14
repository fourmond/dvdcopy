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

#include "badsectors.hh"

#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/time.h>

// use of regular expressions !
#include <regex.h>

void Progress::setupForCopying(const std::vector<DVDFileData * > & files)
{
  totalSectors = 0;
  totalSkipped = 0;
  sectorsDone = 0;
  for(auto it = files.begin(); it != files.end(); it++) {
    DVDFileData * file = *it;
    FileProgress pg;
    pg.totalSectors = (file->dup ? 0 : file->size/2048);
    totalSectors += pg.totalSectors;
    pg.sectorsDone = 0;
    pg.skippedSectors = 0;
    progresses.insert(std::pair<const DVDFileData *, FileProgress>(file, pg));
    DVDFileData * base = DVDFileData::findBase(files, file);
    if(base) {
      auto it2 = progresses.find(base);
      if(it2 == progresses.end())
        throw std::runtime_error("Processing number 2 after number 1");
      it2->second.totalSectors += pg.totalSectors;
      pg.totalSectors = 0;
    }
  }
  /// @todo Use std::chrono
  gettimeofday(&startTime, NULL);
}

void Progress::setupForSecondPass(const std::vector<DVDFileData * > & files,
                                  BadSectorsFile * badSectors)
{
  totalSectors = 0;
  totalSkipped = 0;
  sectorsDone = 0;
  for(auto it = files.begin(); it != files.end(); it++) {
    DVDFileData * file = *it;
    FileProgress pg;
    int nb = badSectors->badSectorsForFile(file).size();

    pg.totalSectors = nb;
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
  printf(" %d skipped, total: %1.3g/%1.3g GB, %d skipped "
         "(%02d:%02d out of %02d:%02d, %1.1f%s)",
         progress.skippedSectors,
         sectorsDone*(1.0/(512*1024)), totalSectors*(1.0/(512*1024)), totalSkipped,
         ((int) elapsed_seconds) / 60, ((int) elapsed_seconds) % 60, 
         ((int) estimated_seconds) / 60, ((int) estimated_seconds) % 60,
         rate, rate_suffix);
  fflush(stdout);
}



//////////////////////////////////////////////////////////////////////


DVDCopy::DVDCopy() : reader(NULL),
                     badSectors(NULL), skipBUP(false),
                     sectorsRead(-1),
                     backwards(false)
{
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
    clearBadSectors(dat, offset, nb);
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
  printf("Current size of file %d, %d, %d\n", current_size, size, firstBlock);
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

int DVDCopy::copy(const char *device, const char * target)
{
  setup(device, target);
  overallProgress.setupForCopying(files);

  /// Methodically copies all listed files
  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++)
    copyFile(*i);
  return overallProgress.totalSkipped;
}

void DVDCopy::secondPass(const char *device, const char * target)
{
  setup(device, target);
  readBadSectors();
  overallProgress.setupForSecondPass(files, badSectors);

  for(auto it = files.begin(); it != files.end(); ++it) {
    const DVDFileData * file = *it;
    std::set<int> bs = badSectors->badSectorsForFile(file);
    if(backwards) {
      for(auto it2 = bs.rbegin(); it2 != bs.rend(); ++it2)
        copyFile(file, *it2, 1, 1);
    }
    else {
      for(auto it2 = bs.begin(); it2 != bs.end(); ++it2)
        copyFile(file, *it2, 1, 1);
    }
  }
}

void DVDCopy::scanForBadSectors(const char *device, 
                                const char * badSectorsFile)
{
  setup(device, NULL);
  setBadSectorsFileName(badSectorsFile);
  badSectors->clear();
  overallProgress.setupForCopying(files);
  

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
          overallProgress.successfulRead(dat, 1);
        else {
          overallProgress.failedRead(dat, 1);
          registerBadSectors(dat, blk + i, 1, true);
        }
      }
      overallProgress.writeCurrentProgress(dat);
    };

    auto failure = [this](int blk, int nb, 
                      const DVDFileData * dat) {
      registerBadSectors(dat, blk, nb, true);
      overallProgress.failedRead(dat, nb);
      overallProgress.writeCurrentProgress(dat);
    };

    file->walkFile(0, sz, (sectorsRead > 0 ? sectorsRead : -1), 
                   success, failure);
  }
  
  badSectors->writeOut();
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
  delete badSectors;
  for(std::vector<DVDFileData *>::iterator i = files.begin(); 
      i != files.end(); i++)
    delete *i;                  // Keep it clean;
}

void DVDCopy::registerBadSectors(const DVDFileData * dat, 
                                 int beg, int size, bool dontWrite)
{
  if(! badSectors) {
    std::string bsf = targetDirectory + ".bad";
    badSectors = new BadSectorsFile(bsf);
  }
  badSectors->markBadSectors(dat, beg, size);
  if(! dontWrite)
    badSectors->writeOut();
}

void DVDCopy::clearBadSectors(const DVDFileData * dat, 
                              int beg, int size, bool dontWrite)
{
  if(! badSectors)
    return;
  badSectors->clearBadSectors(dat, beg, size);
  if(! dontWrite)
    badSectors->writeOut();
}

void DVDCopy::readBadSectors()
{
  if(! badSectors) {            // just ensure it is loaded correctly
    std::string bsf = targetDirectory + ".bad";
    badSectors = new BadSectorsFile(bsf);
  }
}

void DVDCopy::setBadSectorsFileName(const char * file)
{
  delete badSectors;
  std::cout << "Using '" << badSectorsFileName << "' for bad sectors " 
            << std::endl;
  badSectors = new BadSectorsFile(file);
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
