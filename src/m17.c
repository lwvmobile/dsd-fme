/*-------------------------------------------------------------------------------
 * m17.c
 * M17 Decoder (WIP)
 *
 * M17_SCRAMBLER (m17_scramble) Bit Array from SDR++
 * Base40 CharMap from m17-tools
 *
 * LWVMOBILE
 * 2023-07 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/
#include "dsd.h"

//try to find a fancy lfsr or calculation for this and not an array if possible
uint8_t m17_scramble[369] = { 
1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1,
1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0,
1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0,
1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0,
1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0,
1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0,
1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1,
0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0,
0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1,
1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1,
1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0,
0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1,
0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0,
0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0,
1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0,
0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1,
1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1,
1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1,
0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0,
0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1,
0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1 
};

char b40[41] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";

/*
2.5.4
LSF CRC
M17 uses a non-standard version of 16-bit CRC with polynomial x16 + x14 + x12 + x11 + x8 +
x5 + x4 + x2 + 1 or 0×5935 and initial value of 0×FFFF. This polynomial allows for detecting all
errors up to hamming distance of 5 with payloads up to 241 bits, which is less than the amount
of data in each frame.
*/

uint16_t crc16m17(const uint8_t buf[], int len)
{
  uint16_t poly = 0x5935;
  uint16_t crc = 0xFFFF;
  for(int i=0; i<len; i++) 
  {
    uint8_t bit = buf[i] & 1;
    crc = ((crc << 1) | bit) & 0x1ffff;
    if (crc & 0x10000) crc = (crc & 0xffff) ^ poly;
  }

  return crc & 0xffff;
}

void M17decodeLSF(dsd_state * state, dsd_opts * opts)
{
  int i;
  
  unsigned long long int lsf_dst = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[0], 48);
  unsigned long long int lsf_src = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[48], 48);
  uint16_t lsf_type = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[96], 16);

  //this is the way the manual/code expects you to read these bits
  uint8_t lsf_ps = (lsf_type >> 0) & 0x1;
  uint8_t lsf_dt = (lsf_type >> 1) & 0x3;
  uint8_t lsf_et = (lsf_type >> 3) & 0x3;
  uint8_t lsf_es = (lsf_type >> 5) & 0x3;
  uint8_t lsf_cn = (lsf_type >> 7) & 0xF;
  uint8_t lsf_rs = (lsf_type >> 11) & 0x1F;

  //store this so we can reference it for playing voice and/or decoding data
  state->m17_str_dt = lsf_dt;

  fprintf (stderr, "\n");

  //debug
  // fprintf (stderr, " LSF TYPE: %04X", lsf_type);
  // if (lsf_ps == 0) fprintf (stderr, " PACKET - ");
  // if (lsf_ps == 1) fprintf (stderr, " STREAM - ");

  //TODO: Base40 Callsigns
  fprintf (stderr, " CAN: %d", lsf_cn);
  fprintf (stderr, " DST: %012llX SRC: %012llX", lsf_dst, lsf_src);
  
  if (lsf_dt == 0) fprintf (stderr, " Reserved");
  if (lsf_dt == 1) fprintf (stderr, " Data");
  if (lsf_dt == 2) fprintf (stderr, " Voice (3200bps)");
  if (lsf_dt == 3) fprintf (stderr, " Voice + Data");

  if (lsf_et != 0) fprintf (stderr, " ENC");
  if (lsf_et == 1)
    fprintf (stderr, " Scrambler - %d", lsf_es);
  if (lsf_et == 2) fprintf (stderr, " AES-CTR");

}

