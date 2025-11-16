/*-------------------------------------------------------------------------------
 * m17.c
 * M17 Decoder (simplified version)
 *
 * m17_scramble Bit Array from SDR++
 * CRC16, CSD encoder from libM17 / M17-Implementations (thanks again, sp5wwp)
 *
 * LWVMOBILE
 * 2025-10 DSD-FME Florida Man Edition
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

//p2 puncture
static uint8_t p2[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};

uint8_t p3[62] = {1, 1, 1, 1, 1, 1, 1, 0};

//from M17_Implementations / libM17 -- sp5wwp
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
  //evaluate dst and src, and determine if they need to be converted to callsign
  int i;
  char c;
  memset (state->m17_dst_csd, 0, sizeof(state->m17_dst_csd));
  memset (state->m17_src_csd, 0, sizeof(state->m17_src_csd));
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
      if (dst == 0) break;
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
      if (src == 0) break;
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

//NOTE: Current state of DSD-FME is Stripped Down and Simplied M17 stream decoder, only META is passed here
//so, basically, Meta Text, Meta GNSS, Arbitrary Data, and any other Meta only decodes, no packet data SMS, etc
void decodeM17PKT(dsd_opts * opts, dsd_state * state, uint8_t * input, int len)
{
  //Decode the completed packet
  UNUSED(opts); UNUSED(state);
  int i;

  uint8_t protocol = input[0];
  fprintf (stderr, " Protocol:");
  if      (protocol == 0x00) fprintf (stderr, " Raw;");
  else if (protocol == 0x01) fprintf (stderr, " AX.25;");
  else if (protocol == 0x02) fprintf (stderr, " APRS;");
  else if (protocol == 0x03) fprintf (stderr, " 6LoWPAN;");
  else if (protocol == 0x04) fprintf (stderr, " IPv4;");
  else if (protocol == 0x05) fprintf (stderr, " SMS;");
  else if (protocol == 0x06) fprintf (stderr, " Winlink;");
  else if (protocol == 0x07) fprintf (stderr, " TLE;");
  else if (protocol == 0x69) fprintf (stderr, " OTA Key Delivery;"); //m17-fme non standard packet data
  else if (protocol == 0x80) fprintf (stderr, " Meta Text Data V2;"); //internal format only from meta
  else if (protocol == 0x81) fprintf (stderr, " Meta GNSS Position Data;"); //internal format only from meta
  else if (protocol == 0x82) fprintf (stderr, " Meta Extended CSD;"); //internal format only from meta
  else if (protocol == 0x83) fprintf (stderr, " Meta Text Data V3;"); //internal format only from meta
  else if (protocol == 0x91) fprintf (stderr, " PDU GNSS Position Data;"); //PDU Version of GNSS
  else if (protocol == 0x99) fprintf (stderr, " 1600 Arbitrary Data;"); //internal format only from 1600
  else                       fprintf (stderr, " Res/Unk: %02X;", protocol); //any received but unknown protocol type

  //check for encryption, if encrypted, skip decode and report as encrypted
  if (protocol == 0x69) {} //allow OTAKD passthrough (not encrypted ever)
  else if (protocol >= 0x80 && protocol <= 0x83) {} //allow META passthrough (not encrypted ever)
  else if (state->m17_enc != 0)
  {
    fprintf (stderr, " *Encrypted*");
    goto PKT_END;
  }

  //simple UTF-8 SMS Decoder
  if (protocol == 0x05)
  {
    fprintf (stderr, "\n SMS: ");
    for (i = 1; i < len; i++)
      fprintf (stderr, "%c", input[i]);
  }

  //TLE UTF-8 Text Decoder
  else if (protocol == 0x07)
  {
    fprintf (stderr, " TLE:\n");
    for (i = 1; i < len; i++)
      fprintf (stderr, "%c", input[i]);
  }

  //Extended Call Sign Data
  else if (protocol == 0x82)
  {
    //NOTE: If doing a shift addition like this, make sure ALL values have (unsigned long long int) in front of it, not just the ones that 'needed' it
    unsigned long long int src  = ((unsigned long long int)input[1] << 40ULL) + ((unsigned long long int)input[2] << 32ULL) + ((unsigned long long int)input[3] << 24ULL) + ((unsigned long long int)input[4]  << 16ULL) + ((unsigned long long int)input[5]  << 8ULL) + ((unsigned long long int)input[6]  << 0ULL);
    unsigned long long int dst  = ((unsigned long long int)input[7] << 40ULL) + ((unsigned long long int)input[8] << 32ULL) + ((unsigned long long int)input[9] << 24ULL) + ((unsigned long long int)input[10] << 16ULL) + ((unsigned long long int)input[11] << 8ULL) + ((unsigned long long int)input[12] << 0ULL);
    char cf1[10]; memset (cf1, 0, 10*sizeof(char));
    char cf2[10]; memset (cf2, 0, 10*sizeof(char));
    fprintf (stderr, " CF1: "); //Contextual CF1
    for (i = 0; i < 9; i++)
    {
      char c = b40[src % 40];
      fprintf (stderr, "%c", c);
      cf1[i] = c;
      src = src / 40;
    }
    if (dst != 0) //if used
    {
      fprintf (stderr, " CF2: "); //Contextual CF2
      for (i = 0; i < 9; i++)
      {
        char c = b40[dst % 40];
        fprintf (stderr, "%c", c);
        cf2[i] = c;
        dst = dst / 40;
      }
    }

    //check for optional cf2
    if (cf2[0] != 0)  
      sprintf (state->m17_data_string, "Extended CSD - CF1: %s; CF2: %s;", cf1, cf2);
    else sprintf (state->m17_data_string, "Extended CSD - CF1: %s; ", cf1);

  }

  //GNSS Positioning (version 2.0 spec)
  else if (protocol == 0x81)
  {
    //Decode GNSS Elements
    uint8_t  data_source  = (input[1] >> 4);
    uint8_t  station_type = (input[1] & 0xF);
    uint8_t  validity     = (input[2] >> 4);
    uint8_t  radius       = (input[2] >> 1) & 0x7;
    uint16_t bearing      = ((input[2] & 0x1) << 8) + input[3];
    uint32_t latitude     = (input[4] << 16) + (input[5] << 8) + input[6];
    uint32_t longitude    = (input[7] << 16) + (input[8] << 8) + input[9];
    uint16_t altitude     = (input[10] << 8) + input[11];
    uint16_t speed        = (input[12] << 4) + (input[13] >> 4);
    uint16_t reserved     = ((input[13] & 0xF) << 8) + input[14];

    //signed-ness and two's complement (needs to be tested and verified)
    double lat_sign = +1.0;
    if (latitude & 0x800000)
    {
      if (latitude > 0x800000)
      {
        latitude &= 0x7FFFFF;
        latitude = 0x800000 - latitude;
      }

      lat_sign = -1.0f;
    }

    double lon_sign = 1.0f;
    if (longitude & 0x800000)
    {
      if (longitude > 0x800000)
      {
        longitude &= 0x7FFFFF;
        longitude = 0x800000 - longitude;
      }

      lon_sign = -1.0f;
    }

    //decoding calculation
    double lat_float = ((double)latitude * 90.0f)  / 8388607.0f * lat_sign;
    double lon_float = ((double)longitude * 180.0f) / 8388607.0f * lon_sign;

    float radius_float = powf(2.0f, radius);
    float speed_float = ((float)speed * 0.5f);
    float altitude_float = ((float)altitude * 0.5f) - 500.0f;

    char deg_glyph[4];
    sprintf (deg_glyph, "%s", "°");

    if (validity & 0x8)
      fprintf (stderr, "\n GPS: (%f, %f);", lat_float, lon_float);
    else fprintf (stderr, "\n GPS Not Valid;");

    if (validity & 0x4)
      fprintf (stderr, " Altitude: %.1f m;", altitude_float);

    if (validity & 0x2)
    {
      fprintf (stderr, " Speed: %.1f km/h;", speed_float);
      fprintf (stderr, " Bearing: %d%s;", bearing, deg_glyph);
    }

    if (validity & 0x1)
      fprintf (stderr, "\n      Radius: %.1f;", radius_float);

    if (reserved)
      fprintf (stderr, " Reserved: %03X;", reserved);

    if      (data_source == 0) fprintf (stderr, " M17 Client;");
    else if (data_source == 1) fprintf (stderr, " OpenRTX;");
    else  fprintf (stderr, " Other Data Source: %0X;", data_source);

    if      (station_type == 0) fprintf (stderr, " Fixed Station;");
    else if (station_type == 1) fprintf (stderr, " Mobile Station;");
    else if (station_type == 2) fprintf (stderr, " Handheld;");
    else fprintf (stderr, " Reserved Station Type: %0X;", station_type);

    char st[4];
    if      (station_type == 0) sprintf (st, "FS");
    else if (station_type == 1) sprintf (st, "MS");
    else if (station_type == 2) sprintf (st, "HH");
    else                        sprintf (st, "%02X", station_type);

    //TODO: This to ncurses display?
    sprintf (state->m17_gnss_string, "(%f, %f); Alt: %.1f; Spd: %.1f; Ber: %d; St: %s;",
      lat_float, lon_float, altitude_float, speed_float, bearing, st);

    //now it is okay to change the protocol to 0x81
    if (protocol == 0x91) //PDU variant of GNSS
      protocol = 0x81;

  }

  //Meta Text Messages Version 2.0 (4-segment with bitmapping)
  else if (protocol == 0x80)
  {

    uint8_t bitmap_len = (input[1] >> 4);
    uint8_t bitmap_segment = input[1] & 0xF;
    uint8_t segment_len = 1;
    uint8_t segment_num = 1;

    //convert bitmap to actual values
    if (bitmap_len == 0x1)
      segment_len = 1;
    else if (bitmap_len == 0x3)
      segment_len = 2;
    else if (bitmap_len == 0x7)
      segment_len = 3;
    else if (bitmap_len == 0xF)
      segment_len = 4;
    else segment_len = 1; //if none of these, then treat this like a single segment w/ len 1

    //convert bitmap to actual values
    if (bitmap_segment == 0x1)
      segment_num = 1;
    else if (bitmap_segment == 0x2)
      segment_num = 2;
    else if (bitmap_segment == 0x4)
      segment_num = 3;
    else if (bitmap_segment == 0x8)
      segment_num = 4;
    else segment_num = 1; //if none of these, then treat this like a single segment w/ len 1

    //show Control Byte Len and Segment Values on Meta Text
    fprintf (stderr, " %d/%d; ", segment_num, segment_len);
    // if (super->opts.payload_verbosity > 0)
    {
      for (i = 2; i < len; i++)
        fprintf (stderr, "%c", input[i]);
    }

    //copy current segment into m17_text_string
    int ptr = (segment_num-1)*13;
    memcpy (state->m17_text_string+ptr, input+2, 13*sizeof(char));

    //NOTE: there is no checkdown to see if all segments have arrived or not
    //terminate the string on completion, dump completed string
    if (segment_len == segment_num)
    {
      state->m17_text_string[ptr+13] = 0;
      fprintf (stderr, "\n Complete Meta Text: %s", state->m17_text_string);
    }

    //TODO: This
    //send to event_log_writer
    // if (segment_len == segment_num)
    //   event_log_writer (super, state->m17_text_string, protocol);

  }

  //Meta Text Messages Version 3.0 (15-segment sequential)
  else if (protocol == 0x83)
  {

    uint8_t segment_len = (input[1] >> 4) & 0xF;
    uint8_t segment_num = (input[1] >> 0) & 0xF;

    //show Control Byte Len and Segment Values on Meta Text
    fprintf (stderr, " %d/%d; ", segment_num, segment_len);
    // if (super->opts.payload_verbosity > 0)
    {
      for (i = 2; i < len; i++)
        fprintf (stderr, "%c", input[i]);
    }

    //copy current segment into .dat
    int ptr = (segment_num-1)*13;
    memcpy (state->m17_text_string+ptr, input+2, 13*sizeof(char));

    //NOTE: there is no checkdown to see if all segments have arrived or not
    //terminate the string on completion, dump completed string
    if (segment_len == segment_num)
    {
      state->m17_text_string[ptr+13] = 0;
      fprintf (stderr, "\n Complete Meta Text: %s", state->m17_text_string);
    }

    //TODO: This
    // //send to event_log_writer
    // if (segment_len == segment_num)
    //   event_log_writer (super, state->m17_text_string, protocol);

  }

  //1600 Arbitrary Data as ASCII Text String
  else if (protocol == 0x99)
  {
    uint8_t is_ascii = 1;
    for (i = 1; i < len; i++)
    {
      if (input[i] != 0 && (input[i] < 0x20 || input[i] > 0x7F))
      {
        is_ascii = 0;
        break;
      }
    }

    sprintf (state->m17_data_string, "%s", "");

    if (is_ascii == 1)
    {

      fprintf (stderr, " ");
      for (i = 1; i < len; i++)
        fprintf (stderr, "%c", input[i]);

      memcpy (state->m17_data_string, input+1, len);
      state->m17_data_string[len] = '\0'; //terminate string

    }
    else
    {
      fprintf (stderr, " Unknown Format;");
      sprintf (state->m17_data_string, "%s", "Unknown Arbitrary Data Format;");
    } 

    //todo: this
    // event_log_writer (super, state->m17_data_string, protocol);

  }

  //Any Other Raw Data as Hex
  else
  {
    fprintf (stderr, " ");
    for (i = 1; i < len; i++)
      fprintf (stderr, "%02X", input[i]);
  }

  PKT_END: ; //do nothing

}

void decode_lsf_v3_contents(dsd_state * state)
{

  unsigned long long int lsf_dst = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[0], 48);
  unsigned long long int lsf_src = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[48], 48);

  //LSF Verison 3.0 Type Field
  uint16_t lsf_type = (uint16_t)convert_bits_into_output(&state->m17_lsf[96], 16);

  //LSF Type Field broken into its sub fields
  uint16_t payload_contents = (lsf_type >> 12) & 0xF;
  uint16_t encryption_type  = (lsf_type >> 9)  & 0x7;
  uint16_t signature        = (lsf_type >> 8)  & 0x1;
  uint16_t meta_contents    = (lsf_type >> 4)  & 0xF;
  uint16_t can              = (lsf_type >> 0)  & 0xF;

  //store this so we can reference it for playing voice and/or decoding data, dst/src etc
  state->m17_str_dt = payload_contents;
  state->m17_dst = lsf_dst;
  state->m17_src = lsf_src;
  state->m17_can = can;

  fprintf (stderr, "\n");

  fprintf (stderr, " CAN: %d", can);
  M17decodeCSD(state, lsf_dst, lsf_src);

  if      (payload_contents == 0x1) fprintf (stderr, " Stream Data");
  else if (payload_contents == 0x2) fprintf (stderr, " Voice (3200bps)");
  else if (payload_contents == 0x3) fprintf (stderr, " Voice (1600bps)");
  else if (payload_contents == 0xF) fprintf (stderr, " Packet Data");
  else fprintf (stderr, " Reserved: %X", payload_contents);

  if (signature) fprintf (stderr, " Signed (secp256r1);");

  //compatibility shim for current code and call history / event log (remove later if redone)
  if (payload_contents != 0 && payload_contents < 4)
    state->m17_str_dt = payload_contents;
  else if (payload_contents == 0xF)
    state->m17_str_dt = 20;

  //decode encryption_type (to maintain a level of backwards compatibility,
  //the old encryption type and subtype values will be used only for encryption)
  if (encryption_type != 0)
  {
    fprintf (stderr, "\n ENC:");

    if (encryption_type == 0x1)
    {
      fprintf (stderr, " Scrambler (8-bit);");
      state->m17_enc = 1;
      state->m17_enc_st = 0;
    }
    else if (encryption_type == 0x2)
    {
      fprintf (stderr, " Scrambler (16-bit);");
      state->m17_enc = 1;
      state->m17_enc_st = 1;
    }
    else if (encryption_type == 0x3)
    {
      fprintf (stderr, " Scrambler (24-bit);");
      state->m17_enc = 1;
      state->m17_enc_st = 2;
    }

    else if (encryption_type == 0x4)
    {
      fprintf (stderr, " AES-CTR (128-bit);");
      state->m17_enc = 2;
      state->m17_enc_st = 0;
    }
    else if (encryption_type == 0x5)
    {
      fprintf (stderr, " AES-CTR (192-bit);");
      state->m17_enc = 2;
      state->m17_enc_st = 1;
    }
    else if (encryption_type == 0x6)
    {
      fprintf (stderr, " AES-CTR (256-bit);");
      state->m17_enc = 2;
      state->m17_enc_st = 2;
    }

    else if (encryption_type == 0x7)
    {
      fprintf (stderr, " Reserved Enc (0x7);");
      state->m17_enc = 3;
      state->m17_enc_st = 3;
    }

    if (encryption_type > 0x3 && encryption_type <= 0x6)
    {
      fprintf (stderr, " IV: ");
      for (int i = 0; i < 16; i++)
        fprintf (stderr, "%02X", state->m17_aes_iv[i]);
    }

  }
  else
  {
    state->m17_enc = 0;
    state->m17_enc_st = 0;
  }

  //pack meta bits either meta, or AES IV, depending on encryption type
  if (meta_contents != 0xF)
  {
    for (int i = 0; i < 14; i++)
      state->m17_meta[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[(i*8)+112], 8);
  }
  else if (meta_contents == 0xF)
  {
    memset(state->m17_meta, 0, sizeof(state->m17_meta));
    for (int i = 0; i < 14; i++)
      state->m17_aes_iv[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[(i*8)+112], 8);
  }

  //using meta_sum in case some byte fields, particularly meta[0], are zero
  uint32_t meta_sum = 0;
  for (int i = 0; i < 14; i++)
    meta_sum += state->m17_meta[i];

  //Decode Meta Data when meta_contents available and meta sum is not zero
  if (meta_contents != 0 && meta_contents != 0xF && meta_sum != 0)
  {
    uint8_t meta[15]; meta[0] = meta_contents + 0x80; //add identifier for pkt decoder
    for (int i = 0; i < 14; i++) meta[i+1] = state->m17_meta[i];
    fprintf (stderr, "\n ");
    //Note: We don't have opts here, so in the future, if we need it, we will need to pass it here
    decodeM17PKT (NULL, state, meta, 15); //decode META
  }

  //Payload Dump on LSF Type
  // if (opts->payload == 1) opts not passed here
  {
    fprintf (stderr, "\n");
    fprintf (stderr, " FT: %04X;", lsf_type);
    fprintf (stderr, " PAY: %X;", payload_contents);
    fprintf (stderr, " ENC: %X;", encryption_type);
    fprintf (stderr, " SIG: %X;", signature);
    fprintf (stderr, " META: %X;", meta_contents);
  }

}

void decode_lsf_v2_contents(dsd_state * state)
{

  unsigned long long int lsf_dst = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[0], 48);
  unsigned long long int lsf_src = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[48], 48);
  uint16_t lsf_type = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[96], 16);

  //this is the way the manual/code expects you to read these bits
  // uint8_t lsf_ps = (lsf_type >> 0) & 0x1; //not referenced since we just do the embedded LSF in Stream Frame here
  uint8_t lsf_dt = (lsf_type >> 1) & 0x3;
  uint8_t lsf_et = (lsf_type >> 3) & 0x3;
  uint8_t lsf_es = (lsf_type >> 5) & 0x3;
  uint8_t lsf_cn = (lsf_type >> 7) & 0xF;
  uint8_t lsf_rs = (lsf_type >> 11) & 0x1F;

  //decode behavior debug
  // lsf_rs |= 0x10;

  //ECDSA signature included
  uint8_t is_signed = 0;

  //Seperate the Signed bit from the reserved field, shift right once
  if (lsf_rs & 1)
  {
    is_signed = 1;
    lsf_rs >>= 1;
  }

  //if this field is not zero, then this is not a standard V2.0 or older spec'd LSF type,
  //if proposed LSF TYPE field changes occur, this region will be 0 for V2.0, and not 0 for newer
  if (lsf_rs != 0)
  {
    fprintf (stderr, " Unknown LSF TYPE;"); //V3.0 in future spec?
    goto LSF_END;
  }

  //store this so we can reference it for playing voice and/or decoding data, dst/src etc
  state->m17_str_dt = lsf_dt;
  state->m17_dst = lsf_dst;
  state->m17_src = lsf_src;
  state->m17_can = lsf_cn;

  fprintf (stderr, "\n");

  fprintf (stderr, " CAN: %d", lsf_cn);
  M17decodeCSD(state, lsf_dst, lsf_src);

  if (lsf_dt == 0) fprintf (stderr, " Reserved");
  if (lsf_dt == 1) fprintf (stderr, " Data");
  if (lsf_dt == 2) fprintf (stderr, " Voice (3200bps)");
  if (lsf_dt == 3) fprintf (stderr, " Voice (1600bps)");

  if (lsf_rs != 0) fprintf (stderr, " RS: %02X", lsf_rs);

  // fprintf (stderr, "\n"); //do we need this?

  if (lsf_et != 0) fprintf (stderr, "\n ENC:");
  if (lsf_et == 1)
  {
    fprintf (stderr, " Scrambler");
    if (lsf_es == 0)
      fprintf (stderr, " (8-bit);");
    else if (lsf_es == 1)
      fprintf (stderr, " (16-bit);");
    else if (lsf_es == 2)
      fprintf (stderr, " (24-bit);");
  }
  if (lsf_et == 2)
  {
    fprintf (stderr, " AES");
    if (lsf_es == 0)
      fprintf (stderr, " 128;"); 
    else if (lsf_es == 1)
      fprintf (stderr, " 192;");    
    else if (lsf_es == 2)
      fprintf (stderr, " 256;");     
  }

  if (is_signed)
    fprintf (stderr, " Signed (secp256r1);");

  state->m17_enc = lsf_et;
  state->m17_enc_st = lsf_es;

  //pack meta bits either meta, or AES IV, depending on encryption type
  if (lsf_et == 0)
  {
    memset(state->m17_aes_iv, 0, sizeof(state->m17_aes_iv));
    for (int i = 0; i < 14; i++)
      state->m17_meta[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[(i*8)+112], 8);
  }
  else if (lsf_et == 2)
  {
    memset(state->m17_meta, 0, sizeof(state->m17_meta));
    for (int i = 0; i < 14; i++)
      state->m17_aes_iv[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[(i*8)+112], 8);
  }

  //using meta_sum in case some byte fields, particularly meta[0], are zero
  uint32_t meta_sum = 0;
  for (int i = 0; i < 14; i++)
    meta_sum += state->m17_meta[i];

  //Decode Meta Data when not ENC (if meta field is populated with something)
  if (lsf_et == 0 && meta_sum != 0)
  {
    uint8_t meta[15]; meta[0] = lsf_es + 0x80; //add identifier for pkt decoder
    for (int i = 0; i < 14; i++)
      meta[i+1] = state->m17_meta[i];

    fprintf (stderr, "\n ");
    //Note: We don't have opts here, so in the future, if we need it, we will need to pass it here
    decodeM17PKT (NULL, state, meta, 15); //decode META
  }

  if (lsf_et == 2)
  {
    fprintf (stderr, " IV: ");
    for (int i = 0; i < 16; i++)
      fprintf (stderr, "%02X", state->m17_aes_iv[i]);
  }

  LSF_END: ; //do nothing

}

//gate function to determine if this is v2, or v3, and proceed accordingly
void M17decodeLSF(dsd_state * state)
{
  uint16_t lsf_type = (uint16_t)convert_bits_into_output(&state->m17_lsf[96], 16);
  uint16_t version_check = lsf_type >> 12;

  if (version_check == 0)
    decode_lsf_v2_contents(state);
  else decode_lsf_v3_contents(state);
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
  uint8_t lich_reserve = 0; UNUSED(lich_reserve);

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
  // if (err == 0 && opts->m17encoder == 1) //only use when using built in encoder
  // {
  //   state->m17_str_dt = lich_reserve & 0x3;
  //   state->m17_can = (lich_reserve >> 2) & 0x7;
  // }

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
      fprintf (stderr, "\n      (CRC CHK) E: %04X; C: %04X;", crc_ext, crc_cmp);
    }

    memset (state->m17_lsf, 0, sizeof(state->m17_lsf));

    if (crc_err == 1) fprintf (stderr, " EMB LSF CRC ERR");
  }

  return err;
}

void M17processCodec2_1600(dsd_opts * opts, dsd_state * state, uint8_t * payload, uint8_t lich_cnt)
{

  int i;
  unsigned char voice1[8];
  unsigned char voice2[8];

  for (i = 0; i < 8; i++)
  {
    voice1[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+0], 8);
    voice2[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+64], 8);
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

  //look at current viterbi error / cost metric,
  //if exceeds threshold, substitute silence frame
  if (state->m17_viterbi_err >= 25.0f)
  {
    uint64_t silence = 0x010004002575DDF2;
    for (int i = 0; i < 8; i++)
      voice1[i] = (silence >> (56ULL-(i*8))) & 0xFF;
    }

  codec2_decode(state->codec2_1600, samp1, voice1);

  //hpf_d on codec2 sounds better than not on those .rrc samples
  if (opts->use_hpf_d == 1)
    hpf_dL(state, samp1, nsam);

  if (opts->slot1_on == 1) //playback if enabled
  {

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

  }

  //Wav file saving
  if(opts->wav_out_f != NULL && state->m17_enc == 0 && opts->dmr_stereo_wav == 1) //Per Call
  {
    sf_write_short(opts->wav_out_f, samp1, nsam);
  }
  else if (opts->wav_out_f != NULL && state->m17_enc == 0 && opts->static_wav_file == 1) //Static Wav File
  {
    //convert to stereo for new static wav file setup
    short ss[nsam*2];
    memset (ss, 0, sizeof(ss));

    for (i = 0; i < nsam; i++)
    {
      ss[(i*2)+0] = samp1[i];
      ss[(i*2)+1] = samp1[i];
    }

    sf_write_short(opts->wav_out_f, ss, nsam*2);
  }

  //TODO: Codec2 Raw file saving
  // if(mbe_out_dir)
  // {

  // }

  free (samp1);
  free (upsamp);
  free (out);

  #endif

  //Arbitrary Data Storage Across Superframe

  //sanity check
  if (lich_cnt > 5)
    lich_cnt = 5;

  //append incoming arbitrary data segment to pdu_sf array
  memcpy (state->dmr_pdu_sf[0]+(lich_cnt*64), payload+64, 64);

  if (lich_cnt == 5)
  {
    //6 x 8 octets, plus one protocol octet
    uint8_t adata[49]; adata[0] = 0x99;
    pack_bit_array_into_byte_array (state->dmr_pdu_sf[0], adata+1, 48);
    fprintf (stderr, "\n"); //linebreak
    decodeM17PKT (opts, state, adata, 48); //decode Arbitrary Data as UTF-8
    memset (state->dmr_pdu_sf[0], 0, sizeof(state->dmr_pdu_sf[0]));
  }

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

  //look at current viterbi error / cost metric,
  //if exceeds threshold, substitute silence frame
  if (state->m17_viterbi_err >= 25.0f)
  {
    uint64_t silence = 0x010009439CE42108;
    for (int i = 0; i < 8; i++)
    {
      voice1[i] = (silence >> (56ULL-(i*8))) & 0xFF;
      voice2[i] = (silence >> (56ULL-(i*8))) & 0xFF;
    }
  }

  codec2_decode(state->codec2_3200, samp1, voice1);
  codec2_decode(state->codec2_3200, samp2, voice2);

  //hpf_d on codec2 sounds better than not on those .rrc samples
  if (opts->use_hpf_d == 1)
  {
    hpf_dL(state, samp1, nsam);
    hpf_dL(state, samp2, nsam);
  }

  if (opts->slot1_on == 1) //playback if enabled
  {

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

  }

  //Wav file saving
  if(opts->wav_out_f != NULL && state->m17_enc == 0 && opts->dmr_stereo_wav == 1) //WAV
  {
    sf_write_short(opts->wav_out_f, samp1, nsam);
    sf_write_short(opts->wav_out_f, samp2, nsam);
  }
  else if (opts->wav_out_f != NULL && state->m17_enc == 0 && opts->static_wav_file == 1) //Static Wav File
  {
    //convert to stereo for new static wav file setup
    short ss[nsam*2];
    memset (ss, 0, sizeof(ss));

    for (i = 0; i < nsam; i++)
    {
      ss[(i*2)+0] = samp1[i];
      ss[(i*2)+1] = samp1[i];
    }

    sf_write_short(opts->wav_out_f, ss, nsam*2);

    memset (ss, 0, sizeof(ss));

    for (i = 0; i < nsam; i++)
    {
      ss[(i*2)+0] = samp2[i];
      ss[(i*2)+1] = samp2[i];
    }

    sf_write_short(opts->wav_out_f, ss, nsam*2);

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

void prepare_str(dsd_opts * opts, dsd_state * state, float * sbuf)
{
  int i;
  uint16_t soft_bit[2*SYM_PER_PLD];   //raw frame soft bits
  uint16_t d_soft_bit[2*SYM_PER_PLD]; //deinterleaved soft bits
  uint8_t  viterbi_bytes[31];         //packed viterbi return bytes
  uint32_t error = 0;                 //viterbi error

  uint8_t stream_bits[144]; //128+16
  uint8_t payload[128];
  uint8_t end = 9;
  uint16_t fn = 0;

  memset(soft_bit, 0, sizeof(soft_bit));
  memset(d_soft_bit, 0, sizeof(d_soft_bit));
  memset(viterbi_bytes, 0, sizeof(viterbi_bytes));

  memset (stream_bits, 0, sizeof(stream_bits));
  memset (payload, 0, sizeof(payload));

  //libm17 magic
  //slice symbols to soft dibits
  slice_symbols(soft_bit, sbuf);

  //derandomize
  randomize_soft_bits(soft_bit);

  //deinterleave
  reorder_soft_bits(d_soft_bit, soft_bit);

  //viterbi
  error = viterbi_decode_punctured(viterbi_bytes, d_soft_bit+96, p2, 272, 12);

  //track viterbi error / cost metric
  state->m17_viterbi_err = (float)error/(float)0xFFFF;

  //load viterbi_bytes into bits for either data packets or voice packets
  unpack_byte_array_into_bit_array(viterbi_bytes+1, stream_bits, 18); //18*8 = 144

  end = stream_bits[0];
  fn = (uint16_t)convert_bits_into_output(&stream_bits[1], 15);

  //insert fn bits into aes_iv 14 and meta 15 for Initialization Vector
  state->m17_aes_iv[14] = (uint8_t)convert_bits_into_output(&stream_bits[1], 7);
  state->m17_aes_iv[15] = (uint8_t)convert_bits_into_output(&stream_bits[8], 8);

  //Frame Number
  fprintf (stderr, " FN: %04X", fn);

  //viterbi error
  fprintf (stderr, " Ve: %1.1f; ", (float)error/(float)0xFFFF);

  if (end == 1)
    fprintf (stderr, " END;");

  for (i = 0; i < 128; i++)
    payload[i] = stream_bits[i+16];

  //don't play the garbled audio on signature frames
  uint8_t is_sig = 0;
  if (fn >= 0x7FFC)
    is_sig = 1;

  if (state->m17_str_dt == 2 && is_sig == 0)
    M17processCodec2_3200(opts, state, payload);
  else if (state->m17_str_dt == 3 && is_sig == 0)
    M17processCodec2_1600(opts, state, payload, fn%6);

  if (opts->payload == 1 && state->m17_str_dt != 2 && state->m17_str_dt != 3)
  {
    fprintf (stderr, "\n STREAM: ");
    for (i = 0; i < 18; i++)
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&stream_bits[i*8], 8));
  }

  else if (is_sig == 1)
  {
    fprintf (stderr, "\n SIG: ");
    for (i = 2; i < 18; i++)
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&stream_bits[i*8], 8));
  }

}

void processM17STR(dsd_opts * opts, dsd_state * state)
{

  int i, x;
  
  uint8_t dbuf[184];         //384-bit frame - 16-bit (8 symbol) sync pattern (184 dibits)
  float   sbuf[184];         //float symbol buffer
  uint8_t m17_rnd_bits[368]; //368 bits that are still scrambled (randomized)
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_bits[368];     //368 bits that have been de-interleaved and de-scramble
  uint8_t lich_bits[96];
  int lich_err = -1;

  memset (dbuf, 0, sizeof(dbuf));
  memset (sbuf, 0, sizeof(sbuf));
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

  //convert dbuf into a symbol array
  for (i = 0; i < 184; i++)
  {
    if      (dbuf[i] == 0) sbuf[i] = +1.0f;
    else if (dbuf[i] == 1) sbuf[i] = +3.0f;
    else if (dbuf[i] == 2) sbuf[i] = -1.0f;
    else if (dbuf[i] == 3) sbuf[i] = -3.0f;
    else                   sbuf[i] = +0.0f;
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
    prepare_str(opts, state, sbuf);

  //ending linebreak
  fprintf (stderr, "\n");

}
