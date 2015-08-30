///////////////////////////////////////////////////////////////////////////////
//
//  Binary to Intel HEX file converter program
//  Copyright (c) 2015 Roger A. Krupski <rakrupski@verizon.net>
//
//  Last update: 25 August 2015
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program. If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////
//
//  Technical information obtained from the Intel Hexadecimal Object File
//  Format Specification Revision A, January 6, 1988
//
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <inttypes.h>

// Intel HEX record types
#define DATA_RECORD 00
#define EOF_RECORD 01
#define EXT_SEG_ADDR 02
#define SEG_START_ADDR 03
#define EXT_LIN_ADDR 04
#define LIN_START_ADDR 05
#define BLKSIZ 65536

#define MAXBITS 20 // to send 20 bit "8086" style HEX data (max file size 1MB)
//#define MAXBITS 32 // to send 32 bit "80386" style HEX data (max file size 4GB)

uint8_t chksum;

size_t reclen;
size_t fsize;
size_t ofs;
size_t addr;

char **argptr;
char *buffer;
const char *progname;

FILE *infile;
FILE *outfile;

int open_files (void)
{
	if (! (infile = fopen (argptr[1], "rb"))) {
		fprintf (stderr, "\n%s: open input file %s failed\n\n", progname, argptr[1]);
		return 1;
	}

	if (! (outfile = fopen (argptr[2], "wb"))) {
		fprintf (stderr, "\n%s: open output file %s failed\n\n", progname, argptr[2]);
		fclose (infile);
		return 1;
	}

	return 0;
}

size_t parse_number (char *arg)
{
	while (*arg < '0' && *arg > '9') {
		arg++;
	}

	if (strncmp (arg, "0x", 2) == 0) {
		return strtoul (arg, NULL, 16);

	} else {
		return strtoul (arg, NULL, 10);
	}
}

// send one byte (8 bits)
void send_data_byte (uint8_t byte)
{
	fprintf (outfile, "%02x", byte);
	chksum += byte;
}

// send two bytes (16 bits)
void send_data_word (uint16_t word)
{
	send_data_byte (word >> 8);
	send_data_byte (word & 0xFF);
}

// sent the start of a record (: + record length)
void send_record_start (uint8_t reclen)
{
	chksum = 0xFF;
	fprintf (outfile, ":");
	send_data_byte (reclen);
}

// calculate & send the checksum
void send_checksum (void)
{
	chksum ^= 0xFF;
	send_data_byte (chksum);
	fprintf (outfile, "\n");
}

// send segment address for 8086 type processors
// this is the top word of a 32 bit linear address
// shifted right 12 bits.
// example: the 32 bit address 0x0003F800 is sent
// as follows: 0x0003F800 >> 4 = 0x00003F80
// then 0x00003F80 & 0xF000 = 0x3000 which is the
// 16 bit segment address (USBA).
void send_extended_segment_address (uint32_t addr)
{
	chksum = 0xFF; // init checksum
	send_record_start (0x02); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (EXT_SEG_ADDR); // record type 2
	send_data_word ((uint16_t) ((addr >> 4) & 0xF000)); // send extended segment address
	send_checksum();  // send checksum
}

// send a 20 bit CS:IP start address for 8086 type processors
// example: the 32 bit address 0x0003F800 is sent
// as follows: 0x0003F800 >> 4 = 0x00003F80
// then 0x00003F80 & 0xF000 = 0x3000 which is the
// 16 bit segment address (CS).
// then 0x0003F800 & 0xFFFF = 0xF800 which is the
// 16 bit offset address (IP)
void send_start_segment_address (uint32_t addr)
{
	chksum = 0xFF; // init checksum
	send_record_start (0x04); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (SEG_START_ADDR); // record type 3
	send_data_word ((uint16_t) ((addr >> 4) & 0xF000)); // send CS
	send_data_word ((uint16_t) (addr & 0xFFFF)); // send IP
	send_checksum();  // send checksum
}

// sends a 16 bit linear base address (the top half of 32 bits)
// example: 32 bit address 0x0003F800 >> 16 = 0x0003 (ULBA)
void send_extended_linear_address (uint32_t addr)
{
	chksum = 0xFF; // init checksum
	send_record_start (0x02); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (EXT_LIN_ADDR); // record type 4
	send_data_word ((uint16_t) (addr >> 16)); // send upper block address
	send_checksum();  // send checksum
}

// send a 32 bit EIP start address for 32 bit processors
// example: 32 bit address 0x0003F800 >> 16 = 0x0003 (EIP top)
// and 0x0003F800 & 0xFFFF = 0xF800 (EIP bottom)
void send_start_linear_address (uint32_t addr)
{
	chksum = 0xFF; // init checksum
	send_record_start (0x04); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (LIN_START_ADDR); // record type 5
	send_data_word ((uint16_t) (addr >> 16)); // send EIP top half
	send_data_word ((uint16_t) (addr & 0xFFFF)); // send EIP bottom half
	send_checksum();  // send checksum
}

// send the end of file record
void send_end_of_file (void)
{
	chksum = 0xFF; // init checksum
	send_record_start (0x00); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (EOF_RECORD); // record type
	send_checksum();
}

