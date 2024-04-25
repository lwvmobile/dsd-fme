/*-------------------------------------------------------------------------------
 * dmr_bs.c
 * DMR Data (1/2, 3/4, 1) PDU Decoding
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

void dmr_pdu (dsd_opts * opts, dsd_state * state, uint8_t block_len, uint8_t DMR_PDU[])
{

  uint8_t slot = state->currentslot;

  //check for more available flag info, etc on these prior to running
  if (DMR_PDU[0] == 0x01) dmr_locn (opts, state, block_len, DMR_PDU);
  else dmr_lrrp (opts, state, block_len, DMR_PDU);

  //maybe one day we will have more things to do here
  state->data_conf_data[slot] = 0; //flag off confirmed data after processing it 
  state->data_p_head[slot] = 0; //flag off prop_head data after processing it
  
}

//The contents of this function are mostly my reversed engineered efforts by observing DSDPlus output and matching data bytes
//combined with a few external sources such as OK-DMR for some token values and extra data values (rad and alt)
//this is by no means an extensive LRRP list and is prone to error (unless somebody has the manual or something)
void dmr_lrrp (dsd_opts * opts, dsd_state * state, uint8_t block_len, uint8_t DMR_PDU[])
{
  int i;
  uint16_t message_len = 0;
  uint8_t slot = state->currentslot;
  uint8_t blocks = state->data_header_blocks[slot];
  uint8_t padding = state->data_header_padding[slot];
  uint8_t lrrp_confidence = 0; //variable to increment based on number of tokens found, the more, the higher the confidence level

  //source/dest and ports
  uint32_t source = 0;
  uint32_t dest = 0;
  uint16_t port_s = 0;
  uint16_t port_d = 0;

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

  //speed -- NOTE: Velocity is Speed + Direction
  uint16_t vel = 0;
  double velocity = 0;
  uint8_t vel_set = 0;
  UNUSED(vel);

  //track, direction, degrees
  uint8_t degrees = 0;
  uint8_t deg_set = 0;

  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");
  // sprintf (deg_glyph, "%s", "d"); 

  //triggered information report
  uint8_t report = 0;
  uint8_t pot_report = 0; //potential report by finding 0x0D and backtracking a few bytes


  //start looking for tokens (using my best understanding of the ok-dmr library xml and python files)
  for (i = 0; i < ( (blocks*block_len) - (padding+4) ); i++) 
  {

    switch(DMR_PDU[i]){
      case 0x0C: //LRRP_TriggeredLocationReport
        if (source == 0)
        {
          source = (DMR_PDU[i+1] << 16 ) + (DMR_PDU[i+2] << 8) + DMR_PDU[i+3];
          dest = (DMR_PDU[i+5] << 16 ) + (DMR_PDU[i+6] << 8) + DMR_PDU[i+7];
          port_s = (DMR_PDU[i+8] << 8) + DMR_PDU[i+9];
          port_d = (DMR_PDU[i+10] << 8) + DMR_PDU[i+11];
          i += 11;
          lrrp_confidence++;
        }
        break;
      //may need more 'report' types to trigger various scenarios
      //disable all but 0x1F if issues or falsing happens often
      //case 0x1D: //ARRP_TriggeredInformationAnswer_NCDT
      //case 0x1E: //ARRP_TriggeredInformationReport = FALSE?
      //case 0x25: //ARRP_UnsolicitedInformationReport_NCDT = (0x25, True, "")
      //case 0x26: //ARRP_UnsolicitedInformationReport_NCDT
      //case 0x13: //LRRP_UnsolicitedLocationReport_NCDT
      //case 0x15: //LRRP_LocationProtocolReport_NCDT
      //case 0x21: //ARRP_TriggeredInformationStopRequest_NCDT
      case 0x1F: //ARRP_TriggeredInformationReport_NCDT
        if (report == 0)
        {
          report = DMR_PDU[i];
          i += 1; 
          lrrp_confidence++;
        }
        break;
      //0x0D always seems to follow 0x1F or other 'report' types two/three bytes later
      case 0x0D: //message len indicator //LRRP_TriggeredLocationReport_NCDT
        if (report > 0 && message_len == 0)
        {
          message_len = DMR_PDU[i+1]; 
          i += 1;
          lrrp_confidence++;
        }
        else if (message_len == 0)
        {
          if (i > 3)
          {
            if (DMR_PDU[i-3] > 0) pot_report = DMR_PDU[i-3];
            if (pot_report < 0x27)
            {
              report = pot_report;
              message_len = DMR_PDU[i+1]; 
              i += 1;
              //no lrrp_confidence on speculative reporting, but will print it as such.
            }
          } 
          
        }
        break;
      //RESULT TOKEN or Request ID!
      case 0x22: //same comment as below, observed followed the message len
      case 0x23: //this value has been seen after the 0x0D message len, appears to be a 2-byte value, was tripping a false on time stamp 
        i += 2;
        break; 
      //answer and report tokens
      case 0x51: //circle-2d
      case 0x54: //circle-3d
      case 0x55: //circle-3d 
        if (message_len > 0 && lat == 0)
        {
          // lat = ( ( ((DMR_PDU[i+1] & 0x7F ) <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          // lon = ( ( ((DMR_PDU[i+5] & 0x7F ) <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          lat = ( ( (DMR_PDU[i+1]           <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          lon = ( ( (DMR_PDU[i+5]           <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          rad = (DMR_PDU[i+9] << 8) + DMR_PDU[i+10];
          i += 10; 
          if (lat > 0 && lon > 0) lrrp_confidence++; //would be better to set by absolute boundary (180 and 90?)
          else lat = 0;
        }
        break;
      
      case 0x34: //Time Interval Periodic Trigger (timestamp)
      case 0x35: //Time Interval Periodic Trigger (timestamp)
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
          if (year > 2000 && year <= 2025) lrrp_confidence++;
          if (year > 2025 || year < 2000) year = 0; //needs future proofing
        }
        break;
      
      case 0x66: //point-2d 
        if (message_len > 0 && lat == 0)
        {
          // lat = ( ( ((DMR_PDU[i+1] & 0x7F ) <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          // lon = ( ( ((DMR_PDU[i+5] & 0x7F ) <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          lat = ( ( (DMR_PDU[i+1]           <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          lon = ( ( (DMR_PDU[i+5]           <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          i += 8; 
          lrrp_confidence++;
        }
        break;
      case 0x69: //point-3d
      case 0x6A: //point-3d
        if (message_len > 0 && lat == 0)
        {
          // lat = ( ( ((DMR_PDU[i+1] & 0x7F ) <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          // lon = ( ( ((DMR_PDU[i+5] & 0x7F ) <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
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
      //below all unknown  
      case 0x37: //result
      case 0x38: //result
      case 0x39: //result 'operations error'? attributes 0x22?
      case 0x6B: //unknown-uint8
      case 0x65: //lev-conf
      default:
        //do nothing
        break;
    }
  }
  if (report && message_len > 0)
  {
    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, "\n LRRP Confidence: %d - Message Len: %d Octets", lrrp_confidence, message_len);
    if (lrrp_confidence >= 3) //find the sweet magical number
    {
      //now we can open our lrrp file and write to it as well
      FILE * pFile; //file pointer
      if (opts->lrrp_file_output == 1)
      {
        char * timestr  = getTime();
        char * datestr  = getDate();

        //open file by name that is supplied in the ncurses terminal, or cli
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
      }

      if (pot_report)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  Potential ARRP/LRRP Report (Debug): 0x%02X", report);
      }
      if (report)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  Report: 0x%02X", report);
        if (report == 0x1F) fprintf (stderr, " ARRP_TriggeredInformationReport_NCDT "); //customize later when more is learned 
        if (report == 0x21) fprintf (stderr, " ARRP_TriggeredInformationStopRequest_NCDT ");
        if (report == 0x22) fprintf (stderr, " ARRP_TriggeredInformationStopAnswer ");
        if (report == 0x25) fprintf (stderr, " ARRP_UnsolicitedInformationReport_NCDT ");
        if (report == 0x26) fprintf (stderr, " ARRP_UnsolicitedInformationReport_NCDT ");
        if (report == 0x27) fprintf (stderr, " ARRP_InformationProtocolRequest_NCDT ");
        if (report == 0x13) fprintf (stderr, " LRRP_UnsolicitedLocationReport_NCDT ");
        if (report == 0x15) fprintf (stderr, " LRRP_UnsolicitedLocationReport_NCDT ");

      }
      if (source)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "       Source: %08d - %04d", source, port_s);
        fprintf (stderr, "\n");
        fprintf (stderr, "  Destination: %08d - %04d", dest, port_d);

      }
      if (year)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Time: ");
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

        fprintf (stderr, "  LRRP - Lat: %.5lf", lat_fin);
        fprintf (stderr, "  Lon: %.5lf", lon_fin);
        fprintf (stderr, " (%.5lf, %.5lf)", lat_fin , lon_fin);

        // if (opts->lrrp_file_output == 1)
        // {
        //   fprintf (pFile, "%.5lf\t", lat_fin);
        //   fprintf (pFile, "%.5lf\t", lon_fin);
        // }

      }
       //always print into the lrrp file, even if zeroes, keep alignment correct
      if (opts->lrrp_file_output == 1)
      {
        fprintf (pFile, "%.5lf\t", lat_fin);
        fprintf (pFile, "%.5lf\t", lon_fin);
        //
        //
      }
      if (rad)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Radius: %dm", rad); //unsure of 'units' or calculation for radius (meters?)
      }
      if (alt)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Altitude: %dm", alt); //unsure of 'units' or calculation for alt (meters?)
      }
      if (vel_set)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Speed: %.4lf m/s %.4lf km/h %.4lf mph", velocity, (3.6 * velocity), (2.2369 * velocity));
        // if (opts->lrrp_file_output == 1) fprintf (pFile, "%.3lf\t ", (velocity * 3.6) );
      }
      //always print into the lrrp file, even if zeroes, keep alignment correct
      if (opts->lrrp_file_output == 1) fprintf (pFile, "%.3lf\t ", (velocity * 3.6) );
      if (deg_set)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Track: %d%s", degrees, deg_glyph);
        // if (opts->lrrp_file_output == 1) fprintf (pFile, "%d\t",degrees);
      }
      //always print into the lrrp file, even if zeroes, keep alignment correct
      if (opts->lrrp_file_output == 1) fprintf (pFile, "%d\t",degrees);

      //close open file
      if (opts->lrrp_file_output == 1)
      {
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
      if (lat) sprintf (lrrpstr, "LRRP %0d (%lf, %lf)", source, lat_fin, lon_fin);
      if (vel_set) sprintf (velstr, " %.4lf km/h", velocity * 3.6);
      if (deg_set) sprintf (degstr, " %d%s  ", degrees, deg_glyph);
      sprintf (state->dmr_lrrp_gps[slot], "%s%s%s", lrrpstr, velstr, degstr);
      
    }
    
  }
  
  else if (pot_report)
  {
    fprintf (stderr, "\n");
    fprintf (stderr, "  Potential ARRP/LRRP Report (Debug): 0x%02X", report);
  }

  fprintf (stderr, "%s", KNRM);
  
}

void dmr_locn (dsd_opts * opts, dsd_state * state, uint8_t block_len, uint8_t DMR_PDU[])
{
  UNUSED(opts);

  int i;
  uint8_t slot = state->currentslot;
  uint8_t blocks = state->data_header_blocks[slot];
  uint8_t padding = state->data_header_padding[slot];
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

  //need to experiment best way to do this portion
  uint8_t  lat_deg = 0;
  uint8_t  lat_min = 0;
  uint16_t lat_sec = 0;

  uint8_t  lon_deg = 0;
  uint8_t  lon_min = 0;
  uint16_t lon_sec = 0;

  char lat_ord[2];
  char lon_ord[2];
  sprintf (lat_ord, "%s", "N");
  sprintf (lon_ord, "%s", "E");

  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");
  // sprintf (deg_glyph, "%s", "d");

  //more strings...
  char locnstr[50];
  char latstr[75];
  char lonstr[75];
  sprintf (locnstr, "%s", "     ");
  sprintf (latstr, "%s", "     ");
  sprintf (lonstr, "%s", "     ");

  //TODO: conversion to decimal format
  //DD = d + (min/60) + (sec/3600)
  //N is positive, E is positive?? Assuming its like a 2D plane
  int lat_sign = 1; //positive 1 or negative 1
  int lon_sign = 1; //positive 1 or negative 1
  UNUSED2(lat_sign, lon_sign);

  //start looking for specific bytes corresponding to 'letters' A (time), NSEW (ordinal directions), etc
  for (i = 0; i < ( (blocks*block_len) - (padding+4) ); i++)
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
        sprintf (lat_ord, "%s", "S");
        lat_sign = -1;
      case 0x4E: //N -- North
        lat     = 1;
        lat_deg = ((DMR_PDU[i+1] - 0x30) << 4) | (DMR_PDU[i+2] - 0x30);
        lat_min = ((DMR_PDU[i+3] - 0x30) << 4) | (DMR_PDU[i+4] - 0x30);
        lat_sec = ((DMR_PDU[i+6] - 0x30) << 12) | ((DMR_PDU[i+7] - 0x30) << 8) | ((DMR_PDU[i+8] - 0x30) << 4) | ((DMR_PDU[i+9] - 0x30) << 0);
        i += 8;
        break;
      
      case 0x57: //W -- West
        sprintf (lon_ord, "%s", "W");
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
    fprintf (stderr, "%s", KYEL);
    fprintf (stderr, "\n LOCN Report - Source: [%d]\n", source);
    if (time) fprintf (stderr, "  20%02X/%02X/%02X %02X:%02X:%02X ", year, month, day, hour, minute, second);
    fprintf (stderr, "Lat: %s %02X%s%02X\"%04X' Lon: %s %02X%s%02X\"%04X' ", lat_ord, lat_deg, deg_glyph, lat_min, lat_sec, lon_ord, lon_deg, deg_glyph, lon_min, lon_sec);

    //string manip for ncurses terminal display
    sprintf (locnstr, "LOCN %d ", source);
    sprintf (latstr, "%s %02X%s%02X\"%04X' ", lat_ord, lat_deg, deg_glyph, lat_min, lat_sec);
    sprintf (lonstr, "%s %02X%s%02X\"%04X' ", lon_ord, lon_deg, deg_glyph, lon_min, lon_sec);
    sprintf (state->dmr_lrrp_gps[slot], "%s%s%s", locnstr, latstr, lonstr);

  } 

}