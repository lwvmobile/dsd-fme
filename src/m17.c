/*-------------------------------------------------------------------------------
 * m17.c
 * M17 Decoder and Encoder
 *
 * m17_scramble Bit Array from SDR++
 * CRC16, CSD encoder from libM17 / M17-Implementations (thanks again, sp5wwp)
 *
 * LWVMOBILE
 * 2024-03 DSD-FME Florida Man Edition
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

char b40[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";

uint8_t p1[62] = {
1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1
};

uint8_t p3[62] = {1, 1, 1, 1, 1, 1, 1, 0};

//from M17_Implementations / libM17 -- sp5wwp -- should have just looked here to begin with
//this setup looks very similar to the OP25 variant of crc16, but with a few differences (uses packed bytes)
uint16_t crc16m17(const uint8_t *in, const uint16_t len)
{
  uint32_t crc = 0xFFFF; //init val
  uint16_t poly = 0x5935;
  for(uint16_t i=0; i<len; i++)
  {
    crc^=in[i]<<8;
    for(uint8_t j=0; j<8; j++)
    {
      crc<<=1;
      if(crc&0x10000)
        crc=(crc^poly)&0xFFFF;
    }
  }

  return crc&(0xFFFF);
}

void M17decodeCSD(dsd_state * state, unsigned long long int dst, unsigned long long int src)
{
  //evaluate dst and src, and determine if they need to be converted to calsign
  int i;
  char c;

  if (dst == 0xFFFFFFFFFFFF) 
    fprintf (stderr, " DST: BROADCAST");
  else if (dst == 0)
    fprintf (stderr, " DST: RESERVED %012llx", dst);
  else if (dst >= 0xEE6B28000000)
    fprintf (stderr, " DST: RESERVED %012llx", dst);
  else
  {
    fprintf (stderr, " DST: ");
    for (i = 0; i < 9; i++)
    {
      c = b40[dst % 40];
      state->m17_dst_csd[i] = c;
      fprintf (stderr, "%c", c);
      dst = dst / 40;
    }
    //assign completed CSD to a more useful string instead
    sprintf (state->m17_dst_str, "%c%c%c%c%c%c%c%c%c", 
    state->m17_dst_csd[0], state->m17_dst_csd[1], state->m17_dst_csd[2], state->m17_dst_csd[3], 
    state->m17_dst_csd[4], state->m17_dst_csd[5], state->m17_dst_csd[6], state->m17_dst_csd[7], state->m17_dst_csd[8]);

    //debug
    // fprintf (stderr, "DT: %s", state->m17_dst_str);
  }

  if (src == 0xFFFFFFFFFFFF) 
    fprintf (stderr, " SRC:  UNKNOWN FFFFFFFFFFFF");
  else if (src == 0)
    fprintf (stderr, " SRC: RESERVED %012llx", src);
  else if (src >= 0xEE6B28000000)
    fprintf (stderr, " SRC: RESERVED %012llx", src);
  else
  {
    fprintf (stderr, " SRC: ");
    for (i = 0; i < 9; i++)
    {
      c = b40[src % 40];
      state->m17_src_csd[i] = c;
      fprintf (stderr, "%c", c);
      src = src / 40;
    }
    //assign completed CSD to a more useful string instead
    sprintf (state->m17_src_str, "%c%c%c%c%c%c%c%c%c", 
    state->m17_src_csd[0], state->m17_src_csd[1], state->m17_src_csd[2], state->m17_src_csd[3], 
    state->m17_src_csd[4], state->m17_src_csd[5], state->m17_src_csd[6], state->m17_src_csd[7], state->m17_src_csd[8]);

    //debug
    // fprintf (stderr, "ST: %s", state->m17_src_str);
  }

  //debug
  // fprintf (stderr, " DST: %012llX SRC: %012llX", state->m17_dst, state->m17_src);

}

void M17decodeLSF(dsd_state * state)
{
  int i;
  
  unsigned long long int lsf_dst = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[0], 48);
  unsigned long long int lsf_src = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[48], 48);
  uint16_t lsf_type = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[96], 16);

  //this is the way the manual/code expects you to read these bits
  // uint8_t lsf_ps = (lsf_type >> 0) & 0x1;
  uint8_t lsf_dt = (lsf_type >> 1) & 0x3;
  uint8_t lsf_et = (lsf_type >> 3) & 0x3;
  uint8_t lsf_es = (lsf_type >> 5) & 0x3;
  uint8_t lsf_cn = (lsf_type >> 7) & 0xF;
  uint8_t lsf_rs = (lsf_type >> 11) & 0x1F;

  //store this so we can reference it for playing voice and/or decoding data, dst/src etc
  state->m17_str_dt = lsf_dt;
  state->m17_dst = lsf_dst;
  state->m17_src = lsf_src;
  state->m17_can = lsf_cn;

  fprintf (stderr, "\n");

  //packet or stream
  // if (lsf_ps == 0) fprintf (stderr, " P-");
  // if (lsf_ps == 1) fprintf (stderr, " S-");

  fprintf (stderr, " CAN: %d", lsf_cn);
  M17decodeCSD(state, lsf_dst, lsf_src);
  
  if (lsf_dt == 0) fprintf (stderr, " Reserved");
  if (lsf_dt == 1) fprintf (stderr, " Data");
  if (lsf_dt == 2) fprintf (stderr, " Voice (3200bps)");
  if (lsf_dt == 3) fprintf (stderr, " Voice + Data");

  if (lsf_rs != 0) fprintf (stderr, " RS: %02X", lsf_rs);

  if (lsf_et != 0) fprintf (stderr, " ENC:");
  if (lsf_et == 2) fprintf (stderr, " AES-CTR");
  if (lsf_et == 1) fprintf (stderr, " Scrambler - %d", lsf_es);

  state->m17_enc = lsf_et;
  state->m17_enc_st = lsf_es;

  //compare incoming META/IV value on AES, if timestamp 32-bits are not within a time 5 minute window, then throw a warning
  uint32_t tsn = (time(NULL) & 0xFFFFFFFF); //current LSB 32-bit value
  uint32_t tsi = (uint32_t)ConvertBitIntoBytes(&state->m17_lsf[112], 32); //OTA LSB 32-bit value
  uint32_t dif = abs(tsn-tsi);
  if (lsf_et == 2 && dif > 3600) fprintf (stderr, " \n Warning! Time Difference > %d secs; Potential NONCE/IV Replay!\n", dif);

  //debug
  // fprintf (stderr, "TSN: %ld; TSI: %ld; DIF: %ld;", tsn, tsi, dif);

  //pack meta bits into 14 bytes
  for (i = 0; i < 14; i++)
    state->m17_meta[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[(i*8)+112], 8);

  //Examine the Meta data
  if (lsf_et == 0 && state->m17_meta[0] != 0) //not sure if this applies universally, or just to text data byte 0 for null data
  {
    fprintf (stderr,  " Meta:");
    if (lsf_es == 0) fprintf (stderr, " Text Data");
    if (lsf_es == 1) fprintf (stderr, " GNSS Pos");
    if (lsf_es == 2) fprintf (stderr, " Extended CSD");
    if (lsf_es == 3) fprintf (stderr, " Reserved");
  }
  
  if (lsf_et == 2)
  {
    fprintf (stderr, " IV: ");
    for (i = 0; i < 16; i++)
      fprintf (stderr, "%02X", state->m17_meta[i]);
  }
  
}

int M17processLICH(dsd_state * state, dsd_opts * opts, uint8_t * lich_bits)
{
  int i, j, err;
  err = 0;

  uint8_t lich[4][24];
  uint8_t lich_decoded[48];
  uint8_t temp[96];
  bool g[4];

  uint8_t lich_counter = 0;
  uint8_t lich_reserve = 0;

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

  lich_counter = (uint8_t)ConvertBitIntoBytes(&lich_decoded[40], 3); //lich_cnt
  lich_reserve = (uint8_t)ConvertBitIntoBytes(&lich_decoded[43], 5); //lich_reserved

  //sanity check to prevent out of bounds
  if (lich_counter > 5) lich_counter = 5;

  if (err == 0)
    fprintf (stderr, "LC: %d/6 ", lich_counter+1);
  else fprintf (stderr, "LICH G24 ERR");

  // if (err == 0 && lich_reserve != 0) fprintf(stderr, " LRS: %d", lich_reserve);

  //This is not M17 standard, but use the LICH reserved bits to signal data type and CAN value
  if (err == 0 && opts->m17encoder == 1) //only use when using built in encoder
  {
    state->m17_str_dt = lich_reserve & 0x3;
    state->m17_can = (lich_reserve >> 2) & 0x7;
  }

  //transfer to storage
  for (i = 0; i < 40; i++)
    state->m17_lsf[lich_counter*40+i] = lich_decoded[i];

  if (opts->payload == 1)
  {
    fprintf (stderr, " LICH: ");
    for (i = 0; i < 6; i++)
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&lich_decoded[i*8], 8)); 
  }

  uint8_t lsf_packed[30];
  memset (lsf_packed, 0, sizeof(lsf_packed));

  if (lich_counter == 5)
  {

    //need to pack bytes for the sw5wwp variant of the crc (might as well, may be useful in the future)
    for (i = 0; i < 30; i++)
      lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[i*8], 8);

    crc_cmp = crc16m17(lsf_packed, 28);
    crc_ext = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[224], 16);

    if (crc_cmp != crc_ext) crc_err = 1;

    if (crc_err == 0)
      M17decodeLSF(state);
    else if (opts->aggressive_framesync == 0)
      M17decodeLSF(state);

    if (opts->payload == 1)
    {
      fprintf (stderr, "\n LSF: ");
      for (i = 0; i < 30; i++)
      {
        if (i == 15) fprintf (stderr, "\n      ");
        fprintf (stderr, "[%02X]", lsf_packed[i]);
      }
    }

    memset (state->m17_lsf, 0, sizeof(state->m17_lsf));

    //debug
    // fprintf (stderr, " E-%04X; C-%04X (CRC CHK)", crc_ext, crc_cmp);

    if (crc_err == 1) fprintf (stderr, " EMB LSF CRC ERR");
  }

  return err;
}

void M17processCodec2_1600(dsd_opts * opts, dsd_state * state, uint8_t * payload)
{

  int i;
  unsigned char voice1[8];
  unsigned char voice2[8];

  for (i = 0; i < 8; i++)
  {
    voice1[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+0], 8);
    voice2[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+64], 8);
  }

  //TODO: Add some decryption methods
  if (state->m17_enc != 0)
  {
    //process scrambler or AES-CTR decryption 
    //(no AES-CTR right now, Scrambler should be easy enough)
  }

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n CODEC2: ");
    for (i = 0; i < 8; i++)
      fprintf (stderr, "%02X", voice1[i]);
    fprintf (stderr, " (1600)");

    fprintf (stderr, "\n A_DATA: "); //arbitrary data
    for (i = 0; i < 8; i++)
      fprintf (stderr, "%02X", voice2[i]);
  }
  
  #ifdef USE_CODEC2
  size_t nsam;
  nsam = 320;

  //converted to using allocated memory pointers to prevent the overflow issues
  short * samp1 = malloc (sizeof(short) * nsam);
  short * upsamp = malloc (sizeof(short) * nsam * 6);
  short * out = malloc (sizeof(short) * 6);
  short prev;
  int j;

  codec2_decode(state->codec2_1600, samp1, voice1);

  if (opts->audio_out_type == 0 && state->m17_enc == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, samp1, nsam*2, NULL);
  }

  if (opts->audio_out_type == 8 && state->m17_enc == 0) //UDP Audio
  {
    udp_socket_blaster (opts, state, nsam*2, samp1);
  }
    
  if (opts->audio_out_type == 5 && state->m17_enc == 0) //OSS 48k/1
  {
    //upsample to 48k and then play
    prev = samp1[0];
    for (i = 0; i < 160; i++)
    {
      upsampleS (samp1[i], prev, out);
      for (j = 0; j < 6; j++) upsamp[(i*6)+j] = out[j];
    }
    write (opts->audio_out_fd, upsamp, nsam*2*6);

  }

  if (opts->audio_out_type == 1 && state->m17_enc == 0) //STDOUT
  {
    write (opts->audio_out_fd, samp1, nsam*2);
  }

  if (opts->audio_out_type == 2 && state->m17_enc == 0) //OSS 8k/1
  {
    write (opts->audio_out_fd, samp1, nsam*2);
  }

  //WIP: Wav file saving -- still need a way to open/close/label wav files similar to call history
  if(opts->wav_out_f != NULL && state->m17_enc == 0) //WAV
  {
    sf_write_short(opts->wav_out_f, samp1, nsam);
  }

  //TODO: Codec2 Raw file saving
  // if(mbe_out_dir)
  // {

  // }

  free (samp1);
  free (upsamp);
  free (out);

  #endif

}

void M17processCodec2_3200(dsd_opts * opts, dsd_state * state, uint8_t * payload)
{
  int i;
  unsigned char voice1[8];
  unsigned char voice2[8];

  for (i = 0; i < 8; i++)
  {
    voice1[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+0], 8);
    voice2[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+64], 8);
  }

  //TODO: Add some decryption methods
  if (state->m17_enc != 0)
  {
    //process scrambler or AES-CTR decryption 
    //(no AES-CTR right now, Scrambler should be easy enough)
  }

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n CODEC2: ");
    for (i = 0; i < 8; i++)
      fprintf (stderr, "%02X", voice1[i]);
    fprintf (stderr, " (3200)");

    fprintf (stderr, "\n CODEC2: ");
    for (i = 0; i < 8; i++)
      fprintf (stderr, "%02X", voice2[i]);
    fprintf (stderr, " (3200)");
  }
  
  #ifdef USE_CODEC2
  size_t nsam;
  nsam = 160;

  //converted to using allocated memory pointers to prevent the overflow issues
  short * samp1 = malloc (sizeof(short) * nsam);
  short * samp2 = malloc (sizeof(short) * nsam);
  short * upsamp = malloc (sizeof(short) * nsam * 6);
  short * out = malloc (sizeof(short) * 6);
  short prev;
  int j;

  codec2_decode(state->codec2_3200, samp1, voice1);
  codec2_decode(state->codec2_3200, samp2, voice2);

  if (opts->audio_out_type == 0 && state->m17_enc == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, samp1, nsam*2, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, samp2, nsam*2, NULL);
  }

  if (opts->audio_out_type == 8 && state->m17_enc == 0) //UDP Audio
  {
    udp_socket_blaster (opts, state, nsam*2, samp1);
    udp_socket_blaster (opts, state, nsam*2, samp2);
  }
    
  if (opts->audio_out_type == 5 && state->m17_enc == 0) //OSS 48k/1
  {
    //upsample to 48k and then play
    prev = samp1[0];
    for (i = 0; i < 160; i++)
    {
      upsampleS (samp1[i], prev, out);
      for (j = 0; j < 6; j++) upsamp[(i*6)+j] = out[j];
    }
    write (opts->audio_out_fd, upsamp, nsam*2*6);
    prev = samp2[0];
    for (i = 0; i < 160; i++)
    {
      upsampleS (samp2[i], prev, out);
      for (j = 0; j < 6; j++) upsamp[(i*6)+j] = out[j];
    }
    write (opts->audio_out_fd, upsamp, nsam*2*6);
  }

  if (opts->audio_out_type == 1 && state->m17_enc == 0) //STDOUT
  {
    write (opts->audio_out_fd, samp1, nsam*2);
    write (opts->audio_out_fd, samp2, nsam*2);
  }

  if (opts->audio_out_type == 2 && state->m17_enc == 0) //OSS 8k/1 
  {
    write (opts->audio_out_fd, samp1, nsam*2);
    write (opts->audio_out_fd, samp2, nsam*2);
  }

  //WIP: Wav file saving -- still need a way to open/close/label wav files similar to call history
  if(opts->wav_out_f != NULL && state->m17_enc == 0) //WAV
  {
    sf_write_short(opts->wav_out_f, samp1, nsam);
    sf_write_short(opts->wav_out_f, samp2, nsam);
  }

  //TODO: Codec2 Raw file saving
  // if(mbe_out_dir)
  // {

  // }

  free (samp1);
  free (samp2);
  free (upsamp);
  free (out);

  #endif

}

void M17prepareStream(dsd_opts * opts, dsd_state * state, uint8_t * m17_bits)
{

  int i, k, x; 
  uint8_t m17_punc[275]; //25 * 11 = 275
  memset (m17_punc, 0, sizeof(m17_punc));
  for (i = 0; i < 272; i++)
    m17_punc[i] = m17_bits[i+96];

  //depuncture the bits
  uint8_t m17_depunc[300]; //25 * 12 = 300
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
  uint8_t trellis_buf[144];
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

  //insert fn bits into meta 14 and meta 15 for Initialization Vector
  state->m17_meta[14] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[1], 7);
  state->m17_meta[15] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[8], 8);

  if (opts->payload == 1)
    fprintf (stderr, " FSN: %05d", fn);

  if (end == 1)
    fprintf (stderr, " END;");

  for (i = 0; i < 128; i++)
    payload[i] = trellis_buf[i+16];

  if (state->m17_str_dt == 2)
    M17processCodec2_3200(opts, state, payload);
  else if (state->m17_str_dt == 3)
    M17processCodec2_1600(opts, state, payload);
  else if (state->m17_str_dt == 1) fprintf (stderr, " DATA;");
  else if (state->m17_str_dt == 0) fprintf (stderr, "  RES;");
  // else                             fprintf (stderr, "  UNK;");

  if (opts->payload == 1 && state->m17_str_dt < 2)
  {
    fprintf (stderr, "\n STREAM: ");
    for (i = 0; i < 18; i++) 
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8));
  }

}

void processM17STR(dsd_opts * opts, dsd_state * state)
{

  int i, x;
  //overflow/memory issue returns in Cygwin for...reasons...
  uint8_t dbuf[384]; //384-bit frame - 16-bit (8 symbol) sync pattern (184 dibits)
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

  //load dibits into dibit buffer
  for (i = 0; i < 184; i++)
    dbuf[i] = (uint8_t) getDibit(opts, state);

  //convert dbuf into a bit array
  for (i = 0; i < 184; i++)
  {
    m17_rnd_bits[i*2+0] = (dbuf[i] >> 1) & 1;
    m17_rnd_bits[i*2+1] = (dbuf[i] >> 0) & 1;
  }

  //descramble the frame
  for (i = 0; i < 368; i++)
    m17_int_bits[i] = (m17_rnd_bits[i] ^ m17_scramble[i]) & 1;

  //deinterleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
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

  // if (lich_err != 0) state->lastsynctype = -1; //disable

  //ending linebreak
  fprintf (stderr, "\n");

} //end processM17STR

void processM17LSF(dsd_opts * opts, dsd_state * state)
{
  //WIP: Switch to libM17 Viterbi Decoder for better performance
  //Not yet working on actual signal, but debug version works
  //most likely frame sync / demodulation related issue

  //NOTE: Viterbi Decoder seems to respnd better 
  //when the 112-bit nonce value isn't all zeroes

  //TODO: Work on implementing a symbol buffer of some sort 
  //when the preamble starts? 
  
  int i, j, k, x;
  uint8_t dbuf[184]; //384-bit (192 symbol) frame - 16-bit (8 symbol) sync pattern (184 dibits)
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_rnd_bits[368]; //368 bits that are still scrambled
  uint16_t m17_bits[368];    //368 bits that have been de-interleaved and de-scrambled
  uint16_t m17_depunc[488]; //488 weighted byte representation of bits after depuncturing
  uint8_t lsf_bytes[31];
  uint8_t lsf_packed[30];
  uint32_t v_err = 0; //errors in viterbi decoder
  UNUSED(v_err);

  memset (dbuf, 0, sizeof(dbuf));
  memset (state->m17_lsf, 0, sizeof(state->m17_lsf));
  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));
  memset (m17_rnd_bits, 0, sizeof(m17_rnd_bits));
  memset (m17_depunc, 0, sizeof(m17_depunc));
  memset (lsf_packed, 0, sizeof(lsf_packed));
  memset (lsf_bytes, 0, sizeof(lsf_bytes));

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

  //deinterleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368;
    m17_bits[i] = m17_int_bits[x];
  }

  //P1 Depuncture & Add Weights
  x = 0;
  for (i = 0; i < 488; i++)
  {
    if (p1[i%61] == 1)
      m17_depunc[i] = m17_bits[x++];
    else m17_depunc[i] = 0;

    if (m17_depunc[i])
      m17_depunc[i] = 0xFFFF;
    else m17_depunc[i] = 0x7FFF;
  }

  //trailing zeroes
  // for (i = 484; i < 488; i++)
  //   m17_depunc[i] = 0x7FFF;

  //debug -- fill all of the byte array
  // memset (m17_depunc, 0xFFFF, sizeof(m17_depunc));

  //debug
  // fprintf (stderr, "\n depunc: \n");
  // for (i = 0; i < 488; i++)
  //   fprintf (stderr, " %04X", m17_depunc[i]);

  //use the libM17 Viterbi Decoder
  uint16_t len = 488;
  v_err = viterbi_decode(lsf_bytes, m17_depunc, len);
  // v_err -= 3932040; //cost negation (double check this as well as unit, meaning, etc)

  //debug
  // fprintf (stderr, "\n lsf_bytes: \n");
  // for (i = 0; i < 31; i++)
  //   fprintf (stderr, " %02X", lsf_bytes[i]);

  //copy + left shift one octet
  memcpy (lsf_packed, lsf_bytes+1, 30);

  //Unpack bytes into m17_lsf bits
  k = 0;
  for (j = 0; j < 30; j++)
  {
    for (i = 0; i < 8; i++)
      state->m17_lsf[k++] = (lsf_packed[j] >> 7-i) & 1;
  }

  uint16_t crc_cmp = crc16m17(lsf_packed, 28);
  uint16_t crc_ext = (lsf_packed[28] << 8) + (lsf_packed[29] << 0);
  int crc_err = 0;

  if (crc_cmp != crc_ext) crc_err = 1;

  if (crc_err == 0)
    M17decodeLSF(state);

  else if (opts->aggressive_framesync == 0)
      M17decodeLSF(state);

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n LSF: ");
    for (i = 0; i < 30; i++)
    {
      if (i == 15) fprintf (stderr, "\n      ");
      fprintf (stderr, "[%02X]", lsf_packed[i]);
    }
    // fprintf (stderr, " V Err: %d", v_err);
  }

  //debug
  // fprintf (stderr, " E-%04X; C-%04X (CRC CHK)", crc_ext, crc_cmp);

  if (crc_err == 1) fprintf (stderr, " CRC ERR");

  //zero out after decoding
  memset (state->m17_lsf, 0, sizeof(state->m17_lsf));

  //ending linebreak
  fprintf (stderr, "\n");

} //end processM17LSF

//This version is the older method, skipping the depuncturing and just doing deconv
void processM17LSF_debug(dsd_opts * opts, dsd_state * state, uint8_t * m17_depunc)
{

  //NOTE: This version has been stripped down, but will not be used,
  //but will keep in case of future debug for ease of use

  int i, x; UNUSED(x);
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_bits[368]; //368 bits that have been de-interleaved and de-scramble

  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));

  //setup the convolutional decoder
  uint8_t temp[488];
  uint8_t s0;
  uint8_t s1;
  uint8_t m_data[30];
  uint8_t trellis_buf[240]; //30*8 = 240
  memset (trellis_buf, 0, sizeof(trellis_buf));
  memset (temp, 0, sizeof (temp));
  memset (m_data, 0, sizeof (m_data));

  for (i = 0; i < 488; i++)
    temp[i] = m17_depunc[i] << 1; 

  CNXDNConvolution_start();
  for (i = 0; i < 244; i++)
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 240);

  //244/8 = 30, last 4 (244-248) are trailing zeroes
  for(i = 0; i < 30; i++)
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

  memset (state->m17_lsf, 0, sizeof(state->m17_lsf));
  memcpy (state->m17_lsf, trellis_buf, 240);

  uint8_t lsf_packed[30];
  memset (lsf_packed, 0, sizeof(lsf_packed));

  //need to pack bytes for the sw5wwp variant of the crc (might as well, may be useful in the future)
  for (i = 0; i < 30; i++)
    lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[i*8], 8);

  uint16_t crc_cmp = crc16m17(lsf_packed, 28);
  uint16_t crc_ext = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[224], 16);
  int crc_err = 0;

  if (crc_cmp != crc_ext) crc_err = 1;

  if (crc_err == 0)
    M17decodeLSF(state);
  else if (opts->aggressive_framesync == 0)
      M17decodeLSF(state);

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n LSF: ");
    for (i = 0; i < 30; i++)
    {
      if (i == 15) fprintf (stderr, "\n      ");
      fprintf (stderr, "[%02X]", lsf_packed[i]);
    }
  }

  //debug
  // fprintf (stderr, " E-%04X; C-%04X (CRC CHK)", crc_ext, crc_cmp);

  if (crc_err == 1) fprintf (stderr, " CRC ERR");

} //end processM17LSF_debug

void processM17LSF_debug2(dsd_opts * opts, dsd_state * state, uint8_t * m17_rnd_bits)
{
  //Working: Switched to libM17 Viterbi Decoder
  int i, j, k, x;
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint16_t m17_bits[368];    //368 bits that have been de-interleaved and de-scrambled
  uint16_t m17_depunc[488]; //488 weighted byte representation of bits after depuncturing
  uint8_t lsf_bytes[31];
  uint8_t lsf_packed[30];
  uint32_t v_err = 0; //errors in viterbi decoder
  UNUSED(v_err);

  memset (state->m17_lsf, 0, sizeof(state->m17_lsf));
  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));
  memset (m17_depunc, 0, sizeof(m17_depunc));
  memset (lsf_packed, 0, sizeof(lsf_packed));
  memset (lsf_bytes, 0, sizeof(lsf_bytes));

  //descramble the frame
  for (i = 0; i < 368; i++)
    m17_int_bits[i] = (m17_rnd_bits[i] ^ m17_scramble[i]) & 1;

  //deinterleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368;
    m17_bits[i] = m17_int_bits[x];
  }

  //P1 Depuncture & Add Weights
  x = 0;
  for (i = 0; i < 488; i++)
  {
    if (p1[i%61] == 1)
      m17_depunc[i] = m17_bits[x++];
    else m17_depunc[i] = 0;

    if (m17_depunc[i])
      m17_depunc[i] = 0xFFFF;
    else m17_depunc[i] = 0x7FFF;
  }

  //debug -- fill all of the byte array
  // memset (m17_depunc, 0xFFFF, sizeof(m17_depunc));

  //debug
  // fprintf (stderr, "\n depunc: \n");
  // for (i = 0; i < 488; i++)
  //   fprintf (stderr, " %04X", m17_depunc[i]);

  //use the libM17 Viterbi Decoder
  uint16_t len = 488;
  v_err = viterbi_decode(lsf_bytes, m17_depunc, len);
  // v_err -= 3932040; //cost negation (double check this as well as unit, meaning, etc)

  //debug
  // fprintf (stderr, "\n lsf_bytes: \n");
  // for (i = 0; i < 31; i++)
  //   fprintf (stderr, " %02X", lsf_bytes[i]);

  //copy + left shift one octet
  memcpy (lsf_packed, lsf_bytes+1, 30);

  //Unpack bytes into m17_lsf bits
  k = 0;
  for (j = 0; j < 30; j++)
  {
    for (i = 0; i < 8; i++)
      state->m17_lsf[k++] = (lsf_packed[j] >> 7-i) & 1;
  }

  uint16_t crc_cmp = crc16m17(lsf_packed, 28);
  uint16_t crc_ext = (lsf_packed[28] << 8) + (lsf_packed[29] << 0);
  int crc_err = 0;

  if (crc_cmp != crc_ext) crc_err = 1;

  if (crc_err == 0)
    M17decodeLSF(state);
  else if (opts->aggressive_framesync == 0)
      M17decodeLSF(state);

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n LSF: ");
    for (i = 0; i < 30; i++)
    {
      if (i == 15) fprintf (stderr, "\n      ");
      fprintf (stderr, "[%02X]", lsf_packed[i]);
    }
    // fprintf (stderr, " V Err: %d", v_err);
  }

  //debug
  // fprintf (stderr, " E-%04X; C-%04X (CRC CHK)", crc_ext, crc_cmp);

  if (crc_err == 1) fprintf (stderr, " CRC ERR");

  //ending linebreak
  // fprintf (stderr, "\n");

} //end processM17LSF_debug2

//debug M17STR for the encoder to pass bits into
void processM17STR_debug(dsd_opts * opts, dsd_state * state, uint8_t * m17_rnd_bits)
{

  int i, x;
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_bits[368]; //368 bits that have been de-interleaved and de-scrambled
  uint8_t lich_bits[96];
  int lich_err = -1;

  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));
  memset (lich_bits, 0, sizeof(lich_bits));

  //descramble the frame
  for (i = 0; i < 368; i++)
    m17_int_bits[i] = (m17_rnd_bits[i] ^ m17_scramble[i]) & 1;

  //deinterleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
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


} //end processM17STR_debug

//simple convolutional encoder
void simple_conv_encoder (uint8_t * input, uint8_t * output, int len)
{
  int i, k = 0;
  uint8_t g1 = 0;
  uint8_t g2 = 0;
  uint8_t d  = 0;
  uint8_t d1 = 0;
  uint8_t d2 = 0;
  uint8_t d3 = 0;
  uint8_t d4 = 0;

  for (i = 0; i < len; i++)
  {
    d = input[i];

    g1 = (d + d3 + d4) & 1;
    g2 = (d + d1 + d2 + d4) & 1;

    d4 = d3;
    d3 = d2;
    d2 = d1;
    d1 = d;

    output[k++] = g1;
    output[k++] = g2;
  }
}

//dibits-symbols map
const int8_t symbol_map[4] = {+1, +3, -1, -3};

//sample RRC filter for 48kHz sample rate
//alpha=0.5, span=8, sps=10, gain=sqrt(sps)
float m17_rrc[81] =
{
	-0.003195702904062073f, -0.002930279157647190f, -0.001940667871554463f,
	-0.000356087678023658f,  0.001547011339077758f,  0.003389554791179751f,
	 0.004761898604225673f,  0.005310860846138910f,  0.004824746306020221f,
	 0.003297923526848786f,  0.000958710871218619f, -0.001749908029791816f,
	-0.004238694106631223f, -0.005881783042101693f, -0.006150256456781309f,
	-0.004745376707651645f, -0.001704189656473565f,  0.002547854551539951f,
	 0.007215575568844704f,  0.011231038205363532f,  0.013421952197060707f,
	 0.012730475385624438f,  0.008449554307303753f,  0.000436744366018287f,
	-0.010735380379191660f, -0.023726883538258272f, -0.036498030780605324f,
	-0.046500883189991064f, -0.050979050575999614f, -0.047340680079891187f,
	-0.033554880492651755f, -0.008513823955725943f,  0.027696543159614194f,
	 0.073664520037517042f,  0.126689053778116234f,  0.182990955139333916f,
	 0.238080025892859704f,  0.287235637987091563f,  0.326040247765297220f,
	 0.350895727088112619f,  0.359452932027607974f,  0.350895727088112619f,
	 0.326040247765297220f,  0.287235637987091563f,  0.238080025892859704f,
	 0.182990955139333916f,  0.126689053778116234f,  0.073664520037517042f,
	 0.027696543159614194f, -0.008513823955725943f, -0.033554880492651755f,
	-0.047340680079891187f, -0.050979050575999614f, -0.046500883189991064f,
	-0.036498030780605324f, -0.023726883538258272f, -0.010735380379191660f,
	 0.000436744366018287f,  0.008449554307303753f,  0.012730475385624438f,
	 0.013421952197060707f,  0.011231038205363532f,  0.007215575568844704f,
	 0.002547854551539951f, -0.001704189656473565f, -0.004745376707651645f,
	-0.006150256456781309f, -0.005881783042101693f, -0.004238694106631223f,
	-0.001749908029791816f,  0.000958710871218619f,  0.003297923526848786f,
	 0.004824746306020221f,  0.005310860846138910f,  0.004761898604225673f,
	 0.003389554791179751f,  0.001547011339077758f, -0.000356087678023658f,
	-0.001940667871554463f, -0.002930279157647190f, -0.003195702904062073f
};

//convert bit array into symbols and RF/Audio
void encodeM17RF (dsd_opts * opts, dsd_state * state, uint8_t * input, int type)
{

  //NOTE: Reorganized type numbers as following:
  //Single Digit numbers 1,2,3,4 are LSF, STR, PKT, and BRT
  //Double Digit numbers 11,33,55,99 are preamble, EOT, or Dead Air

  int i, j, k, x;

  //Preamble A - 0x7777 (+3, -3, +3, -3, +3, -3, +3, -3)
  uint8_t m17_preamble_a[16] = {0,1,1,1, 0,1,1,1, 0,1,1,1, 0,1,1,1};

  //Preamble B - 0xEEEE (-3, +3, -3, +3, -3, +3, -3, +3)
  uint8_t m17_preamble_b[16] = {1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0};

  //EOT Marker - 0x555D
  uint8_t m17_eot_marker[16] = {0,1,0,1, 0,1,0,1, 0,1,0,1, 1,1,0,1};

  //LSF frame sync pattern - 0x55F7 +3, +3, +3, +3, -3, -3, +3, -3
  uint8_t m17_lsf_fs[16] = {0,1,0,1, 0,1,0,1, 1,1,1,1, 0,1,1,1};

  //STR frame sync pattern - 0xFF5D (-3, -3, -3, -3, +3, +3, -3, +3)
  uint8_t m17_str_fs[16] = {1,1,1,1, 1,1,1,1 ,0,1,0,1, 1,1,0,1};

  //PKT frame sync pattern - 0x75FF (-3, -3, -3, -3, +3, +3, -3, +3)
  uint8_t m17_pkt_fs[16] = {0,1,1,1, 0,1,0,1,1,1,1,1,1,1,1,1};

  //BRT frame sync pattern - 0xDF55 (-3, +3, -3, -3, +3, +3, +3, +3)
  uint8_t m17_brt_fs[16] = {1,1,0,1, 1,1,1,1, 0,1,0,1, 0,1,0,1};

  //load bits into a dibit array plus the framesync bits
  uint8_t output_dibits[192]; memset (output_dibits, 0, sizeof(output_dibits));

  //Preamble (just repeat the preamble 12 times to make 192 symbols)
  if (type == 11) //A Pattern prepends LSF (last symbol opposite of first symbol to prevent zero-crossing)
  {
    for (i = 0; i < 192; i++)
      output_dibits[i] = (m17_preamble_a[ (i*2+0)%16 ] << 1) + (m17_preamble_a[ (i*2+1)%16 ] << 0);
  }

  //Preamble (just repeat the preamble 12 times to make 192 symbols)
  if (type == 33) //B Pattern prepends BRT (last symbol opposite of first symbol to prevent zero-crossing)
  {
    for (i = 0; i < 192; i++)
      output_dibits[i] = (m17_preamble_b[ (i*2+0)%16 ] << 1) + (m17_preamble_b[ (i*2+1)%16 ] << 0);
  }

  //EOT Marker (just repeat the EOT marker 12 times to make 192 symbols)
  if (type == 55)
  {
    for (i = 0; i < 192; i++)
      output_dibits[i] = (m17_eot_marker[ (i*2+0)%16 ] << 1) + (m17_eot_marker[ (i*2+1)%16 ] << 0);
  }

  //load frame sync pattern
  if (type == 1) //LSF
  {
    for (i = 0; i < 8; i++)
      output_dibits[i] = (m17_lsf_fs[i*2+0] << 1) + (m17_lsf_fs[i*2+1] << 0);
  }

  if (type == 2) //Stream
  {
    for (i = 0; i < 8; i++)
      output_dibits[i] = (m17_str_fs[i*2+0] << 1) + (m17_str_fs[i*2+1] << 0);
  }

  if (type == 3) //BRT
  {
    for (i = 0; i < 8; i++)
      output_dibits[i] = (m17_brt_fs[i*2+0] << 1) + (m17_brt_fs[i*2+1] << 0);
  }

  if (type == 4) //PKT
  {
    for (i = 0; i < 8; i++)
      output_dibits[i] = (m17_pkt_fs[i*2+0] << 1) + (m17_pkt_fs[i*2+1] << 0);
  }

  //load rest of frame (if not preamble, EOT marker, or dead air)
  if (type < 5)
  {
    for (i = 0; i < 184; i++)
      output_dibits[i+8] = (input[i*2+0] << 1) + (input[i*2+1] << 0);
  }

  //convert to symbols
  int output_symbols[192]; memset (output_symbols, 0, 192*sizeof(int));
  for (i = 0; i < 192; i++)
    output_symbols[i] = symbol_map[output_dibits[i]];

  //debug output symbols
  // fprintf (stderr, "\n sym:");
  // for (i = 0; i < 192; i++)
  // {
  //   if (i%24 == 0) fprintf (stderr, "\n");
  //   fprintf (stderr, " %d", output_symbols[i]);
  // }

  //symbols to audio

  //upsample 10x
  int output_up[192*10]; memset (output_up, 0, 192*10*sizeof(int));
  for (i = 0; i < 192; i++)
  {
    for (j = 0; j < 10; j++)
      output_up[(i*10)+j] = output_symbols[i];
  }

  //debug upsample
  // fprintf (stderr, "\n up:");
  // for (i = 0; i < 1920; i++)
  // {
  //   if (i%24 == 0) fprintf (stderr, "\n");
  //   fprintf (stderr, " %d", output_up[i]);
  // }

  //craft baseband with deviation + filter
  short baseband[1920]; memset (baseband, 0, 1920*sizeof(short));

  //simple, no filtering
  if (opts->use_cosine_filter == 0)
  {
    for (i = 0; i < 1920; i++)
      baseband[i] = output_up[i] * 7168.0f;
  }
  
  //version w/ filtering lifted from M17_Implementations / libM17
  else if (opts->use_cosine_filter == 1)
  {
    float mem[81]; memset (mem, 0.0f, 81*sizeof(float));
    float mac = 0.0f;
    x = 0;
    for (i = 0; i < 192; i++)
    {
      mem[0] = (float)output_symbols[i] * 7168.0f;

      for (j = 0; j < 10; j++)
      {

        mac = 0.0f;

        //calc the sum of products
        for (k = 0; k < 81; k++)
          mac += mem[k]*m17_rrc[k]*sqrtf(10.0);

        //shift the delay line right by 1
        for (k = 80; k > 0; k--)
          mem[k] = mem[k-1];

        mem[0] = 0.0f;

        baseband[x++] = (short)mac;
      }

    }
  }

  //debug baseband
  // fprintf (stderr, "\n bb:");
  // for (i = 0; i < 1920; i++)
  // {
  //   if (i%24 == 0) fprintf (stderr, "\n");
  //   fprintf (stderr, " %d", baseband[i]);
  // }

  //debug insert random 'noise' into the baseband audio
  // for (i = 0; i < 40; i++)
  //   baseband[i] -= rand()& 0x7FFF;

  //dead air type, output to all enabled formats zero sample to simulate dead air
  //NOTE: 25 rounds is approximately 1 second even, seems optimal
  if (type == 99)
  {
    memset (output_dibits, 0xFF, sizeof(output_dibits)); //NOTE: 0xFF works better on bin files
    memset (baseband, 0, 1920*sizeof(short));
  }

  //save symbols (dibits, actually) to symbol capture bin file format
  if (opts->symbol_out_f) //use -c output.bin to use this format (default type for DSD-FME)
  {
    for (i = 0; i < 192; i++)
      fputc (output_dibits[i], opts->symbol_out_f);
  }

  /* //reference from m17-packet-encode.c
  //standard mode - int16 symbol stream
    else if(out_type==1)
    {
        for(uint16_t i=0; i<pkt_sym_cnt; i++)
        {
            int16_t val=full_packet[i];
            fwrite(&val, 2, 1, fp);
        }
    }
  */

  //save symbol stream format (M17_Implementations)
  if (opts->use_dsp_output) //use -Q output.bin to use this format, will be placed in the DSP folder (reusing DSP)
  {
    FILE * pFile; //file pointer
    pFile = fopen (opts->dsp_out_file, "a"); //append, not write
    int16_t val = 0;
    for (i = 0; i < 192; i++)
    {
      val = (int16_t)output_symbols[i];
      fwrite(&val, 2, 1, pFile);
    }
    fclose(pFile);
  }

  //playing back signal audio into device/udp
  //NOTE: Open the analog output device, use -8
  if (opts->monitor_input_audio == 1)
  {
    //Pulse Audio
    if (opts->audio_out_type == 0)
      pa_simple_write(opts->pulse_raw_dev_out, baseband, 1920*2, NULL);
    
    //UDP
    if (opts->audio_out_type == 8)
      udp_socket_blasterA (opts, state, 1920*2, baseband);

    //STDOUT or OSS 48k/1
    if (opts->audio_out_type == 1 || opts->audio_out_type == 5)
      write (opts->audio_out_fd, baseband, 1920*2);

  }
  
  //if we have a raw signal wav file, write to it now
  if (opts->wav_out_raw != NULL)
  {
    sf_write_short(opts->wav_out_raw, baseband, 1920);
    sf_write_sync (opts->wav_out_raw);
  }

  //NOTE: Internal voice decoding is disabled when tx audio over a hardware device, wav/bin still enabled
  UNUSED(state);

}

