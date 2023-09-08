/** 
    \file dvddump.cc
    dvddump, a small utility to dump the information about a DVD
    Copyright Vincent Fourmond, 2023
 
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

#include <stdio.h>
#include <stdint.h>

#define SECTOR_SIZE 2048

#include "headers.hh"
#include "dvdreader.hh"

#include <dvdread/ifo_read.h>

int main(int argc, char ** argv)
{
  DVDReader reader;
  if(argc < 2)
    return  1;
  reader.open(argv[1]);
  ifo_handle_t * ifo = ifoOpen(reader.handle(), 0);

  int programs = ifo->vts_atrt->nr_of_vtss;
  printf("Number of programs: %d\n", programs);
  ifo_handle_t ** ifos = new ifo_handle_t*[programs];
  for(int i = 0; i < programs; i++)
    ifos[i] = ifoOpen(reader.handle(), i+1);

  int titles = ifo->tt_srpt->nr_of_srpts;
  printf("Number of titles: %d\n", titles);

  for(int i = 0; i < titles; i++) {
    printf("TS for title %d: %d\n", i, ifo->tt_srpt->title[i].title_set_nr);
    
  }


  ifoClose(ifo);
  return 0;
}
