///////////////////////////////////////////////////////////////////////////////
//
//  Binary to Intel HEX file converter program
//  Copyright (c) 2015 Roger A. Krupski <rakrupski@verizon.net>
//
//  Last update: 10 January 2016
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>

// Intel HEX record types
#define DATA_RECORD 00
#define EOF_RECORD 01
#define EXT_SEG_ADDR 02
#define SEG_START_ADDR 03
#define EXT_LIN_ADDR 04
#define LIN_START_ADDR 05
#define BLKSIZ 65536

uint8_t x, y;
uint8_t checksum;

size_t default_record_len;
size_t record_len;
size_t fsize;
size_t offset;
size_t address;

char **argptr;
char *buffer;

const char *progname;

const char *help[] = {
	"help",
	"-help",
	"--help",
};

uint8_t helpsiz = (sizeof (help) / sizeof (*help));

FILE *infile;
FILE *outfile;

/*** for debug only
    int hexdump (const char *str)
    {
	int x, len;
	len = (strlen (str) + 1);

	for (x = 0; x < len; x++) {
		fprintf (stdout, "%s", (x % 256) ? "" : "\n\n      +0 +1 +2 +3 +4 +5 +6 +7  +8 +9 +A +B +C +D +E +F");
		fprintf (stdout, "%s", (x % 8)   ? "" : " ");
		(x % 16)
		? fprintf (stdout, "%02X ", (unsigned char) str[x])
		: fprintf (stdout, "\n%04X  %02X ", x, (unsigned char) str[x]);
	}

	fprintf (stdout, "\n");
	return x;
    }
***/

int show_help (int rc)
{
	fprintf (stderr,
			 "\n"
			 "Usage:\n"
			 "    %s infile outfile [load offset] [record length]\n"
			 "\n"
			 "Purpose:\n"
			 "    To convert raw binary files into Intel HEX format files with an\n"
			 "    optional user specified load offset and record length.\n"
			 "\n"
			 "Notes:\n"
			 "    Both infile and outfile are required.\n"
			 "\n"
			 "    If the destination (outfile) already exists, a choice will be\n"
			 "    given to overwrite the file or quit this program. If you choose\n"
			 "    to overwrite the outfile, you must enter the entire word yes\n"
			 "    to replace the existing file.\n"
			 "\n"
			 "    If the optional load offset is not specified, it defaults to 0.\n"
			 "\n"
			 "    If the optional record length is not specified, it defaults to 16.\n"
			 "\n"
			 "    Both the load offset and record length may be entered as decimal or\n"
			 "    hexadecimal. For hexadecimal, prefix the number with 0x.\n"
			 "\n"
			 "    The largest load offset is 4294967295 (0xFFFFFFFF) [4 GiB] and the\n"
			 "    largest record length is 255 (0xFF).\n"
			 "\n"
			 "Updates:\n"
			 "    Newest releases are available at https://github.com/krupski/bin2hex\n"
			 "\n",
			 progname
			);
	return rc;
}

int readline (char *str, int limit)
{
	int len = 0;
	int ch;
	*str = 0;

	while ((ch = fgetc (stdin)) && (len < limit)) {

		if ((ch == EOF) || (ch == 0x0A)) {
			str[len] = ch;
			break;
		}

		// handle backspace
		if ((ch == 0x08) && (len > 0)) {
			len--;
			str[len] = 0;
		}

		str[len] = ch;
		len++;
	}

	// len++; // uncomment this to remove the ending LF

	str[len] = 0;
	return len;
}

int search (const char *find, const char *buf, size_t bsize)
{
	size_t x, y, z;
	y = 0;
	z = strlen (find); // how many bytes to find a match for

	for (x = 0; x < bsize; x++) {
		if (toupper (buf[x]) == toupper (find[y])) {
			y++;

			// if match count == size and match is the right size
			if ((y == z) && (z == strlen (buf))) {
				return 1; // return true if match found
			}

		} else {
			y = 0;
		}
	}

	return 0; // return false if no match
}

