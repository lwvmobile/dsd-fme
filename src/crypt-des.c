/*-------------------------------------------------------------------------------
 * des.c         Crypthings
 * DES Alg
 *-----------------------------------------------------------------------------*/

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <stdlib.h>
#include <stdio.h>
#include "dsd.h"

//NOTE: The SLUT boxes are S boxes with additional calculations so we don't have
//to manually sort out row and column values and can cut down on operations

uint8_t SLUT1[64] = {
0xE0,0x00,0x40,0xF0,0xD0,0x70,0x10,0x40,
0x20,0xE0,0xF0,0x20,0xB0,0xD0,0x80,0x10,
0x30,0xA0,0xA0,0x60,0x60,0xC0,0xC0,0xB0,
0x50,0x90,0x90,0x50,0x00,0x30,0x70,0x80,
0x40,0xF0,0x10,0xC0,0xE0,0x80,0x80,0x20,
0xD0,0x40,0x60,0x90,0x20,0x10,0xB0,0x70,
0xF0,0x50,0xC0,0xB0,0x90,0x30,0x70,0xE0,
0x30,0xA0,0xA0,0x00,0x50,0x60,0x00,0xD0
};

uint8_t SLUT2[64] = {
0x0F,0x03,0x01,0x0D,0x08,0x04,0x0E,0x07,
0x06,0x0F,0x0B,0x02,0x03,0x08,0x04,0x0E,
0x09,0x0C,0x07,0x00,0x02,0x01,0x0D,0x0A,
0x0C,0x06,0x00,0x09,0x05,0x0B,0x0A,0x05,
0x00,0x0D,0x0E,0x08,0x07,0x0A,0x0B,0x01,
0x0A,0x03,0x04,0x0F,0x0D,0x04,0x01,0x02,
0x05,0x0B,0x08,0x06,0x0C,0x07,0x06,0x0C,
0x09,0x00,0x03,0x05,0x02,0x0E,0x0F,0x09
};

uint8_t SLUT3[64] = {
0xA0,0xD0,0x00,0x70,0x90,0x00,0xE0,0x90,
0x60,0x30,0x30,0x40,0xF0,0x60,0x50,0xA0,
0x10,0x20,0xD0,0x80,0xC0,0x50,0x70,0xE0,
0xB0,0xC0,0x40,0xB0,0x20,0xF0,0x80,0x10,
0xD0,0x10,0x60,0xA0,0x40,0xD0,0x90,0x00,
0x80,0x60,0xF0,0x90,0x30,0x80,0x00,0x70,
0xB0,0x40,0x10,0xF0,0x20,0xE0,0xC0,0x30,
0x50,0xB0,0xA0,0x50,0xE0,0x20,0x70,0xC0
};

uint8_t SLUT4[64] = {
0x07,0x0D,0x0D,0x08,0x0E,0x0B,0x03,0x05,
0x00,0x06,0x06,0x0F,0x09,0x00,0x0A,0x03,
0x01,0x04,0x02,0x07,0x08,0x02,0x05,0x0C,
0x0B,0x01,0x0C,0x0A,0x04,0x0E,0x0F,0x09,
0x0A,0x03,0x06,0x0F,0x09,0x00,0x00,0x06,
0x0C,0x0A,0x0B,0x01,0x07,0x0D,0x0D,0x08,
0x0F,0x09,0x01,0x04,0x03,0x05,0x0E,0x0B,
0x05,0x0C,0x02,0x07,0x08,0x02,0x04,0x0E
};

uint8_t SLUT5[64] = {
0x20,0xE0,0xC0,0xB0,0x40,0x20,0x10,0xC0,
0x70,0x40,0xA0,0x70,0xB0,0xD0,0x60,0x10,
0x80,0x50,0x50,0x00,0x30,0xF0,0xF0,0xA0,
0xD0,0x30,0x00,0x90,0xE0,0x80,0x90,0x60,
0x40,0xB0,0x20,0x80,0x10,0xC0,0xB0,0x70,
0xA0,0x10,0xD0,0xE0,0x70,0x20,0x80,0xD0,
0xF0,0x60,0x90,0xF0,0xC0,0x00,0x50,0x90,
0x60,0xA0,0x30,0x40,0x00,0x50,0xE0,0x30
};