//encode and create audio of a Project M17 Stream signal
void encodeM17STR(dsd_opts * opts, dsd_state * state)
{

  //WIP: IP Frame Things and User Variables for Reflectors, etc
  uint8_t nil[368]; //empty array to send to RF during Preamble, EOT Marker, or Dead Air
  memset (nil, 0, sizeof(nil));

  //Enable frame, TX and Ncurses Printer
  opts->frame_m17 = 1;
  state->m17encoder_tx = 1;
  
  if (opts->use_ncurses_terminal == 1)
    ncursesOpen(opts, state);

  //User Defined Variables
  int use_ip = 0; //1 to enable IP Frame Broadcast over UDP
  int udpport = 17000; //port number for M17 IP Frmae (default is 17000)
  sprintf (opts->m17_hostname, "%s", "127.0.0.1"); //enter IP address or hostname here
  uint8_t reflector_module = 0x41; //'A', single letter reflector module A-Z, 0x41 is A
  uint8_t can = 7; //channel access number
  //numerical representation of dst and src after b40 encoding, or special/reserved value
  unsigned long long int dst = 0;
  unsigned long long int src = 0;
  //DST and SRC Callsign Data (pick up to 9 characters from the b40 char array)
  char d40[50] = "DSD-FME  "; //DST
  char s40[50] = "DSD-FME  "; //SRC
  //end User Defined Variables

  //configure User Defined Variables, if defined at CLI
  if (state->m17_can_en != -1) //has a set value
    can = state->m17_can_en;

  if (state->str50c[0] != 0)
    sprintf (s40, "%s", state->str50c);

  if (state->str50b[0] != 0)
    sprintf (d40, "%s", state->str50b);

  //if special values, then assign them
  if (strcmp (d40, "BROADCAST") == 0)
    dst = 0xFFFFFFFFFFFF;
  if (strcmp (d40, "ALL") == 0)
    dst = 0xFFFFFFFFFFFF;
  //end
  
  int i, j, k, x;    //basic utility counters
  short sample = 0;  //individual audio sample from source
  size_t nsam = 160; //number of samples to be read in (default is for codec2 3200 bps)
  int dec = state->m17_rate / 8000; //number of samples to run before selecting a sample from source input
  int sql_hit = 11; //squelch hits, hit enough, and deactivate vox
  int eot_out =  1; //if we have already sent the eot out once

  //send dead air with type 99
  for (i = 0; i < 25; i++)
    encodeM17RF (opts, state, nil, 99);

  //WIP: Open UDP port to 17000 and see if any other config info is necessary for running standard IP frames, etc,
  //or perhaps just also configure an input format for that ip streaming method, may need to handle UDP control packets (conn, ackn, nack, ping, pong, disc)
  int sock_err;
  if (opts->m17_use_ip == 1)
  {
    opts->m17_portno = udpport;
    sock_err = udp_socket_connectM17(opts, state);
    if (sock_err < 0)
    {
      fprintf (stderr, "Error Configuring UDP Socket for M17 IP Frame :( \n");
      use_ip = 0;
    }
    else use_ip = 1;
  }
  //TODO: See if we need to send a conn and/or receive any ackn/nack/ping/pong commands later

  //WIP: Standard IP Framing
  uint8_t magic[4] = {0x4D, 0x31, 0x37, 0x20};
  uint8_t ackn[4]  = {0x41, 0x43, 0x4B, 0x4E}; UNUSED(ackn);
  uint8_t nack[4]  = {0x4E, 0x41, 0x43, 0x4B}; UNUSED(nack);
  uint8_t conn[11]; memset (conn, 0, sizeof(conn));
  uint8_t disc[10]; memset (disc, 0, sizeof(disc));
  uint8_t ping[10]; memset (ping, 0, sizeof(ping));
  uint8_t pong[10]; memset (pong, 0, sizeof(pong));
  uint8_t retn[10]; memset (retn, 0, sizeof(retn));
  int udp_return = 0; UNUSED(udp_return);
  uint8_t sid[2];   memset (sid, 0, sizeof(sid));

  //NONCE
  time_t ts = time(NULL); //timestamp since epoch / "Unix Time"
  srand(ts); //randomizer seed based on timestamp

  //Stream ID value
  sid[0] = rand() & 0xFF;
  sid[1] = rand() & 0xFF;

  //initialize a nonce (if ENC is required in future)
  uint8_t nonce[14]; memset (nonce, 0, sizeof(nonce));
  //32bit LSB of the timestamp
  nonce[0]  = (ts >> 24) & 0xFF;
  nonce[1]  = (ts >> 16) & 0xFF;
  nonce[2]  = (ts >> 8)  & 0xFF;
  nonce[3]  = (ts >> 0)  & 0xFF;
  //64-bit of rnd data
  nonce[4]  = rand() & 0xFF;
  nonce[5]  = rand() & 0xFF;
  nonce[6]  = rand() & 0xFF;
  nonce[7]  = rand() & 0xFF;
  nonce[8]  = rand() & 0xFF;
  nonce[9]  = rand() & 0xFF;
  nonce[10] = rand() & 0xFF;
  nonce[11] = rand() & 0xFF;
  //The last two octets are the CTR_HIGH value (upper 16 bits of the frame number),
  //but you would need to talk non-stop for over 20 minutes to roll it, so just using rnd
  //also, using zeroes seems like it may be a security issue, so using rnd as a base
  nonce[12] = rand() & 0xFF;
  nonce[13] = rand() & 0xFF;

  //debug print nonce
  // fprintf (stderr, " nonce:");
  // for (i = 0; i < 14; i++)
  //   fprintf (stderr, " %02X", nonce[i]);

  #ifdef USE_CODEC2
  nsam = codec2_samples_per_frame(state->codec2_3200);
  #endif

  short * samp1 = malloc (sizeof(short) * nsam);
  short * samp2 = malloc (sizeof(short) * nsam);

  short voice1[nsam]; //read in xxx ms of audio from input source
  short voice2[nsam]; //read in xxx ms of audio from input source
  
  //frame sequence number and eot bit
  uint16_t fsn = 0;
  uint8_t eot = 0;
  uint8_t lich_cnt = 0; //lich frame number counter

  uint8_t lsf_chunk[6][48]; //40 bit chunks of link information spread across 6 frames
  uint8_t m17_lsf[240];    //the complete LSF

  memset (lsf_chunk, 0, sizeof(lsf_chunk));
  memset (m17_lsf, 0, sizeof(m17_lsf));

  //NOTE: Most lich and lsf_chunk bits can be pre-set before the while loop,
  //only need to refresh the lich_cnt value, nonce, and golay
  uint16_t lsf_ps   = 1; //packet or stream indicator bit
  uint16_t lsf_dt   = 2; //voice (3200bps)
  uint16_t lsf_et   = 0; //encryption type
  uint16_t lsf_es   = 0; //encryption sub-type
  uint16_t lsf_cn = can; //can value
  uint16_t lsf_rs   = 0; //reserved bits

  //Viterbi decoder seems to respond better to non-zero fill, so, when no enc is used
  //let's set the lsf_es to 2 for RES, and fill the nonce value with repeating 0x69
  //NOTE: This has seemingly been fixed with the dead air tweaks
  // if (lsf_et == 0)
  // {
  //   lsf_es = 3; //RES
  //   for (i = 0; i < 14; i++)
  //     nonce[i] = 0x69;
  // }

  //compose the 16-bit frame information from the above sub elements
  uint16_t lsf_fi = 0;
  lsf_fi = (lsf_ps & 1) + (lsf_dt << 1) + (lsf_et << 3) + (lsf_es << 5) + (lsf_cn << 7) + (lsf_rs << 11);
  for (i = 0; i < 16; i++) m17_lsf[96+i] = (lsf_fi >> 15-i) & 1;

  //Convert base40 CSD to numerical values (lifted from libM17)

  //Only if not already set to a reserved value
  if (dst < 0xEE6B27FFFFFF)
  {
    for(i = strlen((const char*)d40)-1; i >= 0; i--)
    {
      for(j = 0; j < 40; j++)
      {
        if(d40[i]==b40[j])
        {
          dst=dst*40+j;
          break;
          }
      }
    }
  }

  if (src < 0xEE6B27FFFFFF)
  {
    for(i = strlen((const char*)s40)-1; i >= 0; i--)
    {
      for(j = 0; j < 40; j++)
      {
        if(s40[i]==b40[j])
        {
          src=src*40+j;
          break;
          }
      }
    }
  }
  //end CSD conversion

  //Setup conn, disc, ping, pong values
  conn[0] = 0x43; conn[1] = 0x4F; conn[2] = 0x4E; conn[3] = 0x4E; conn[10] = reflector_module;
  disc[0] = 0x44; disc[1] = 0x49; disc[2] = 0x53; disc[3] = 0x43;
  ping[0] = 0x50; ping[1] = 0x49; ping[2] = 0x4E; ping[3] = 0x47;
  pong[0] = 0x50; pong[1] = 0x4F; pong[2] = 0x4E; pong[3] = 0x47;

  for (i = 0; i < 6; i++)
  {
    conn[i+4] = (src >> 48-(8*i)) & 0xFF;
    disc[i+4] = (src >> 48-(8*i)) & 0xFF;
    ping[i+4] = (src >> 48-(8*i)) & 0xFF;
    pong[i+4] = (src >> 48-(8*i)) & 0xFF;
  }

  //debug
  // fprintf (stderr, " CONN: ");
  // for (i = 0; i < 11; i++)
  //   fprintf (stderr, " %02X", conn[i]);

  //SEND CONN to reflector
  if (use_ip == 1)
    udp_return = m17_socket_blaster (opts, state, 11, conn);

  //WIP: Read UDP ACKN/NACK value, disable use_ip if NULL or nack return
  // if (use_ip && udp_return) //disabled for now, need to look into how to view the return or received value
  // {
  //   retn[0] = (udp_return >> 24) & 0xFF;
  //   retn[1] = (udp_return >> 16) & 0xFF;
  //   retn[2] = (udp_return >> 8)  & 0xFF;
  //   retn[3] = (udp_return >> 0)  & 0xFF;
  //   if (memcmp (nack, retn, 4) == 0) use_ip = 0;

  //   if (use_ip) fprintf (stderr, "\n Connected to reflector successfully!");
  //   else fprintf (stderr, "\n NACK from reflector!");

  //   //debug
  //   fprintf (stderr, " RETURN: %08llX; RETN: ", udp_return);
  //   for (i = 0; i < 4; i++)
  //     fprintf (stderr, " %02X", retn[i]);

  //   memset (retn, 0, sizeof(retn));
  // }
  // else
  // {
  //   use_ip = 0;
  //   fprintf (stderr, "\n No Reply from Reflector!");
  // }

  //load dst and src values into the LSF
  for (i = 0; i < 48; i++) m17_lsf[i] = (dst >> 47ULL-(unsigned long long int)i) & 1;
  for (i = 0; i < 48; i++) m17_lsf[i+48] = (src >> 47ULL-(unsigned long long int)i) & 1;

  //load the nonce from packed bytes to a bitwise iv array
  uint8_t iv[112]; memset(iv, 0, sizeof(iv));
  k = 0;
  for (j = 0; j < 14; j++)
  {
    for (i = 0; i < 8; i++)
      iv[k++] = (nonce[j] >> 7-i)&1;
  }

  //if AES enc employed, insert the iv into LSF
  if (lsf_et == 2)
  {
    for (i = 0; i < 112; i++)
      m17_lsf[i+112] = iv[i];
  }

  //pack and compute the CRC16 for LSF
  uint16_t crc_cmp = 0;
  uint8_t lsf_packed[30];
  memset (lsf_packed, 0, sizeof(lsf_packed));
  for (i = 0; i < 28; i++)
      lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_lsf[i*8], 8);
  crc_cmp = crc16m17(lsf_packed, 28);

  //attach the crc16 bits to the end of the LSF data
  for (i = 0; i < 16; i++) m17_lsf[224+i] = (crc_cmp >> 15-i) & 1;

  //pack the CRC
  for (i = 28; i < 30; i++)
      lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_lsf[i*8], 8);

  //Craft and Send Initial LSF frame to be decoded

  //LSF w/ convolutional encoding (double check array sizes)
  uint8_t m17_lsfc[488]; memset (m17_lsfc, 0, sizeof(m17_lsfc));

  //LSF w/ P1 Puncturing
  uint8_t m17_lsfp[368]; memset (m17_lsfp, 0, sizeof(m17_lsfp));

  //LSF w/ Interleave
  uint8_t m17_lsfi[368]; memset (m17_lsfi, 0, sizeof(m17_lsfi));

  //LSF w/ Scrambling
  uint8_t m17_lsfs[368]; memset (m17_lsfs, 0, sizeof(m17_lsfs));

  //Use the convolutional encoder to encode the LSF Frame
  simple_conv_encoder (m17_lsf, m17_lsfc, 244);

  //P1 puncture
  x = 0;
  for (i = 0; i < 488; i++)
  {
    if (p1[i%61] == 1)
      m17_lsfp[x++] = m17_lsfc[i];
  }

  //interleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368;
    m17_lsfi[x] = m17_lsfp[i];
  }

  //scramble/randomize the frame
  for (i = 0; i < 368; i++)
    m17_lsfs[i] = (m17_lsfi[i] ^ m17_scramble[i]) & 1;

  //flag to determine if we send a new LSF frame for new encode
  //only send once at the appropriate time when encoder is toggled on
  int new_lsf = 1;

  while (!exitflag) //while the software is running
  {

    //if not decoding internally, assign values for ncurses display
    if (opts->monitor_input_audio == 1)
    {
      sprintf (state->m17_src_str, "%s", s40);
      sprintf (state->m17_dst_str, "%s", d40);
      state->m17_src = src;
      state->m17_dst = dst;
      state->m17_can = can;
      state->m17_str_dt = lsf_dt;
      state->m17_enc = lsf_et;
      state->m17_enc_st = lsf_es;
      for (i = 0; i < 16; i++)
        state->m17_meta[i] = 0;
      if (lsf_et == 2)
      {
        for (i = 0; i < 14; i++)
          state->m17_meta[i] = nonce[i];
        for (i = 0; i < 8; i++)
        {
          state->m17_meta[14] <<= 1;
          state->m17_meta[15] <<= 1;
          state->m17_meta[14] += ((fsn >> 7) >> 7-i) & 1;
          state->m17_meta[15] += ((fsn >> 0) >> 7-i) & 1;
        }
      }
    }

    //read some audio samples from source and load them into an audio buffer
    if (opts->audio_in_type == 0) //pulse audio
    {
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
        voice1[i] = sample; //only store the 6th sample
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
        voice2[i] = sample; //only store the 6th sample
      }
    }

    else if (opts->audio_in_type == 1) //stdin
    {
      int result = 0;
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          result = sf_read_short(opts->audio_in_file, &sample, 1);
        voice1[i] = sample;
        if (result == 0)
        {
          sf_close(opts->audio_in_file);
          fprintf (stderr, "Connection to STDIN Disconnected.\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          exitflag = 1;
          break;
        }
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          result = sf_read_short(opts->audio_in_file, &sample, 1);
        voice2[i] = sample;
        if (result == 0)
        {
          sf_close(opts->audio_in_file);
          fprintf (stderr, "Connection to STDIN Disconnected.\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          exitflag = 1;
          break;
        }
      }
    }

    else if (opts->audio_in_type == 5) //OSS
    {
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          read (opts->audio_in_fd, &sample, 2);
        voice1[i] = sample;
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          read (opts->audio_in_fd, &sample, 2);
        voice2[i] = sample;
      }
    }

    else if (opts->audio_in_type == 8) //TCP
    {
      int result = 0;
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          result = sf_read_short(opts->tcp_file_in, &sample, 1);
        voice1[i] = sample;
        if (result == 0)
        {
          sf_close(opts->tcp_file_in);
          fprintf (stderr, "Connection to TCP Server Disconnected.\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          exitflag = 1;
          break;
        }
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          result = sf_read_short(opts->tcp_file_in, &sample, 1);
        voice2[i] = sample;
        if (result == 0)
        {
          sf_close(opts->tcp_file_in);
          fprintf (stderr, "Connection to TCP Server Disconnected.\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          exitflag = 1;
          break;
        }
      }
    }

    else if (opts->audio_in_type == 3) //RTL
    {
      #ifdef USE_RTLSDR
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          if (get_rtlsdr_sample(&sample, opts, state) < 0)
            cleanupAndExit(opts, state);
        voice1[i] = sample;
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < dec; j++)
          if (get_rtlsdr_sample(&sample, opts, state) < 0)
            cleanupAndExit(opts, state);
        voice2[i] = sample;
      }
      
      #endif
    }

    //read in RMS value for vox function; NOTE: may not work correctly on STDIN and TCP due to blocking when no samples to read
    if (opts->audio_in_type == 3) opts->rtl_rms = rtl_return_rms();
    else opts->rtl_rms = raw_rms(voice1, nsam, 1) / 2; //dividing by two so mic isn't so sensitive on vox

    //decimate audio input (default 100% on mic is WAY TOO LOUD for the encoder, fine tune in volume control)
    for (i = 0; i < 160; i++)
    {
      //NOTE: Use + and - in ncurses to fine tune manually
      voice1[i] *= (float)state->aout_gain/ (float)25.0f;
      voice2[i] *= (float)state->aout_gain/ (float)25.0f;
    }

    //run lpf and hpf
    // lpf8 (state, voice1, 160);
    // lpf8 (state, voice2, 160);
    hpf8 (state, voice1, 160);
    hpf8 (state, voice2, 160);

    //convert out audio input into CODEC2 (3200bps) 8 byte data stream
    uint8_t vc1_bytes[8]; memset (vc1_bytes, 0, sizeof(vc1_bytes));
    uint8_t vc2_bytes[8]; memset (vc2_bytes, 0, sizeof(vc2_bytes));

    #ifdef USE_CODEC2
    codec2_encode(state->codec2_3200, vc1_bytes, voice1);
    codec2_encode(state->codec2_3200, vc2_bytes, voice2);
    #endif

    //initialize and start assembling the completed frame

    //Data/Voice Portion of Stream Data Link Layer w/ FSN
    uint8_t m17_v1[148]; memset (m17_v1, 0, sizeof(m17_v1));

    //Data/Voice Portion of Stream Data Link Layer w/ FSN (after Convolutional Encode)
    uint8_t m17_v1c[296]; memset (m17_v1c, 0, sizeof(m17_v1c));

    //Data/Voice Portion of Stream Data Link Layer w/ FSN (after P2 Puncturing)
    uint8_t m17_v1p[272]; memset (m17_v1p, 0, sizeof(m17_v1p));

    //LSF Chunk + LICH CNT of Stream Data Link Layer
    uint8_t m17_l1[48]; memset (m17_l1, 0, sizeof(m17_l1));

    //LSF Chunk + LICH CNT of Stream Data Link Layer (after Golay 24,12 Encoding)
    uint8_t m17_l1g[96]; memset (m17_l1g, 0, sizeof(m17_l1g));

    //Type 4c - Combined LSF Content Chuck and Voice/Data (96 + 272)
    uint8_t m17_t4c[368]; memset (m17_t4c, 0, sizeof(m17_t4c));

    //Type 4i - Interleaved Bits
    uint8_t m17_t4i[368]; memset (m17_t4i, 0, sizeof(m17_t4i));

    //Type 4s - Interleaved Bits with Scrambling Applied
    uint8_t m17_t4s[368]; memset (m17_t4s, 0, sizeof(m17_t4s));

    //insert the voice bytes into voice bits, and voice bits into v1 in their appropriate location
    uint8_t v1_bits[64]; memset (v1_bits, 0, sizeof(v1_bits));
    uint8_t v2_bits[64]; memset (v2_bits, 0, sizeof(v2_bits));

    k = 0; x = 0;
    for (j = 0; j < 8; j++)
    {
      for (i = 0; i < 8; i++)
      {
        v1_bits[k++] = (vc1_bytes[j] >> 7-i) & 1;
        v2_bits[x++] = (vc2_bytes[j] >> 7-i) & 1;
      }
    }

    for (i = 0; i < 64; i++)
    {
      m17_v1[i+16]    = v1_bits[i];
      m17_v1[i+16+64] = v2_bits[i];
    }

    //tally consecutive squelch hits based on RMS value, or reset
    if (opts->rtl_rms > opts->rtl_squelch_level) sql_hit = 0;
    else sql_hit++; //may eventually roll over to 0 again 

    //if vox enabled, toggle tx/eot with sql_hit comparison
    if (state->m17_vox == 1)
    {
      if (sql_hit > 10 && lich_cnt == 0) //licn_cnt 0 to prevent new LSF popping out
      {
        state->m17encoder_tx = 0;
        eot = 1;
      }
      else
      {
        state->m17encoder_tx = 1;
        eot = 0;
      }
    }

    //set end of tx bit on the exitflag (sig, results not gauranteed) or toggle eot flag (always triggers)
    if (exitflag) eot = 1;
    if (state->m17encoder_eot) eot = 1;
    m17_v1[0] = (uint8_t)eot; //set as first bit of the stream

    //set current frame number as bits 1-15 of the v1 stream
    for (i = 0; i < 15; i++)
      m17_v1[i+1] = ( (uint8_t)(fsn >> 14-i) ) &1;

    //Use the convolutional encoder to encode the voice / data stream
    simple_conv_encoder (m17_v1, m17_v1c, 148); //was 144, not 144+4

    //use the P2 puncture to...puncture and collapse the voice / data stream
    k = 0; x = 0;
    for (i = 0; i < 25; i++)
    {
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      //quit early on last set of i when 272 k bits reached 
      //index from 0 to 271,so 272 is breakpoint with k++
      if (k == 272) break;
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      x++;
    }

    //add punctured voice / data bits to the combined frame
    for (i = 0; i < 272; i++)
      m17_t4c[i+96] = m17_v1p[i];

    //load up the lsf chunk for this cnt
    for (i = 0; i < 40; i++)
      lsf_chunk[lich_cnt][i] = m17_lsf[((lich_cnt)*40)+i];

    //update lich_cnt in the current LSF chunk
    lsf_chunk[lich_cnt][40] = (lich_cnt >> 2) & 1;
    lsf_chunk[lich_cnt][41] = (lich_cnt >> 1) & 1;
    lsf_chunk[lich_cnt][42] = (lich_cnt >> 0) & 1;

    //This is not M17 standard, but use the LICH reserved bits to signal can and dt
    lsf_chunk[lich_cnt][43] = (lsf_cn >> 2) & 1;
    lsf_chunk[lich_cnt][44] = (lsf_cn >> 1) & 1;
    lsf_chunk[lich_cnt][45] = (lsf_cn >> 0) & 1;

    lsf_chunk[lich_cnt][46] = (lsf_dt >> 1) & 1;
    lsf_chunk[lich_cnt][47] = (lsf_dt >> 0) & 1;

    //encode with golay 24,12 and load into m17_l1g
    Golay_24_12_encode (lsf_chunk[lich_cnt]+00, m17_l1g+00);
    Golay_24_12_encode (lsf_chunk[lich_cnt]+12, m17_l1g+24);
    Golay_24_12_encode (lsf_chunk[lich_cnt]+24, m17_l1g+48);
    Golay_24_12_encode (lsf_chunk[lich_cnt]+36, m17_l1g+72);

    //add lsf chunk to the combined frame
    for (i = 0; i < 96; i++)
      m17_t4c[i] = m17_l1g[i];

    //interleave the bit array using Quadratic Permutation Polynomial
    //function π(x) = (45x + 92x^2 ) mod 368
    for (i = 0; i < 368; i++)
    {
      x = ((45*i)+(92*i*i)) % 368;
      m17_t4i[x] = m17_t4c[i];
    }

    //scramble/randomize the frame
    for (i = 0; i < 368; i++)
      m17_t4s[i] = (m17_t4i[i] ^ m17_scramble[i]) & 1;

    //debug insert 3 random bit flips in the finished stream to test conv/golay
    // int rnd1 = rand()%368;
    // int rnd2 = rand()%368;
    // int rnd3 = rand()%368;
    // m17_t4s[rnd1] ^= 1;
    // m17_t4s[rnd2] ^= 1;
    // m17_t4s[rnd3] ^= 1;

    //-----------------------------------------

    //decode stream with the M17STR_debug
    if (state->m17encoder_tx == 1) //when toggled on
    {
      //Enable Carrier, synctype, etc
      state->carrier = 1;
      state->synctype = 8;

      //send LSF frame once, if new encode session
      if (new_lsf == 1)
      {

        //debug LSF before (see after in decoder for comparison)
        // fprintf (stderr, "\n lsf: ");
        // for (i = 0; i < 30; i++)
        // {
        //   if ((i%15)==0) fprintf (stderr, "\n");
        //   fprintf (stderr, " %02X", lsf_packed[i]);
        // }

        fprintf (stderr, "\n M17 LSF    (ENCODER): ");
        if (opts->monitor_input_audio == 0)
          processM17LSF_debug2(opts, state, m17_lsfs);
        else fprintf (stderr, " To Audio Out Device Type: %d; ", opts->audio_out_type);

        //convert bit array into symbols and RF/Audio
        memset (nil, 0, sizeof(nil));
        encodeM17RF (opts, state, nil, 11); //Preamble
        // for (i = 0; i < 6; i++) //test sending multiple LSF OTA
        encodeM17RF (opts, state, m17_lsfs, 1); //LSF

        //flag off after sending
        new_lsf = 0;

        //flag to indicate to send one eot afterwards
        eot_out = 0;
      }

      fprintf (stderr, "\n M17 Stream (ENCODER): ");
      if (opts->monitor_input_audio == 0)
        processM17STR_debug(opts, state, m17_t4s);
      else fprintf (stderr, " To Audio Out Device Type: %d; ", opts->audio_out_type);

      //debug RMS Value
      if (state->m17_vox == 1)
      {
        fprintf (stderr, " RMS: %04ld", opts->rtl_rms);
        fprintf (stderr, " SQL HIT: %d;", sql_hit);
      }

      //convert bit array into symbols and RF/Audio
      encodeM17RF (opts, state, m17_t4s, 2);
      
      //Contruct an IP frame using previously created arrays
      uint8_t m17_ip_frame[432]; memset (m17_ip_frame, 0, sizeof(m17_ip_frame));
      uint8_t m17_ip_packed[54]; memset (m17_ip_packed, 0, sizeof(m17_ip_packed));
      uint16_t ip_crc = 0;

      //NOTE: The Manual doesn't say much about this area, so assuming 
      //the elements are arranged as per the table

      //add MAGIC
      k = 0;
      for (j = 0; j < 4; j++)
      {
        for (i = 0; i < 8; i++)
          m17_ip_frame[k++] = (magic[j] >> 7-i) &1;
      }

      //add StreamID
      for (j = 0; j < 2; j++)
      {
        for (i = 0; i < 8; i++)
          m17_ip_frame[k++] = (sid[j] >> 7-i) &1;
      }

      //add the current LSF, sans CRC
      for (i = 0; i < 224; i++)
        m17_ip_frame[k++] = m17_lsf[i];

      //add eot bit flag
      m17_ip_frame[k++] = eot&1;

      //add current fsn value
      for (i = 0; i < 15; i++)
        m17_ip_frame[k++] = (fsn >> 14-i)&1;

      //voice and/or data payload
      for (i = 0; i < 64; i++)
        m17_ip_frame[k++] = v1_bits[i];

      //voice and/or data payload
      for (i = 0; i < 64; i++)
        m17_ip_frame[k++] = v2_bits[i];

      //pack current bit array into a byte array for a CRC check
      for (i = 0; i < 52; i++)
        m17_ip_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_ip_frame[i*8], 8);
      ip_crc = crc16m17(m17_ip_packed, 52);

      //add CRC value to the ip frame
      for (i = 0; i < 16; i++)
        m17_ip_frame[k++] = (ip_crc >> 15-i)&1;
      
      //pack CRC into the byte array as well
      for (i = 52; i < 54; i++)
        m17_ip_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_ip_frame[i*8], 8);

      //debug print packed byte output for IP frame
      // fprintf (stderr, "\n IP: ");
      // for (i = 0; i < 54; i++)
      // {
      //   if ( (i%12)==0) fprintf (stderr, "\n");
      //   fprintf (stderr, " %02X", m17_ip_packed[i]);
      // }

      //WIP: Send packed IP frame to UDP port 17000 if opened successfully
      if (use_ip == 1)
        udp_return = m17_socket_blaster (opts, state, 54, m17_ip_packed);

      //increment lich_cnt, reset on 6
      lich_cnt++;
      if (lich_cnt == 6) lich_cnt = 0;

      //increment frame sequency number, trunc to maximum value, roll nonce if needed
      fsn++;
      if (fsn > 0x7FFF)
      {
        fsn = 0;
        nonce[13]++;
        if (nonce[13] > 0xFF)
        {
          nonce[13] = 0; //roll over to zero of exceeds 0xFF
          nonce[12]++;
          nonce[12] &= 0xFF; //trunc for potential rollover (doesn't spill over)
        }
      }

    } //end if (state->m17encoder_tx)

    else //if not tx, reset values, drop carrier and sync
    {
      lich_cnt = 0;
      fsn = 0;
      state->carrier = 0;
      state->synctype = -1;

      //update timestamp
      ts = time(NULL);

      //update randomizer seed and SID
      srand(ts); //randomizer seed based on time

      //update Stream ID
      sid[0] = rand() & 0xFF;
      sid[1] = rand() & 0xFF;

      //update nonce
      nonce[0]  = (ts >> 24) & 0xFF;
      nonce[1]  = (ts >> 16) & 0xFF;
      nonce[2]  = (ts >> 8)  & 0xFF;
      nonce[3]  = (ts >> 0)  & 0xFF;
      nonce[4]  = rand() & 0xFF;
      nonce[5]  = rand() & 0xFF;
      nonce[6]  = rand() & 0xFF;
      nonce[7]  = rand() & 0xFF;
      nonce[8]  = rand() & 0xFF;
      nonce[9]  = rand() & 0xFF;
      nonce[10] = rand() & 0xFF;
      nonce[11] = rand() & 0xFF;
      nonce[12] = rand() & 0xFF;
      nonce[13] = rand() & 0xFF;

      //debug print nonce
      // fprintf (stderr, "\n nonce:");
      // for (i = 0; i < 14; i++)
      //   fprintf (stderr, " %02X", nonce[i]);

      //Viterbi decoder seems to respond better to non-zero fill, so, when no enc is used
      //let's set the lsf_es to 2 for RES, and fill the nonce value with repeating 0x69
      //NOTE: This has seemingly been fixed with the dead air tweaks
      // if (lsf_et == 0)
      // {
      //   lsf_es = 3; //RES
      //   for (i = 0; i < 14; i++)
      //     nonce[i] = 0x69;
      // }

      //load the nonce from packed bytes to a bitwise iv array
      memset(iv, 0, sizeof(iv));
      k = 0;
      for (j = 0; j < 14; j++)
      {
        for (i = 0; i < 8; i++)
          iv[k++] = (nonce[j] >> 7-i)&1;
      }

      //if AES enc employed, insert the iv into LSF
      if (lsf_et == 2) //disable to allow the 0x69 repeating non-zero fill on RES
      {
        for (i = 0; i < 112; i++) m17_lsf[i+112] = iv[i];
      }

      //repack, new CRC, and update rest of lsf as well
      memset (lsf_packed, 0, sizeof(lsf_packed));
      for (i = 0; i < 28; i++)
        lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_lsf[i*8], 8);
      crc_cmp = crc16m17(lsf_packed, 28);

      //attach the crc16 bits to the end of the LSF data
      for (i = 0; i < 16; i++) m17_lsf[224+i] = (crc_cmp >> 15-i) & 1;

      //repack the CRC
      for (i = 28; i < 30; i++)
          lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_lsf[i*8], 8);

      //Recraft and Prepare New LSF frame for next encoding session
      //this is primarily to make sure the LSF has the refreshed nonce
      memset (m17_lsfc, 0, sizeof(m17_lsfc));
      memset (m17_lsfp, 0, sizeof(m17_lsfp));
      memset (m17_lsfi, 0, sizeof(m17_lsfi));
      memset (m17_lsfs, 0, sizeof(m17_lsfs));

      //Use the convolutional encoder to encode the LSF Frame
      simple_conv_encoder (m17_lsf, m17_lsfc, 244);

      //P1 puncture
      x = 0;
      for (i = 0; i < 488; i++)
      {
        if (p1[i%61] == 1)
          m17_lsfp[x++] = m17_lsfc[i];
      }

      //interleave the bit array using Quadratic Permutation Polynomial
      //function π(x) = (45x + 92x^2 ) mod 368
      for (i = 0; i < 368; i++)
      {
        x = ((45*i)+(92*i*i)) % 368;
        m17_lsfi[x] = m17_lsfp[i];
      }

      //scramble/randomize the frame
      for (i = 0; i < 368; i++)
        m17_lsfs[i] = (m17_lsfi[i] ^ m17_scramble[i]) & 1;

      //flush the last frame with the eot bit on
      if (eot && !eot_out)
      {
        fprintf (stderr, "\n M17 Stream (ENCODER): ");
        if (opts->monitor_input_audio == 0)
          processM17STR_debug(opts, state, m17_t4s);
        else fprintf (stderr, " To Audio Out Device Type: %d; ", opts->audio_out_type);

        //debug RMS Value
        if (state->m17_vox == 1)
        {
          fprintf (stderr, " RMS: %04ld", opts->rtl_rms);
          fprintf (stderr, " SQL HIT: %d;", sql_hit);
        }

        //convert bit array into symbols and RF/Audio
        encodeM17RF (opts, state, m17_t4s, 2); //Last Stream Frame
        memset (nil, 0, sizeof(nil));
        encodeM17RF (opts, state, nil, 55);    //EOT Marker

        //send dead air with type 99
        for (i = 0; i < 25; i++)
          encodeM17RF (opts, state, nil, 99);

        //reset indicators
        eot = 0;
        eot_out = 1;
        state->m17encoder_eot = 0;
      }

      //flag on when restarting the encoder
      new_lsf = 1;

      //flush decoder side meta last, primarily the last two octets with the lich_cnt in them
      memset(state->m17_meta, 0, sizeof(state->m17_meta));

      //flush decoder side lsf, may be redundant, but using to make sure no stale values loaded during debug
      memset(state->m17_lsf, 0, sizeof(state->m17_lsf));

    }

    //refresh ncurses printer, if enabled
    if (opts->use_ncurses_terminal == 1)
      ncursesPrinter(opts, state);
    
  }

  //SEND DISC to reflector
  if (use_ip == 1)
    udp_return = m17_socket_blaster (opts, state, 10, disc);
  
  //free allocated memory
  free(samp1);
  free(samp2);

}

