/** 
    \file secdump.cc
    secdump, a file to dump sector information from stdin to stdout
    Copyright Vincent Fourmond, 2012
 
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

/// This comes from dvdauthor
int64_t readpts(unsigned char *buf)
{
    int64_t a1,a2,a3,pts;
    
    a1=(buf[0]&0xf)>>1;
    a2=((buf[1]<<8)|buf[2])>>1;
    a3=((buf[3]<<8)|buf[4])>>1;
    pts=(((int64_t)a1)<<30)|
        (a2<<15)|
        a3;
    return pts;
}

/// This comes from dvdauthor
int64_t readscr(const unsigned char *buf)
  /* returns the timestamp as found in the pack header. This is actually supposed to
    be units of a 27MHz clock, but I ignore the extra precision and truncate it to
    the usual 90kHz clock units. */
{
  return
    ((int64_t)(buf[0] & 0x38)) << 27 /* SCR 32 .. 30 */
    |
    (buf[0] & 3) << 28 /* SCR 29 .. 28 */
    |
    buf[1] << 20 /* SCR 27 .. 20 */
    |
    (buf[2] & 0xf8) << 12 /* SCR 19 .. 15 */
    |
    (buf[2] & 3) << 13 /* SCR 14 .. 13 */
    |
    buf[3] << 5 /* SCR 12 .. 5 */
    |
    (buf[4] & 0xf8) >> 3; /* SCR 4 .. 0 */
  /* ignore SCR_ext */
} 

// Many useful information come from:
// http://stnsoft.com/DVD/packhdr.html
// http://stnsoft.com/DVD/pes-hdr.html


/* check if ptr points to nav pack */
static int is_nav_pack (unsigned char *ptr)
{
  uint32_t start_code;

  start_code  = (uint32_t) (ptr [0]) << 24;
  start_code |= (uint32_t) (ptr [1]) << 16;
  start_code |= (uint32_t) (ptr [2]) <<  8;
  start_code |= (uint32_t) (ptr [3]);

  if (start_code != 0x000001ba)
    return 0;

  if ((ptr [4] & 0xc0) != 0x40)
    return 0;

  ptr += 14;

  start_code  = (uint32_t) (ptr [0]) << 24;
  start_code |= (uint32_t) (ptr [1]) << 16;
  start_code |= (uint32_t) (ptr [2]) <<  8;
  start_code |= (uint32_t) (ptr [3]);

  if (start_code != 0x000001bb)
    return 0;

  ptr += 24;

  start_code  = (uint32_t) (ptr [0]) << 24;
  start_code |= (uint32_t) (ptr [1]) << 16;
  start_code |= (uint32_t) (ptr [2]) <<  8;
  start_code |= (uint32_t) (ptr [3]);

  if (start_code != 0x000001bf)
    return 0;

  ptr += 986;

  start_code  = (uint32_t) (ptr [0]) << 24;
  start_code |= (uint32_t) (ptr [1]) << 16;
  start_code |= (uint32_t) (ptr [2]) <<  8;
  start_code |= (uint32_t) (ptr [3]);

  if (start_code != 0x000001bf)
    return 0;

  return 1;
}


// Possibly one use of this file would be to re-read a DVD file and
// find out which sectors were not read properly ? That would be
// helpful for sure...

int main(int argc, char ** argv)
{
  unsigned char buffer[SECTOR_SIZE];

  long nb = 0;
  int64_t last_scr = 0;
  while(! feof(stdin)) {
    fread(buffer, sizeof(buffer), 1, stdin);
    int first_pes_offset = 13 + (buffer[13] & 0x7);

    if(buffer[2] == 1 && buffer[first_pes_offset + 3] == 1) {
      int64_t scr = readscr(buffer + 4);
      printf("%7ld: 0x01%hhX, SCR: %9ld -- PES: 0x01%hhX%s%s\n",
             nb, buffer[3], scr, buffer[first_pes_offset + 4],
             is_nav_pack(buffer) ? " NAV" : "",
             scr < last_scr ? " REVERSE" : ""
             );
      last_scr = scr;
    }
    else 
      printf("%7ld: invalid\n", nb);
    nb++;
  }
}