int M17processLICH(dsd_state * state, dsd_opts * opts, uint8_t lich_bits[96])
{
  int i, j, err;
  err = 0;

  uint8_t lich[4][24];
  uint8_t lich_decoded[48];
  uint8_t temp[96];
  bool g[4];

  uint16_t crc_cmp = 0;
  uint16_t crc_ext = 0;
  uint8_t crc_err = 0;

  memset(lich, 0, sizeof(lich));
  memset(lich_decoded, 0, sizeof(lich_decoded));
  memset(temp, 0, sizeof(temp));

  //execute golay 24,12 or 4 24-bit chunks and reorder into 4 12-bit chunks
  for (i = 0; i < 4; i++)
  {
    g[i] = TRUE;

    for (j = 0; j < 24; j++)
      lich[i][j] = lich_bits[(i*24)+j];

    g[i] = Golay_24_12_decode(lich[i]);
    if(g[i] == FALSE) err = -1;

    for (j = 0; j < 12; j++)
    lich_decoded[i*12+j] = lich[i][j];

  }

  uint8_t lich_counter = (uint8_t)ConvertBitIntoBytes(&lich_decoded[40], 3); //lich_cunt
  uint8_t lich_reserve = (uint8_t)ConvertBitIntoBytes(&lich_decoded[43], 5); //lich_reserved

  if (err == 0)
    fprintf (stderr, "LC: %d/6 RES: %02X ", lich_counter+1, lich_reserve); //do the same thing we did on fusion and nxdn
  else fprintf (stderr, "LICH G24 ERR");

  //this is what 2.rrc is supposed to be -- according to m17-demod -- code fixed now and reflects this
  //SRC: AB2CDE, DEST: BROADCAST, STR:V/V, ENC:NONE, CAN:10, NONCE: 0000000000000000000000000000, CRC: 99af

  //transfer to storage
  for (i = 0; i < 40; i++)
    state->m17_lsf[lich_counter*40+i] = lich_decoded[i];

  if (opts->payload == 1)
  {
    fprintf (stderr, " LICH: ");
    for (i = 0; i < 6; i++)
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&lich_decoded[i*8], 8)); 
  }

  if (lich_counter == 5)
  {
    crc_cmp = crc16m17 (state->m17_lsf, 240);
    crc_ext = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[224], 16);

    if (crc_cmp != crc_ext) crc_err = 1;

    if (crc_err == 0)
      M17decodeLSF(state, opts);

    if (opts->payload == 1)
    {
      fprintf (stderr, "\n LSF: ");
      for (i = 0; i < 30; i++)
      {
        if (i == 15) fprintf (stderr, "\n      ");
        fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[i*8], 8));
      }
    }

    // if (crc_err != 0) 
      // fprintf (stderr, " %04X - %04X (CRC ERR)", crc_cmp, crc_ext);
    if (crc_err == 1) fprintf (stderr, " LSF CRC ERR");
  }

  return err;
}

void M17processCodec2(dsd_opts * opts, dsd_state * state, uint8_t payload[128])
{
  int i;
  unsigned char bytes[16];
  unsigned char bytes1[8];
  unsigned char bytes2[8];

  for (i = 0; i < 8; i++)
  {
    bytes[i+0]  = (unsigned char)ConvertBitIntoBytes(&payload[i*8*1], 8);
    bytes[i+8]  = (unsigned char)ConvertBitIntoBytes(&payload[i*8*2], 8);
    bytes1[i]   = (unsigned char)ConvertBitIntoBytes(&payload[i*8*1], 8);
    bytes2[i]   = (unsigned char)ConvertBitIntoBytes(&payload[i*8*2], 8);
  }

  //TODO: A switch to flip whether or not fullrate or halfrate
  //halfrate format not available yet in manual, but assuming 
  //it'll be either byte 1 or byte 2 when passed this far
  if (opts->payload == 1)
  {
    fprintf (stderr, "\n CODEC2: ");
    for (i = 0; i < 16; i++)
      fprintf (stderr, "%02X", bytes[i]);
    fprintf (stderr, " (3200)");
  }
  
  //TODO: Fix Speech Output speed/rate issues and also find/figure out potential memory overflow here
  #ifdef USE_CODEC2
  size_t nsam;
  nsam = codec2_samples_per_frame(state->codec2_3200);

  //use pointer and allocate memory -- increasing allocation doesn't appear to cause any issues of note just yet
  short * speech = malloc (sizeof (short) * (nsam*2)); //*2 

  //original -- working but speech rate in .rrc files is too fast, but sounds really clean
  //may need to collect enough samples first for smooth playback, or upsample them
  codec2_decode(state->codec2_3200, speech, bytes);
  pa_simple_write(opts->pulse_digi_dev_out, speech, nsam*2, NULL);

  //testing area
  // codec2_decode(state->codec2_3200, speech, bytes1);
  // codec2_decode(state->codec2_3200, speech, bytes2);
  // pa_simple_write(opts->pulse_digi_dev_out, speech, nsam, NULL);
  // pa_simple_write(opts->pulse_digi_dev_out, speech, nsam, NULL);

  //testing area
  // codec2_decode(state->codec2_3200, speech, bytes1);
  // pa_simple_write(opts->pulse_digi_dev_out, speech, nsam, NULL);
  // codec2_decode(state->codec2_3200, speech, bytes2);
  // pa_simple_write(opts->pulse_digi_dev_out, speech, nsam, NULL);

  free(speech); //ba dum tiss

  #endif

}