uint8_t SLUT6[64] = {
0x0C,0x0A,0x01,0x0F,0x0A,0x04,0x0F,0x02,
0x09,0x07,0x02,0x0C,0x06,0x09,0x08,0x05,
0x00,0x06,0x0D,0x01,0x03,0x0D,0x04,0x0E,
0x0E,0x00,0x07,0x0B,0x05,0x03,0x0B,0x08,
0x09,0x04,0x0E,0x03,0x0F,0x02,0x05,0x0C,
0x02,0x09,0x08,0x05,0x0C,0x0F,0x03,0x0A,
0x07,0x0B,0x00,0x0E,0x04,0x01,0x0A,0x07,
0x01,0x06,0x0D,0x00,0x0B,0x08,0x06,0x0D
};

uint8_t SLUT7[64] = {
0x40,0xD0,0xB0,0x00,0x20,0xB0,0xE0,0x70,
0xF0,0x40,0x00,0x90,0x80,0x10,0xD0,0xA0,
0x30,0xE0,0xC0,0x30,0x90,0x50,0x70,0xC0,
0x50,0x20,0xA0,0xF0,0x60,0x80,0x10,0x60,
0x10,0x60,0x40,0xB0,0xB0,0xD0,0xD0,0x80,
0xC0,0x10,0x30,0x40,0x70,0xA0,0xE0,0x70,
0xA0,0x90,0xF0,0x50,0x60,0x00,0x80,0xF0,
0x00,0xE0,0x50,0x20,0x90,0x30,0x20,0xC0
};

uint8_t SLUT8[64] = {
0x0D,0x01,0x02,0x0F,0x08,0x0D,0x04,0x08,
0x06,0x0A,0x0F,0x03,0x0B,0x07,0x01,0x04,
0x0A,0x0C,0x09,0x05,0x03,0x06,0x0E,0x0B,
0x05,0x00,0x00,0x0E,0x0C,0x09,0x07,0x02,
0x07,0x02,0x0B,0x01,0x04,0x0E,0x01,0x07,
0x09,0x04,0x0C,0x0A,0x0E,0x08,0x02,0x0D,
0x00,0x0F,0x06,0x0C,0x0A,0x09,0x0D,0x00,
0x0F,0x03,0x03,0x05,0x05,0x06,0x08,0x0B
};

//initial permutation IP (on input)
uint8_t initial_register_permutation[64] = {
58, 50, 42, 34, 26, 18, 10, 2,
60, 52, 44, 36, 28, 20, 12, 4,
62, 54, 46, 38, 30, 22, 14, 6,
64, 56, 48, 40, 32, 24, 16, 8,
57, 49, 41, 33, 25, 17,  9, 1,
59, 51, 43, 35, 27, 19, 11, 3,
61, 53, 45, 37, 29, 21, 13, 5,
63, 55, 47, 39, 31, 23, 15, 7};

//D-Box Expansion (sometimes noted as E BIT-SELECTION TABLE)
uint8_t message_expansion[48] = {
32,  1,  2,  3,  4,  5,
4,   5,  6,  7,  8,  9,
8,   9, 10, 11, 12, 13,
12, 13, 14, 15, 16, 17,
16, 17, 18, 19, 20, 21,
20, 21, 22, 23, 24, 25,
24, 25, 26, 27, 28, 29,
28, 29, 30, 31, 32,  1};

// (sometimes noted as simply 'P')
uint8_t right_half_permutation[32] = {
16,  7, 20, 21,
29, 12, 28, 17,
1,  15, 23, 26,
5,  18, 31, 10,
2,   8, 24, 14,
32, 27,  3,  9,
19, 13, 30,  6,
22, 11,  4, 25};

//inverse initial PI, a.k.a. final permutation (on output)
uint8_t final_register_permutation[64] = {
40,  8, 48, 16, 56, 24, 64, 32,
39,  7, 47, 15, 55, 23, 63, 31,
38,  6, 46, 14, 54, 22, 62, 30,
37,  5, 45, 13, 53, 21, 61, 29,
36,  4, 44, 12, 52, 20, 60, 28,
35,  3, 43, 11, 51, 19, 59, 27,
34,  2, 42, 10, 50, 18, 58, 26,
33,  1, 41,  9, 49, 17, 57, 25};