int main (int argc, char *argv[])
{
	argptr = argv;

	ofs = strlen (argptr[0]);

	while (ofs--) {
		if ((argptr[0][ofs] == '/') || (argptr[0][ofs] == '\\')) {
			break;
		}
	}

	ofs++;

	progname = (argptr[0] + ofs);

	ofs = 0;
	reclen = 16;

	switch (argc) {

		case 3: {
				if (open_files()) {
					return 1;
				}

				break;
			}

		case 4: {
				if (open_files()) {
					return 1;
				}

				ofs = parse_number (argptr[3]);
				break;
			}

		case 5: {
				if (open_files()) {
					return 1;
				}

				ofs = parse_number (argptr[3]);
				reclen = parse_number (argptr[4]);
				break;
			}

		default: {
				fprintf (stderr,
						 "\n"
						 "Usage:\n"
						 "    %s infile outfile [load offset] [record length]\n"
						 "\n"
						 "Purpose:\n"
						 "    To convert raw binary files into Intel HEX format files with\n"
						 "    an optional user specified load offset and record length.\n"
						 "\n"
						 "Notes:\n"
						 "    Both \"infile\" and \"outfile\" are required.\n"
						 "    If the optional load offset is not specified, it defaults to 0.\n"
						 "    If the optional record length is not specified, it defaults to 16.\n"
						 "    Both the load offset and record length may be entered as either\n"
						 "    decimal or hexadecimal. For hexadecimal, prefix the number with \"0x\".\n"
#if MAXBITS == 20
						 "    The largest allowable load offset is 1048575 (0x000fffff) (1 MiB)\n"
						 "    (20 bit mode) and the largest allowable record length is 255 (0xff).\n"
#elif MAXBITS == 32
						 "    The largest allowable load offset is 4294967295 (0xffffffff) (4 GiB)\n"
						 "    (32 bit mode) and the largest allowable record length is 255 (0xff).\n"
#else
#error MAXBITS NOT DEFINED!!!
#endif
						 "\n"
						 "License:\n"
						 "    This program is free software: you can redistribute it and/or modify\n"
						 "    it under the terms of the GNU General Public License as published by\n"
						 "    the Free Software Foundation, either version 3 of the License, or\n"
						 "    (at your option) any later version.\n"
						 "\n"
						 "Updates:\n"
						 "    Newest releases are available at https://github.com/krupski/bin2hex\n"
						 "\n",
						 progname
						);
				return 1;
			}
	}

	fseek (infile, 0, SEEK_END);
	fsize = ftell (infile);
	fseek (infile, 0, SEEK_SET);

	if (reclen > 255) {
		fprintf (stderr, "\n%s: record length %lu (0x%02lx) is too large\n\n", progname, reclen, reclen);
		fclose (infile);
		fclose (outfile);
		return 1;
	}


#if MAXBITS == 20

	if (ofs > (0x000FFFFF - fsize)) {
		fprintf (stderr, "\n%s: load offset %lu (0x%06lx) is too large\n\n", progname, ofs, ofs);
#elif MAXBITS == 32

	if (ofs > (0xFFFFFFFF - fsize)) {
		fprintf (stderr, "\n%s: load offset %lu (0x%08lx) is too large\n\n", progname, ofs, ofs);
#else
#error MAXBITS NOT DEFINED!!!
#endif
		fclose (infile);
		fclose (outfile);
		return 1;
	}

	if (! (buffer = (char *) malloc (fsize *sizeof (char))))
	{
		fprintf (stderr, "\n%s: allocate read buffer failed\n\n", progname);
		fclose (infile);
		fclose (outfile);
		return 1;
	}

	if (fread (buffer, sizeof (char), fsize, infile) != fsize)
	{
		fprintf (stderr, "\n%s: read of input file failed\n\n", progname);
		fclose (infile);
		fclose (outfile);
		return 1;
	}

	addr = ofs;

#if MAXBITS == 20
	send_extended_segment_address (addr);
#elif MAXBITS == 32
	send_extended_linear_address (addr);
#else
	#error MAXBITS NOT DEFINED!!!
#endif

	while (fsize)
	{

		reclen = (reclen - (addr % reclen));

		if (reclen > fsize) {
			reclen = fsize;
		}

		if (reclen) {

			if ((addr - ofs) && ((addr % BLKSIZ) == 0)) { // don't send for address block 0
#if MAXBITS == 20
				send_extended_segment_address (addr);
#elif MAXBITS == 32
				send_extended_linear_address (addr); // rolled over 0xFFFF, set next block
#else
#error MAXBITS NOT DEFINED!!!
#endif
			}

			send_record_start (reclen);
			send_data_word (addr % BLKSIZ); // data address
			send_data_byte (DATA_RECORD); // record type 0

			while (reclen && fsize) {
				send_data_byte (buffer[addr - ofs]);
				addr++;
				fsize--;
				reclen--;
			}

			send_checksum();
		}
	}

#if MAXBITS == 20
	send_start_segment_address (ofs);
#elif MAXBITS == 32
	send_start_linear_address (ofs);
#else
	#error MAXBITS NOT DEFINED!!!
#endif

	send_end_of_file();

	free (buffer);
	fclose (infile);
	fclose (outfile);

	fprintf (stdout,
			 "\n"
#if MAXBITS == 20
	"%s: (20 bit CS:IP mode)\n"
#elif MAXBITS == 32
	"%s: (32 bit EIP mode)\n"
#else
	#error MAXBITS NOT DEFINED!!!
#endif
			 "Info for '%s':\n"
			 "\n"
			 "Start address %10lu (0x%08lx)\n"
			 "End address   %10lu (0x%08lx)\n"
			 "File size     %10lu (0x%08lx)\n"
			 "Record length %10lu       (0x%02lx)\n"
			 "\n",
			 progname, argptr[2],
			 ofs, ofs,
			 addr, addr,
			 (addr - ofs), (addr - ofs),
			 reclen, reclen
			);

	return 0;
}