//encode and create audio of a Project M17 BERT signal
void encodeM17BRT(dsd_opts * opts, dsd_state * state)
{

  //NOTE: BERT will not use the nucrses terminal,
  //just strictly for making a BERT test signal
  
  uint8_t nil[368]; //empty array
  memset (nil, 0, sizeof(nil));

  int i, j, k, x; //basic utility counters

  //send dead air with type 99
  for (i = 0; i < 25; i++)
    encodeM17RF (opts, state, nil, 99);

  //send preamble_b for the BERT frame
  encodeM17RF (opts, state, nil, 33);

  //BERT - 197 bits generated from a PRBS9 Generator
  uint8_t m17_b1[201]; memset (m17_b1, 0, sizeof(m17_b1));

  uint16_t lfsr = 1; //starting value of the LFSR
  uint16_t  bit = 1; //result bit of taps XOR
  m17_b1[0] = 1;

  while (!exitflag)
  {

    //Generate sequence (if doing 197 at a time)
    // for (j = 0; j < 197; j++)
    // {
    //   bit = ( (lfsr >> 8) ^ (lfsr ^ 4) ) & 1;
    //   lfsr = (lfsr << 1) | bit;
    //   m17_b1[j] = bit;
    // }

    //BERT (reversed sequence for output)
    uint8_t m17_b1r[208]; memset (m17_b1r, 0, sizeof(m17_b1r));

    //BERT (after Convolutional Encode)
    uint8_t m17_b1c[402]; memset (m17_b1c, 0, sizeof(m17_b1c));

    //BERT (after P2 Puncturing)
    uint8_t m17_b2p[368]; memset (m17_b2p, 0, sizeof(m17_b2p));

    //BERT (Interleaved Bits)
    uint8_t m17_b3i[368]; memset (m17_b3i, 0, sizeof(m17_b3i));

    //BERT (Scrambling Applied)
    uint8_t m17_b4s[368]; memset (m17_b4s, 0, sizeof(m17_b4s));

    simple_conv_encoder (m17_b1, m17_b1c, 201); //197+4

    //use the P2 puncture to...puncture and collapse the BERT Frame
    k = 0; x = 0;
    for (i = 0; i < 34; i++)
    {
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      //quit early on last set of i when 368 k bits reached 
      //index from 0 to 367,so 368 is breakpoint with k++
      if (k == 368) break;
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      m17_b2p[k++] = m17_b1c[x++];
      x++;
    }

    //debug K and X bit positions
    fprintf (stderr, " K: %d; X: %d", k, x);

    //interleave the bit array using Quadratic Permutation Polynomial
    //function π(x) = (45x + 92x^2 ) mod 368
    for (i = 0; i < 368; i++)
    {
      x = ((45*i)+(92*i*i)) % 368;
      m17_b3i[x] = m17_b2p[i];
    }

    //scramble/randomize the frame
    for (i = 0; i < 368; i++)
      m17_b4s[i] = (m17_b3i[i] ^ m17_scramble[i]) & 1;

    //debug insert 3 random bit flip in the finished BERT frame
    // int rnd1 = rand()%368;
    // int rnd2 = rand()%368;
    // int rnd3 = rand()%368;
    // m17_b4s[rnd1] ^= 1;
    // m17_b4s[rnd2] ^= 1;
    // m17_b4s[rnd3] ^= 1;

    //-----------------------------------------

    fprintf (stderr, "\n M17 BERT   (ENCODER): ");

    //reverse the sequence and shift so it appears correct on display
    for (i = 0; i < 197; i++)
      m17_b1r[i+3] = m17_b1[196-i];
    
    //Dump Output of the BERT array (reversed and shifted sequence)
    for (i = 0; i < 25; i++)
      fprintf (stderr, "%02X", (uint8_t)ConvertBitIntoBytes(&m17_b1r[i*8], 8));

    //convert bit array into symbols and RF/Audio
    encodeM17RF (opts, state, m17_b4s, 3);

    //advance sequence if doing 1 bit each BERT frame
    bit = ( (lfsr >> 8) ^ (lfsr ^ 4) ) & 1;
    lfsr = (lfsr << 1) | bit;

    //shift the array once and put the result bit into the zero position
    //NOTE: The endian-ness of this may be opposite of what is expected
    for (j = 1; j < 197; j++)
      m17_b1[197-j] = m17_b1[197-j-1];
    m17_b1[0] = bit;

    //end sequence advancement

  }
}