int open_files (void)
{
	char tmpbuf[32];

	if (strcmp (argptr[1], argptr[2]) == 0) {
		fprintf (stderr, "\n%s: source and destination files are the same - exiting\n\n", progname);
		return 1;
	}

	if (! (infile = fopen (argptr[1], "rb"))) {
		fprintf (stderr, "\n%s: open input file %s failed\n\n", progname, argptr[1]);
		return 1;
	}

	if ((outfile = fopen (argptr[2], "rb"))) {
		fprintf (stderr, "\n%s: output file %s exists, overwrite it? (yes/N) ", progname, argptr[2]);
		readline (tmpbuf, 32);

		if (search ("yes", tmpbuf, 32)) {
			fclose (outfile);

		} else {
			fclose (infile);
			fclose (outfile);
			fprintf (stderr, "\n%s: will not overwrite %s - exiting\n\n", progname, argptr[2]);
			return 1;
		}
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
	// skip any possible leading spaces so we can detect "0x" properly
	while (*arg < '0' && *arg > '9') {
		arg++;
	}

	if (strncmp (arg, "0x", 2) == 0) {
		return strtoul (arg, NULL, 16); // parse as hex if it's "0x"

	} else {
		return strtoul (arg, NULL, 10); // else it must be decimal
	}
}

// send one byte (8 bits)
void send_data_byte (uint8_t byte)
{
	fprintf (outfile, "%02X", byte);
	checksum += byte;
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
	checksum = 0xFF;
	fprintf (outfile, ":");
	send_data_byte (reclen);
}

// calculate & send the checksum
void send_checksum (void)
{
	checksum ^= 0xFF;
	send_data_byte (checksum);
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
	checksum = 0xFF; // init checksum
	send_record_start (0x02); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (EXT_SEG_ADDR); // record type 2
	send_data_word ((uint16_t) ((addr >> 4) & 0xF000)); // send extended segment address
	send_checksum(); // send checksum
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
	checksum = 0xFF; // init checksum
	send_record_start (0x04); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (SEG_START_ADDR); // record type 3
	send_data_word ((uint16_t) ((addr >> 4) & 0xF000)); // send CS
	send_data_word ((uint16_t) (addr & 0xFFFF)); // send IP
	send_checksum(); // send checksum
}

// sends a 16 bit linear base address (the top half of 32 bits)
// example: 32 bit address 0x0003F800 >> 16 = 0x0003 (ULBA)
void send_extended_linear_address (uint32_t addr)
{
	checksum = 0xFF; // init checksum
	send_record_start (0x02); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (EXT_LIN_ADDR); // record type 4
	send_data_word ((uint16_t) (addr >> 16)); // send upper block address
	send_checksum(); // send checksum
}

// send a 32 bit EIP start address for 32 bit processors
// example: 32 bit address 0x0003F800 >> 16 = 0x0003 (EIP top)
// and 0x0003F800 & 0xFFFF = 0xF800 (EIP bottom)
void send_start_linear_address (uint32_t addr)
{
	checksum = 0xFF; // init checksum
	send_record_start (0x04); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (LIN_START_ADDR); // record type 5
	send_data_word ((uint16_t) (addr >> 16)); // send EIP top half
	send_data_word ((uint16_t) (addr & 0xFFFF)); // send EIP bottom half
	send_checksum(); // send checksum
}

// send the end of file record
void send_end (void)
{
	checksum = 0xFF; // init checksum
	send_record_start (0x00); // record length (byte count)
	send_data_word (0x0000); // load offset (always 0)
	send_data_byte (EOF_RECORD); // record type
	send_checksum();
}

int main (int argc, char *argv[])
{
	argptr = argv; // pointer to argv
	offset = strlen (argptr[0]);

	while (offset--) {
		if ((argptr[0][offset] == '/') || (argptr[0][offset] == '\\')) {
			break;
		}
	}

	offset++;

	progname = (argptr[0] + offset);

	offset = 0;
	default_record_len = 16;

	for (x = 0; x < argc; x++) {
		for (y = 0; y < helpsiz; y++) {
			if (search (help[y], argv[x], strlen (help[y]))) {
				return show_help (1);
			}
		}
	}

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

				offset = parse_number (argptr[3]);
				break;
			}

		case 5: {
				if (open_files()) {
					return 1;
				}

				offset = parse_number (argptr[3]);
				default_record_len = parse_number (argptr[4]);
				break;
			}

		default: {
				fprintf (stderr,
						 "\ncommand line error - try '%s -help'\n\n",
						 progname
						);
				return 1;
			}
	}

	fseek (infile, 0, SEEK_END);
	fsize = ftell (infile);
	fseek (infile, 0, SEEK_SET);

	if (default_record_len > 255) {
		fprintf (stderr, "\n%s: record length %lu (0x%02lX) is too large\n\n", progname, default_record_len, default_record_len);
		fclose (infile);
		fclose (outfile);
		return 1;
	}

	if (offset > (0xFFFFFFFF - fsize)) {
		fprintf (stderr, "\n%s: load offset %lu (0x%08lX) is too large\n\n", progname, offset, offset);
		fclose (infile);
		fclose (outfile);
		return 1;
	}

	if (! (buffer = (char *) malloc (fsize * sizeof (char)))) {
		fprintf (stderr, "\n%s: allocate read buffer failed\n\n", progname);
		fclose (infile);
		fclose (outfile);
		return 1;
	}

	if (fread (buffer, sizeof (char), fsize, infile) != fsize) {
		fprintf (stderr, "\n%s: read input file failed\n\n", progname);
		free (buffer);
		fclose (infile);
		fclose (outfile);
		return 1;
	}

	address = offset; // start at "offset"

	send_extended_linear_address (address); // 32 bit version

	while (fsize) {
		record_len = (default_record_len - (address % default_record_len));

		if (record_len > fsize) { // truncate record line if less than full line
			record_len = fsize;
		}

		if (record_len) {

			if ((address - offset) && ((address % BLKSIZ) == 0)) { // don't send for address block 0
				send_extended_linear_address (address); // rolled over 0xFFFF, set next block
			}

			send_record_start (record_len); // record start (may be less than reclen)
			send_data_word (address % BLKSIZ); // data address
			send_data_byte (DATA_RECORD); // record type 0

			while (record_len && fsize) { // while we have data to send...
				send_data_byte (buffer[address - offset]); // send it
				address++; // next address
				fsize--; // count down filesize
				record_len--; // count down record length
			}

			send_checksum(); // send checksum at the end of a record
		}
	}

	send_start_linear_address (offset); // send the binary image start address

	send_end(); // send the "end of file" thing

	free (buffer); // release the memory we used

	fclose (infile); // close...
	fclose (outfile); // ...files

	fprintf (stdout, // print mini report
			 "\n"
			 "%s: (32 bit EIP mode)\n"
			 "\n"
			 "Converted %s -> %s\n"
			 "\n"
			 "Start address %10lu (0x%08lX)\n"
			 "End address   %10lu (0x%08lX)\n"
			 "File size     %10lu (0x%08lX)\n"
			 "Record length %10lu       (0x%02lX)\n"
			 "\n",
			 progname, argptr[1], argptr[2],
			 offset, offset,
			 address, address,
			 (address - offset), (address - offset),
			 default_record_len, default_record_len
			);

	return 0;
}
// end of bin2hex.cpp