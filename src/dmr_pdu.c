/*-------------------------------------------------------------------------------
 * dmr_bs.c
 * DMR Data (1/2, 3/4, 1) PDU Decoding
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

char * getTimeL(void) //get pretty hh:mm:ss timestamp
{
  time_t t = time(NULL);

  char * curr;
  char * stamp = asctime(localtime( & t));

  curr = strtok(stamp, " ");
  curr = strtok(NULL, " ");
  curr = strtok(NULL, " ");
  curr = strtok(NULL, " ");

  return curr;
}

char * getDateL(void) {
  char datename[120]; 
  char * curr2;
  struct tm * to;
  time_t t;
  t = time(NULL);
  to = localtime( & t);
  strftime(datename, sizeof(datename), "%Y/%m/%d", to);
  curr2 = strtok(datename, " ");
  return curr2;
}

void dmr_pdu (dsd_opts * opts, dsd_state * state, uint8_t block_len, uint8_t DMR_PDU[])
{

  uint8_t message_len = 0;
  uint8_t slot = state->currentslot;
  uint8_t blocks  = state->data_header_blocks[slot]; 
  uint8_t padding = state->data_header_padding[slot];
  uint8_t lrrp_conf = 0;

  //consider disabling on prop_head data blocks if bad/false decodes
  //if (state->data_p_head[slot] == 0)
   lrrp_conf = dmr_lrrp (opts, state, block_len, DMR_PDU);

  //maybe one day we will have more things to do here
  state->data_conf_data[slot] = 0; //flag off confirmed data after processing it 
  state->data_p_head[slot] = 0; //flag off prop_head data after processing it
  
}

//The contents of this function are mostly my reversed engineered efforts by observing DSDPlus output and matching data bytes
//combined with a few external sources such as OK-DMR for some token values and extra data values (rad and alt)
//this is by no means an extensive LRRP list and is prone to error (unless somebody has the manual or something)
uint8_t dmr_lrrp (dsd_opts * opts, dsd_state * state, uint8_t block_len, uint8_t DMR_PDU[])
{
  int i, j;
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
  uint8_t lat_sign = 0;
  uint32_t lon = 0;
  uint16_t rad = 0;
  uint8_t alt = 0;
  double lat_unit = (double)180/(double)4294967295;
  double lon_unit = (double)360/(double)4294967295;

  //speed/velocity
  uint16_t vel = 0;
  double velocity = 0;
  uint8_t vel_set = 0;

  //track, direction, degrees
  uint8_t degrees = 0;
  uint8_t deg_set = 0; 

  //triggered information report
  uint8_t report = 0;
  uint8_t pot_report = 0; //potential report by finding 0x0D and backtracking a few bytes

  //shim for getting LRRP out of some prop head data blocks
  if (state->data_p_head[slot] == 1)
  {
    i = 0;
    message_len = blocks * block_len; 
  }
  else i = 12; 

  
  //start looking for tokens (using my best understanding of the ok-dmr library xml and python files)
  for (i; i < ( (blocks*block_len) - (padding+4) ); i++) 
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
      case 0x26: //ARRP_UnsolicitedInformationReport_NCDT
      case 0x13: //LRRP_UnsolicitedLocationReport_NCDT
      case 0x15: //LRRP_LocationProtocolReport_NCDT
      case 0x21: //ARRP_TriggeredInformationStopRequest_NCDT
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
      case 0x23: //this value has been seen after the 0x0D message len, appears to be a 2-byte value, was tripping a false on time stamp 
        i += 2;
        break; 
      //answer and report tokens
      case 0x51: //circle-2d
      case 0x54: //circle-3d
      case 0x55: //circle-3d <- false positives on one sample, so either an earlier token occurred (that we don't have) or just random data?
        if (message_len > 0 && lat == 0)
        {
          if (DMR_PDU[i+1] & 0x80) lat_sign = 1;
          lat = ( ( ((DMR_PDU[i+1] & 0x7F ) <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          lon = ( ( (DMR_PDU[i+5]           <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          rad = (DMR_PDU[i+9] << 8) + DMR_PDU[i+10];
          i += 10; //double check
          lrrp_confidence++;
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
          if (DMR_PDU[i+1] & 0x80) lat_sign = 1;
          lat = ( ( ((DMR_PDU[i+1] & 0x7F ) <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          lon = ( ( (DMR_PDU[i+5]           <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          i += 8; //check
          lrrp_confidence++;
        }
        break;
      case 0x69: //point-3d
      case 0x6A: //point-3d
        if (message_len > 0 && lat == 0)
        {
          if (DMR_PDU[i+1] & 0x80) lat_sign = 1;
          lat = ( ( ((DMR_PDU[i+1] & 0x7F ) <<  24 ) + (DMR_PDU[i+2] << 16) + (DMR_PDU[i+3] << 8) + DMR_PDU[i+4]) * 1 );
          lon = ( ( (DMR_PDU[i+5]           <<  24 ) + (DMR_PDU[i+6] << 16) + (DMR_PDU[i+7] << 8) + DMR_PDU[i+8]) * 1 );
          alt =  DMR_PDU[i+9];
          i += 9; //check
          lrrp_confidence++;
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
          vel = (DMR_PDU[i+1] << 8) + DMR_PDU[i+2]; //don't think this is meant to be shifted, but doing it anyways for comparison
          velocity = ( ((double)( (DMR_PDU[i+1] ) + DMR_PDU[i+2] )) / ( (double)128));
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
        //open file by name that is supplied in the ncurses terminal, or cli
        pFile = fopen (opts->lrrp_out_file, "a");
        //write current date/time if not present in LRRP data
        if (!year) fprintf (pFile, "%s\t", getDateL() ); //current date, only add this IF no included timestamp in LRRP data?
        if (!year) fprintf (pFile, "%s\t", getTimeL() ); //current timestamp, only add this IF no included timestamp in LRRP data?
        if (year) fprintf (pFile, "%04d/%02d/%02d\t%02d:%02d:%02d\t", year, month, day, hour, minute, second); //add timestamp from decoded audio if available
        //write data header source (or destination?) if not available in lrrp data
        if (!source) fprintf (pFile, "%08lld\t", state->dmr_lrrp_source[state->currentslot]); //source address from data header
        if (source) fprintf (pFile, "%08d\t", source); //add source form decoded audio if available, else its from the header
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
        if (lat_sign) //print a '-' in front of the value
          fprintf (stderr, "  LRRP - Lat: -%.5lf", (double)lat * lat_unit);
        else fprintf (stderr, "  LRRP - Lat: %.5lf", (double)lat * lat_unit);
        fprintf (stderr, "  Lon: %.5lf", (double)lon * lon_unit);
        if (lat_sign)
          fprintf (stderr, " (-%.5lf, %.5lf)", (double)lat * lat_unit , (double)lon * lon_unit);
        else fprintf (stderr, " (%.5lf, %.5lf)", (double)lat * lat_unit , (double)lon * lon_unit);

        if (opts->lrrp_file_output == 1)
        {
          if (lat_sign) fprintf (pFile, "-");
          fprintf (pFile, "%.5lf\t", (double)lat * lat_unit);
          fprintf (pFile, "%.5lf\t", (double)lon * lon_unit);
        }

      }
      if (rad)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Radius: %d", rad); //unsure of 'units' or calculation for radius (meters?)
      }
      if (alt)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Altitude: %d", alt); //unsure of 'units' or calculation for alt (meters?)
      }
      if (vel_set)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Velocity: %.4lf m/s %.4lf km/h %.4lf mph", velocity, (3.6 * velocity), (2.2369 * velocity));
        if (opts->lrrp_file_output == 1) fprintf (pFile, "%.3lf\t ", (velocity * 3.6) );
      }
      if (deg_set)
      {
        fprintf (stderr, "\n");
        fprintf (stderr, "  LRRP - Track: %d Degrees", degrees);
        if (opts->lrrp_file_output == 1) fprintf (pFile, "%d\t",degrees);
      }

      //close open file
      if (opts->lrrp_file_output == 1)
      {
        fprintf (pFile, "\n");
        fclose (pFile);
      }

      //save to array for ncurses
      if (!source) source = state->dmr_lrrp_source[state->currentslot];
      char sign[8];
      char velstr[20];
      char degstr[20];
      char lrrpstr[100];
      sprintf (lrrpstr, "%s", "");
      sprintf (velstr, "%s", "");
      sprintf (degstr, "%s", "");
      if (lat_sign) sprintf (sign, "%s", "-");
      else sprintf (sign, "%s", ""); 
      if (lat) sprintf (lrrpstr, "LRRP %0d (%s%lf, %lf)", source, sign, (double)lat * lat_unit, (double)lon * lon_unit);
      if (vel_set) sprintf (velstr, " %.4lf km/h", velocity * 3.6);
      if (deg_set) sprintf (degstr, " %d deg", degrees);
      sprintf (state->dmr_lrrp_gps[slot], "%s%s%s", lrrpstr, velstr, degstr);
      
    }
    
  }
  
  else if (pot_report)
  {
    fprintf (stderr, "\n");
    fprintf (stderr, "  Potential ARRP/LRRP Report (Debug): 0x%02X", report);
  }

  // else //debug only, disabled
  // {
  //   fprintf (stderr, "\n");
  //   fprintf (stderr, "\n LRRP Confidence: %d - Message Len: %d Octets", lrrp_confidence, message_len);
  // }

  fprintf (stderr, "%s", KNRM);
  
  
  return (lrrp_confidence);
}