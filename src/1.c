/*-------------------------------------------------------------------------------
 * 1.c
 * RC4 and RC4 Block Output For Keystream Application
 *
 * 
 *
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//this is for PDU usage
void rc4_block_output (int drop, int keylen, int meslen, uint8_t * key, uint8_t * output_blocks)
{
	int i, j, x, count;
	unsigned int keylength = (unsigned int)keylen;
  unsigned int messagelength = (unsigned int)meslen;
	unsigned int S[256];

	for(i=0; i<256; i++)
		S[i] = i;

	j = 0;
	for(i = 0; i<256; i++)
	{
		j = (j + S[i] + key[i % keylength]) % 256;
		unsigned int temp = S[i];
		S[i] = S[j];
		S[j] = temp;
	}

	//Generate Keystream
	i = 0;
	j = 0;
  x = 0;
	unsigned int byte;

	// fprintf (stderr, " Keystream Octets = ");
	for(count = 0; count < (messagelength + drop); count++)
	{
		i = (i + 1) % 256;
		j = (j + S[i]) % 256;
		unsigned int temp = S[i];
		S[i] = S[j];
		S[j] = temp;
		byte = S[(S[i] + S[j]) % 256];

		//Collect Output blocks
		if (count >= drop)
      output_blocks[x++] = byte;

	}

}