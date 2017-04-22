/** 
    \file main.c
    main file for dvdcopy
    Copyright Vincent Fourmond, 2006, 2008, 2011
 
    This is dvdcopy, a wrapper around libreaddvd facilities for
    reading DVDs.
  
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "headers.hh"
#include "dvdcopy.hh"
#include "dvdreader.hh"

#include <getopt.h>

void printHelp(const char * progname)
{
  std::cout << "Usage: " << progname 
            << " source target\n\n" 
            << "Copies the DVD at the device source to the directory target\n\n"
            << "Options: \n" 
            << " -h, --help: print this help message\n"
            << " -l, --list: list files contained on the DVD\n"
            << " -n, --number NB:  read NB sectors at a time\n"
            << " -s, --second-pass: run a second pass reading only bad sectors\n"
            << " -b, --bad-sectors: specify an alternate bad sectors file\n" 
            << " -S, --scan: scan directory for bad sectors\n" 
            << " -I, --ifo-scan: scan ifo files for info\n" 
            << " -e, --eject: attempts to eject the source after copying\n";
    
}

static struct option long_options[] = {
  { "help", 0, NULL, 'h'},
  { "eject", 0, NULL, 'e' },
  { "list", 1, NULL, 'l' },
  { "number", 1, NULL, 'n' },
  { "second-pass", 0, NULL, 's' },
  { "bad-sectors", 1, NULL, 'b' },
  { "scan", 0, NULL, 'S' },
  { "ifo-scan", 0, NULL, 'I' },
  { "splice-ifos", 0, NULL, 10 },
  { "splice-ifos-base", 1, NULL, 11 },
  { NULL, 0, NULL, 0}
};

int main(int argc, char ** argv)
{
  DVDCopy dvd;

  int option;
  int secondPass = 0;
  int scan = 0;
  int ifoScan = 0;
  int eject = 0;
  int spliceIFOs = 0;

  do {
    option = getopt_long(argc, argv, "b:heIl:sSn:",
                         long_options, NULL);
    
    switch(option) {
    case -1: break;
    case 10: 
      spliceIFOs = 1;
      break;
    case 11:
      spliceIFOs = atoi(optarg);
      break;
    case 'h': 
      printHelp(argv[0]);
      return 0;
    case 'e': 
      eject = 1;
      break;
    case 'b': 
      dvd.setBadSectorsFileName(optarg);
      break;
    case 'n':  {
      int nb = atoi(optarg);
      if(nb > 0)
        dvd.sectorsRead = nb;
    }
      break;
    case 'I':
      ifoScan = 1;
      break;
    case 'l': 
      {
        DVDReader dvd(optarg);
        dvd.displayFiles();
      }
      return 0;
    case 's':
      secondPass = 1;
      break;
    case 'S':
      scan = 1;
      break;
    }
  } while(option != -1);
  if(argc != optind + (ifoScan ? 1 : 2)) {
    printHelp(argv[0]);
    return 1;
  }
  
  if(secondPass)
    dvd.secondPass(argv[optind], argv[optind+1]);
  else if(scan)
    dvd.scanForBadSectors(argv[optind], argv[optind+1]);
  else if(ifoScan)
    dvd.scanIFOs(argv[optind]);
  else if(spliceIFOs > 0)
    dvd.spliceIFO(argv[optind], argv[optind+1], spliceIFOs);
  else {
    int skipped = dvd.copy(argv[optind], argv[optind+1]);
    if(skipped > 0) {
      printf("Found %d skipped sectors, proceeding through a second pass\n");
      dvd.secondPass(argv[optind], argv[optind+1]);
    }
  }

  if(eject)
    dvd.ejectDrive();
  return 0;
}
