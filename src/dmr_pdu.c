/*-------------------------------------------------------------------------------
 * dmr_bs.c
 * DMR Data (1/2, 3/4, 1) PDU Decoding
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

//convert a value that is stored as a string decimal into a decimal uint16_t
uint16_t convert_hex_to_dec(uint16_t input)
{
  char num_str[10];
  memset (num_str, 0, sizeof(num_str));
  sprintf (num_str, "%X", input);
  input = 0;
  sscanf (num_str, "%hd", &input);
  return input;
}

void utf16_to_text (dsd_state * state, uint8_t wr, uint16_t len, uint8_t * input)
{
  uint8_t slot = state->currentslot;
  if (wr == 1)
    sprintf (state->event_history_s[slot].Event_History_Items[0].text_message, "%s", ""); //full text string
  // fprintf (stderr, "\n UTF16 Text: ");
  uint16_t ch16 = 0;
  for (uint16_t i = 0; i < len; i += 2)
  {
    ch16 = (uint16_t)input[i+0];
    ch16 <<= 8;
    ch16 |= (uint16_t)input[i+1];
    // fprintf (stderr, " %04X; ", ch16); //debug for raw values to check grouping for offset

    if (ch16 >= 0x20 && ch16 != 0x040D) //if not a linebreak or terminal commmands
      fprintf (stderr, "%lc", ch16);
    else if (ch16 == 0) //if padding (0 could also indicate end of text terminator?)
      fprintf (stderr, "_");
    else if (ch16 == 0x040D) //Ѝ or 0x040D may be ETLF
      fprintf (stderr, " / ");
    else fprintf (stderr, "-");

    //convert to ascii range (will break eastern langauge, but can't do much about that right now)
    char c[2]; c[0] = (char)input[i+1]; c[1] = 0;

    //short version (disabled)
    // if (wr == 1 && i < 76 && input[i] == 0 && input[i+1] < 0x7F && input[i+1] >= 0x20)
    //   strcat (state->dmr_lrrp_gps[slot], c);

    //this is the long version, complete message for logging purposes
    if (wr == 1 && input[i] == 0 && input[i+1] < 0x7F && input[i+1] >= 0x20)
      strcat (state->event_history_s[slot].Event_History_Items[0].text_message, c);

  }

  //add elipses to indicate this is possibly truncated
  // if (wr == 1)
  //   strcat (state->dmr_lrrp_gps[slot], "...");

  //debug
  // if (wr == 1)
  //   fprintf (stderr, "%s", state->dmr_lrrp_gps[slot]);
}

void utf8_to_text (dsd_state * state, uint8_t wr, uint16_t len, uint8_t * input)
{
  uint8_t slot = state->currentslot;
  fprintf (stderr, "\n UTF8 Text: ");

  if (wr == 1)
    sprintf (state->event_history_s[slot].Event_History_Items[0].text_message, "%s", ""); //full text string

  for (uint16_t i = 0; i < len; i++)
  {
    if (input[i] >= 0x20 && input[i] < 0x7F) //if not a linebreak or terminal commmands
      fprintf (stderr, "%c", input[i]);
    else if (input[i] == 0) //if padding (0 could also indicate end of text terminator?)
      fprintf (stderr, "_");
    // else if (input[i] == 0x03) //ASCII end of text (observed on the NMEA LOCN ones anyways)
    //   break;
    else fprintf (stderr, "-");

    
    char c = input[i];

    //for now, just rip the first 40 or so chars lower byte value
    //in the ASCII Range (should be alright for a quick visual)
    // if (wr == 1 && i < 38 && c < 0x7F && c >= 0x20)
    //   strcat (state->dmr_lrrp_gps[slot], &c);

    //this is the long version, complete message for logging purposes
    if (wr == 1 && c < 0x7F && c >= 0x20)
      strcat (state->event_history_s[slot].Event_History_Items[0].text_message, &c);

  }

  //add elipses to indicate this is possibly truncated
  // if (wr == 1)
  //   strcat (state->dmr_lrrp_gps[slot], "...");

}

void dmr_sd_pdu (dsd_opts * opts, dsd_state * state, uint16_t len, uint8_t * DMR_PDU)
{

  uint8_t slot = state->currentslot;
  uint16_t offset = 0; //sanity check of sorts, prevent extra long line print outs in the console
  if (len > 23)
    offset = 23;

  // if (DMR_PDU[0] == 0x01) //found some on another system that is 00 here, and not a Loction
  if (state->data_header_format[state->currentslot] == 13) //only short data: defined format (testing)
  {
    utf8_to_text(state, 0, len-offset, DMR_PDU+offset);
    dmr_locn(opts, state, len, DMR_PDU);
    sprintf (state->event_history_s[slot].Event_History_Items[0].gps_s, "%s", state->dmr_lrrp_gps[slot]);
    state->event_history_s[slot].Event_History_Items[0].color_pair = 4; //Remus, add this line to a decode to change its line color
  }
  else
  {
    if (len >= (127*18)) len = 127*18; //sanity check of sorts, prevent extra long line print outs in the console
    utf8_to_text(state, 0, len, DMR_PDU); //generic catch-all to see if anything relevant is there
    // utf16_to_text(state, 0, len, DMR_PDU); //generic catch-all to see if anything relevant is there
  }

  //dump to event history
  uint32_t source = state->dmr_lrrp_source[slot];
  uint32_t target = state->dmr_lrrp_target[slot];
  char comp_string[500]; memset (comp_string, 0, sizeof(comp_string));
  sprintf (comp_string, "Short Data SRC: %d; TGT: %d; ", source, target);
  watchdog_event_datacall (opts, state, source, target, comp_string, slot);

}

//reading ETSI, seems like these aren't compressed, just that they are preset indexed values on the radio
void dmr_udp_comp_pdu (dsd_opts * opts, dsd_state * state, uint16_t len, uint8_t * DMR_PDU)
{
  UNUSED(opts); UNUSED(state);
  uint16_t ipid = (DMR_PDU[0] << 8) | DMR_PDU[1];
  uint16_t said = (DMR_PDU[2] >> 4) & 0xF;
  uint16_t daid = (DMR_PDU[2] >> 0) & 0xF;

  //the manual shows this is the lsb and msb of the header compression 'opcode', but only zero is defined
  uint8_t  op1  = (DMR_PDU[3] >> 7) & 1;
  uint8_t  op2  = (DMR_PDU[4] >> 7) & 1;
  uint8_t opcode = (op1 << 1) | op2;

  uint8_t  spid  = (DMR_PDU[3] >> 0) & 0x7F;
  uint8_t  dpid  = (DMR_PDU[4] >> 0) & 0x7F;

  //configure SAID / DAID string
  char addrstring[2][35]; memset(addrstring, 0, sizeof(addrstring));

  //said
  if (said == 0)
    sprintf (addrstring[0], "%s", "Radio Network");
  else if (said == 1)
    sprintf (addrstring[0], "%s", "Ethernet");
  else if (said > 1 && said < 11)
    sprintf (addrstring[0], "%s", "Reserved");
  else
    sprintf (addrstring[0], "%s", "Manufacturer Specific");

  //daid
  if (daid == 0)
    sprintf (addrstring[1], "%s", "Radio Network");
  else if (daid == 1)
    sprintf (addrstring[1], "%s", "Ethernet");
  else if (daid == 1)
    sprintf (addrstring[1], "%s", "Group Network");
  else if (daid > 2 && daid < 11)
    sprintf (addrstring[1], "%s", "Reserved");
  else
    sprintf (addrstring[1], "%s", "Manufacturer Specific");

  //according to ETSI, if spid and/or dpid is zero, their respective port is defined in this header, otherwise, they are preset indexed values

  //look for and set optional port values and the ptr value to start of data
  uint16_t ptr = 5;

  if (spid == 0 && dpid == 0)
  {
    spid = (DMR_PDU[5] << 8) | DMR_PDU[6];
    dpid = (DMR_PDU[7] << 8) | DMR_PDU[8];
    ptr = 9;
  }
  else if (spid == 0)
  {
    spid = (DMR_PDU[5] << 8) | DMR_PDU[6];
    ptr = 7;
  }
  else if (dpid == 0)
  {
    dpid = (DMR_PDU[5] << 8) | DMR_PDU[6];
    ptr = 7;
  }
  else ptr = 5;

  char portstring[2][35]; memset(portstring, 0, sizeof(portstring));

  //spid
  if (spid == 1)
  {
    // spid = 5016;
    sprintf (portstring[0], "%s", "UTF-16BE Text Message");
  }
  else if (spid == 2)
  {
    // spid = 5017;
    sprintf (portstring[0], "%s", "Location Interface Protocol");
  }
  else if (spid > 2 && spid < 191)
  {
    // spid = spid;
    sprintf (portstring[0], "%s", "Reserved");
  }
  else
  {
    // spid = spid;
    sprintf (portstring[0], "%s", "Manufacturer Specific");
  }

  //dpid
  if (dpid == 1)
  {
    // dpid = 5016;
    sprintf (portstring[1], "%s", "UTF-16BE Text Message");
  }
  else if (dpid == 2)
  {
    // dpid = 5017;
    sprintf (portstring[1], "%s", "Location Interface Protocol");
  }
  else if (dpid > 2 && dpid < 191)
  {
    // dpid = dpid;
    sprintf (portstring[1], "%s", "Reserved");
  }
  else
  {
    // dpid = dpid;
    sprintf (portstring[1], "%s", "Manufacturer Specific");
  }

  fprintf (stderr, "\n Compressed IP Idx: %d; Opcode: %d; Src Idx: %d (%s); Dst Idx: %d (%s); ", ipid, opcode, said, addrstring[0], daid, addrstring[1]);
  fprintf (stderr, "\n Src Port Idx: %d (%s); Dst Port Idx: %d (%s); ", spid, portstring[0], dpid, portstring[1]);

  //sanity check
  if (len > ptr)
    len -= ptr;

  //decode known types
  if (spid == 1 || dpid == 1)
    utf16_to_text(state, 1, len, DMR_PDU+ptr); //assumming text starts right at the ptr value
  else if (spid == 2 || dpid == 2)
  {
    //untested
    uint8_t bits[127*8]; memset(bits, 0, sizeof(bits));
    unpack_byte_array_into_bit_array(DMR_PDU+ptr, bits, len*sizeof(uint8_t));
    lip_protocol_decoder(opts, state, bits);
  }
  else fprintf (stderr, "Unknown Decode Format;");

  uint8_t slot = 0;
  if (state->currentslot == 1)
    slot = 1;

  char comp_string[500]; memset (comp_string, 0, sizeof(comp_string));
  sprintf (comp_string, "IPC: %d; OP: %d; SRC: %d:%d (%s):(%s); DST: %d:%d (%s):(%s); ", ipid, opcode, said, spid, addrstring[0], portstring[0], daid, dpid, addrstring[1], portstring[1]);
  watchdog_event_datacall (opts, state, said, daid, comp_string, slot);

}

//IP PDU header decode and port forward to appropriate decoder
void decode_ip_pdu (dsd_opts * opts, dsd_state * state, uint16_t len, uint8_t * input)
{

  uint8_t slot = state->currentslot;

  //the IPv4 Header
  uint8_t version = input[0] >> 4; //may need to read ahead and get this value before coming here
  uint8_t ihl = input[0] & 0xF; //decompressed value is 0x05 (may need to check this before preceeding)
  uint8_t tos = input[1]; //0
  uint16_t tlen = (input[2] << 8) | input[3]; //IPv4 header length (20 bytes) + UDP header length (5 bytes) + Packet Data
  uint16_t iden = (input[4] << 8) | input[5];
  uint8_t ipf = input[6] >> 5; //0
  uint16_t offset = ((input[6] & 0x1F) << 8) | input[7]; //0
  uint8_t ttl = input[8]; //should be 0x40 (64)
  uint8_t prot = input[9];
  uint16_t hsum = (input[10] << 8) | input[11];

  if (opts->payload == 1)
    fprintf (stderr, "\n IPv%d; IHL: %d; Type of Service: %d; Total Len: %d; IP ID: %04X; Flags: %X;\n Fragment Offset: %d; TTL: %d; Protocol: 0x%02X; Checksum: %04X; PDU Len: %d;", version, ihl, tos, tlen, iden, ipf, offset, ttl, prot, hsum, len);

  //take a look at the src, dst, and port indicated (assuming both ports will match)
  uint32_t src24 = (input[13] << 16) | (input[14] << 8) | input[15];
  uint32_t dst24 = (input[17] << 16) | (input[18] << 8) | input[19];
  uint16_t port1 = (input[20] << 8) | input[21];
  uint16_t port2 = (input[22] << 8) | input[23];
  fprintf (stderr, "\n SRC(24): %08d; IP: %03d.%03d.%03d.%03d; ", src24, input[12], input[13], input[14], input[15]);
  if (prot == 0x11) fprintf (stderr, "Port: %04d; ", port1);
  fprintf (stderr, "\n DST(24): %08d; IP: %03d.%03d.%03d.%03d; ", dst24, input[16], input[17], input[18], input[19]);
  if (prot == 0x11) fprintf (stderr, "Port: %04d; ", port2);

  //IP Protocol List: https://en.wikipedia.org/wiki/List_of_IP_protocol_numbers
  if (prot == 0x01) //ICMP
  {
    uint8_t icmp_type = input[20];
    uint8_t icmp_code = input[21];
    uint16_t icmp_chk = port2;
    fprintf (stderr, "\n ICMP Protocol; Type: %02X; Code: %02X; Checksum: %02X;", icmp_type, icmp_code, icmp_chk);
    if (icmp_type == 3)
    {
      fprintf (stderr, " Destination");
      if (icmp_code == 0)
        fprintf (stderr, " Network");
      else if (icmp_code == 1)
        fprintf (stderr, " Host");
      else if (icmp_code == 2)
        fprintf (stderr, " Protocol");
      else if (icmp_code == 3)
        fprintf (stderr, " Port");
      fprintf (stderr, " Unreachable;");
    }
    //see: https://en.wikipedia.org/wiki/Internet_Control_Message_Protocol
    //look at attached message, if present
    if (input[28] == 0x45) //if another chained IPv4 header and/or message
    {
      fprintf (stderr, "\n ------------Attached Message-------------");
      decode_ip_pdu(opts, state, len-28, input+28);
    }

  }

  else if (prot == 0x11) //UDP
  {
    uint16_t udp_len = (input[24] << 8) | input[25]; //This UDP Length information element is the length in bytes of this user datagram including this header and the application data (no IP header)
    uint16_t udp_chk = (input[26] << 8) | input[27];
    fprintf (stderr, "\n UDP Protocol; Datagram Len: %d; UDP Checksum: %04X; ", udp_len, udp_chk);

    //if dst port and src prt don't match, then make it so
    if (port2 != port1) port1 = port2;

    if (port1 == 231 && port2 == 231)
    {
      fprintf (stderr, "Cellocator;");
      sprintf (state->dmr_lrrp_gps[slot], "Cellocator SRC: %d; DST: %d;", src24, dst24);
      if (len > 28)
        decode_cellocator(opts, state, input+28, len-28);
    }
    else if (port1 == 4001 && port2 == 4001)
    {
      //sanity check
      if (len > 33)
        len -= 33;

      fprintf (stderr, "LRRP;");
      dmr_lrrp (opts, state, len, src24, dst24, input+28); //len is offset with IP and UDP header lens, 4 CRC, and 1 for the 0D token
      state->event_history_s[slot].Event_History_Items[0].color_pair = 4; //Remus, add this line to a decode to change its line color
    }
    else if (port1 == 4004 && port2 == 4004)
    {
      fprintf (stderr, "XCMP;");
      sprintf (state->dmr_lrrp_gps[slot], "XCMP SRC: %d; DST: %d;", src24, dst24);
      state->event_history_s[slot].Event_History_Items[0].color_pair = 4; //Remus, add this line to a decode to change its line color
    }
    else if (port1 == 4005 && port2 == 4005)
    {
      fprintf (stderr, "ARS;");
      //TODO: ARS Decoder
      sprintf (state->dmr_lrrp_gps[slot], "ARS SRC: %d; DST: %d; ", src24, dst24);
      utf8_to_text(state, 0, 10, input+28); //seen some ARS radio IDs in ASCII/ISO7/UTF8 format here
    }
    else if (port1 == 4007 && port2 == 4007)
    {
      int tms_len = (input[28] << 8) | input[29];
      fprintf (stderr, " TMS ");
      fprintf (stderr, "Len: %d; ", tms_len);

      //loosely based on information found here: https://github.com/OK-DMR/ok-dmrlib/blob/master/okdmr/dmrlib/motorola/text_messaging_service.py
      //look at header and any optional values (simplified version)
      int tms_ptr = 30;
      uint8_t tms_hdr = input[tms_ptr++]; //first header
      uint8_t tms_ack = (tms_hdr >> 0) & 0xF;
      if (opts->payload == 1)
        fprintf (stderr, "HDR: %02X; ", tms_hdr);

      //optional address len and address value
      uint8_t tms_adl = input[tms_ptr++];
      if (tms_adl != 0)
      {
        //the encoding seems to start at the adl (len) byte, but does not include it
        //so, to get the decoder to work, we well go back one byte and zero it out
        tms_ptr--; //back up one position
        input[tms_ptr] = 0;
        fprintf (stderr, "Address Len: %d; Address: ", tms_adl);
        utf16_to_text(state, 1, tms_adl-4, input+tms_ptr); //addresses seem to have an extra .4 value on end (4 octets)
        input[tms_ptr] = tms_adl; //restore this byte
        tms_ptr += tms_adl; //the len value seems to include the len byte
        tms_ptr += 1; //advance the ptr back to negate the negation
        fprintf (stderr, "; ");
      }

      //any additional headers (don't care)
      uint8_t tms_more = input[tms_ptr] >> 7;
      while(tms_more)
      {
        uint8_t tms_b1 = input[tms_ptr++];
        uint8_t tms_b2 = input[tms_ptr];
        if (opts->payload == 1)
          fprintf (stderr, "B1: %02X; B2: %02X; ", tms_b1, tms_b2);
        tms_more = tms_b1 >> 7;
        if (tms_more) tms_ptr++;
      }

      sprintf (state->dmr_lrrp_gps[slot], "TMS SRC: %d; DST: %d; ", src24, dst24);
      if (tms_ack == 0)
      {
        //sanity check, see if the ptr is odd (start always seems to be an odd value)
        if (!tms_ptr%1) tms_ptr++;

        //sanity check on tms_len at this point (fix offset for below)
        if (tms_len > 31)
          tms_len -= (tms_ptr - 31);

        //the first utf16 char seems to be encoded as XXYY where XX is not part of the
        //encoding for the character, so zero that byte out and restore it later
        tms_ptr -= 2; //back up two positions
        uint8_t temp = input[tms_ptr];
        input[tms_ptr] = 0;

        //debug
        if (opts->payload == 1)
          fprintf (stderr, "Ptr: %d; Len: %d;", tms_ptr, tms_len);
        fprintf (stderr, "\n Text: ");
        utf16_to_text(state, 1, tms_len, input+tms_ptr);

        input[tms_ptr] = temp; //restore byte
        tms_ptr += 2;
      }
      else
      {
        strcat (state->dmr_lrrp_gps[slot], "Acknowledgment;");
        fprintf (stderr, "Acknowledgment;");
      }
    }
    else if (port1 == 4008 && port2 == 4008)
    {
      fprintf (stderr, "Telemetry;");
      sprintf (state->dmr_lrrp_gps[slot], "Telemetry SRC: %d; DST: %d;", src24, dst24);
    }
    else if (port1 == 4009 && port2 == 4009)
    {
      fprintf (stderr, "OTAP;");
      sprintf (state->dmr_lrrp_gps[slot], "OTAP SRC: %d; DST: %d;", src24, dst24);
    }
    else if (port1 == 4012 && port2 == 4012)
    {
      fprintf (stderr, "Battery Management;");
      sprintf (state->dmr_lrrp_gps[slot], "Batt. Man. SRC: %d; DST: %d;", src24, dst24);
    }
    else if (port1 == 4013 && port2 == 4013)
    {
      fprintf (stderr, "Job Ticket Server;");
      sprintf (state->dmr_lrrp_gps[slot], "JTS SRC: %d; DST: %d;", src24, dst24);
    }
    else if (port1 == 4069 && port2 == 4069)
    {
      //https://trbonet.com/kb/how-to-configure-dt500-and-mobile-radio-to-work-with-scada-sensors/
      fprintf (stderr, "TRBOnet SCADA;");
      sprintf (state->dmr_lrrp_gps[slot], "SCADA SRC: %d; DST: %d;", src24, dst24);
    }
    //ETSI specific -- unknown entry value, assuming +28
    else if (port1 == 5016 && port2 == 5016)
    {
      //sanity check
      if (len > 29)
        len -= 29;

      fprintf (stderr, "ETSI TMS;");
      sprintf (state->dmr_lrrp_gps[slot], "ETSI TMS SRC: %d; DST: %d; ", src24, dst24);
      utf16_to_text(state, 1, len, input+28);
    }
    //Vertex Standard TMS Port
    else if (port1 == 5007 && port2 == 5007)
    {

      //get pad amount to see where to stop parsing UTF16 text
      int pad = state->data_block_poc[slot];

      //sanity check
      if (len > (50+pad))
        len = len - pad - 50;

      fprintf (stderr, "VTX STD TMS; ");

      //+28, get 9 bytes for unknown "header" information
      //unknown elements 0E0002000000000000, could include a len value or type (utf-16, utf-8, is07?)
      //the sample has a len of 8 utf-16 chars, only way to get that is on the 2 if not byte aligned (1000)
      if (opts->payload == 1)
      {
        // fprintf (stderr, "Len: %03d; ", len);
        fprintf (stderr, "??: ");
        for (int i = 0; i < 9; i++)
          fprintf (stderr, "%02X", input[i+28]);
        fprintf (stderr, "; Text: ");
      }

      sprintf (state->dmr_lrrp_gps[slot], "VTX TMS SRC: %d; DST: %d; ", src24, dst24);
      utf16_to_text(state, 1, len, input+49);
    }
    else if (port1 == 5017 && port2 == 5017)
    {
      //sanity check
      if (len > 32)
        len -= 32;

      uint8_t bits[127*12*8]; memset(bits, 0, sizeof(bits));
      unpack_byte_array_into_bit_array(input+28, bits, len*sizeof(uint8_t));
      lip_protocol_decoder(opts, state, bits);
    }
    //known P25 Ports
    else if (port1 == 49198 && port2 == 49198)
    {
      sprintf (state->dmr_lrrp_gps[slot], "P25 Tier 2 LOCN SRC(IP): %d.%d.%d.%d; DST(IP): %d.%d.%d.%d; ",
               input[12], input[13], input[14], input[15], input[16], input[17], input[18], input[19]);
      fprintf (stderr, "P25 Tier 2 Location Service;"); //LRRP
      dmr_lrrp (opts, state, len, src24, dst24, input+28); //same offsets as DMR variety (unknown)
    }
    else
    {
      sprintf (state->dmr_lrrp_gps[slot], "IP SRC: %d.%d.%d.%d:%d; DST: %d.%d.%d.%d:%d; Unknown UDP Port;", input[12], input[13], input[14], input[15], port1, input[16], input[17], input[18], input[19], port2);
      fprintf (stderr, "Unknown UDP Port;");
      // if (len > 28) //default catch all (debug only)
      //   utf8_to_text(state, 0, len-28, input+28);
      // else utf8_to_text(state, 0, len, input+28);
    }

  }

  else
  {
    sprintf (state->dmr_lrrp_gps[slot], "IP SRC: %d.%d.%d.%d; DST: %d.%d.%d.%d; Unknown IP Protocol: %d; ", input[12], input[13], input[14], input[15], input[16], input[17], input[18], input[19], prot);
    fprintf(stderr, "Unknown IP Protocol: %02X;", prot);
    // if (len > 28) //default catch all (debug only)
    //   utf8_to_text(state, 0, len-28, input+28);
    // else utf8_to_text(state, 0, len, input+28);
  }

  watchdog_event_datacall (opts, state, src24, dst24, state->dmr_lrrp_gps[slot], slot);

}

//The contents of this function are mostly trial and error
void dmr_lrrp (dsd_opts * opts, dsd_state * state, uint16_t len, uint32_t source, uint32_t dest, uint8_t * DMR_PDU)
{

  uint16_t message_len = 0;
  uint8_t slot = state->currentslot;
  uint8_t lrrp_confidence = 0; //variable to increment based on number of tokens found, the more, the higher the confidence level
  uint8_t lrrp_type = DMR_PDU[0];
  uint8_t is_request = 0;
  uint8_t is_response = 0;

  //source/dest and ports (this is grabbed in the IP decoding phase)
  if (source != 0) lrrp_confidence++;
  if (dest   != 0) lrrp_confidence++;

  //time
  uint16_t year = 0;
  uint16_t month = 0;
  uint16_t day = 0;
  uint16_t hour = 0;
  uint16_t minute = 0;
  uint16_t second = 0;

  //lat-long-radius-alt
  uint32_t lat = 0;
  uint32_t lon = 0;
  uint16_t rad = 0; //or Azimuth in Circle 3D?
  uint8_t alt = 0;
  double lat_unit = (double)180/(double)4294967295;
  double lon_unit = (double)360/(double)4294967295;
  double lat_fin = 0.0; //calculated values
  double lon_fin = 0.0; //calculated values
  int lat_sign = 1; //positive 1, or negative 1
  int lon_sign = 1; //positive 1, or negative 1

  uint16_t vel = 0;
  double velocity = 0;
  uint8_t vel_set = 0;
  UNUSED(vel);

  //track, direction, degrees
  uint8_t degrees = 0;
  uint8_t deg_set = 0;

  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");

  //debug passed LRRP message
  // fprintf (stderr, "\n LRRP (Debug): ");
  // for (uint16_t i = 0; i < len; i++)
  //   fprintf (stderr, "%02X ", DMR_PDU[i]);

  //start looking for tokens
  for (uint16_t i = 0; i < len; i++)
  {
    uint8_t token = DMR_PDU[i];
    switch(token){
      case 0x0F: //Triggered Location Stop Request
      case 0x05: //Immediate Location Request
      case 0x09: //Triggered Location Start Request
      case 0x14: //Protocol Version Request
        if (i == 0) //see if this is the first octet, otherwise, can't verify this is going to work
        {
          message_len = DMR_PDU[i+1];
          i = len; //go to end
          lrrp_confidence++;
          is_request = 1;
        }
        break;
      case 0x07: //Immediate Location Response
      case 0x0B: //Triggered Location Start Response
      case 0x0D: //Triggered Location
      case 0x11: //Triggered Location Stop Response
      case 0x15: //Protocol Version Response
        if (i == 0) //see if this is the first octet, otherwise, can't verify this is going to work
        {
          message_len = DMR_PDU[i+1];
          i += 3; //next byte is len, then next two are usually 0x22 0xXX or 0x23 0xXX
          lrrp_confidence++;
          is_response = 1;
        }
        break;

      //tokens
      case 0x51: //circle-2d
      case 0x54: //circle-3d
      case 0x55: //circle-3d
        if (message_len > 0 && lat == 0)
        {
          //debug
          // fprintf (stderr, "\n I: %d; Token: %02X; ", i, token); //must be a bad sample

          lat = ( ( (DMR_PDU[i+1]           <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          lon = ( ( (DMR_PDU[i+5]           <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          rad = (DMR_PDU[i+9] << 8) + DMR_PDU[i+10];
          i += 10;
          if (lat > 0 && lon > 0) lrrp_confidence++;
          else lat = 0;
        }
        break;

      case 0x34: //Time
      case 0x35: //Time
        if (message_len > 0 && year == 0)
        {
          year = (DMR_PDU[i+1] << 6) + (DMR_PDU[i+2] >> 2);
          month = ((DMR_PDU[i+2] & 0x3) << 2) + ((DMR_PDU[i+3] & 0xC0) >> 6);
          day =  ((DMR_PDU[i+3] & 0x30) >> 1) + ((DMR_PDU[i+3] & 0x0E) >> 1);
          hour = ((DMR_PDU[i+3] & 0x01) << 4) + ((DMR_PDU[i+4] & 0xF0) >> 4);
          minute = ((DMR_PDU[i+4] & 0x0F) << 2) + ((DMR_PDU[i+5] & 0xC0) >> 6);
          second = (DMR_PDU[i+5] & 0x3F);
          i += 5;
          //sanity check
          if (year > 2000 && year <= 2026) lrrp_confidence++;
          if (year > 2025 || year < 2000) year = 0; //needs future proofing
        }
        break;

      case 0x66: //point-2d
        if (message_len > 0 && lat == 0)
        {
          lat = ( ( (DMR_PDU[i+1]           <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          lon = ( ( (DMR_PDU[i+5]           <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          i += 8; //+= 9? including current ptr?
          lrrp_confidence++;
        }
        break;
      case 0x69: //point-3d
      case 0x6A: //point-3d
        if (message_len > 0 && lat == 0)
        {
          lat = ( ( (DMR_PDU[i+1]           <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          lon = ( ( (DMR_PDU[i+5]           <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          alt =  DMR_PDU[i+9];
          i += 9;
          if (lat > 0 && lon > 0) lrrp_confidence++;
          else lat = 0;
        }
        break;
      case 0x36: //protocol-version
        i += 1;
        //lrrp_confidence++;
        break;
      //speed/velocity
      case 0x70: //speed-virt
      case 0x6C: //speed-hor
        if (message_len > 0 && vel_set == 0)
        {
          vel = (DMR_PDU[i+1] << 8) + DMR_PDU[i+2]; //raw value
          velocity = ( ((double)( (DMR_PDU[i+1] << 8) + DMR_PDU[i+2] )) / ( (double)255.0f)); //mps
          vel_set = 1;
          i += 2;
          lrrp_confidence++;
        }
        break;
      case 0x56: //direction-hor
        if (message_len > 0 && deg_set == 0)
        {
          degrees = DMR_PDU[i+1] * 2;
          deg_set = 1;
          i += 1;
          lrrp_confidence++;
        }
        break;

      //unknown tokens
      default:
        //do nothing
        break;
    }
  }

  if (message_len > 0)
  {
    fprintf (stderr, "%s", KYEL);

    if (lrrp_confidence >= 3) //minimal of src, dst, and message len indicator
    {

      if (year)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Time:");
        fprintf (stderr, " %04d.%02d.%02d %02d:%02d:%02d", year, month, day, hour, minute, second);

      }

      if (lat)
      {
        fprintf (stderr, "\n");
        //need to check these new calcs for accuracy accross the globe, both lat and lon

        //two's compliment-ish testing on these bytes
        if (lat & 0x80000000) //8
        {
          lat = lat & 0x7FFFFFFF;
          lat_sign = -1;
          // lat = 0x80000000 - lat; //not sure why this doesn't work here like it does on lon, extra bit?
        }
        if (lon & 0x80000000)
        {
          lon = lon & 0x7FFFFFFF;
          lon_sign = -1;
          lon = 0x80000000 - lon;
        }

        lat_fin = (double)lat * lat_unit * lat_sign;
        lon_fin = (double)lon * lon_unit * lon_sign;

        fprintf (stderr, " Lat: %.5lf", lat_fin);
        fprintf (stderr, " Lon: %.5lf", lon_fin);
        fprintf (stderr, " (%.5lf, %.5lf)", lat_fin , lon_fin);

      }

      if (rad)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Radius: %dm", rad); //unsure of 'units' or calculation for radius (meters?)
      }
      if (alt)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Altitude: %dm", alt); //unsure of 'units' or calculation for alt (meters?)
      }
      if (vel_set)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Speed: %.4lf m/s %.4lf km/h %.4lf mph", velocity, (3.6 * velocity), (2.2369 * velocity));
      }

      if (deg_set)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, " Track: %d%s", degrees, deg_glyph);
      }

      //write to LRRP file, if a lat/lon is present
      if (opts->lrrp_file_output == 1 && lat_fin != 0.0f && lon_fin != 0.0f)
      {
        char * timestr  = getTimeC();
        char * datestr  = getDateS();

        //open file by name that is supplied in the ncurses terminal, or cli
        FILE * pFile; //file pointer
        pFile = fopen (opts->lrrp_out_file, "a");

        //write current date/time if not present in LRRP data
        if (!year) fprintf (pFile, "%s\t", datestr ); //current date, only add this IF no included timestamp in LRRP data?
        if (!year) fprintf (pFile, "%s\t", timestr ); //current timestamp, only add this IF no included timestamp in LRRP data?
        if (year) fprintf (pFile, "%04d/%02d/%02d\t%02d:%02d:%02d\t", year, month, day, hour, minute, second); //add timestamp from decoded audio if available

        //write data header source if not available in lrrp data
        if (!source) fprintf (pFile, "%08lld\t", state->dmr_lrrp_source[state->currentslot]); //source address from data header
        if (source) fprintf (pFile, "%08d\t", source); //add source form decoded audio if available, else its from the header

        if (timestr != NULL)
        {
          free (timestr);
          timestr = NULL;
        }

        if (datestr != NULL)
        {
          free (datestr);
          datestr = NULL;
        }

        fprintf (pFile, "%.5lf\t", lat_fin);
        fprintf (pFile, "%.5lf\t", lon_fin);
        fprintf (pFile, "%.3lf\t ", (velocity * 3.6) );
        fprintf (pFile, "%d\t",degrees);

        fprintf (pFile, "\n");
        fclose (pFile);
      }

      //save to string for ncurses
      if (!source) source = state->dmr_lrrp_source[state->currentslot];
      char velstr[20];
      char degstr[20];
      char lrrpstr[100];
      sprintf (lrrpstr, "%s", "");
      sprintf (velstr, "%s", "");
      sprintf (degstr, "%s", "");
      if (lat) sprintf (lrrpstr, "LRRP SRC: %0d; (%lf, %lf)", source, lat_fin, lon_fin);
      else if (is_request) sprintf (lrrpstr, "LRRP SRC: %0d; Request from TGT: %d;", source, dest);
      else if (is_response) sprintf (lrrpstr, "LRRP SRC: %0d; Response to TGT: %d;", source, dest);
      else sprintf (lrrpstr, "LRRP SRC: %0d; Unknown Format %02X; TGT: %d;; ", lrrp_type, source, dest);
      if (vel_set) sprintf (velstr, " %.4lf km/h", velocity * 3.6);
      if (deg_set) sprintf (degstr, " %d%s  ", degrees, deg_glyph);
      sprintf (state->dmr_lrrp_gps[slot], "%s%s%s", lrrpstr, velstr, degstr);

      if (!lat)
        fprintf (stderr, "\n %s", state->dmr_lrrp_gps[slot]);


    }
    else
    {
      char lrrpstr[100];
      sprintf (lrrpstr, "%s", "");
      sprintf (lrrpstr, "LRRP SRC: %0d; Unknown Format %02X; TGT: %d;", lrrp_type, source, dest);
      sprintf (state->dmr_lrrp_gps[slot], "%s", lrrpstr);
      fprintf (stderr, "\n %s", state->dmr_lrrp_gps[slot]);
    }

  }

  else
  {
    char lrrpstr[100];
    sprintf (lrrpstr, "%s", "");
    sprintf (lrrpstr, "LRRP SRC: %0d; Unknown Format %02X; TGT: %d;", lrrp_type, source, dest);
    sprintf (state->dmr_lrrp_gps[slot], "%s", lrrpstr);
    fprintf (stderr, "\n %s", state->dmr_lrrp_gps[slot]);
  }

  fprintf (stderr, "%s", KNRM);

}

void dmr_locn (dsd_opts * opts, dsd_state * state, uint16_t len, uint8_t * DMR_PDU)
{

  uint8_t slot = state->currentslot;
  uint8_t source = state->dmr_lrrp_source[slot];

  //flags if certain data type is present
  uint8_t time = 0;
  uint8_t lat  = 0;
  uint8_t lon  = 0;

  //date-time variables
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  uint8_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;

  //lat and lon variables
  uint16_t lat_deg = 0;
  uint16_t lat_min = 0;
  uint16_t lat_sec = 0;

  uint16_t lon_deg = 0;
  uint16_t lon_min = 0;
  uint16_t lon_sec = 0;

  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");

  //more strings...
  char locnstr[50];
  char latstr[75];
  char lonstr[75];
  sprintf (locnstr, "%s", "");
  sprintf (latstr,  "%s", "");
  sprintf (lonstr,  "%s", "");

  int lat_sign = 1; //positive 1 or negative 1
  int lon_sign = 1; //positive 1 or negative 1

  //start looking for specific bytes corresponding to 'letters' A (time), NSEW (ordinal directions), etc
  for (uint16_t i = 0; i < len; i++)
  {
    switch(DMR_PDU[i]){
      case 0x41: //A -- time and date
        time   = 1;
        hour   = ((DMR_PDU[i+1] - 0x30) << 4) | (DMR_PDU[i+2] - 0x30);
        minute = ((DMR_PDU[i+3] - 0x30) << 4) | (DMR_PDU[i+4] - 0x30);
        second = ((DMR_PDU[i+5] - 0x30) << 4) | (DMR_PDU[i+6] - 0x30);
        //think this is in day, mon, year format
        day   = ((DMR_PDU[i+7] -  0x30) << 4) | (DMR_PDU[i+8] - 0x30);
        month = ((DMR_PDU[i+9] -  0x30) << 4) | (DMR_PDU[i+10] - 0x30);
        year  = ((DMR_PDU[i+11] - 0x30) << 4) | (DMR_PDU[i+12] - 0x30);
        i += 12;
        break;

      case 0x53: //S -- South
        lat_sign = -1;
      case 0x4E: //N -- North
        lat     = 1;
        lat_deg = ((DMR_PDU[i+1] - 0x30) << 4) | (DMR_PDU[i+2] - 0x30);
        lat_min = ((DMR_PDU[i+3] - 0x30) << 4) | (DMR_PDU[i+4] - 0x30);
        lat_sec = ((DMR_PDU[i+6] - 0x30) << 12) | ((DMR_PDU[i+7] - 0x30) << 8) | ((DMR_PDU[i+8] - 0x30) << 4) | ((DMR_PDU[i+9] - 0x30) << 0);
        i += 8;
        break;

      case 0x57: //W -- West
        lon_sign = -1;
      case 0x45: //E -- East
        lon     = 1;
        lon_deg = ((DMR_PDU[i+1] - 0x30) << 8) | ((DMR_PDU[i+2] - 0x30) << 4) | ((DMR_PDU[i+3] - 0x30) << 0);
        lon_min = ((DMR_PDU[i+4] - 0x30) << 4) | (DMR_PDU[i+5] - 0x30);
        lon_sec = ((DMR_PDU[i+7] - 0x30) << 12) | ((DMR_PDU[i+8] - 0x30) << 8) | ((DMR_PDU[i+9] - 0x30) << 4) | ((DMR_PDU[i+10] - 0x30) << 0);
        i += 8;
        break;

      default:
        //do nothing
        break;

    } //end switch

  } //for i

  if (lat && lon)
  {
    //convert dd.MMmmmm to decimal format
    double latitude  = 0.0f;
    double longitude = 0.0f;
    double velocity  = 0.0f;
    uint16_t degrees  = 0;

    //convert hex string chars representing decimal values to integers
    lat_deg = convert_hex_to_dec (lat_deg);
    lat_min = convert_hex_to_dec (lat_min);
    lat_sec = convert_hex_to_dec (lat_sec);

    lon_deg = convert_hex_to_dec (lon_deg);
    lon_min = convert_hex_to_dec (lon_min);
    lon_sec = convert_hex_to_dec (lon_sec);

    //dd.MMmmmm to decimal formatting
    latitude  = (double)lat_sign * ( (double)lat_deg + ((double)lat_min / (double)60.0f) + ((double)lat_sec / (double)600000.0f));
    longitude = (double)lon_sign * ( (double)lon_deg + ((double)lon_min / (double)60.0f) + ((double)lon_sec / (double)600000.0f));

    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, "\n NMEA / LOCN; Source: %d;", source);
    if (time) fprintf (stderr, " 20%02X/%02X/%02X %02X:%02X:%02X", year, month, day, hour, minute, second);
    fprintf (stderr, " (%.5lf%s, %.5lf%s);", latitude, deg_glyph, longitude, deg_glyph);

    //string manip for ncurses terminal display
    sprintf (locnstr, "NMEA / LOCN; Source: %d ", source);
    sprintf (latstr, "(%.5lf%s, ", latitude, deg_glyph);
    sprintf (lonstr, "%.5lf%s)", longitude, deg_glyph);
    sprintf (state->dmr_lrrp_gps[slot], "%s%s%s", locnstr, latstr, lonstr);

    //write to LRRP file
    if (opts->lrrp_file_output == 1)
    {
      char * timestr  = getTimeC();
      char * datestr  = getDateS();

      //open file by name that is supplied in the ncurses terminal, or cli
      FILE * pFile; //file pointer
      pFile = fopen (opts->lrrp_out_file, "a");

      //write current date/time if not present in LOCN data
      if (!time) fprintf (pFile, "%s\t", datestr );
      if (!time) fprintf (pFile, "%s\t", timestr );
      if (time)  fprintf (pFile, "20%02X/%02X/%02X\t%02X:%02X:%02X\t", year, month, day, hour, minute, second);

      //write data header source from data header
      fprintf (pFile, "%08lld\t", state->dmr_lrrp_source[state->currentslot]);

      if (timestr != NULL)
      {
        free (timestr);
        timestr = NULL;
      }

      if (datestr != NULL)
      {
        free (datestr);
        datestr = NULL;
      }

      fprintf (pFile, "%.5lf\t", latitude);
      fprintf (pFile, "%.5lf\t", longitude);
      fprintf (pFile, "%.3lf\t ", (velocity * 3.6) );
      fprintf (pFile, "%d\t",degrees);

      fprintf (pFile, "\n");
      fclose (pFile);
    }

  }

}