//initial key permutation
uint8_t pc1_key_permutation[56] = {
57, 49,  41, 33,  25,  17,  9,
1,  58,  50, 42,  34,  26, 18,
10,  2,  59, 51,  43,  35, 27,
19, 11,   3, 60,  52,  44, 36,
63, 55,  47, 39,  31,  23, 15,
7,  62,  54, 46,  38,  30, 22,
14,  6,  61, 53,  45,  37, 29,
21, 13,   5, 28,  20,  12,  4};

//sub c and d key permutation
uint8_t pc2_key_permutation[48] = {
//c bits
14,17,11,24,1,5,
3,28,15,6,21,10,
23,19,12,4,26,8,
16,7,27,20,13,2,
//d bits (with -28 offset to simply/only have one permute function)
13,24,3,9,19,27,
2,12,23,17,5,20,
16,21,11,28,6,25,
18,14,22,8,1,4
};

uint8_t key_shift_sizes[17] = {
1,
1, 1, 2, 2,
2, 2, 2, 2,
1, 2, 2, 2,
2, 2, 2, 1};

uint8_t key_shift_bytes[17] = {
0x80,
0x80, 0x80, 0xC0, 0xC0,
0xC0, 0xC0, 0xC0, 0xC0,
0x80, 0xC0, 0xC0, 0xC0,
0xC0, 0xC0, 0xC0, 0x80};

uint8_t key_shift_x[4] = {8,8,8,4};

uint8_t key_shift_y[4] = {1,2,3,0};

//input is byte-wise array, output is byte-wise array
void permute(uint8_t * input, uint8_t * output, uint8_t * table, uint8_t start, uint8_t end)
{

  for (uint8_t i = start; i < end; i++)
  {
    uint8_t tdiv = (table[i] - 1) / 8;
    uint8_t tmod = (table[i] - 1) % 8;
    uint8_t idiv = i/8;
    uint8_t imod = i%8;

    //need a way to simplify or get rid of all these shifts
    uint8_t bit = (((input[tdiv] & (0x80 >> tmod)) << tmod) >> imod);
    output[idiv] |= bit;
  }

}

void rotate_sub_keys(uint8_t * Kc, uint8_t * Kd, uint8_t shift_size, uint8_t shift_byte)
{

  uint8_t sb1[4]; memset(sb1, 0, sizeof(sb1));
  uint8_t sb2[4]; memset(sb2, 0, sizeof(sb2));

  for (uint8_t i = 0; i < 4; i++)
  {
    sb1[i] = shift_byte & Kc[i];
    sb2[i] = shift_byte & Kd[i];
  }

  for (uint8_t i = 0; i < 4; i++)
  {

    uint8_t x = key_shift_x[i];
    uint8_t y = key_shift_y[i];

    Kc[i] <<= shift_size;
    Kd[i] <<= shift_size;

    Kc[i] |= (sb1[y] >> (x - shift_size));
    Kd[i] |= (sb2[y] >> (x - shift_size));

  }
}