//encode and create audio of a Project M17 PKT signal
void encodeM17PKT(dsd_opts * opts, dsd_state * state)
{

  uint8_t nil[368]; //empty array
  memset (nil, 0, sizeof(nil));

  int i, j, k, x; //basic utility counters

  //User Defined Variables
  uint8_t can = 7; //channel access number
  //numerical representation of dst and src after b40 encoding, or special/reserved value
  unsigned long long int dst = 0;
  unsigned long long int src = 0;
  //DST and SRC Callsign Data (pick up to 9 characters from the b40 char array)
  char d40[50] = "DSD-FME  "; //DST
  char s40[50] = "DSD-FME  "; //SRC

  // dst = 0xEE6B28000000; //viterbi debug values
  // src = 0xEE6B28000000; //viterbi debug values

  //Default
  // char text[800] = "This is a simple SMS text message sent over M17 Packet Data.";

  //short
  //NOTE: Working on full payload w/o padding
  // char text[800] = "Lorem";

  //medium
  //NOTE: Fixed w/ the pad < 1, then add a block (not just if 0)
  // char text[800] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";

  //large
  //NOTE: Working on full payload w/o padding
  char text[800] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

  //Preamble of the Declaration of Independence (U.S.A.)
  //NOTE: Fixed again with block != 31 check and manual terminator insertion into text byte 772
  // char text[] = "When in the Course of human events, it becomes necessary for one people to dissolve the political bands which have connected them with another, and to assume among the powers of the earth, the separate and equal station to which the Laws of Nature and of Nature's God entitle them, a decent respect to the opinions of mankind requires that they should declare the causes which impel them to the separation. We hold these truths to be self-evident, that all men are created equal, that they are endowed by their Creator with certain unalienable Rights, that among these are Life, Liberty and the pursuit of Happiness.--That to secure these rights, Governments are instituted among Men, deriving their just powers from the consent of the governed, --That whenever any Form of Government becomes destructive of these ends, it is the Right of the People to alter or to abolish it, and to institute new Government, laying its foundation on such principles and organizing its powers in such form, as to them shall seem most likely to effect their Safety and Happiness. Prudence, indeed, will dictate that Governments long established should not be changed for light and transient causes; and accordingly all experience hath shewn, that mankind are more disposed to suffer, while evils are sufferable, than to right themselves by abolishing the forms to which they are accustomed. But when a long train of abuses and usurpations, pursuing invariably the same Object evinces a design to reduce them under absolute Despotism, it is their right, it is their duty, to throw off such Government, and to provide new Guards for their future security.--Such has been the patient sufferance of these Colonies; and such is now the necessity which constrains them to alter their former Systems of Government. The history of the present King of Great Britain is a history of repeated injuries and usurpations, all having in direct object the establishment of an absolute Tyranny over these States. To prove this, let Facts be submitted to a candid world.";

  //end User Defined Variables

  //configure User Defined Variables, if defined at CLI
  if (state->m17_can_en != -1) //has a set value
    can = state->m17_can_en;

  if (state->str50c[0] != 0)
    sprintf (s40, "%s", state->str50c);

  if (state->str50b[0] != 0)
    sprintf (d40, "%s", state->str50b);

  if (state->m17sms[0] != 0)
    sprintf (text, "%s", state->m17sms);

  // switch to this if issues crop up (cygwin, etc)
  // strncpy (s40, state->str50c, 9);
  // strncpy (d40, state->str50b, 9);
  // s40[10] = '\0';
  // d40[10] = '\0';

  //if special values, then assign them
  if (strcmp (d40, "BROADCAST") == 0)
    dst = 0xFFFFFFFFFFFF;
  if (strcmp (d40, "ALL") == 0)
    dst = 0xFFFFFFFFFFFF;

  //debug
  // fprintf (stderr, " SRC: %s; DST: %s; SRC_D: %llX; DST_D: %llX; SLEN %d; DLEN: %d;", s40, d40, src, dst, strlen(s40), strlen(d40) );

  //end CLI Configuration

  //send dead air with type 99
  for (i = 0; i < 25; i++)
    encodeM17RF (opts, state, nil, 99);

  //send preamble_a for the LSF frame
  encodeM17RF (opts, state, nil, 33);

  //NOTE: PKT mode does not seem to have an IP format specified by M17 standard,
  //so I will assume that you do not send PKT data over IP to a reflector

  uint8_t m17_lsf[240];
  memset (m17_lsf, 0, sizeof(m17_lsf));

  //Setup LSF Variables, these are not sent in chunks like with voice
  //but only once at start of PKT TX
  uint16_t lsf_ps   = 0; //packet or stream indicator bit
  uint16_t lsf_dt   = 1; //Data
  uint16_t lsf_et   = 0; //encryption type
  uint16_t lsf_es   = 0; //encryption sub-type
  uint16_t lsf_cn = can; //can value
  uint16_t lsf_rs   = 0; //reserved bits
  uint8_t protocol  = 5; //SMS Protocol

  //compose the 16-bit frame information from the above sub elements
  uint16_t lsf_fi = 0;
  lsf_fi = (lsf_ps & 1) + (lsf_dt << 1) + (lsf_et << 3) + (lsf_es << 5) + (lsf_cn << 7) + (lsf_rs << 11);
  for (i = 0; i < 16; i++) m17_lsf[96+i] = (lsf_fi >> 15-i) & 1;

  //Convert base40 CSD to numerical values (lifted from libM17)

  //Only if not already set to a reserved value
  if (dst < 0xEE6B27FFFFFF)
  {
    for(i = strlen((const char*)d40)-1; i >= 0; i--)
    {
      for(j = 0; j < 40; j++)
      {
        if(d40[i]==b40[j])
        {
          dst=dst*40+j;
          break;
          }
      }
    }
  }

  if (src < 0xEE6B27FFFFFF)
  {
    for(i = strlen((const char*)s40)-1; i >= 0; i--)
    {
      for(j = 0; j < 40; j++)
      {
        if(s40[i]==b40[j])
        {
          src=src*40+j;
          break;
          }
      }
    }
  }
  //end CSD conversion

  //debug
  // fprintf (stderr, "\n SRC: %s; DST: %s; SRC_D: %llX; DST_D: %llX", s40, d40, src, dst);

  //load dst and src values into the LSF
  for (i = 0; i < 48; i++) m17_lsf[i] = (dst >> 47ULL-(unsigned long long int)i) & 1;
  for (i = 0; i < 48; i++) m17_lsf[i+48] = (src >> 47ULL-(unsigned long long int)i) & 1;

  //TODO: Any extra meta fills (extended callsign, etc?)

  //pack and compute the CRC16 for LSF
  uint16_t crc_cmp = 0;
  uint8_t lsf_packed[30];
  memset (lsf_packed, 0, sizeof(lsf_packed));
  for (i = 0; i < 28; i++)
      lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_lsf[i*8], 8);
  crc_cmp = crc16m17(lsf_packed, 28);

  //attach the crc16 bits to the end of the LSF data
  for (i = 0; i < 16; i++) m17_lsf[224+i] = (crc_cmp >> 15-i) & 1;

  //pack the CRC
  for (i = 28; i < 30; i++)
      lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_lsf[i*8], 8);

  //Craft and Send Initial LSF frame to be decoded

  //LSF w/ convolutional encoding (double check array sizes)
  uint8_t m17_lsfc[488]; memset (m17_lsfc, 0, sizeof(m17_lsfc));

  //LSF w/ P1 Puncturing
  uint8_t m17_lsfp[368]; memset (m17_lsfp, 0, sizeof(m17_lsfp));

  //LSF w/ Interleave
  uint8_t m17_lsfi[368]; memset (m17_lsfi, 0, sizeof(m17_lsfi));

  //LSF w/ Scrambling
  uint8_t m17_lsfs[368]; memset (m17_lsfs, 0, sizeof(m17_lsfs));

  //Use the convolutional encoder to encode the LSF Frame
  simple_conv_encoder (m17_lsf, m17_lsfc, 244);

  //P1 puncture
  x = 0;
  for (i = 0; i < 488; i++)
  {
    if (p1[i%61] == 1)
      m17_lsfp[x++] = m17_lsfc[i];
  }

  //interleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368;
    m17_lsfi[x] = m17_lsfp[i];
  }

  //scramble/randomize the frame
  for (i = 0; i < 368; i++)
    m17_lsfs[i] = (m17_lsfi[i] ^ m17_scramble[i]) & 1;

  //a full sized complete packet paylaod to break into smaller frames
  uint8_t m17_p1_full[31*200]; memset (m17_p1_full, 0, sizeof(m17_p1_full));

  //load protocol value into first 8 bits
  k = 0;
  for (i = 0; i < 8; i++)
    m17_p1_full[k++] = (protocol >> 7-i) & 1; 

  //byte representation of a single string char
  uint8_t cbyte;

  //counter values
  int tlen = strlen((const char*)text);
  int block = 0; //number of blocks in total
  int ptr = 0;  //ptr to current position of the text
  int pad = 0; //amount of padding to apply to last frame
  int lst = 0; //amount of significant octets in the last block

  //encode elements
  uint8_t pbc = 0; //packet/octet counter
  uint8_t eot = 0; //end of tx bit

  //sanity check, if tlen%25 is 23 or 24, need to increment to another block value
  if ( (tlen%25) > 23) tlen += (tlen%23) + 1;

  //sanity check, maximum strlen should not exceed 771 for a full encode
  if (tlen > 771) tlen = 771;

  //insert a zero byte as the terminator
  text[tlen++] = 0x00;

  //insert one at the last available byte position
  text[772] = 0x00;

  //debug tlen value
  // fprintf (stderr, " STRLEN: %d; ", tlen);

  //Convert a string text message into UTF-8 octets and load into full if using SMS (we are)
  fprintf (stderr, "\n SMS:\n      ");
  for (i = 0; i < tlen; i++)
  {
    cbyte = (uint8_t)text[ptr];
    fprintf (stderr, "%c", cbyte);

    for (j = 0; j < 8; j++)
      m17_p1_full[k++] = (cbyte >> 7-j) & 1;

    if (cbyte == 0) break; //if terminator reached

    ptr++; //increment pointer
    

    //add line break to keep it under 80 columns
    if ( (i%71) == 0 && i != 0)
      fprintf (stderr, "\n      ");
  }
  fprintf (stderr, "\n");

  //end UTF-8 Encoding


  //calculate blocks, pad, and last values for pbc
  block = (ptr / 25) + 1;
  pad = (block * 25) - ptr - 4;
  // if (pad == 0 && block != 31) //fallback if issues arise
  if (pad < 1 && block != 31)
  {
    block++;
    pad = (block * 25) - ptr - 4;
  }
  lst = 23-pad; //pbc value for last block out

  //sanity check block value
  // if (block > 31) block = 31;
  
  //debug position values
  fprintf (stderr, " BLOCK: %02d; PAD: %02d; LST: %d; K: %04d; PTR: %04d;", block, pad, lst, k, ptr);

  //Calculate the CRC and attach it here
  x = 0;
  uint8_t m17_p1_packed[31*25]; memset (m17_p1_packed, 0, sizeof(m17_p1_packed));
  for (i = 0; i < 25*31; i++)
  {
    m17_p1_packed[x] = (uint8_t)ConvertBitIntoBytes(&m17_p1_full[i*8], 8);
    if (m17_p1_packed[x] == 0) break; //stop at the termination byte
    x++;
  }

  //NOTE to self: Revert changes in this commit, if issues with CRC on PKT, or
  //the CRC really does go on the last 16 bits of the payload

  //debug dump the packed payload up to x, or x+1
  // fprintf (stderr, "\n P1P:");
  // for (i = 0; i < x+1; i++)
  //   fprintf (stderr, "%02X", m17_p1_packed[i]);
  // fprintf (stderr, "\n");

  crc_cmp = crc16m17(m17_p1_packed, x+1); //either x, or x+1?

  //debug dump CRC (when pad is literally zero)
  fprintf (stderr, " X: %d; LAST: %02X; TERM: %02X; CRC: %04X", x, m17_p1_packed[x-1], m17_p1_packed[x], crc_cmp);

  ptr = (block*25*8) - 16;

  //attach the crc16 bits to the end of the PKT data
  // for (i = 0; i < 16; i++) m17_p1_full[ptr+i] = (crc_cmp >> 15-i) & 1; //this one puts it as the last 16-bits of the full payload

  for (i = 0; i < 16; i++) m17_p1_full[k++] = (crc_cmp >> 15-i) & 1; //this one puts it immediately after the terminating byte

  //debug the full payload
  fprintf (stderr, "\n M17 Packet      FULL: ");
  for (i = 0; i < 25*block; i++)
  {
    if ( (i%25) == 0 && i != 0 ) fprintf (stderr, "\n                       ");
    fprintf (stderr, "%02X", (uint8_t)ConvertBitIntoBytes(&m17_p1_full[i*8], 8));
  }
  fprintf (stderr, "\n");

  //flag to determine if we send a new LSF frame for new encode
  //only send once at the appropriate time when encoder is toggled on
  int new_lsf = 1;

  while (!exitflag)
  {
    //send LSF frame once, if new encode session
    if (new_lsf == 1)
    {

      fprintf (stderr, "\n M17 LSF    (ENCODER): ");
      processM17LSF_debug2(opts, state, m17_lsfs);

      //convert bit array into symbols and RF/Audio
      memset (nil, 0, sizeof(nil));
      encodeM17RF (opts, state, nil, 11); //Preamble
      // for (i = 0; i < 6; i++) //test sending multiple LSF OTA
      encodeM17RF (opts, state, m17_lsfs, 1); //LSF

      //flag off after sending
      new_lsf = 0;

      //reset ptr value to use during chunk loading
      ptr = 0;
    }

    //PKT - 206 bits of Packet Data + 4 trailing bits
    uint8_t m17_p1[210]; memset (m17_p1, 0, sizeof(m17_p1));

    //PKT - 420 bits of Packet Data (after Convolutional Encode)
    uint8_t m17_p2c[420]; memset (m17_p2c, 0, sizeof(m17_p2c));

    //PKT - 368 bits of Packet Data (after P2 Puncturing)
    uint8_t m17_p3p[368]; memset (m17_p3p, 0, sizeof(m17_p3p));

    //PKT - 368 bits of Packet Data (after Interleaving)
    uint8_t m17_p4i[368]; memset (m17_p4i, 0, sizeof(m17_p4i));

    //PKT - 368 bits of Packet Data (after Scrambling)
    uint8_t m17_p4s[368]; memset (m17_p4s, 0, sizeof(m17_p4s));

    //Break the full payload into 25 octet chunks and load into p1
    for (i = 0; i < 200; i++)
      m17_p1[i] = m17_p1_full[ptr++];

    //Trigger EOT when out of data to encode
    if (ptr/8 >= block*25)
    {
      eot = 1;
      pbc = lst;
    }
    m17_p1[200] = eot;

    //set pbc counter to last 5 bits
    for (i = 0; i < 5; i++)
      m17_p1[201+i] = (pbc >> 4-i) & 1;

    //Use the convolutional encoder to encode the packet data
    simple_conv_encoder (m17_p1, m17_p2c, 210); //206 + 4 trailing bits

    //P3 puncture
    x = 0;
    for (i = 0; i < 420; i++)
    {
      if (p3[i%8] == 1)
        m17_p3p[x++] = m17_p2c[i];
    }

    //debug X bit positions
    // fprintf (stderr, " X: %d", x);

    //interleave the bit array using Quadratic Permutation Polynomial
    //function π(x) = (45x + 92x^2 ) mod 368
    for (i = 0; i < 368; i++)
    {
      x = ((45*i)+(92*i*i)) % 368;
      m17_p4i[x] = m17_p3p[i];
    }

    //scramble/randomize the frame
    for (i = 0; i < 368; i++)
      m17_p4s[i] = (m17_p4i[i] ^ m17_scramble[i]) & 1;

    //debug insert 3 random bit flip in the finished PKT frame
    // int rnd1 = rand()%368;
    // int rnd2 = rand()%368;
    // int rnd3 = rand()%368;
    // m17_b4s[rnd1] ^= 1;
    // m17_b4s[rnd2] ^= 1;
    // m17_b4s[rnd3] ^= 1;

    //-----------------------------------------


    fprintf (stderr, "\n M17 Packet (ENCODER): ");

    //Dump Output of the current Packet Frame
    for (i = 0; i < 26; i++)
      fprintf (stderr, "%02X", (uint8_t)ConvertBitIntoBytes(&m17_p1[i*8], 8));

    //debug PBC
    // fprintf (stderr, " PBC: %d;", pbc);

    //convert bit array into symbols and RF/Audio
    encodeM17RF (opts, state, m17_p4s, 4);

    //send the EOT Marker and some dead air
    if (eot)
    {
      memset (nil, 0, sizeof(nil));
      encodeM17RF (opts, state, nil, 55); //EOT Marker

      //send dead air with type 99
      for (i = 0; i < 25; i++)
        encodeM17RF (opts, state, nil, 99);

      //shut it down
      exitflag = 1;
    }

    //increment packet / byte counter
    pbc++;

  }
}