void M17prepareStream(dsd_opts * opts, dsd_state * state, uint8_t m17_bits[368])
{
  //this function will prepare Stream frames

  // Scheme P2 is for frames (excluding LICH chunks, which are coded differently). This takes 296
  // encoded bits and selects 272 of them. Every 12th bit is being punctured out, leaving 272 bits.
  // The full matrix shall have 12 entries with 11 being ones.

  // P2 = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0]

  int i, j, k, x; 
  uint8_t m17_punc[310]; //25 * 11 = 275
  memset (m17_punc, 0, sizeof(m17_punc));
  for (i = 0; i < 272; i++)
    m17_punc[i] = m17_bits[i+96];

  //depuncture the bits
  uint8_t m17_depunc[310]; //25 * 12 = 300
  memset (m17_depunc, 0, sizeof(m17_depunc));
  k = 0; x = 0;
  for (i = 0; i < 25; i++)
  {
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = 0; 
  }

  //setup the convolutional decoder
  uint8_t temp[300];
  uint8_t s0;
  uint8_t s1;
  uint8_t m_data[28];
  uint8_t trellis_buf[400];
  memset (trellis_buf, 0, sizeof(trellis_buf));
  memset (temp, 0, sizeof (temp));
  memset (m_data, 0, sizeof (m_data));

  for (i = 0; i < 296; i++)
    temp[i] = m17_depunc[i] << 1; 

  CNXDNConvolution_start();
  for (i = 0; i < 148; i++)
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 144);

  //144/8 = 18, last 4 (144-148) are trailing zeroes
  for(i = 0; i < 18; i++)
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

  //load m_data into bits for either data packets or voice packets
  uint8_t payload[128];
  uint8_t end = 9;
  uint16_t fn = 0;
  memset (payload, 0, sizeof(payload));

  end = trellis_buf[0];
  fn = (uint16_t)ConvertBitIntoBytes(&trellis_buf[1], 15);

  if (opts->payload == 1)
    fprintf (stderr, " FSN: %05d", fn);

  if (end == 1)
    fprintf (stderr, " END;");

  for (i = 0; i < 128; i++)
    payload[i] = trellis_buf[i+16];

  if (state->m17_str_dt == 2)
    M17processCodec2(opts, state, payload);
  else if (state->m17_str_dt == 3) fprintf (stderr, "  V+D;"); //format not available yet in manual
  else if (state->m17_str_dt == 1) fprintf (stderr, " DATA;");
  else if (state->m17_str_dt == 0) fprintf (stderr, "  RES;");
  else                             fprintf (stderr, "  UNK;");

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n STREAM: ");
    for (i = 0; i < 18; i++) 
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8)); 
  }

}

void processM17(dsd_opts * opts, dsd_state * state)
{

  int i, x;
  uint8_t dbuf[184]; //384-bit frame - 16-bit (8 symbol) sync pattern
  uint8_t m17_rnd_bits[368]; //368 bits that are still scrambled (randomized)
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_bits[368]; //368 bits that have been de-interleaved and de-scramble
  uint8_t lich_bits[96];
  int lich_err = -1;

  memset (dbuf, 0, sizeof(dbuf));
  memset (m17_rnd_bits, 0, sizeof(m17_rnd_bits));
  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));
  memset (lich_bits, 0, sizeof(lich_bits));

  //this is a shim to allot extra memory until I can find the overflow issue
  //when decoding the codec2 data
  uint8_t big_chungus[0xFFFF];
  memset (big_chungus, 0, sizeof(big_chungus));

  /*
    Within the Physical Layer, the 368 Type 4 bits are randomized and combined with the 16-bit
    Stream Sync Burst, which results in a complete frame of 384 bits (384 bits / 9600bps = 40 ms).
  */

  //load dibits into dibit buffer
  for (i = 0; i < 184; i++)
    dbuf[i] = getDibit(opts, state);

  //convert dbuf into a bit array
  for (i = 0; i < 184; i++)
  {
    m17_rnd_bits[i*2+0] = (dbuf[i] >> 1) & 1;
    m17_rnd_bits[i*2+1] = (dbuf[i] >> 0) & 1;
  }

  //descramble the frame
  for (i = 0; i < 368; i++)
    m17_int_bits[i] = (m17_rnd_bits[i] ^ m17_scramble[i]) & 1;

  //deinterleave the bit array using Quadratic Permutation Polynomial function π(x) = (45x + 92x2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368;
    m17_bits[i] = m17_int_bits[x];
  }

  for (i = 0; i < 96; i++)
    lich_bits[i] = m17_bits[i];

  //check lich first, and handle LSF chunk and completed LSF
  lich_err = M17processLICH(state, opts, lich_bits);

  if (lich_err == 0)
    M17prepareStream(opts, state, m17_bits);

  if (lich_err != 0) state->lastsynctype = -1;

  //ending linebreak
  fprintf (stderr, "\n");

} //end processM17