void des_cipher (uint8_t * main_key, uint8_t * input_register, uint8_t * output_register, uint8_t de)
{

  //starting and ending permutations
  uint8_t initial_permutation[8]; memset(initial_permutation, 0, sizeof(initial_permutation));
  uint8_t pre_end_permutation[8]; memset(pre_end_permutation, 0, sizeof(pre_end_permutation));

  //intermediate left, right, and expansion/selection arrays
  uint8_t left[4];  memset(left, 0, sizeof(left));
  uint8_t right[4]; memset(right, 0, sizeof(right));
  uint8_t rperm[4]; memset(rperm, 0, sizeof(rperm));
  uint8_t exp[6];   memset(exp, 0, sizeof(exp));
  uint8_t sel[4];   memset(sel, 0, sizeof(sel));

  //key sets
  uint8_t Ks[16][7]; memset(Ks, 0, sizeof(Ks));
  uint8_t Kc[4];     memset(Kc, 0, sizeof(Kc));
  uint8_t Kd[4];     memset(Kd, 0, sizeof(Kd));

  //initial Ks permutation and shuffle to Kc and Kd
  permute (main_key, Ks[0], pc1_key_permutation, 0, 56);
  for (uint8_t i = 0; i < 3; i++)
  {
    Kc[i] = Ks[0][i];
  }
  Kc[3] = Ks[0][3] & 0xF0;
  for (uint8_t i = 0; i < 3; i++)
  {
    Kd[i]  = (Ks[0][i+3] & 0x0F) << 4;
    Kd[i] |= (Ks[0][i+4] & 0xF0) >> 4;
  }
  Kd[3] = (Ks[0][6] & 0x0F) << 4;

  //reset Ks
  memset(Ks, 0, sizeof(Ks));

  //create the 16 Ks rounds
  for (uint8_t i = 0; i < 16; i++)
  {
    uint8_t shift_size = key_shift_sizes[i+1];
    uint8_t shift_byte = key_shift_bytes[i+1];
    rotate_sub_keys(Kc, Kd, shift_size, shift_byte);
    permute (Kc, Ks[i], pc2_key_permutation, 0, 24);
    permute (Kd, Ks[i], pc2_key_permutation, 24, 48);
  }

  //permute the input_register with the ip (initial_register_permutation) table
  permute (input_register, initial_permutation, initial_register_permutation, 0, 64);

  //copy the initial permutation to left and right half arrays
  memcpy (left, initial_permutation+0, sizeof(left));
  memcpy (right, initial_permutation+4, sizeof(right));

  //16 fiestel rounds
  for (uint8_t f = 0; f < 16; f++)
  {

    //reset rperm and exp arrays
    memset(rperm, 0, sizeof(rperm));
    memset(exp, 0, sizeof(exp));

    //expand right half from 32-bit to 48-bit
    permute (right, exp, message_expansion, 0, 48);

    //apply current iteration keyset to the expanded array (encryption cycles Ks forward, decryption goes in reverse)
    if (de == 1)
    {
      for (uint8_t i = 0; i < 6; i++)
        exp[i] ^= Ks[f][i];
    }
    else
    {
      for (uint8_t i = 0; i < 6; i++)
        exp[i] ^= Ks[15-f][i];
    }

    //extract 6 bits for each sbox swap from the expanded array
    uint8_t e1 =   exp[0] >> 2;
    uint8_t e2 = ((exp[0] & 0x3) << 4) | (exp[1] >> 4);
    uint8_t e3 = ((exp[1] & 0xF) << 2) | (exp[2] >> 6);
    uint8_t e4 =   exp[2] & 0x3F;

    uint8_t e5 =   exp[3] >> 2;
    uint8_t e6 = ((exp[3] & 0x3) << 4) | (exp[4] >> 4);
    uint8_t e7 = ((exp[4] & 0xF) << 2) | (exp[5] >> 6);
    uint8_t e8 =   exp[5] & 0x3F;

    //select 32-bits of each 48-bit expansion via modified S Box Look Up Tables
    //swap the 6 bits for 4 bits via SLUT tables and OR them together into a selection array
    sel[0] = SLUT1[e1] | SLUT2[e2];
    sel[1] = SLUT3[e3] | SLUT4[e4];
    sel[2] = SLUT5[e5] | SLUT6[e6];
    sel[3] = SLUT7[e7] | SLUT8[e8];

    //permute the selected 32-bits into a new right half
    permute (sel, rperm, right_half_permutation, 0, 32);

    //xor new right half against left half
    for (uint8_t i = 0; i < 4; i++)
      rperm[i] ^= left[i];

    //swap old right half to left, set new right half to rperm
    memcpy (left, right, sizeof(left));
    memcpy (right, rperm, sizeof(right));

  } //end fiestel rounds f

  //combine right and left into the pre_end_permutation array
  memcpy (pre_end_permutation+0, right, sizeof(right));
  memcpy (pre_end_permutation+4, left, sizeof(left));

  //carry out the final permuation to get the completed output_register for this cipher iteration
  permute (pre_end_permutation, output_register, final_register_permutation, 0, 64);

}

