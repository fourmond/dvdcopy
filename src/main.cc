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
            << " -e, --eject: attempts to eject the source at the end\n";
    
}

static struct option long_options[] = {
  { "help", 0, NULL, 'h'},
  { "eject", 0, NULL, 'e' },
  { "list", 1, NULL, 'l' },
  { "second-pass", 0, NULL, 's' },
  { "scan", 0, NULL, 'S' },
  { NULL, 0, NULL, 0}
};

int main(int argc, char ** argv)
{
  DVDCopy dvd;

  int option;
  int secondPass = 0;
  int scan = 0;

  do {
    option = getopt_long(argc, argv, "hel:sS",
                         long_options, NULL);
    
    switch(option) {
    case -1: break;
    case 'h': 
      printHelp(argv[0]);
      return 0;
    case 'e': 
      std::cerr << "Eject not implemented yet" << std::endl;
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
  if(argc != optind + 2) {
    printHelp(argv[0]);
    return 1;
  }
  
  if(secondPass)
    dvd.secondPass(argv[optind], argv[optind+1]);
  else if(scan)
    dvd.scanForBadSectors(argv[optind], argv[optind+1]);
  else
    dvd.copy(argv[optind], argv[optind+1]);
  return 0;
}
