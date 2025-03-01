/*-------------------------------------------------------------------------------
 * dmr_block.c
 * DMR Data Header and Data Block Assembly/Handling
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"


#define DMR_PDU_DECRYPTION //disable to skip attempting to decrypt DMR PDUs

 //just a bunch of BP keys, move to shared header later on or something
int BPK[256] = {
0x0000, 0x1F00, 0xE300, 0xFC00, 0x2503, 0x3A03, 0xC603, 0xD903,
0x4A05, 0x5505, 0xA905, 0xB605, 0x6F06, 0x7006, 0x8C06, 0x9306,
0x2618, 0x3918, 0xC518, 0xDA18, 0x031B, 0x1C1B, 0xE01B, 0xFF1B,
0x6C1D, 0x731D, 0x8F1D, 0x901D, 0x491E, 0x561E, 0xAA1E, 0xB51E,
0x4B28, 0x5428, 0xA828, 0xB728, 0x6E2B, 0x712B, 0x8D2B, 0x922B,
0x012D, 0x1E2D, 0xE22D, 0xFD2D, 0x242E, 0x3B2E, 0xC72E, 0xD82E,
0x6D30, 0x7230, 0x8E30, 0x9130, 0x4833, 0x5733, 0xAB33, 0xB433,
0x2735, 0x3835, 0xC435, 0xDB35, 0x0236, 0x1D36, 0xE136, 0xFE36,
0x2B49, 0x3449, 0xC849, 0xD749, 0x0E4A, 0x114A, 0xED4A, 0xF24A,
0x614C, 0xAE4C, 0x824C, 0x9D4C, 0x444F, 0x5B4F, 0xA74F, 0xB84F,
0x0D51, 0x1251, 0xEE51, 0xF151, 0x2852, 0x3752, 0xCB52, 0xD452,
0x4754, 0x5854, 0xA454, 0xBB54, 0x6257, 0x7D57, 0x8157, 0x9E57,
0x6061, 0x7F61, 0x8361, 0x9C61, 0x4562, 0x5A62, 0xA662, 0xB962,
0x2A64, 0x3564, 0xC964, 0xD664, 0x0F67, 0x1067, 0xEC67, 0xF367,
0x4679, 0x5979, 0xA579, 0xBA79, 0x637A, 0x7C7A, 0x807A, 0x9F7A,
0x0C7C, 0x137C, 0xEF7C, 0xF07C, 0x297F, 0x367F, 0xCA7F, 0xD57F,
0x4D89, 0x5289, 0xAE89, 0xB189, 0x688A, 0x778A, 0x8B8A, 0x948A,
0x078C, 0x188C, 0xE48C, 0xFB8C, 0x228F, 0x3D8F, 0xC18F, 0xDE8F,
0x6B91, 0x7491, 0x8891, 0x9791, 0x4E92, 0x5192, 0xAD92, 0xB292,
0x2194, 0x3E94, 0xC294, 0xDD94, 0x0497, 0x1B97, 0xE797, 0xF897,
0x06A1, 0x19A1, 0xE5A1, 0xFAA1, 0x23A2, 0x3CA2, 0xC0A2, 0xDFA2,
0x4CA4, 0x53A4, 0xAFA4, 0xB0A4, 0x69A7, 0x76A7, 0x8AA7, 0x95A7,
0x20B9, 0x3FB9, 0xC3B9, 0xDCB9, 0x05BA, 0x1ABA, 0xE6BA, 0xF9BA,
0x6ABC, 0x75BC, 0x89BC, 0x96BC, 0x4FBF, 0x50BF, 0xACBF, 0xB3BF,
0x66C0, 0x79C0, 0x85C0, 0x9AC0, 0x43C3, 0x5CC3, 0xA0C3, 0xBFC3,
0x2CC5, 0x33C5, 0xCFC5, 0xD0C5, 0x09C6, 0x16C6, 0xEAC6, 0xF5C6,
0x84D0, 0x85DF, 0x8AD3, 0x8BDC, 0xB6D5, 0xB7DA, 0xB8D6, 0xB9D9,
0xD0DA, 0xD1D5, 0xDED9, 0xDFD6, 0xE2DF, 0xE3D0, 0xECDC, 0xEDD3,
0x2DE8, 0x32E8, 0xCEE8, 0xD1E8, 0x08EB, 0x17EB, 0xEBEB, 0xF4EB,
0x67ED, 0x78ED, 0x84ED, 0x9BED, 0x42EE, 0x5DEE, 0xA1EE, 0xBEEE,
0x0BF0, 0x14F0, 0xE8F0, 0xF7F0, 0x2EF3, 0x31F3, 0xCDF3, 0xD2F3,
0x41F5, 0x5EF5, 0xA2F5, 0xBDF5, 0x64F6, 0x7BF6, 0x87F6, 0x98F6
};

//hopefully a more simplified (or logical) version...once you get past all the variables
void dmr_dheader (dsd_opts * opts, dsd_state * state, uint8_t dheader[], uint8_t dheader_bits[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors)
{

  uint8_t slot = state->currentslot;

  //clear out unified pdu 'superframe' slot
  for (int i = 0; i < 24*127; i++) state->dmr_pdu_sf[slot][i] = 0;

  //reset block counter to 1
  state->data_block_counter[slot] = 1;

  if (IrrecoverableErrors == 0 && CRCCorrect == 1) //&&CRCCorrect == 1
  {

    uint8_t gi = dheader_bits[0]; //group or individual data
    uint8_t a  = dheader_bits[1]; //response requested flag
    uint8_t ab = (uint8_t)ConvertBitIntoBytes(&dheader_bits[2], 2); //appended blocks
    uint8_t dpf = (uint8_t)ConvertBitIntoBytes(&dheader_bits[4], 4); //data packet format
    uint8_t sap = (uint8_t)ConvertBitIntoBytes(&dheader_bits[8], 4); //service access point
    uint8_t mpoc = dheader_bits[3]; //most significant bit of the 5-bit Pad Octet Count
    uint8_t poc = (uint8_t)ConvertBitIntoBytes(&dheader_bits[12], 4) + (mpoc << 4); //padding octets
    UNUSED(ab);

    uint32_t target = (uint32_t)ConvertBitIntoBytes(&dheader_bits[16], 24); //destination llid 
    uint32_t source = (uint32_t)ConvertBitIntoBytes(&dheader_bits[40], 24); //source llid

    //extra tgt/src handling for XPT 
    uint8_t target_hash[24]; 
    uint8_t tg_hash = 0;
    uint8_t is_xpt = 0;
    //set flag if XPT
    if (strcmp (state->dmr_branding_sub, "XPT ") == 0) is_xpt = 1;

    //truncate tgt/src to 16-bit values if XPT
    if (is_xpt == 1)
    {
      target = (uint32_t)ConvertBitIntoBytes(&dheader_bits[24], 16); //destination llid 
      source = (uint32_t)ConvertBitIntoBytes(&dheader_bits[48], 16); //source llid
      if (gi == 0) 
      {
        for (int i = 0; i < 16; i++) target_hash[i] = dheader_bits[24+i];
        tg_hash = crc8 (target_hash, 16);
      }
    }

    uint8_t is_cap = 0;
    if (strcmp (state->dmr_branding_sub, "Cap+ ") == 0) is_cap = 1;

    if (is_cap)
    {
      //truncate tg on group? or just on private/individual data?
      if (gi == 0) target = (uint32_t)ConvertBitIntoBytes(&dheader_bits[24], 16);
      source = (uint32_t)ConvertBitIntoBytes(&dheader_bits[48], 16);
    }

    //store source and target for dmr pdu packet handling (lrrp) when not available in completed message
    if (dpf != 15)
    {
      state->dmr_lrrp_source[slot] = source;
      state->dmr_lrrp_target[slot] = target;
    }

    //store number of padding octets in a header to be used
    if (dpf != 15) state->data_block_poc[slot] = poc;

    //set dpf to storage for later use (UDT, SD, etc)
    state->data_header_format[slot] = dpf; 

    //Type Strings
    char sap_string[20];
    char mfid_string[20];
    char udtf_string[20];
    char sddd_string[20];
    sprintf (sap_string, "%s", "");
    sprintf (mfid_string, "%s", "");
    sprintf (udtf_string, "%s", "");
    sprintf (sddd_string, "%s", "");

    //see 9.3 - ETSI TS 102 361-1 V2.5.1 (2017-10) for more info
    uint8_t f  = dheader_bits[64]; //F -- Full message flag (F)
    uint8_t bf = (uint8_t)ConvertBitIntoBytes(&dheader_bits[65], 7); //Blocks to Follow (BF)

    //confirmed data header
    uint8_t s   = dheader_bits[72]; //S -- Re-Synchronize Flag
    uint8_t ns  = (uint8_t)ConvertBitIntoBytes(&dheader_bits[73], 3); //N(S) -- send sequence number
    uint8_t fsn = (uint8_t)ConvertBitIntoBytes(&dheader_bits[76], 4); //Fragment Sequence Number (FSN)

    //response header
    uint8_t r_class  = (uint8_t)ConvertBitIntoBytes(&dheader_bits[72], 2);
    uint8_t r_type   = (uint8_t)ConvertBitIntoBytes(&dheader_bits[74], 3);
    uint8_t r_status = (uint8_t)ConvertBitIntoBytes(&dheader_bits[77], 3);

    //short data - status/precoded
    uint8_t s_ab_msb = (uint8_t)ConvertBitIntoBytes(&dheader_bits[2], 2); //appended block, msb
    uint8_t s_ab_lsb = (uint8_t)ConvertBitIntoBytes(&dheader_bits[12], 4);//appended block, lsb
    uint8_t s_ab_fin = (s_ab_msb << 2) | s_ab_lsb; //appended blocks, final value
    uint8_t s_source_port = (uint8_t)ConvertBitIntoBytes(&dheader_bits[64], 3);
    uint8_t s_dest_port = (uint8_t)ConvertBitIntoBytes(&dheader_bits[67], 3);
    uint8_t s_status_precoded = (uint8_t)ConvertBitIntoBytes(&dheader_bits[70], 10);

    //short data - raw
    uint8_t sd_sarq = dheader_bits[70]; //Selective Automatic Repeat reQuest
    uint8_t sd_f = dheader_bits[71];   //full message flag
    uint8_t sd_bp = (uint8_t)ConvertBitIntoBytes(&dheader_bits[72], 8); //bit padding

    //short data - defined
    uint8_t dd_format = (uint8_t)ConvertBitIntoBytes(&dheader_bits[64], 6);

    //Unified Data Transport (UDT)
    uint8_t udt_format = (uint8_t)ConvertBitIntoBytes(&dheader_bits[12], 4);
    uint8_t udt_padnib = (uint8_t)ConvertBitIntoBytes(&dheader_bits[64], 5);
    uint8_t udt_uab = (uint8_t)ConvertBitIntoBytes(&dheader_bits[70], 2); //udt appended blocks
    uint8_t udt_sf = dheader_bits[72];
    uint8_t udt_pf = dheader_bits[73];
    uint8_t udt_op = (uint8_t)ConvertBitIntoBytes(&dheader_bits[74], 6);

    //ETSI TS 102 361-4 V1.12.1 (2023-07) p281 
    udt_uab += 1; //add 1 internally, up to 4 appended blocks are carried, min is 1

    //NMEA Specific Fix for unspecified MFID format w/ 2 appended blocks (UAB 2) p291
    if (udt_uab == 0x5 && udt_uab == 3)
      udt_uab = 2; //set to two if long unspecified format

    //Note: NMEA Reserved value UAB 3 is not referenced in ETSI, so unknown number of 
    //appended blocks but we will assume it is supposed to be 4 appended blocks

    //p_head
    uint8_t p_sap  = (uint8_t)ConvertBitIntoBytes(&dheader_bits[0], 4);
    uint8_t p_mfid = (uint8_t)ConvertBitIntoBytes(&dheader_bits[8], 8);

    fprintf (stderr, "%s ", KGRN);
    fprintf (stderr, "\n");
    fprintf (stderr, " Slot %d Data Header - ", slot+1);

    if (gi == 1 && dpf != 15) fprintf(stderr, "Group - ");
    if (gi == 0 && dpf != 15) fprintf(stderr, "Indiv - ");
    
    if (dpf == 0)       fprintf (stderr, "Unified Data Transport (UDT) ");
    else if (dpf == 1)  fprintf (stderr, "Response Packet ");
    else if (dpf == 2)  fprintf (stderr, "Unconfirmed Delivery ");
    else if (dpf == 3)  fprintf (stderr, "Confirmed Delivery ");
    else if (dpf == 13) fprintf (stderr, "Short Data: Defined ");
    else if (dpf == 14) fprintf (stderr, "Short Data: Raw or S/P ");
    // else if (dpf == 15) fprintf (stderr, "Proprietary Packet Data");
    else if (dpf == 15) fprintf (stderr, "Extended"); //
    else fprintf (stderr, "Reserved/Unknown DPF %X ", dpf);

    if (a == 1 && dpf != 15) fprintf(stderr, "- Response Requested ");
    if (dpf != 15) fprintf (stderr, "- Source: %d Target: %d ", source, target);

    //include the hash value if this is an XPT and if its IND data
    if (dpf != 15 && is_xpt == 1 && gi == 0) fprintf (stderr, "Hash: %d ", tg_hash);

    //sap string handling
    if (dpf == 15) sap = p_sap;

    if      (sap == 0)  sprintf (sap_string, "%s", "UDT Data"); //apparently, both dpf and sap for UDT is 0
    else if (sap == 2)  sprintf (sap_string, "%s", "TCP Comp"); //TCP/IP header compression
    else if (sap == 3)  sprintf (sap_string, "%s", "UDP Comp"); //UDP/IP header compression
    else if (sap == 4)  sprintf (sap_string, "%s", "IP Based"); //IP based Packet Data
    else if (sap == 5)  sprintf (sap_string, "%s", "ARP Prot"); //Address Resoution Protocol (ARP)
    else if (sap == 9)  sprintf (sap_string, "%s", "EXTD HDR"); //Extended Header (Proprietary)
    else if (sap == 10) sprintf (sap_string, "%s", "Short DT"); //Short Data
    else if (sap == 1 && p_mfid == 0x10)
                        sprintf (sap_string, "%s", "Moto EXT"); //motorola extended format
    else                sprintf (sap_string, "%s", "Reserved"); //reserved, or err/unk

    //mfid string handling
    if (dpf == 15)
    {
      if      (p_mfid == 0x10) sprintf (mfid_string, "%s", "Moto"); //could just also be a generic catch all for DMRA
      else if (p_mfid == 0x58) sprintf (mfid_string, "%s", "Tait");
      else if (p_mfid == 0x68) sprintf (mfid_string, "%s", "Hytera");
      else if (p_mfid == 0x08) sprintf (mfid_string, "%s", "Hytera");
      else if (p_mfid == 0x06) sprintf (mfid_string, "%s", "Trid/Mot"); 
      else if (p_mfid == 0x00) sprintf (mfid_string, "%s", "Standard"); 
      else                     sprintf (mfid_string, "%s", "Other");
    }

    //udt format string handling
    if (dpf == 0)
    {
      if      (udt_format == 0x00) sprintf (udtf_string, "%s", "Binary");
      else if (udt_format == 0x01) sprintf (udtf_string, "%s", "MS/TG Adr");
      else if (udt_format == 0x02) sprintf (udtf_string, "%s", "4-bit BCD");
      else if (udt_format == 0x03) sprintf (udtf_string, "%s", "ISO7 Char");
      else if (udt_format == 0x04) sprintf (udtf_string, "%s", "ISO8 Char"); 
      else if (udt_format == 0x05) sprintf (udtf_string, "%s", "NMEA LOCN"); 
      else if (udt_format == 0x06) sprintf (udtf_string, "%s", "IP Addr");
      else if (udt_format == 0x07) sprintf (udtf_string, "%s", "UTF-16");    //16-bit Unicode Chars
      else if (udt_format == 0x08) sprintf (udtf_string, "%s", "Manu Spec"); //Manufacturer Specific
      else if (udt_format == 0x09) sprintf (udtf_string, "%s", "Manu Spec"); //Manufacturer Specific
      else if (udt_format == 0x0A) sprintf (udtf_string, "%s", "Mixed UTF"); //Appended block contains addr and 16-bit UTF-16BE
      else if (udt_format == 0x0B) sprintf (udtf_string, "%s", "LIP LOCN");
      else                         sprintf (udtf_string, "%s", "Reserved");
    }

    //short data dd_head format string
    if (dpf == 13)
    {
      if      (dd_format == 0x00) sprintf (sddd_string, "%s", "Binary");
      else if (dd_format == 0x01) sprintf (sddd_string, "%s", "BCD   ");
      else if (dd_format == 0x02) sprintf (sddd_string, "%s", "7-bit Char");
      else if (dd_format == 0x03) sprintf (sddd_string, "%s", "IEC 8859-1");
      else if (dd_format == 0x04) sprintf (sddd_string, "%s", "IEC 8859-2");
      else if (dd_format == 0x05) sprintf (sddd_string, "%s", "IEC 8859-3");
      else if (dd_format == 0x06) sprintf (sddd_string, "%s", "IEC 8859-4");
      else if (dd_format == 0x07) sprintf (sddd_string, "%s", "IEC 8859-5");
      else if (dd_format == 0x08) sprintf (sddd_string, "%s", "IEC 8859-6");
      else if (dd_format == 0x09) sprintf (sddd_string, "%s", "IEC 8859-7");
      else if (dd_format == 0x0A) sprintf (sddd_string, "%s", "IEC 8859-8");
      else if (dd_format == 0x0B) sprintf (sddd_string, "%s", "IEC 8859-9");
      else if (dd_format == 0x0C) sprintf (sddd_string, "%s", "IEC 8859-10");
      else if (dd_format == 0x0D) sprintf (sddd_string, "%s", "IEC 8859-11"); 
      else if (dd_format == 0x0E) sprintf (sddd_string, "%s", "IEC 8859-13"); //there is no 8059-12
      else if (dd_format == 0x0F) sprintf (sddd_string, "%s", "IEC 8859-14");
      else if (dd_format == 0x10) sprintf (sddd_string, "%s", "IEC 8859-15");
      else if (dd_format == 0x11) sprintf (sddd_string, "%s", "IEC 8859-16");
      else if (dd_format == 0x12) sprintf (sddd_string, "%s", "UTF-8   ");
      else if (dd_format == 0x13) sprintf (sddd_string, "%s", "UTF-16  ");
      else if (dd_format == 0x14) sprintf (sddd_string, "%s", "UTF-16BE");
      else if (dd_format == 0x15) sprintf (sddd_string, "%s", "UTF-16LE");
      else if (dd_format == 0x16) sprintf (sddd_string, "%s", "UTF-32  ");
      else if (dd_format == 0x17) sprintf (sddd_string, "%s", "UTF-32BE");
      else if (dd_format == 0x18) sprintf (sddd_string, "%s", "UTF-32LE");
      else                        sprintf (sddd_string, "%s", "Reserved");
    }

    if (dpf == 0) //UDT
    {
      //UDT packet info -- samples needed for testing
      //NOTE: This format's completed message has a CRC16 - like MBC - but has number of blocks (appended blocks) like R 1/2, etc.
      fprintf (stderr, "\n  SAP %02d [%s] - FMT %d [%s] - PDn %d - BLOCKS %d SF %d - PF %d OP %02X", sap, sap_string, udt_format, udtf_string, udt_padnib, udt_uab, udt_sf, udt_pf, udt_op);

      //set number of blocks to follow (appended blocks) for block assembler
      state->data_header_blocks[slot] = udt_uab;

      //set data header to valid
      state->data_header_valid[slot] = 1;

      //reset block counter to zero
      state->data_block_counter[slot] = 0;

      //send to assembler as type 3, rearrange into CSBK type PDU (to verify), and send to dmr_cspdu
      dmr_block_assembler (opts, state, dheader, 12, 0x0B, 3);

    }

    if (dpf == 1) //response data packet header
    {
      //mostly fleshed out response packet info
      fprintf (stderr, "\n  SAP %02d [%s] - Class %d - Type %0d - ", sap, sap_string, r_class, r_type);
      if (r_class == 0 && r_type == 1) fprintf (stderr, "ACK - Success");
      if (r_class == 1)
      {
        fprintf (stderr, "NACK - ");
        if (r_type == 0) fprintf (stderr, "Illegal Format");
        if (r_type == 1) fprintf (stderr, "Illegal Format");
        if (r_type == 2) fprintf (stderr, "Packet CRC ERR");
        if (r_type == 3) fprintf (stderr, "Memory Full");
        if (r_type == 4) fprintf (stderr, "FSN Out of Seq");
        if (r_type == 5) fprintf (stderr, "Undeliverable");
        if (r_type == 6) fprintf (stderr, "PKT Out of Seq");
        if (r_type == 7) fprintf (stderr, "Invalid User");
      }
      if (r_class == 2) fprintf (stderr, "SACK - Retry");
      if (r_status) fprintf (stderr, " - %d", r_status); //the object/value of the ack/nack/sack

    }

    //Confirmed or Unconfirmed Data Packets Header
    if (dpf == 2 || dpf == 3) 
    {
      if (dpf == 2) fprintf (stderr, "\n  SAP %02d [%s] - FMF %d - BLOCKS %02d - PAD %02d - FSN %d", sap, sap_string, f, bf, poc, fsn);
      if (dpf == 3) fprintf (stderr, "\n  SAP %02d [%s] - FMF %d - BLOCKS %02d - PAD %02d - S %d - NS %d - FSN %d", sap, sap_string, f, bf, poc, s, ns, fsn);
      state->data_header_blocks[slot] = bf;
      if (dpf == 3) state->data_conf_data[slot] = 1; //set confirmed data delivery flag for additional CRC checks, block assembly, etc.

    }

    //Short Data DD_Head (13), and R_Head or SP_Head (14)
    if (dpf == 13 || dpf == 14) 
    {
      //only set if not all zeroes
      if (s_ab_fin) state->data_header_blocks[slot] = s_ab_fin;

      //Short Data: Defined
      if (dpf == 13) fprintf (stderr, "\n  SD:D [DD_HEAD] - SAP %02d [%s] - BLOCKS %02d - DD %02X - PADb %d - FMT %02X [%s]", sap, sap_string, s_ab_fin, dd_format, sd_bp, dd_format, sddd_string);
      //Short Data: Raw or S/P
      if (dpf == 14)
      {
        //S/P has all appended block bits set to zero -- any other way to tell difference?
        if (s_ab_fin == 0) fprintf (stderr, "\n  SD:S/P [SP_HEAD] - SAP %02d [%s] - SP %02d - DP %02d - S/P %02X", sap, sap_string, s_source_port, s_dest_port, s_status_precoded);

        //Raw
        else fprintf (stderr, "\n  SD:RAW [R_HEAD] - SAP %02d [%s] - BLOCKS %02d - SP %02d - DP %02d - SARQ %d - FMF %d - PDb %d", sap, sap_string, s_ab_fin, s_source_port, s_dest_port, sd_sarq, sd_f, sd_bp);
      }

      //6.2.2 The Response Requested (A) information element of the header shall be set to 0 
      //for unconfirmed data and shall be set to 1 for confirmed data. DD_HEAD, R_HEAD, or SP_HEAD (double check)
      if (a == 1)
      {
        state->data_conf_data[slot] = 1;
        fprintf (stderr, " - Confirmed Data");
      } 

    }
    
    //Proprietary Data Header
    if (dpf == 15) 
    {

      //The SAP found here is the actual SAP of the message (like a P25 ndary SAP, and can chain together according to ETSI)
      fprintf (stderr, " - SAP %02d [%s] - MFID %02X [%s]", p_sap, sap_string, p_mfid, mfid_string);

      //p_sap 1 on mfid 10 (moto) has been observed as the first block of LRRP data (unique fixed format?)
      if (p_mfid == 0x10 && p_sap == 1)
      {
        //add the header to the first 10 bytes of the storage (sans this header's CRC)
        int8_t start = 0;
        int8_t len = 10-start;
        memcpy(state->dmr_pdu_sf[slot], dheader+start, len*sizeof(uint8_t));
        state->data_block_counter[slot]++;
        state->data_byte_ctr[slot] = len;
        state->data_p_head[slot] = 1;

        //my observation on chained p_head is that the enc header will come first, and then
        //a second extended header, and the keystream on that starts after the sap/dpf and mfid value
        //this is the only time starting the keystream at an offset value will be required

        //set ks start value to 3 (testing based on only example I have, need more samples)
        state->data_ks_start[slot] = 3;
      }

      else //if (p_sap != 1) //anything else
      {
        //sanity check to prevent segfault (this happened when the regular header was not received beforehand)
        if (state->data_header_blocks[slot] > 1)
          state->data_header_blocks[slot]--;

        //reset the ctr
        state->data_byte_ctr[slot] = 0;
      }

      //Start Setting DMR Data Packet Encryption Variables
      if (p_sap != 1 && p_mfid == 0x10)
      {

        //check ENC bit, assuming this is an ENC bit, or SVC OPT like thing (or could be an opcode for the rest of the extended header)
        if ((uint8_t)ConvertBitIntoBytes(&dheader_bits[20], 4) == 1)
        {
          //set to 0x100 so it won't trigger any weird flags, but still has a non-zero value to be checked later
          if (state->currentslot == 0) state->dmr_so = 0x100;
          else state->dmr_soR = 0x100;
        }

        fprintf (stderr, "\n PDU ENC Header:");
        fprintf (stderr, " MFID: %02X;", (uint8_t)ConvertBitIntoBytes(&dheader_bits[8], 8));
        fprintf (stderr, " ENC: %X;", (uint8_t)ConvertBitIntoBytes(&dheader_bits[20], 4));

        if (state->currentslot == 0)
          state->payload_keyid = (uint8_t)ConvertBitIntoBytes(&dheader_bits[24], 8);
        else state->payload_keyidR = (uint8_t)ConvertBitIntoBytes(&dheader_bits[24], 8);
        fprintf (stderr, " Key ID: %02X;", (uint8_t)ConvertBitIntoBytes(&dheader_bits[24], 8));

        //this uses the same 3 bit method found in the 'late entry' alg
        if (state->currentslot == 0) //could be 17,3
          state->payload_algid = (uint8_t)ConvertBitIntoBytes(&dheader_bits[17], 3);
        else state->payload_algidR = (uint8_t)ConvertBitIntoBytes(&dheader_bits[17], 3);
        fprintf (stderr, " ALG: %02X;", (uint8_t)ConvertBitIntoBytes(&dheader_bits[17], 3));
        if ((uint8_t)ConvertBitIntoBytes(&dheader_bits[17], 3) == 0) fprintf (stderr, " BP;");
        if ((uint8_t)ConvertBitIntoBytes(&dheader_bits[17], 3) == 1) fprintf (stderr, " RC4;");
        if ((uint8_t)ConvertBitIntoBytes(&dheader_bits[17], 3) == 2) fprintf (stderr, " DES56;");
        if ((uint8_t)ConvertBitIntoBytes(&dheader_bits[17], 3) == 3) fprintf (stderr, " AES128;");
        if ((uint8_t)ConvertBitIntoBytes(&dheader_bits[17], 3) == 4) fprintf (stderr, " AES256;");

        if (state->currentslot == 0)
          state->payload_mi = (uint32_t)ConvertBitIntoBytes(&dheader_bits[48], 32);
        else state->payload_miR = (uint32_t)ConvertBitIntoBytes(&dheader_bits[48], 32);

        //print MI only if this is not Moto BP (no MI on those)
        if ((uint32_t)ConvertBitIntoBytes(&dheader_bits[48], 32) != 0)
          fprintf (stderr, " MI(32): %08X", (uint32_t)ConvertBitIntoBytes(&dheader_bits[48], 32));

        //reset ks start value
        state->data_ks_start[slot] = 0;

      }
      else if (p_sap == 1 && p_mfid == 0x10)
      {
        //This can be an LRRP packet first block (but slightly different configuration)
        fprintf (stderr, "\n Motorola Extended Header; ");
      }
      else //if (p_mfid == 0x10)
      {
        fprintf (stderr, "\n Unknown Extended Header: ");
        for (uint8_t i = 2; i < 10; i++)
          fprintf (stderr, "%02X", (uint8_t)ConvertBitIntoBytes(&dheader_bits[0+(i*8)], 8));
      }
      //End Setting DMR Data Packet Encryption Variables
    }
    else //if (dpf != 15) //normal data header, we want to reset the enc states in case of any record needle drop playback occurrences
    {
      //reset alg/keyid/mi
      if (state->currentslot == 0)
      {
        state->payload_mi = 0;
        state->payload_algid = 0;
        state->payload_keyid = 0;
        state->dmr_so = 0;
      }
      else
      {
        state->payload_miR = 0;
        state->payload_algidR = 0;
        state->payload_keyidR = 0;
        state->dmr_soR = 0;
      }

      //reset the ctr
      state->data_byte_ctr[slot] = 0; 
    }

    //block storage sanity
    if (state->data_header_blocks[slot] > 127) state->data_header_blocks[slot] = 127;
    //assuming we didn't receive the initial data header block on a p_head and then decremented it
    //3 or 4 seems to be the average safe value
    if (state->data_header_blocks[slot] < 1) state->data_header_blocks[slot] = 1; //
    //set data header validity unless its a p_head (should be set prior, if received)
    if (dpf != 15) state->data_header_valid[slot] = 1;

    if (dpf != 1)
    {
      sprintf (state->dmr_lrrp_gps[slot], "Data Call - %s TGT: %d SRC: %d ", sap_string, target, source);
      if (a == 1) strcat (state->dmr_lrrp_gps[slot], "- RSP REQ ");
    }

    //store SAP value
    state->data_header_sap[slot] = sap;

  } //End Irrecoverable Errors

  if (IrrecoverableErrors != 0)
  {
    state->data_header_valid[slot] = 0;
    sprintf (state->dmr_lrrp_gps[slot], "%s", "");
    state->data_p_head[slot] = 0;
    state->data_conf_data[slot] = 0;
    state->data_block_counter[slot] = 1; 
    state->data_header_blocks[slot] = 1; 
    state->data_header_format[slot] = 7;

  }

  fprintf (stderr, "%s", KNRM);

}

void dmr_udt_decoder (dsd_opts * opts, dsd_state * state, uint8_t * block_bytes, uint32_t CRCCorrect)
{
  //TODO: double check end rep values to Text Format Modes vs padnibs/uab values
  //TODO: Make Text Format Modes seperate function to be used more generically by other things
  //TODO: LIP Format (make seperate functions, LIP also used by USBD)
  int i, j;
  UNUSED(CRCCorrect);
  UNUSED(opts);
  int slot = state->currentslot;

  uint8_t cs_bits[8*12*5]; //maximum of 1 header and 4 blocks at 96 bits
  UNUSED(cs_bits);

  //bytes to bits
  for(i = 0, j = 0; i < 36; i++, j+=8)
  {
    cs_bits[j + 0] = (block_bytes[i] >> 7) & 0x01;
    cs_bits[j + 1] = (block_bytes[i] >> 6) & 0x01;
    cs_bits[j + 2] = (block_bytes[i] >> 5) & 0x01;
    cs_bits[j + 3] = (block_bytes[i] >> 4) & 0x01;
    cs_bits[j + 4] = (block_bytes[i] >> 3) & 0x01;
    cs_bits[j + 5] = (block_bytes[i] >> 2) & 0x01;
    cs_bits[j + 6] = (block_bytes[i] >> 1) & 0x01;
    cs_bits[j + 7] = (block_bytes[i] >> 0) & 0x01;
  }

  //Unified Data Transport (UDT) -- already checked, but may need a few of these here as well
  uint8_t udt_ig = cs_bits[0]; //group or individual data
  uint8_t udt_a  = cs_bits[1]; //response required
  uint8_t udt_res = (uint8_t)ConvertBitIntoBytes(&cs_bits[2], 2);
  uint8_t udt_format1 = (uint8_t)ConvertBitIntoBytes(&cs_bits[4], 4); //header format of UDT
  uint8_t udt_sap = (uint8_t)ConvertBitIntoBytes(&cs_bits[8], 4);
  uint8_t udt_format2 = (uint8_t)ConvertBitIntoBytes(&cs_bits[12], 4); //UDT Format referenced below
  uint32_t udt_target = (uint32_t)ConvertBitIntoBytes(&cs_bits[16], 24);
  uint32_t udt_source = (uint32_t)ConvertBitIntoBytes(&cs_bits[40], 24);
  uint8_t udt_padnib = (uint8_t)ConvertBitIntoBytes(&cs_bits[64], 5);
  uint8_t udt_zero = cs_bits[69]; //should always be zero?
  uint8_t udt_uab = (uint8_t)ConvertBitIntoBytes(&cs_bits[70], 2) + 1; //udt appended blocks
  uint8_t udt_sf = cs_bits[72];
  uint8_t udt_pf = cs_bits[73];
  uint8_t udt_op = (uint8_t)ConvertBitIntoBytes(&cs_bits[74], 6);
  UNUSED4(udt_ig, udt_a, udt_res, udt_format1);
  UNUSED4(udt_sap, udt_format2, udt_target, udt_source);
  UNUSED4(udt_padnib, udt_zero, udt_sf, udt_pf);
  UNUSED(udt_op);

  //number of repititions required in various bit grabs
  int end = 3; UNUSED(end);

  //char strings
  uint8_t iso7c;
  uint8_t iso8c;
  uint16_t utf16c;

  //appended addresses -- max is 15 across 4 blocks
  uint32_t address[16]; memset (address, 0, sizeof(address)); UNUSED(address);
  uint8_t add_res = (uint8_t)ConvertBitIntoBytes(&cs_bits[96], 7); UNUSED(add_res);
  uint8_t add_ok = cs_bits[103];

  //BCD Format (Dialer Digits) -- max is 92 digits
  uint8_t bcd_digits[93]; memset (bcd_digits, 0, sizeof(bcd_digits)); UNUSED(bcd_digits);

  //NMEA Debug Testing (need real world samples)
  // udt_format2 = 0x5;
  // for (int i = 0; i < 8; i++) cs_bits[184+i] = 0; //spare bits to zero
  // cs_bits[96] = 0; //enc bit to zero
  // uint8_t test[96*4]; memset (test, 1, sizeof(test)); // all ones test vector
  // nmea_iec_61162_1 (opts, state, test, udt_source, 1);
  // nmea_iec_61162_1 (opts, state, cs_bits+96, udt_source, 2);

  //LIP Debug Testing (need real world samples)
  // udt_format2 = 0x0B;

  //initial linebreak
  fprintf (stderr, "%s", KCYN);
  fprintf (stderr, "\n ");
  fprintf (stderr, "Slot %d - SRC: %d; TGT: %d; UDT ", slot+1, udt_source, udt_target);

  if (udt_format2 == 0x00)
  {
    fprintf (stderr, "Binary Data;"); 
  }
  else if (udt_format2 == 0x01) //appended addresses
  {
    fprintf (stderr, "Appended Addressing;\n ");
    if (udt_uab == 1) end = 3;
    if (udt_uab == 2) end = 7;
    if (udt_uab == 3) end = 11;
    if (udt_uab == 4) end = 15;
    if (add_res) fprintf (stderr, "RES: %d; ", add_res);
    fprintf (stderr, "OK: %d; ", add_ok); 
    fprintf (stderr, "ADDR:");
    for (i = 0; i < end; i++)
    {
      fprintf (stderr, " %d;", (uint32_t)ConvertBitIntoBytes(&cs_bits[(i*24)+104], 24));
    }
  }
  else if (udt_format2 == 0x02) //BCD 4-bit Dialer Digits
  {
    if (udt_uab == 1) end = 20;
    if (udt_uab == 2) end = 44;
    if (udt_uab == 3) end = 95;
    if (udt_uab == 4) end = 92;
    end -= udt_padnib; //subtract padnib since its also 4 bits

    fprintf (stderr, "Dialer BCD: ");
    for (i = 0; i < end; i++)
    {
      //dialer digits 7.2.9
      int digit = (int)ConvertBitIntoBytes(&cs_bits[(i*4)+96], 4);
      if (digit <  10) fprintf (stderr, "%d", digit); //numbers 0-9
      else if (digit == 10) fprintf (stderr, "*"); //asterisk/star
      else if (digit == 11) fprintf (stderr, "#"); //pound/hash
      else if (digit == 15) fprintf (stderr, " "); //null character
      else fprintf (stderr, "R:%X", digit); //reserved values on 12,13, and 14
    }
  }
  else if (udt_format2 == 0x03) //ISO7 format
  {
    if (udt_uab == 1) end = 11;
    if (udt_uab == 2) end = 25;
    if (udt_uab == 3) end = 38;
    if (udt_uab == 4) end = 52;
    end -= udt_padnib/7; //is this correct?
    fprintf (stderr, "ISO7 Text: "  );
    for (i = 0; i < end; i++) //max 368/7 = 52 character max?
    {
      iso7c = (uint8_t)ConvertBitIntoBytes(&cs_bits[(i*7)+96], 7);
      if (iso7c >= 0x20 && iso7c <= 0x7E) //Standard ASCII Set
        fprintf (stderr, "%c", iso7c);
      else fprintf (stderr, " ");
    }
  }
  else if (udt_format2 == 0x04) //ISO8 format
  {
    fprintf (stderr, "ISO8 Text: "  );
    if (udt_uab == 1) end = 10;
    if (udt_uab == 2) end = 22;
    if (udt_uab == 3) end = 34;
    if (udt_uab == 4) end = 46;
    end -= udt_padnib/8; //is this correct?
    for (i = 0; i < end; i++)
    {
      iso8c = (uint8_t)ConvertBitIntoBytes(&cs_bits[(i*8)+96], 8);
      if (iso8c >= 0x20 && iso8c <= 0x7E) //Standard ASCII Set
        fprintf (stderr, "%c", iso8c);
      // else if (iso8c >= 0x81 && iso8c <= 0xFE) //Extended ASCII Set
      //   fprintf (stderr, "%c", iso8c);
      else fprintf (stderr, " ");
    }
  }
  else if (udt_format2 == 0x07) //UTF-16BE format
  {
    if (udt_uab == 1) end = 5;
    if (udt_uab == 2) end = 11;
    if (udt_uab == 3) end = 17;
    if (udt_uab == 4) end = 23;
    end -= udt_padnib/4; //example, 4 blocks sets 23 - (20nibs/4bits) = 18 chars, may need to check this again
    fprintf (stderr, "UTF16 Text: "  );
    for (i = 0; i < end; i++) //368/16 = 23 character max?
    {
      utf16c = (uint16_t)ConvertBitIntoBytes(&cs_bits[(i*16)+96], 16);
      if (utf16c >= 0x20 && utf16c != 0x7F) //avoid control chars
        fprintf (stderr, "%lc", utf16c); //will using lc work here? May depend on console locale settings?
      else fprintf (stderr, " ");
    }
  }
  else if (udt_format2 == 0x06) //IP
  {
    if (udt_uab == 1) //IP4
    {
      fprintf (stderr, "IP4: ");
      fprintf (stderr, "%d.",(uint8_t)ConvertBitIntoBytes(&cs_bits[96+0], 8));
      fprintf (stderr, "%d.",(uint8_t)ConvertBitIntoBytes(&cs_bits[96+8], 8));
      fprintf (stderr, "%d.",(uint8_t)ConvertBitIntoBytes(&cs_bits[96+16], 8));
      fprintf (stderr, "%d", (uint8_t)ConvertBitIntoBytes(&cs_bits[96+24], 8));
    }
    else //IP6
    {
      fprintf (stderr, "IP6: ");
      fprintf (stderr, "%04X:",(uint16_t)ConvertBitIntoBytes(&cs_bits[96+0], 16));
      fprintf (stderr, "%04X:",(uint16_t)ConvertBitIntoBytes(&cs_bits[96+16], 16));
      fprintf (stderr, "%04X:",(uint16_t)ConvertBitIntoBytes(&cs_bits[96+32], 16));
      fprintf (stderr, "%04X:",(uint16_t)ConvertBitIntoBytes(&cs_bits[96+48], 16));
      fprintf (stderr, "%04X:",(uint16_t)ConvertBitIntoBytes(&cs_bits[96+56], 16));
      fprintf (stderr, "%04X:",(uint16_t)ConvertBitIntoBytes(&cs_bits[96+72], 16));
      fprintf (stderr, "%04X:",(uint16_t)ConvertBitIntoBytes(&cs_bits[96+88], 16));
      fprintf (stderr, "%04X", (uint16_t)ConvertBitIntoBytes(&cs_bits[96+104], 16));
    }
  }
  else if (udt_format2 == 0x0A) //Mixed Address/UTF-16BE
  {
    if (udt_uab == 1) end = 3;
    if (udt_uab == 2) end = 9;
    if (udt_uab == 3) end = 15;
    if (udt_uab == 4) end = 21;
    end -= udt_padnib/4; //is this correct?
    fprintf (stderr, "Address: %d", (uint32_t)ConvertBitIntoBytes(&cs_bits[96+8], 24));
    fprintf (stderr, "Text: "  );
    for (i = 0; i < end; i++) //368/16 = 21 character max
    {
      utf16c = (uint16_t)ConvertBitIntoBytes(&cs_bits[(i*16)+96], 16);
      if (utf16c >= 0x20 && utf16c != 0x7F) //avoid control chars
        fprintf (stderr, "%lc", utf16c);
      else fprintf (stderr, " ");
    }
  }
  else if (udt_format2 == 0x05)
  {
    //Would be nice to be able to test these all out to make sure the conditions are okay, etc
    fprintf (stderr, "NMEA"  );
    if (cs_bits[96] == 1) //check if its encrypted first
      fprintf (stderr, " Encrypted Format :("  ); //sad face
    else if (udt_uab == 1)
      nmea_iec_61162_1 (opts, state, cs_bits+96, udt_source, 1); //short format w/ 1 appended block
    else if (udt_uab == 2) 
      nmea_iec_61162_1 (opts, state, cs_bits+96, udt_source, 2); //standard long format w/ 2 appended blocks
    //check the 'spare' bits from 184 to 192 to see if this is a manufacturer specific nmea format
    else if (udt_uab == 3) //mfid format w/ 2 appended blocks -- format unspecified
      fprintf  (stderr, " Unspecified MFID Format: %02X;", (uint8_t)ConvertBitIntoBytes(&cs_bits[184], 8));
    else
      fprintf (stderr, " Reserved Format; ");
  }
  else if (udt_format2 == 0x0B)
  {
    //unsure of how this is structured for UDT Blocks, would assume one appended block of same format
    //but could also be full blown LIP protocol that is also found in tetra that would require the PDU
    //type bit to be read and then to decode accordingly, this assumes its the modified Short PDU that USBD uses
    fprintf (stderr, "\n");
    lip_protocol_decoder (opts, state, cs_bits+96); //start on first appended block, and not header

  }
  else if (udt_format2 == 0x08 || udt_format2 == 0x09)
  {
    fprintf (stderr, "MFID SPEC %02X: ", udt_format2);
    //use -Z to expose this
  }
  else
  {
    fprintf (stderr, "Reserved %02X: ", udt_format2);
    //use -Z to expose this
  }
  fprintf (stderr, "%s", KNRM);
}

//assemble the blocks as they come in, shuffle them into the unified dmr_pdu_sf
void dmr_block_assembler (dsd_opts * opts, dsd_state * state, uint8_t block_bytes[], uint8_t block_len, uint8_t databurst, uint8_t type)
{
  UNUSED(databurst);

  int i, j;
  uint8_t lb = 0; //mbc last block
  uint8_t pf = 0; //mbc protect flag

  uint8_t slot = state->currentslot; 
  int blocks; 
  uint8_t blockcounter = state->data_block_counter[slot];
  int block_num = state->data_header_blocks[slot];

  uint32_t CRCCorrect = 0;
  uint32_t CRCComputed = 0;
  uint32_t CRCExtracted = 0;
  uint32_t IrrecoverableErrors = 0; 

  uint8_t dmr_pdu_sf_bits[8*24*129]; //give plenty of space so we don't go oob

  //MBC Header and Block CRC
  uint8_t mbc_crc_good[2]; //header and blocks crc pass/fail local storage
  memset (mbc_crc_good, 0, sizeof (mbc_crc_good)); //init on 0 - bad crc

  if (type == 1) blocks = state->data_header_blocks[slot] - 1; 
  if (type == 2) blocks = state->data_block_counter[slot];

  //UDT Header and Block Check
  uint8_t is_udt = 0;
  if (type == 3)
  {
    blocks = state->data_header_blocks[slot];
    is_udt = 1;
    type = 2;
  }

  //sanity check, setting block_len and block values to sane numbers in case of missing header, else could overload array (crash) or print out
  if (blocks < 1) blocks = 1; //changed blocks from uint8_t to int
  if (blocks > 127) blocks = 127;
  if (block_len == 0) block_len = 18;
  if (block_len > 24) block_len = 24;

  if (type == 1)
  {

    //type 1 data block, append current block_bytes to end of ctr location
    uint16_t ctr = state->data_byte_ctr[slot];
    for (i = 0; i < block_len; i++)
      state->dmr_pdu_sf[slot][ctr++] = block_bytes[i];

    //debug
    // fprintf (stderr, " CTR: %02d; ", ctr);

    //add block_len to current byte counter 
    state->data_byte_ctr[slot] += block_len;

    //time to send the completed 'superframe' to the DMR PDU message handler
    if (state->data_block_counter[slot] == state->data_header_blocks[slot] && state->data_header_valid[slot] == 1)
    {
      //CRC32 on completed messages
      for(i = 0, j = 0; i < ctr; i++, j+=8)
      {
        dmr_pdu_sf_bits[j + 0] = (state->dmr_pdu_sf[slot][i] >> 7) & 0x01;
        dmr_pdu_sf_bits[j + 1] = (state->dmr_pdu_sf[slot][i] >> 6) & 0x01;
        dmr_pdu_sf_bits[j + 2] = (state->dmr_pdu_sf[slot][i] >> 5) & 0x01;
        dmr_pdu_sf_bits[j + 3] = (state->dmr_pdu_sf[slot][i] >> 4) & 0x01;
        dmr_pdu_sf_bits[j + 4] = (state->dmr_pdu_sf[slot][i] >> 3) & 0x01;
        dmr_pdu_sf_bits[j + 5] = (state->dmr_pdu_sf[slot][i] >> 2) & 0x01;
        dmr_pdu_sf_bits[j + 6] = (state->dmr_pdu_sf[slot][i] >> 1) & 0x01;
        dmr_pdu_sf_bits[j + 7] = (state->dmr_pdu_sf[slot][i] >> 0) & 0x01;

      }

      //sanity check to prevent a negative or zero block_num or sending a negative value into the bit array
      if (block_num < 1 || block_num == 0) block_num = 1;

      CRCExtracted = (state->dmr_pdu_sf[slot][ctr-4] << 24) | (state->dmr_pdu_sf[slot][ctr-3] << 16) | 
                     (state->dmr_pdu_sf[slot][ctr-2] << 8)  | (state->dmr_pdu_sf[slot][ctr-1] << 0);

      int offset = 0;
      if (state->data_p_head[slot] == 1)
        offset = 12;

      //rearrage for ridiculously stupid CRC32 LSO/MSO ordering
      for(i = 0, j = 0; i < ctr; i+=2, j+=16) 
      {
        dmr_pdu_sf_bits[j + 0] = (state->dmr_pdu_sf[slot][i+1] >> 7) & 0x01;
        dmr_pdu_sf_bits[j + 1] = (state->dmr_pdu_sf[slot][i+1] >> 6) & 0x01;
        dmr_pdu_sf_bits[j + 2] = (state->dmr_pdu_sf[slot][i+1] >> 5) & 0x01;
        dmr_pdu_sf_bits[j + 3] = (state->dmr_pdu_sf[slot][i+1] >> 4) & 0x01;
        dmr_pdu_sf_bits[j + 4] = (state->dmr_pdu_sf[slot][i+1] >> 3) & 0x01;
        dmr_pdu_sf_bits[j + 5] = (state->dmr_pdu_sf[slot][i+1] >> 2) & 0x01;
        dmr_pdu_sf_bits[j + 6] = (state->dmr_pdu_sf[slot][i+1] >> 1) & 0x01;
        dmr_pdu_sf_bits[j + 7] = (state->dmr_pdu_sf[slot][i+1] >> 0) & 0x01;

        dmr_pdu_sf_bits[j +  8] = (state->dmr_pdu_sf[slot][i] >> 7) & 0x01;
        dmr_pdu_sf_bits[j +  9] = (state->dmr_pdu_sf[slot][i] >> 6) & 0x01;
        dmr_pdu_sf_bits[j + 10] = (state->dmr_pdu_sf[slot][i] >> 5) & 0x01;
        dmr_pdu_sf_bits[j + 11] = (state->dmr_pdu_sf[slot][i] >> 4) & 0x01;
        dmr_pdu_sf_bits[j + 12] = (state->dmr_pdu_sf[slot][i] >> 3) & 0x01;
        dmr_pdu_sf_bits[j + 13] = (state->dmr_pdu_sf[slot][i] >> 2) & 0x01;
        dmr_pdu_sf_bits[j + 14] = (state->dmr_pdu_sf[slot][i] >> 1) & 0x01;
        dmr_pdu_sf_bits[j + 15] = (state->dmr_pdu_sf[slot][i] >> 0) & 0x01;

        //increment extra 2 bytes on each nth byte (depends on data type 1/2, 3/4, 1) if conf data
        if ( i == (block_len - 1 + offset) )
        {
          
          if (state->data_conf_data[slot] == 1) i+=2; //should this be 0, or 1?
        }

      }

      //confirmed working now!
      CRCComputed = (uint32_t)ComputeCrc32Bit(dmr_pdu_sf_bits, (ctr * 8) - 32);

      //if the CRC32 is correct, I think its fair to assume we don't need to worry about if the 
      //individual CRC9s are correct on confirmed data blocks (but we can confirm now that they are all good)
      if (CRCComputed == CRCExtracted) CRCCorrect = 1;

      //check for encryption on PDU
      uint8_t enc_check = 0;
      uint8_t decrypted_pdu = 1;
      if (slot == 0 && state->dmr_so == 0x100)
      {
        enc_check = 1;
        decrypted_pdu = 0;
      }
      else if (slot == 1 && state->dmr_soR == 0x100)
      {
        enc_check = 1;
        decrypted_pdu = 0;
      }

      //Start DMR Data PDU Decryption
      #ifdef DMR_PDU_DECRYPTION
      if (enc_check)
      {
        // decrypted_pdu = 0; //hasn't been decrypted yet
        int poc = (int)state->data_block_poc[slot]; //say that three times real fast
        int end = ((blocks+1)*block_len)-4-poc;
        int start = (int)state->data_ks_start[slot];
        //sanity check on end, has to be a positive value
        if (end < 0) end = 3096; //its a signed interger, so should reflect negative values here and not rollover
        int alg = 0;
        int kid = 0;
        if (state->currentslot == 0)
          alg = state->payload_algid;
        else alg = state->payload_algidR;

        if (state->currentslot == 0)
          kid = state->payload_keyid;
        else kid = state->payload_keyidR;

        //start keystream creation
        uint8_t ob[129*24]; //may need more blocks (enough for 127 * 24)
        uint8_t kiv[9]; UNUSED(kiv);
        long int mi = 0;
        unsigned long long int R = 0;
        if (state->currentslot == 0)
          mi = state->payload_mi;
        else mi = state->payload_miR;

        //mini key loader for RC4/DES
        if (state->currentslot == 0)
          R = state->rkey_array[state->payload_keyid]; 
        else
          R = state->rkey_array[state->payload_keyidR];


        if (R == 0 && state->R != 0) R = state->R;

        //easier to manually load up rather than make a loop (RC4)
        kiv[0] = ((R & 0xFF00000000) >> 32);
        kiv[1] = ((R & 0xFF000000) >> 24);
        kiv[2] = ((R & 0xFF0000) >> 16);
        kiv[3] = ((R & 0xFF00) >> 8);
        kiv[4] = ((R & 0xFF) >> 0);
        kiv[5] = ((mi & 0xFF000000) >> 24);
        kiv[6] = ((mi & 0xFF0000) >> 16);
        kiv[7] = ((mi & 0xFF00) >> 8);
        kiv[8] = ((mi & 0xFF) >> 0);

        //print alg/key and value if loaded
        fprintf (stderr, "\n PDU ALG: %02X; Key ID: %02X;", alg, kid);
        if (alg != 0) fprintf (stderr, " MI(32): %08X;", mi);
        if (alg == 0) fprintf (stderr, " Moto BP;");
        if (alg == 1) fprintf (stderr, " RC4;");
        if (alg == 2) fprintf (stderr, " DES1;");
        if (alg == 3) fprintf (stderr, " AES128;");
        if (alg == 4) fprintf (stderr, " AES256;");
        if (R && alg != 0) fprintf (stderr, " Key: %010llX;", R);

        if (alg == 1 && R != 0) //RC4
        {
          rc4_block_output (256, 9, (int)state->data_byte_ctr[slot], kiv, ob);
          decrypted_pdu = 1;
        }

        //NOTE: Observed that keystream should not be applied to pad bytes or CRC
        //apply keystream here, only if alg is 1 or 4 AND key is available!
        //using modulus 3096 to prevent sefgault (increased storage and changed ofb modes to a ptr)
        //will want to rework output_blocks to be a pointer instead of a fixed size (done)
        if (alg == 1 && R != 0)
        {
          for (i = start; i < end; i++)
            state->dmr_pdu_sf[slot][i] ^= ob[i%3096];
        }
        //BP key application
        else if (alg == 0) //&& state->K != 0
        {

          //NOTE: using dmr_sr and dmr_soR now for a enc check on all encrypted PDUs
          uint16_t bp_key = 0;
          if (state->K != 0) //state->M == 1 && 
          {
            //load the BP key into the output blocks (only need two)
            bp_key = BPK[state->K];
            ob[0] = (bp_key >> 8) & 0xFF;
            ob[1] = (bp_key >> 0) & 0xFF;

            //
            fprintf (stderr, " Key: %lld:%04X;", state->K, bp_key);
            // fprintf (stderr, "ob1: %02X; ob2: %02X", ob[0], ob[1]);
          }

          if (bp_key != 0)
          {
            for (i = start; i < end; i++) 
              state->dmr_pdu_sf[slot][i] ^= ob[i%2]; //modulus 2 here, just rinse and repeat

            decrypted_pdu = 1;
          }
        }

        //reset alg/keyid/mi after running?
        if (state->currentslot == 0)
        {
          state->payload_mi = 0;
          state->payload_algid = 0;
          state->payload_keyid = 0;
          state->dmr_so = 0;
        }
        else
        {
          state->payload_miR = 0;
          state->payload_algidR = 0;
          state->payload_keyidR = 0;
          state->dmr_soR = 0;
        }

      } //end enc check
      #endif
      //End DMR Data PDU Decryption

      //decode PDU
      if (enc_check == 1 && decrypted_pdu == 0) //check for encryption and if it was decrypted first or not
      {
        fprintf (stderr, "%s", KRED);
        fprintf (stderr, "\n Slot %d - Encrypted Data Packet;", slot+1);
        fprintf (stderr, "%s", KNRM);
      }
      else if (CRCCorrect || opts->aggressive_framesync == 0)
      {
        //may need to make adjustments for various compressed headers for starting point, etc?
        if (state->data_header_sap[slot] == 4) //IP based
        {
          uint16_t len = ((blocks+1)*block_len)-4; //total number of bytes in PDU minus 4 CRC32 bytes
          dmr_ip_pdu (opts, state, len, state->dmr_pdu_sf[slot]);
        }
        else if (state->data_header_sap[slot] == 10) //short data, may also need to check for SD:D [DD_HEAD]
        {
          uint16_t len = ((blocks+1)*block_len)-4; //total number of bytes in PDU minus 4 CRC32 bytes
          dmr_sd_pdu (opts, state, len, state->dmr_pdu_sf[slot]);
        }
        else if (state->data_header_sap[slot] == 2 || state->data_header_sap[slot] == 3) //TCP and UDP Compression (may only be 3 UDP compression, unknown)
        {
          uint16_t len = ((blocks+1)*block_len)-4; //total number of bytes in PDU minus 4 CRC32 bytes
          dmr_udp_comp_pdu (opts, state, len, state->dmr_pdu_sf[slot]);
        }
        else if (state->data_header_sap[slot] == 1 && state->dmr_pdu_sf[slot][1] == 0x10) //test SAP 1 MFID 10 (with -F) as a potential LRRP message (header inclusive)
        {
          //len calc
          uint16_t ctr = state->data_byte_ctr[slot];
          uint8_t  poc = state->data_block_poc[slot];
          uint16_t len = len = ctr-poc-4-7-3;

          //sanity check
          if (len > 150)
            len = 150;

          //this may be some form of compressed header, but contents are thus unknown (aside from sap/dpf and mfid)
          uint32_t msrc = state->dmr_lrrp_source[slot];
          uint32_t mdst = state->dmr_lrrp_target[slot];

          fprintf (stderr, "\n SRC(Header): %08d; ", msrc);
          fprintf (stderr, "\n DST(Header): %08d; ", mdst);

          //+7 offset
          dmr_lrrp (opts, state, len, msrc, mdst, state->dmr_pdu_sf[slot]+7);
        }
      }

      //debug
      // fprintf (stderr, " CRC32: %08X / %08X", CRCExtracted, CRCComputed);

      if (CRCCorrect) ; //print nothing
      else
      {
        fprintf (stderr, "%s", KRED);
        fprintf (stderr, "\n Slot %d - Multi Block PDU Message CRC32 ERR", slot+1);

        fprintf (stderr, "%s", KNRM);

      }

      //Full Super Frame Type 1 - Payload Output
      if (opts->payload == 1)
      {
        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, "\n Slot %d - Multi Block PDU Message\n  ", slot+1);
        for (i = 0; i < ((blocks+1)*block_len); i++) 
        {
          if ( (i != 0) && (i % 12 == 0) ) fprintf (stderr, "\n  "); 
          fprintf (stderr, "%02X", state->dmr_pdu_sf[slot][i]);
        }

        fprintf (stderr, "%s ", KNRM);
      }

      //reset data header format storage
      state->data_header_format[slot] = 7;
      //flag off data header validity 
      state->data_header_valid[slot] = 0; 
      //flag off conf data flag
      state->data_conf_data[slot] = 0;
      //reset padding
      state->data_block_poc[slot] = 0;
      //reset byte counter
      state->data_byte_ctr[slot] = 0;
      //reset ks start value
      state->data_ks_start[slot] = 0;

    } //end completed sf

  } //end type 1 blocks
  

  //type 2 - MBC and UDT header, MBC and UDT continuation blocks
  if (type == 2) 
  {
    //sanity check (marginal signal, bad decodes, etc) -- may go a little lower (find out max number of MBC blocks supported)
    if (state->data_block_counter[slot] > 4) state->data_block_counter[slot] = 4;

    //Type 2 data block, additive method
    for (i = 0; i < block_len; i++)
    {
      state->dmr_pdu_sf[slot][i+(blockcounter*block_len)] = block_bytes[i]; 
    }
  
    memset (dmr_pdu_sf_bits, 0, sizeof(dmr_pdu_sf_bits));

    lb = block_bytes[0] >> 7; //last block flag
    pf = (block_bytes[0] >> 6) & 1;

    if (is_udt)
    {
      lb = 0; //set to zero, data header may erroneously the lb flag check above (IG)
      pf = 0; //set to zero, data header may erroneously the pf flag check above (A)
      if (blocks == blockcounter) lb = 1; //seems to work now with the +1 on udt_uab
      
      //debug -- evaluate current block count vs the number of expected blocks
      // fprintf (stderr, " BL: %d; BC: %d; ", blocks, blockcounter);
    }

    //last block arrived and we have a valid data header, time to send to cspdu decoder
    if (lb == 1 && state->data_header_valid[slot] == 1) 
    {
      for(i = 0, j = 0; i < 12*6; i++, j+=8) //4 blocks max?
      {
        dmr_pdu_sf_bits[j + 0] = (state->dmr_pdu_sf[slot][i] >> 7) & 0x01;
        dmr_pdu_sf_bits[j + 1] = (state->dmr_pdu_sf[slot][i] >> 6) & 0x01;
        dmr_pdu_sf_bits[j + 2] = (state->dmr_pdu_sf[slot][i] >> 5) & 0x01;
        dmr_pdu_sf_bits[j + 3] = (state->dmr_pdu_sf[slot][i] >> 4) & 0x01;
        dmr_pdu_sf_bits[j + 4] = (state->dmr_pdu_sf[slot][i] >> 3) & 0x01;
        dmr_pdu_sf_bits[j + 5] = (state->dmr_pdu_sf[slot][i] >> 2) & 0x01;
        dmr_pdu_sf_bits[j + 6] = (state->dmr_pdu_sf[slot][i] >> 1) & 0x01;
        dmr_pdu_sf_bits[j + 7] = (state->dmr_pdu_sf[slot][i] >> 0) & 0x01;
      }

      //check UDT PF
      if (is_udt) pf = dmr_pdu_sf_bits[73];

      //CRC check on Header and full frames as appropriate (header crc already stored)
      //The 16 bit CRC in the header shall include the data carried by the header. 
      //The 16 bit CRC in the last block shall be performed on 
      //all MBC blocks (conmbined) except the header block.
      
      mbc_crc_good[0] = state->data_block_crc_valid[slot][0];
      
      CRCExtracted = 0;
      //extract crc from last block, apply to completed 'superframe' minus header
      for(i = 0; i < 16; i++) 
      {
        CRCExtracted = CRCExtracted << 1;
        CRCExtracted = CRCExtracted | (uint32_t)(dmr_pdu_sf_bits[i + 96*(1+blocks) - 16] & 1); 
      }

      uint8_t mbc_block_bits[12*8*6]; //needed more room
      memset (mbc_block_bits, 0, sizeof(mbc_block_bits));
      //shift continuation blocks and last block into seperate array for crc check
      for (i = 0; i < 12*8*3; i++) //only doing 3 blocks (4 minus the header), probably need to re-evalutate this
      {
        mbc_block_bits[i] = dmr_pdu_sf_bits[i+96]; //skip mbc header
      }

      if (is_udt)
      {
        memset (mbc_block_bits, 0, sizeof(mbc_block_bits));
        for (i = 0; i < 12*8*blocks; i++)
        {
          mbc_block_bits[i] = dmr_pdu_sf_bits[i+96]; //skip udt header
        }
      }

      CRCComputed = ComputeCrcCCITT16d (mbc_block_bits, ((blocks+0)*96)-16 );

      if (CRCComputed == CRCExtracted) mbc_crc_good[1] = 1;

      CRCCorrect = 0;
      IrrecoverableErrors = 1;

      //set good on good header and good blocks
      if (mbc_crc_good[0] == 1 && mbc_crc_good[1] == 1)
      {
        CRCCorrect = 1;
        IrrecoverableErrors = 0;
      }
      else
      {
        fprintf (stderr, "%s", KRED);
        fprintf (stderr, "\n Slot %d - Multi Block Control Message CRC16 ERR", slot+1);

        //debug print
        fprintf (stderr, " %X - %X", CRCExtracted, CRCComputed);

        fprintf (stderr, "%s", KNRM);
      }

      //cspdu will only act on any fid/opcodes if good CRC to prevent falsing on control signalling
      if (!is_udt && !pf) dmr_cspdu (opts, state, dmr_pdu_sf_bits, state->dmr_pdu_sf[slot], CRCCorrect, IrrecoverableErrors);

      //send to udt decoder for handling
      if (is_udt && !pf) dmr_udt_decoder (opts, state, state->dmr_pdu_sf[slot], CRCCorrect);

      //Full Super Frame MBC/UDT - Debug Output
      if (opts->payload == 1)
      {
        fprintf (stderr, "%s", KGRN);
        fprintf (stderr, "\n Slot %d - Multi Block Control Message\n  ", slot+1);
        for (i = 0; i < ((blocks+1)*block_len); i++) 
        {
          fprintf (stderr, "%02X", state->dmr_pdu_sf[slot][i]);
          if (i == 11 || i == 23 || i == 35 || i == 47 || i == 59 || i == 71 || i == 83 || i == 95) 
          {
            fprintf (stderr, "\n  "); 
          }
        }
        fprintf (stderr, "%s", KRED);
        if (mbc_crc_good[0] == 0) fprintf (stderr, "MBC/UDT Header CRC ERR ");
        if (mbc_crc_good[1] == 0) fprintf (stderr, "MBC/UDT Blocks CRC ERR ");
        if (pf) fprintf (stderr, "MBC/UDT Header/Blocks Protected ");
        fprintf (stderr, "%s ", KNRM);
      }
    } //end last block flag on MBC

  } //end type 2 (MBC Header and Continuation)

  //leave this seperate so we can reset/zero stuff in case the data header isn't valid, etc
  //switched to an if-elseif-else so we could get the block counter increment on the end
  //without having it in an akward position

  //if the end of normal data header and blocks
  if (type == 1 && state->data_block_counter[slot] == state->data_header_blocks[slot])
  {

    //clear out unified pdu 'superframe' slot
    for (int i = 0; i < 24*127; i++)
    {
      state->dmr_pdu_sf[slot][i] = 0;
    }

    //Zero Out MBC Header Block CRC Valid
    state->data_block_crc_valid[slot][0] = 0; 
    //reset the block counter (data blocks)
    state->data_block_counter[slot] = 1; 
    //reset data header format storage
    state->data_header_format[slot] = 7;
    //flag off data header validity 
    state->data_header_valid[slot] = 0; 
    //flag off conf data flag
    state->data_conf_data[slot] = 0;
    //flag off p_head
    state->data_p_head[slot] = 0;
    //reset padding
    state->data_block_poc[slot] = 0;
    //reset byte counter
    state->data_byte_ctr[slot] = 0;
    //reset ks start value
    state->data_ks_start[slot] = 0;
  }

  //else if the end of MBC Header and Blocks
  else if (type == 2 && lb == 1)
  {

    //clear out unified pdu 'superframe' slot
    for (int i = 0; i < 24*127; i++)
    {
      state->dmr_pdu_sf[slot][i] = 0;
    }

    //Zero Out MBC Header Block CRC Valid
    state->data_block_crc_valid[slot][0] = 0; 
    //reset the block counter (data blocks)
    state->data_block_counter[slot] = 1; 
    //reset data header format storage
    state->data_header_format[slot] = 7;
    //flag off data header validity 
    state->data_header_valid[slot] = 0; 
    //flag off conf data flag
    state->data_conf_data[slot] = 0;
    //flag off p_head
    state->data_p_head[slot] = 0;

  }

  //else increment block counter after sorting/shuffling blocks
  else state->data_block_counter[slot]++;

}

//failsafe to clear old data header, block info, cach, in case of tact/emb/slottype failures 
//or tuning away and we can no longer verify accurate data block reporting
void dmr_reset_blocks (dsd_opts * opts, dsd_state * state)
{
  UNUSED(opts);

  memset (state->data_p_head, 0, sizeof(state->data_p_head));
  memset (state->data_conf_data, 0, sizeof(state->data_conf_data));
  memset (state->dmr_pdu_sf, 0, sizeof(state->dmr_pdu_sf));
  memset (state->data_block_counter, 1, sizeof(state->data_block_counter));
  memset (state->data_block_poc, 0, sizeof(state->data_block_poc));
  memset (state->data_byte_ctr, 0, sizeof(state->data_byte_ctr));
  memset (state->data_ks_start, 0, sizeof(state->data_ks_start));
  memset (state->data_header_blocks, 1, sizeof(state->data_header_blocks));
  memset (state->data_block_crc_valid, 0, sizeof(state->data_block_crc_valid));
  memset (state->dmr_lrrp_source, 0, sizeof(state->dmr_lrrp_source));
  memset (state->dmr_lrrp_target, 0, sizeof(state->dmr_lrrp_target));
  memset (state->dmr_cach_fragment, 1, sizeof (state->dmr_cach_fragment));
  memset (state->cap_plus_csbk_bits, 0, sizeof(state->cap_plus_csbk_bits));
  memset (state->cap_plus_block_num, 0, sizeof(state->cap_plus_block_num));
  memset (state->data_header_valid, 0, sizeof(state->data_header_valid));
  memset (state->data_header_format, 7, sizeof(state->data_header_format));
  //reset some strings -- resetting call string here causes random blink on ncurses terminal (cap+)
  // sprintf (state->call_string[0], "%s", "                     "); //21 spaces
  // sprintf (state->call_string[1], "%s", "                     "); //21 spaces
  sprintf (state->dmr_lrrp_gps[0], "%s", "");
  sprintf (state->dmr_lrrp_gps[1], "%s", "");
}