void des56_ofb_keystream_output (uint8_t * main_key, uint8_t * iv, uint8_t * ks_bytes, uint8_t de, int16_t nblocks)
{

  //cipher input and output
  uint8_t input_register[8];  memset(input_register, 0, sizeof(input_register));
  uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

  //copy the IV to the input_register (make copy so we don't manipulate the calling functions copy)
  memcpy(input_register, iv, sizeof(input_register));

  //execute the des_cipher in output feedback mode
  for (int16_t i = 0; i < nblocks; i++)
  {
    //de should be 1 here for encryption mode
    des_cipher(main_key, input_register, output_register, de);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input register
    memcpy(ks_bytes+(i*8), output_register, sizeof(output_register)); //copy output_register to ks_bytes + iteration offset of times 8
    memset(output_register, 0, sizeof(output_register)); //reset output register
  }

}

//the des_cipher function is already ECB mode, this convenience wrapper is just available as a reminder to illustrate that point
void des56_ecb_payload_crypt (uint8_t * main_key, uint8_t * input_register, uint8_t * output_register, uint8_t de)
{
  //de 1 for encryption mode, 0 for decryption mode
  des_cipher(main_key, input_register, output_register, de);
}

//TDEA, or triple data encryption algorithm, or triple DES, in electronic codebook mode
void tdea_ecb_payload_crypt (uint8_t * K1, uint8_t * K2, uint8_t * K3, uint8_t * input, uint8_t * output, uint8_t de)
{

  //cipher input and output
  uint8_t input_register[8];  memset(input_register, 0, sizeof(input_register));
  uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

  //copy the input to the input_register (make copy so we don't manipulate the calling functions copy)
  memcpy(input_register, input, sizeof(input_register));

  //For TDEA, the cipher alternates between encryption and decryption to the payload
  //so, for example, K1 is run as de=1, K2 is run as de=0, and K3 is run as de=1,
  //K1 and K3 will always use the same mode and K2 will be the opposite

  //NOTE: If running ECB mode in decryption, make sure to send the keys in reverse order
  //so that its K3, K2, and K1 for decryption, and K1, K2, K3 for encryption

  //K1
  des_cipher(K1, input_register, output_register, de);
  memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
  memset(output_register, 0, sizeof(output_register)); //reset output register

  //K2
  de = (de ^ 1) & 1; //flip the de bit
  des_cipher(K2, input_register, output_register, de);
  memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
  memset(output_register, 0, sizeof(output_register)); //reset output register

  //K3
  de = (de ^ 1) & 1; //flip the de bit back
  des_cipher(K3, input_register, output_register, de);

  //copy payload out
  memcpy(output, output_register, sizeof(output_register)); //copy output_register to output

}