void decodeM17PKT(dsd_opts * opts, dsd_state * state, uint8_t * input, int len)
{
  //WIP: Decode the completed packet
  UNUSED(opts); UNUSED(state);
  int i;

  uint8_t protocol = input[0];
  fprintf (stderr, " Protocol:");
  if (protocol == 0) fprintf (stderr, " Raw;");
  else if (protocol == 1) fprintf (stderr, " AX.25;");
  else if (protocol == 2) fprintf (stderr, " APRS;");
  else if (protocol == 3) fprintf (stderr, " 6LoWPAN;");
  else if (protocol == 4) fprintf (stderr, " IPv4;");
  else if (protocol == 5) fprintf (stderr, " SMS;");
  else if (protocol == 6) fprintf (stderr, " Winlink;");
  else fprintf (stderr, " Res: %02X;", protocol);

  //simple UTF-8 SMS Decoder
  if (protocol == 5)
  {
    fprintf (stderr, "\n SMS:\n      ");
    for (i = 1; i < len; i++)
    {
      fprintf (stderr, "%c", input[i]);

      //add line break to keep it under 80 columns
      if ( (i%71) == 0 && i != 0)
        fprintf (stderr, "\n      ");
    }
  }
  else //if
  {
    //decode or just dump the raw data?
  }

}

