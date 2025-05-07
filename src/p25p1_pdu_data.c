/*-------------------------------------------------------------------------------
 * p25p1_pdu_data.c
 * P25p1 PDU Data Decoding
 *
 * LWVMOBILE
 * 2025-03 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

void p25_decode_rsp(uint8_t C, uint8_t T, uint8_t S, char * rsp_string)
{

  if      (C == 0)  sprintf (rsp_string, " ACK (Success);");
  else if (C == 2)  sprintf (rsp_string, " SACK (Retry);");
  else if (C == 1)
  {
    if      (T == 0) sprintf (rsp_string, " NACK (Illegal Format);");
    else if (T == 1) sprintf (rsp_string, " NACK (CRC32 Failure);");
    else if (T == 2) sprintf (rsp_string, " NACK (Memory Full);");
    else if (T == 3) sprintf (rsp_string, " NACK (FSN Sequence Error);");
    else if (T == 4) sprintf (rsp_string, " NACK (Undeliverable);");
    else if (T == 5) sprintf (rsp_string, " NACK (NS/VR Sequence Error);"); //depreciated
    else if (T == 6) sprintf (rsp_string, " NACK (Invalid User on System);");
  }

  //catch all for everything else
  else               sprintf (rsp_string, " Unknown RSP;");

  fprintf (stderr, " Response Packet:%s C: %X; T: %X; S: %X; ", rsp_string, C, T, S);

}

void p25_decode_sap(uint8_t SAP, char * sap_string)
{

  if      (SAP == 0)  sprintf (sap_string, " User Data;");
  else if (SAP == 1)  sprintf (sap_string, " Encrypted User Data;");
  else if (SAP == 2)  sprintf (sap_string, " Circuit Data;");
  else if (SAP == 3)  sprintf (sap_string, " Circuit Data Control;");
  else if (SAP == 4)  sprintf (sap_string, " Packet Data;");
  else if (SAP == 5)  sprintf (sap_string, " Address Resolution Protocol;");
  else if (SAP == 6)  sprintf (sap_string, " SNDCP Packet Data Control;");
  else if (SAP == 15) sprintf (sap_string, " Packet Data Scan Preamble;");
  else if (SAP == 29) sprintf (sap_string, " Packet Data Encryption Support;");
  else if (SAP == 31) sprintf (sap_string, " Extended Address;");
  else if (SAP == 32) sprintf (sap_string, " Registration and Authorization;");
  else if (SAP == 33) sprintf (sap_string, " Channel Reassignment;");
  else if (SAP == 34) sprintf (sap_string, " System Configuration;");
  else if (SAP == 35) sprintf (sap_string, " Mobile Radio Loopback;");
  else if (SAP == 36) sprintf (sap_string, " Mobile Radio Statistics;");
  else if (SAP == 37) sprintf (sap_string, " Mobile Radio Out of Service;");
  else if (SAP == 38) sprintf (sap_string, " Mobile Radio Paging;");
  else if (SAP == 39) sprintf (sap_string, " Mobile Radio Configuration;");
  else if (SAP == 40) sprintf (sap_string, " Unencrypted Key Management;");
  else if (SAP == 41) sprintf (sap_string, " Encrypted Key Management;");
  else if (SAP == 48) sprintf (sap_string, " Location Service;");
  else if (SAP == 61) sprintf (sap_string, " Trunking Control;");
  else if (SAP == 63) sprintf (sap_string, " Encrypted Trunking Control;");

  //catch all for everything else
  else                sprintf (sap_string, " Unknown SAP;");

  fprintf (stderr, "SAP: 0x%02X;%s ", SAP, sap_string);

}

void lfsr_64_to_128(uint8_t * iv)
{
  uint64_t lfsr = 0, bit = 0;

  lfsr = ((uint64_t)iv[0] << 56ULL) + ((uint64_t)iv[1] << 48ULL) + ((uint64_t)iv[2] << 40ULL) + ((uint64_t)iv[3] << 32ULL) +
         ((uint64_t)iv[4] << 24ULL) + ((uint64_t)iv[5] << 16ULL) + ((uint64_t)iv[6] << 8ULL)  + ((uint64_t)iv[7] << 0ULL);

  uint8_t cnt = 0, x = 64;

  for(cnt = 0;cnt < 64; cnt++)
  {
    //63,61,45,37,27,14
    // Polynomial is C(x) = x^64 + x^62 + x^46 + x^38 + x^27 + x^15 + 1
    bit = ((lfsr >> 63) ^ (lfsr >> 61) ^ (lfsr >> 45) ^ (lfsr >> 37) ^ (lfsr >> 26) ^ (lfsr >> 14)) & 0x1;
    lfsr = (lfsr << 1) | bit;

    //continue packing iv
    iv[x/8] = (iv[x/8] << 1) + bit;

    x++;
  }

}


uint8_t p25_decrypt_pdu(dsd_opts * opts, dsd_state * state, uint8_t * input, uint8_t alg_id, uint16_t key_id, unsigned long long int mi, int len)
{

  UNUSED(opts);
  uint8_t encrypted = 1;

  int i = 0;
  int ks_idx = 0;
  uint8_t ks_bytes[3096]; memset (ks_bytes, 0, sizeof(ks_bytes));

  //create keystream
  if (alg_id == 0x84 || alg_id == 0x89)
  {
    //aes specific arrays and things
    uint8_t aes_iv[16];  memset (aes_iv, 0, sizeof(aes_iv));
    uint8_t aes_key[32]; memset (aes_key, 0, sizeof(aes_key));
    uint8_t empt[64];    memset (empt, 0, sizeof(empt));

    uint8_t akl = 0; //aes key loaded into array flag
    unsigned long long int a1 = state->rkey_array[key_id+0x000];
    unsigned long long int a2 = state->rkey_array[key_id+0x101];
    unsigned long long int a3 = state->rkey_array[key_id+0x201];
    unsigned long long int a4 = state->rkey_array[key_id+0x301];

    //checkdown to see if anything in a1-a4
    if ( (a1 == 0) && (a2 == 0) && (a3 == 0) && (a4 == 0) )
    {
      //try loading from state->H instead (could clash if keys loaded that trigger any a1-a4 above)
      a1 = state->K1;
      a2 = state->K2;
      a3 = state->K3;
      a4 = state->K4;
    }

    //loader for aes keys
    for (uint64_t i = 0; i < 8; i++)
    {
      aes_key[i+0]   = (a1 >> (56ULL-(i*8))) & 0xFF;
      aes_key[i+8]   = (a2 >> (56ULL-(i*8))) & 0xFF;
      aes_key[i+16]  = (a3 >> (56ULL-(i*8))) & 0xFF;
      aes_key[i+24]  = (a4 >> (56ULL-(i*8))) & 0xFF;
    }

    //check to see if a key is loaded into any part of the array
    if (memcmp(aes_key, empt, sizeof(aes_key)) != 0) akl = 1;
    else akl = 0;

    //convert mi to aes_iv and expand it
    aes_iv[0] = ((mi & 0xFF00000000000000) >> 56);
    aes_iv[1] = ((mi & 0xFF000000000000) >> 48);
    aes_iv[2] = ((mi & 0xFF0000000000) >> 40);
    aes_iv[3] = ((mi & 0xFF00000000) >> 32);
    aes_iv[4] = ((mi & 0xFF000000) >> 24);
    aes_iv[5] = ((mi & 0xFF0000) >> 16);
    aes_iv[6] = ((mi & 0xFF00) >> 8);
    aes_iv[7] = ((mi & 0xFF) >> 0);

    lfsr_64_to_128(aes_iv);

    ks_idx = 16; //offset for OFB discard round

    if (akl == 1)
    {
      int nblocks = (len / 16) + 1;
      if (alg_id == 0x84) //AES256
        aes_ofb_keystream_output (aes_iv, aes_key, ks_bytes, 2, nblocks);
      else aes_ofb_keystream_output (aes_iv, aes_key, ks_bytes, 0, nblocks);

      fprintf (stderr, "\n Key: ");
      for (i = 0; i < 32; i++)
      {
        if ( (i != 0) && ((i%8) == 0) )
          fprintf (stderr, " ");
        fprintf (stderr, "%02X", aes_key[i]);
      }

      encrypted = 0;
    }

    fprintf (stderr, "\n IV(128): ");
    for (i = 0; i < 16; i++)
      fprintf (stderr, "%02X", aes_iv[i]);

  }

  if (alg_id == 0x81) //DES56
  {

    int nblocks = (len / 8) + 1;
    unsigned long long int des_key = 0;

    des_key = state->rkey_array[key_id];

    //if no key loaded from loader, check state->R for key
    if (des_key == 0) des_key = state->R;

    ks_idx = 8;   //offset for OFB discard round

    if (des_key)
      des_multi_keystream_output (mi, des_key, ks_bytes, 1, nblocks);

    encrypted = 0;

    //debug, print key, iv, and keystream stuff
    fprintf (stderr, "\n Key: %16llX", des_key);

  }

  if (alg_id == 0xAA) //RC4, or 'ADP'
  {

    unsigned long long int rc4_key = 0;

    rc4_key = state->rkey_array[key_id];

    //if no key loaded from loader, check state->R for key
    if (rc4_key == 0) rc4_key = state->R;

    ks_idx = 0;   //offset

    uint8_t rc4_kiv[13]; memset (rc4_kiv, 0, sizeof(rc4_key));

    rc4_kiv[0] = ((rc4_key & 0xFF00000000) >> 32);
    rc4_kiv[1] = ((rc4_key & 0xFF000000) >> 24);
    rc4_kiv[2] = ((rc4_key & 0xFF0000) >> 16);
    rc4_kiv[3] = ((rc4_key & 0xFF00) >> 8);
    rc4_kiv[4] = ((rc4_key & 0xFF) >> 0);

    rc4_kiv[5]  = ((mi & 0xFF00000000000000) >> 56);
    rc4_kiv[6]  = ((mi & 0xFF000000000000) >> 48);
    rc4_kiv[7]  = ((mi & 0xFF0000000000) >> 40);
    rc4_kiv[8]  = ((mi & 0xFF00000000) >> 32);
    rc4_kiv[9]  = ((mi & 0xFF000000) >> 24);
    rc4_kiv[10] = ((mi & 0xFF0000) >> 16);
    rc4_kiv[11] = ((mi & 0xFF00) >> 8);
    rc4_kiv[12] = ((mi & 0xFF) >> 0);

    if (rc4_key)
      rc4_block_output (256, 13, len, rc4_kiv, ks_bytes);

    encrypted = 0;

    //debug, print key, iv, and keystream stuff
    fprintf (stderr, "\n Key: %16llX", rc4_key);

  }

  //debug input offset
  // fprintf (stderr, "\n INPUT: ");
  // for (i = 0; i < 16; i++)
  //   fprintf (stderr, "%02X", input[i]);

  // fprintf (stderr, "\n    KS: ");
  // for (i = 0; i < 16; i++)
  //   fprintf (stderr, "%02X", ks_bytes[i]);

  //apply keystream
  for (i = 0; i < len; i++) //need to subtract pad bytes and crc bytes from keystream application
    input[i] ^= ks_bytes[i+ks_idx];

  if (alg_id == 0x80) encrypted = 0;

  return encrypted;
}

//SAP 1
uint8_t p25_decode_es_header(dsd_opts * opts, dsd_state * state, uint8_t * input, uint8_t * sap, int * ptr, int len)
{

  uint8_t encrypted = 0;

  uint8_t bits[13*8]; memset (bits, 0, sizeof(bits));
  unpack_byte_array_into_bit_array(input, bits, 13);

  fprintf (stderr, "%s",KYEL);
  unsigned long long int mi = (unsigned long long int)ConvertBitIntoBytes(bits, 64);
  uint8_t  mi_res = (uint8_t)ConvertBitIntoBytes(bits+64, 8);
  uint8_t  alg_id = (uint8_t)ConvertBitIntoBytes(bits+72, 8);
  uint16_t key_id = (uint16_t)ConvertBitIntoBytes(bits+80, 16);
  fprintf (stderr, "\n ES Aux Encryption Header; ALG: %02X; KEY ID: %04X; MI: %016llX; ", alg_id, key_id, mi);
  if (mi_res != 0)
    fprintf (stderr, " RES: %02X;", mi_res);

  //The Auxiliary Header signals the actual SAP value of the encrypted message (this byte is not encrypted)
  uint8_t aux_res = (uint8_t)ConvertBitIntoBytes(&bits[96], 2); //these two bits should always be signalled as 1's, so 0b11, and if combined with the 2ndary SAP, 0xC0 if SAP == 0x00
  uint8_t aux_sap = (uint8_t)ConvertBitIntoBytes(&bits[98], 6); //the SAP of the message that is encrypted immediately after
  char aux_sap_string[99];
  p25_decode_sap (aux_sap, aux_sap_string);
  fprintf (stderr, "%s",KNRM);
  UNUSED(aux_res);

  //Decrypt PDU
  if (alg_id != 0x80)
    encrypted = p25_decrypt_pdu(opts, state, input+13, alg_id, key_id, mi, len-13);

  *sap = aux_sap;
  *ptr += 13;

  //append enc at this point
  if (encrypted)
  {
    char ess_str[200]; memset(ess_str, 0, sizeof(ess_str));
    sprintf (ess_str, "ALG: %02X; KID: %04X; SAP:%02X;%s", alg_id, key_id, aux_sap, aux_sap_string);
    strcat (state->dmr_lrrp_gps[0], ess_str);
  }

  return encrypted;

}

//alternate configuration for this (no Aux SAP)
uint8_t p25_decode_es_header_2(dsd_opts * opts, dsd_state * state, uint8_t * input, int * ptr, int len)
{

  uint8_t encrypted = 0;

  uint8_t bits[12*8]; memset (bits, 0, sizeof(bits));
  unpack_byte_array_into_bit_array(input, bits, 12);

  fprintf (stderr, "%s",KYEL);
  uint8_t  alg_id = (uint8_t)ConvertBitIntoBytes(bits+0, 8);
  uint16_t key_id = (uint16_t)ConvertBitIntoBytes(bits+8, 16);
  unsigned long long int mi = (unsigned long long int)ConvertBitIntoBytes(bits+24, 64);
  uint8_t  mi_res = (uint8_t)ConvertBitIntoBytes(bits+88, 8);
  fprintf (stderr, "\n ES Aux Encryption Header 2; ALG: %02X; KEY ID: %04X; MI: %016llX;", alg_id, key_id, mi);
  if (mi_res != 0)
    fprintf (stderr, " RES: %02X;", mi_res);
  fprintf (stderr, "%s",KNRM);

  //Decrypt PDU
  if (alg_id != 0x80)
    encrypted = p25_decrypt_pdu(opts, state, input+12, alg_id, key_id, mi, len-12);

  *ptr += 12;

  return encrypted;

}

//SAP 31 //Extended Addressing
void p25_decode_extended_address(dsd_opts * opts, dsd_state * state, uint8_t * input, uint8_t * sap, int * ptr)
{

  UNUSED(opts);

  uint8_t bits[12*8]; memset (bits, 0, sizeof(bits));
  unpack_byte_array_into_bit_array(input, bits, 12);

  uint8_t  ea_sap  = (uint8_t)ConvertBitIntoBytes(bits+10, 6);
  uint8_t  ea_mfid = (uint8_t)ConvertBitIntoBytes(bits+16, 6);
  uint32_t ea_llid = (uint32_t)ConvertBitIntoBytes(bits+24, 24);
  uint32_t ea_res  = (uint32_t)ConvertBitIntoBytes(bits+48, 32);
  uint16_t ea_crc  = (uint16_t)ConvertBitIntoBytes(bits+80, 16);

  fprintf (stderr, "\n Extended Addressing Header; MFID: %02X; SRC LLID: %d; RES: %08X; CRC: %04X; ", ea_mfid, ea_llid, ea_res, ea_crc);
  char ea_sap_string[99];
  p25_decode_sap (ea_sap, ea_sap_string);
  UNUSED(ea_sap_string);

  //Print to Data Call String for Ncurses Terminal
  state->lastsrc = ea_llid;
  char ea_str[200]; memset(ea_str, 0, sizeof(ea_str));
  sprintf (ea_str, "EXT ADD SRC: %d; SAP:%02X;%s", ea_llid, ea_sap, ea_sap_string);
  strcat (state->dmr_lrrp_gps[0], ea_str);

  *sap = ea_sap;
  *ptr += 12;

}

//PDU Format Header Decode
void p25_decode_pdu_header(dsd_opts * opts, dsd_state * state, uint8_t * input)
{

  UNUSED(opts);

  uint8_t an   = (input[0] >> 6) & 0x1;
  uint8_t io   = (input[0] >> 5) & 0x1;
  uint8_t fmt  = input[0] & 0x1F;
  uint8_t sap  = input[1] & 0x3F;
  uint8_t MFID = input[2];
  uint32_t address = (input[3] << 16) | (input[4] << 8) | input[5];
  uint8_t blks = input[6] & 0x7F;

  uint8_t fmf = (input[6] >> 7) & 0x1;
  uint8_t pad = input[7] & 0x1F;
  uint8_t ns = (input[8] >> 4) & 0x7;
  uint8_t fsnf = input[8] & 0xF;
  uint8_t offset = input[9] & 0x3F;

  //response packet
  uint8_t class  = (input[1] >> 6) & 0x3;
  uint8_t type   = (input[1] >> 3) & 0x7;
  uint8_t status = (input[1] >> 0) & 0x7;

  fprintf (stderr, "%s",KGRN);
  fprintf (stderr, " P25 Data - AN: %d; IO: %d; FMT: %02X; ", an, io, fmt);
  char sap_string[40]; sprintf (sap_string, "%s", " ");
  char rsp_string[40]; sprintf (rsp_string, "%s", " ");
  if (fmt != 3) p25_decode_sap (sap, sap_string); //decode SAP to see what kind of data we are dealing with
  else          p25_decode_rsp (class, type, status, rsp_string); //decode the response type (ack, nack, sack)
  if (sap != 61 && sap != 63) //Not too interested in viewing these on trunking control, just data packets mostly
    fprintf (stderr, "\n F: %d; Blocks: %02X; Pad: %d; NS: %d; FSNF: %d; Offset: %d; MFID: %02X;", fmf, blks, pad, ns, fsnf, offset, MFID);
  if (io == 1 && sap != 61 && sap != 63) //destination address if IO bit set
    fprintf (stderr, " DST LLID: %d;", address);
  else if (io == 0 && sap != 61 && sap != 63) //Source address if IO bit not set
    fprintf (stderr, " SRC LLID: %d;", address);
  //Print to Data Call String for Ncurses Terminal
  if (sap != 61 && sap != 63 && fmt != 3)
    sprintf (state->dmr_lrrp_gps[0], "Data Call:%s SAP:%02X; LLID: %d; ", sap_string, sap, address);
  else if (sap != 61 && sap != 63 && fmt == 3)
  {
      //watchdog the data call and make it push to event history
      sprintf (state->dmr_lrrp_gps[0], "Data Call Response:%s LLID: %d; ", rsp_string, address);
      state->lastsrc = 0xFFFFFF;
      watchdog_event_datacall (opts, state, state->lastsrc, state->lasttg, state->dmr_lrrp_gps[0], 0);
      state->lastsrc = 0;
      state->lasttg = 0;
      watchdog_event_history(opts, state, 0);
      watchdog_event_current(opts, state, 0);
  }

  //following is for a continued PDU and not a response nor a trunking message
  if (sap != 61 && sap != 63) //trunking blocks, don't set address (LID)
  {
    state->lasttg = address;
    state->lastsrc = 0xFFFFFF; //none given, unless extended, so put any here for now
  }
  
}

//user or other data delivered via PDU format
void p25_decode_pdu_data(dsd_opts * opts, dsd_state * state, uint8_t * input, int len)
{

  uint8_t sap = input[1] & 0x3F;
  uint8_t pad = input[7] & 0x1F;
  uint8_t offset = input[9] & 0x3F; UNUSED(offset); //determine the best way to use this
  uint8_t encrypted = 0;
  int ptr = 12; //initial ptr index value past the first header

  //may need a sanity check on this value to make sure it doesn't go negative
  if (len > (12 + 4 + pad))
    len -= (12 + 4 + pad); //substract the header, crc, and padding bytes from total len value

  //debug
  fprintf (stderr, " PDU Len: %d;", len);

  //check for additional headers
  if (sap == 31) //extended address header
    p25_decode_extended_address(opts, state, input+ptr, &sap, &ptr);

  //test shows this occurs after an extended address header
  if (sap == 1) //encryption sync header
    encrypted = p25_decode_es_header(opts, state, input+ptr, &sap, &ptr, len);

  if (!encrypted)
  {

    //test if an offset value set, then take the difference between it and the ptr and and add that to the ptr
    //or perhaps, just assign the ptr to that value + 12?
    if (offset) ptr = 12 + offset;

    //now start checking for the actual message
    if (sap == 0 || sap == 4) //User Data or Packet Data (both are UDP typically, same format dmr UDP/IP data)
      decode_ip_pdu (opts, state, len+1, input+ptr);

    else if (sap == 48) //Tier 1 Location Service (or does it depend on the io bit?)
      utf8_to_text(state, 1, len-ptr+1, input+ptr); //TODO, read initial string, i.e., $GPRMC and properly decode

    // else //default catch all (debug only)
    // {
    //   if (len > ptr)
    //     utf8_to_text(state, 0, len-ptr+1, input+ptr);
    //   else utf8_to_text(state, 0, len+1, input+ptr);
    // }
  }
  else
  {
    fprintf (stderr, " Encrypted PDU;");
  }

  //watchdog the data call and make it push to event history
  watchdog_event_datacall (opts, state, state->lastsrc, state->lasttg, state->dmr_lrrp_gps[0], 0);
  state->lastsrc = 0;
  state->lasttg = 0;
  watchdog_event_history(opts, state, 0);
  watchdog_event_current(opts, state, 0);

}