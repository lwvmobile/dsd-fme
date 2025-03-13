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


uint8_t p25_decrypt_pdu(dsd_opts * opts, dsd_state * state, uint8_t * input, uint8_t alg_id, uint16_t key_id, unsigned long long int mi, int len)
{

  uint8_t encrypted = 1;

  UNUSED(opts);
  UNUSED(state);
  UNUSED(input);
  UNUSED(mi);
  UNUSED(key_id);
  UNUSED(alg_id);
  UNUSED(len);


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
  sprintf (state->dmr_lrrp_gps[0], "Data Call:%s SAP:%02X; LLID: %d; ", ea_sap_string, ea_sap, ea_llid);

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
  char sap_string[40];
  char rsp_string[40];
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

  //check for any additional headers first
  if (sap == 1) //encryption sync header
    encrypted = p25_decode_es_header(opts, state, input+ptr, &sap, &ptr, len);

  if (!encrypted)
  {
    //additional header (will be encrypted, so check above first)
    if (sap == 31) //extended address header
      p25_decode_extended_address(opts, state, input+ptr, &sap, &ptr);

    //test if an offset value set, then take the difference between it and the ptr and and add that to the ptr
    //or perhaps, just assign the ptr to that value + 12?
    if (offset) ptr = 12 + offset;
    
    //now start checking for the actual message
    if (sap == 0 || sap == 4) //User Data or Packet Data (both are UDP typically, same format dmr UDP/IP data)
      decode_ip_pdu (opts, state, len, input+ptr);

    else if (sap == 48) //Tier 1 Location Service (or does it depend on the io bit?)
      utf8_to_text(state, 0, len-ptr, input+ptr); //TODO, read initial string, i.e., $GPRMC and properly decode

    else utf8_to_text(state, 0, len-ptr, input+ptr); //default catch all
  }
  else
  {
    fprintf (stderr, " Encrypted PDU;");
  }

}