//WIP PKT decoder
void processM17PKT(dsd_opts * opts, dsd_state * state)
{
 
  int i, x;
  uint8_t dbuf[184]; //384-bit (192 symbol) frame - 16-bit (8 symbol) sync pattern (184 dibits)
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_rnd_bits[368]; //368 bits that are still scrambled
  uint16_t m17_bits[368];    //368 bits that have been de-interleaved and de-scrambled
  uint16_t m17_depunc[488]; //488 weighted byte representation of bits after depuncturing
  uint8_t pkt_packed[50];
  uint8_t pkt_bytes[48];

  uint32_t v_err = 0; //errors in viterbi decoder
  UNUSED(v_err);

  memset (dbuf, 0, sizeof(dbuf));
  memset (state->m17_lsf, 0, sizeof(state->m17_lsf));
  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));
  memset (m17_rnd_bits, 0, sizeof(m17_rnd_bits));
  memset (m17_depunc, 0, sizeof(m17_depunc));

  memset (pkt_packed, 0, sizeof(pkt_packed));
  memset (pkt_bytes, 0, sizeof(pkt_bytes));


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

  //deinterleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368;
    m17_bits[i] = m17_int_bits[x];
  }

  //P3 Depuncture and Add Weights
  x = 0;
  for (i = 0; i < 420; i++)
  {
    if (p3[i%8] == 1)
      m17_depunc[i] = m17_bits[x++];
    else m17_depunc[i] = 0;

    if (m17_depunc[i])
      m17_depunc[i] = 0xFFFF;
    else m17_depunc[i] = 0x7FFF;
  }


  //use the libM17 Viterbi Decoder
  uint16_t len = 420;
  v_err = viterbi_decode(pkt_bytes, m17_depunc, len);
  // v_err -= 3932040; //cost negation (double check this as well as unit, meaning, etc)

  //debug
  // fprintf (stderr, "\n pkt_bytes: \n");
  // for (i = 0; i < 27; i++)
  //   fprintf (stderr, " %02X", pkt_bytes[i]);

  //copy + left shift one octet
  memcpy (pkt_packed, pkt_bytes+1, 26);

  uint8_t counter = (pkt_packed[25] >> 2) & 0x1F;
  uint8_t eot = (pkt_packed[25] >> 7) & 1;

  //disabled these checks, this fails if 2 blocks are sent, better just to keep track of it by internal counter
  // if (!eot) state->m17_pbc_ct = counter;
  // else if (eot && state->m17_pbc_ct == 0) {} //pass if single block packet (bugfix super short text messages)
  // else if (eot && state->m17_pbc_ct != 0) state->m17_pbc_ct++; //increment if eot and counter not zero

  int ptr = state->m17_pbc_ct*25;
  int total = ptr + counter - 1;
  int end = ptr + 25;

  //debug counter and eot value
  if (!eot) fprintf (stderr, " CNT: %02d; PBC: %02d; EOT: %d;", state->m17_pbc_ct, counter, eot);
  else fprintf (stderr, " CNT: %02d; LST: %02d; EOT: %d;", state->m17_pbc_ct, counter, eot);

  //put packet into storage
  memcpy (state->m17_pkt+ptr, pkt_packed, 25);

  //debug TOTAL and its value
  // fprintf (stderr, " T: %d; P: %02X", total, state->m17_pkt[total]);

  //debug
  if (opts->payload == 1)
  {
    fprintf (stderr, "\n pkt: ");
    for (i = 0; i < 26; i++)
      fprintf (stderr, " %02X", pkt_packed[i]);
  }

  if (eot)
  {
    //do a CRC check
    uint16_t crc_cmp = crc16m17(state->m17_pkt, total+1); //total, or total+1?
    // uint16_t crc_ext = (state->m17_pkt[end-2] << 8) + state->m17_pkt[end-1]; //extract from last 16-bits of the payload, or
    uint16_t crc_ext = (state->m17_pkt[total+1] << 8) + state->m17_pkt[total+2]; //immediately after the terminating byte

    if (crc_cmp == crc_ext)
      // decodeM17PKT(opts, state, state->m17_pkt, end-2);
      decodeM17PKT(opts, state, state->m17_pkt, total); //end-2 if CRC at the end, total if CRC after term
    else if (opts->aggressive_framesync == 0) //CRC Bypass, check anyways (if enabled)
      // decodeM17PKT(opts, state, state->m17_pkt, end-2);
      decodeM17PKT(opts, state, state->m17_pkt, total); //end-2 if CRC at the end, total if CRC after term

    if (crc_cmp != crc_ext)
      fprintf (stderr, " (CRC ERR) ");

    if (opts->payload == 1)
    {
      fprintf (stderr, "\n PKT:");
      for (i = 0; i < end; i++)
      {
        if ( (i%25) == 0 && i != 0)
          fprintf (stderr, "\n     ");
        fprintf (stderr, " %02X", state->m17_pkt[i]);
      }
      fprintf (stderr, "\n      CRC - C: %04X; E: %04X", crc_cmp, crc_ext);
    }
    
    //reset after processing
    memset (state->m17_pkt, 0, sizeof(state->m17_pkt));
    state->m17_pbc_ct = 0;
  }

  //increment pbc counter last
  if (!eot) state->m17_pbc_ct++;

  //ending linebreak
  fprintf (stderr, "\n");

} //end processM17PKT