//TDEA, or triple data encryption algorithm, or triple DES, in cipher block chain mode (64-bit)
//same as usual inputs (byte wise), also, if needing DES56, just feed the same key into K1, K2, and K3
//if running in decryption mode, this will reverse the key bundle order from (K1, K2, K3) to (K3, K2, K1) on this end
void tdea_cbc_payload_crypt (uint8_t * K1, uint8_t * K2, uint8_t * K3, uint8_t * iv, uint8_t * in, uint8_t * out, int16_t nblocks, uint8_t de)
{

  int16_t i, j;

  //cipher input and output
  uint8_t input_register[8];  memset(input_register, 0, sizeof(input_register));
  uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

  //load first round of input_register accordingly
  if (de)
    memcpy (input_register, iv, sizeof(input_register)); //load the IV as first input_register if encrypting
  else memcpy (input_register, in, sizeof(input_register)); //load first cipher text as input_register is decrypting

  //run payload for number of payload nblocks required
  for (i = 0; i < nblocks; i++)
  {

    if (de)
    {

      //xor the current input 'in' pt to the current state of the input_register for cbc feedback
      for (j = 0; j < 8; j++)
        input_register[j] ^= in[j+(i*8)];

      //K1
      des_cipher(K1, input_register, output_register, de);
      memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
      memset(output_register, 0, sizeof(output_register)); //reset output register

      //K2
      de = (de ^ 1) & 1; //flip the de bit
      des_cipher(K2, input_register, output_register, de);
      memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
      memset(output_register, 0, sizeof(output_register)); //reset output register

      //K3
      de = (de ^ 1) & 1; //flip the de bit back
      des_cipher(K3, input_register, output_register, de);
      memcpy (input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register

      //copy ciphered output_register to output 'out'
      memcpy (out+(i*8), output_register, sizeof(output_register));
      memset(output_register, 0, sizeof(output_register)); //reset output register

    }
    else
    {

      //K3
      des_cipher(K3, input_register, output_register, de);
      memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
      memset(output_register, 0, sizeof(output_register)); //reset output register

      //K2
      de = (de ^ 1) & 1; //flip the de bit
      des_cipher(K2, input_register, output_register, de);
      memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
      memset(output_register, 0, sizeof(output_register)); //reset output register

      //K1
      de = (de ^ 1) & 1; //flip the de bit back
      des_cipher(K1, input_register, output_register, de);
      memcpy (input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register

      //copy ciphered input_register to output 'out'
      memcpy (out+(i*8), output_register, sizeof(output_register));

      //xor the current output by IV, or by last received CT, depending on round
      if (i == 0)
      {
        for (j = 0; j < 8; j++)
          out[j] ^= iv[j];
      }
      else
      {
        for (j = 0; j < 8; j++)
          out[j+(i*8)] ^= in[j+((i-1)*8)];
      }

      //copy in next segment for input_register (if not last)
      if (i < nblocks)
        memcpy(input_register, in+((i+1)*8), sizeof(input_register));

      memset(output_register, 0, sizeof(output_register)); //reset output register
    }

  }

}

//TDEA, or triple data encryption algorithm, or triple DES, in cipher block chain mode (64-bit)
//same as usual inputs (byte wise), also, if needing DES56, just feed the same key into K1, K2, and K3
//only the last output_register is returned for the variable len MAC bytes, always run in encryption mode
//but if iv is desireable, it will need to be pre-XOR'd with the first plaintext input block by the calling function
void tdea_cbc_mac_generator (uint8_t * K1, uint8_t * K2, uint8_t * K3, uint8_t * in, uint8_t * out, int16_t nblocks)
{

  int16_t i, j;

  //cipher input and output
  uint8_t input_register[8];  memset(input_register, 0, sizeof(input_register));
  uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

  //run payload for number of payload nblocks required
  for (i = 0; i < nblocks; i++)
  {

    //the cipher is always run in the foward, or encryption mode (1,0,1)

    //xor the current input 'in' pt to the current state of the input_register for cbc feedback
    for (j = 0; j < 8; j++)
      input_register[j] ^= in[j+(i*8)];

    //K1
    des_cipher(K1, input_register, output_register, 1);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //K2
    des_cipher(K2, input_register, output_register, 0);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //K3
    des_cipher(K3, input_register, output_register, 1);
    memcpy (input_register, output_register, sizeof(output_register));

    //copy ciphered output_register to output 'out'
    //Since we only want the last output_register,
    //this will overwrite the out until completion
    memcpy (out, output_register, sizeof(output_register));

    memset(output_register, 0, sizeof(output_register)); //reset output register

  }

}

//TDEA, or triple data encryption algorithm, or triple DES, in cihper feedback mode (64-bit)
//same as usual inputs (byte wise), also, if needing DES56, just feed the same key into K1, K2, and K3
void tdea_cfb_payload_crypt (uint8_t * K1, uint8_t * K2, uint8_t * K3, uint8_t * iv, uint8_t * in, uint8_t * out, int16_t nblocks, uint8_t de)
{

  int16_t i, j;

  //cipher input and output
  uint8_t input_register[8];  memset(input_register, 0, sizeof(input_register));
  uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

  //copy the IV to the input_register (make copy so we don't manipulate the calling functions copy)
  memcpy(input_register, iv, sizeof(input_register));

  //execute the des_cipher in output feedback mode 3 times using each key and transferring output to input each time
  for (i = 0; i < nblocks; i++)
  {

    //the cipher is always run in the foward, or encryption mode (1,0,1)

    //K1
    des_cipher(K1, input_register, output_register, 1);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //K2
    des_cipher(K2, input_register, output_register, 0);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //K3
    des_cipher(K3, input_register, output_register, 1);

    //xor the current input 'in' to the current state of the input_register for cipher feedback
    for (j = 0; j < 8; j++)
      output_register[j] ^= in[j+(i*8)];

    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register

    //copy keystream out and reset
    memcpy(out+(i*8), output_register, sizeof(output_register)); //copy output_register to ks_bytes + iteration offset of times 8
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //if running in decryption mode, we feed in the next round of input
    if (!de)
      memcpy(input_register, in+(i*8), sizeof(input_register));
  }

}

//TDEA, or triple data encryption algorithm, or triple DES, in IV counter mode (tested, working)
void tdea_ctr_payload_crypt (uint8_t * K1, uint8_t * K2, uint8_t * K3, uint8_t * iv, uint8_t * input, uint8_t * output, int16_t nblocks)
{

  int16_t i, j;

  //cipher input and output
  uint8_t input_register[8];  memset(input_register, 0, sizeof(input_register));
  uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

  //copy the IV to the input_register (ctr mode will manipulate the IV, since it needs to keep a rolling counter
  memcpy(input_register, iv, sizeof(input_register));

  //execute the des_cipher in counter mode 3 times using each key and transferring output to input each time
  //then the IV is iterated and fed back into the input_register to start the next nblocks loop
  for (i = 0; i < nblocks; i++)
  {

    //CTR mode cipher should always run in the forward (encryption) mode

    //K1
    des_cipher(K1, input_register, output_register, 1);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //K2
    des_cipher(K2, input_register, output_register, 0);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //K3
    des_cipher(K3, input_register, output_register, 1);

    //set output at current pointer to the xor of input current pointer and the output_register
    for (j = 0; j < 8; j++)
      output[j+(i*8)] = input[j+(i*8)] ^ output_register[j];

    memset(output_register, 0, sizeof(output_register)); //reset output register

    //increment the IV, and handle roll over (uint8_t will rollover to 0 after 0xFF)
    for (j = 7; j >= 0; j--)
    {
      iv[j]++;
      if (iv[j] == 0)
        continue;
      else break;
    }

    //debug IV iteration
    // fprintf (stderr, "\n IV: ");
    // for (j = 0; j < 8; j++)
    //   fprintf (stderr, "%02X", iv[j]);

    //feed the new IV into the input register
    memcpy (input_register, iv, sizeof(input_register));

  }

}

//TDEA, or triple data encryption algorithm, or triple DES, in output feedback mode
void tdea_tofb_keystream_output (uint8_t * K1, uint8_t * K2, uint8_t * K3, uint8_t * iv, uint8_t * ks_bytes, uint8_t de, int16_t nblocks)
{

  //cipher input and output
  uint8_t input_register[8];  memset(input_register, 0, sizeof(input_register));
  uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

  //copy the IV to the input_register (make copy so we don't manipulate the calling functions copy)
  memcpy(input_register, iv, sizeof(input_register));

  //execute the des_cipher in output feedback mode 3 times using each key and transferring output to input each time
  for (int16_t i = 0; i < nblocks; i++)
  {
    //K1
    des_cipher(K1, input_register, output_register, de);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //K2
    de = (de ^ 1) & 1; //flip the de bit
    des_cipher(K2, input_register, output_register, de);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register
    memset(output_register, 0, sizeof(output_register)); //reset output register

    //K3
    de = (de ^ 1) & 1; //flip the de bit back
    des_cipher(K3, input_register, output_register, de);
    memcpy(input_register, output_register, sizeof(output_register)); //recycle output_register back into input_register

    //copy keystream out and reset
    memcpy(ks_bytes+(i*8), output_register, sizeof(output_register)); //copy output_register to ks_bytes + iteration offset of times 8
    memset(output_register, 0, sizeof(output_register)); //reset output register
  }

}

//a linear feedback shift register with maximal taps on 64-bit values that can be run to any specified len,
//its input is a byte array of up to 8 bytes, and its output is same array packed with new LFSR value in it.
uint64_t lfsr_64_to_len_ca(uint8_t * iv, int16_t len)
{

  uint64_t lfsr = 0, bit = 0;

  lfsr = ((uint64_t)iv[0] << 56ULL) + ((uint64_t)iv[1] << 48ULL) + ((uint64_t)iv[2] << 40ULL) + ((uint64_t)iv[3] << 32ULL) +
         ((uint64_t)iv[4] << 24ULL) + ((uint64_t)iv[5] << 16ULL) + ((uint64_t)iv[6] << 8ULL)  + ((uint64_t)iv[7] << 0ULL);

  memset (iv, 0, 8*sizeof(uint8_t));

  for(int16_t cnt = 0; cnt < len; cnt++)
  {
    //63,61,45,37,27,14
    // Polynomial is C(x) = x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1
    bit = ((lfsr >> 63) ^ (lfsr >> 61) ^ (lfsr >> 45) ^ (lfsr >> 37) ^ (lfsr >> 26) ^ (lfsr >> 14)) & 0x1;
    lfsr = (lfsr << 1) | bit;
  }

  for (int16_t i = 0; i < 8; i++)
    iv[i] = (lfsr >> (56-(i*8))) & 0xFF;

  // fprintf (stderr, "\n IV(%02d): ", len);
  // for (int16_t i = 0; i < 8; i++)
  //   fprintf (stderr, "%02X", iv[i]);

  return bit;

}

void des56_ca_keystream_output (uint8_t * main_key, uint8_t * iv, uint8_t * ks_bytes, uint8_t de, int16_t ff, int16_t nbits)
{

  //cipher input and output
  uint8_t input_register[8];  memset(input_register, 0, sizeof(input_register));
  uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

  //copy the IV to the input_register (make copy so we don't manipulate the calling functions copy)
  memcpy(input_register, iv, sizeof(input_register));

  //fast forward the current input_register state
  lfsr_64_to_len_ca(input_register, ff);

  //execute the des_cipher in (CA) mode with 1-bit output
  for (int16_t i = 0; i < nbits; i++)
  {

    //de should be 1 here for encryption mode
    des_cipher(main_key, input_register, output_register, de);

    //keystream accumulation, shift current byte and append
    //single bit from current output register's most significant bit
    ks_bytes[i/8] <<= 1;
    ks_bytes[i/8] |= ((output_register[0] >> 7) & 1);

    //advance the input_register with the lfsr 1 time
    lfsr_64_to_len_ca(input_register, 1);

    //reset output register
    memset(output_register, 0, sizeof(output_register));

  }

  // fprintf (stderr, "\n  IR: ");
  // for (int16_t i = 0; i < 8; i++)
  //   fprintf (stderr, "%02X", input_register[i]);

}

//transitional function mainly to load key and iv into an array
void des_multi_keystream_output (unsigned long long int mi, unsigned long long int key_ulli, uint8_t * output, int type, int len)
{
  int i = 0;
  uint8_t iv[8]; memset (iv, 0, sizeof(iv));
  uint8_t key[8]; memset(key, 0, sizeof(key));

  //convert unsinged long long int values into array values
  for (i = 7; i >= 0; i--)
    iv[7-i] = (mi >> (8*i)) & 0xFF;

  for (i = 7; i >= 0; i--)
    key[7-i] = (key_ulli >> (8*i)) & 0xFF;

  //debug
  // fprintf (stderr, "\n  IV: ");
  // for (i = 0; i < 8; i++)
  //   fprintf (stderr, "%02X", iv[i]);
  // fprintf (stderr, "\n Key: ");
  // for (i = 0; i < 8; i++)
  //   fprintf (stderr, "%02X", key[i]);

  //types: 1 = DES56 OFB; 2 = DES56 CA (XL)
  if (type == 2)
  {
    if (len == 0)
      des56_ca_keystream_output(key, iv, output, 1, 110+696, 1704);
    else des56_ca_keystream_output(key, iv, output, 1, 110+000, 1704);
  }
  else des56_ofb_keystream_output(key, iv, output, 1, len);

}

//transitional function mainly to load iv into an array
void tdea_multi_keystream_output (unsigned long long int mi, uint8_t * key, uint8_t * output, int type, int len)
{
  int i = 0;
  uint8_t iv[8]; memset (iv, 0, sizeof(iv));

  //convert unsinged long long int values into array values
  for (i = 7; i >= 0; i--)
    iv[7-i] = (mi >> (8*i)) & 0xFF;

  //debug
  // fprintf (stderr, "\n  IV: ");
  // for (i = 0; i < 8; i++)
  //   fprintf (stderr, "%02X", iv[i]);

  //type 1 = TDEA TOFB //TODO: Add more types like 2DES?
  if (type == 1)
    tdea_tofb_keystream_output(key, key+8, key+16, iv, output, 1, len);
  else tdea_tofb_keystream_output(key, key+8, key+16, iv, output, 1, len);

}