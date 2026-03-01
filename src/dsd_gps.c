/*-------------------------------------------------------------------------------
 * dsd_gps.c
 * GPS Handling Functions for Various Protocols
 *
 * LWVMOBILE
 * 2023-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

void lip_protocol_decoder (dsd_opts * opts, dsd_state * state, uint8_t * input)
{
  //NOTE: This is defined in ETSI TS 102 361-4 V1.12.1 (2023-07) p208
  //6.6.11.3.2 USBD Polling Service Poll Response PDU for LIP (That's a mouthful)

  int slot = state->currentslot;

  //May need to set this in the UDT header, or pass it into this function to be sure
  // uint32_t src = 0;
  // if (slot == 0)
  //   src = state->lastsrc;
  // else src = state->lastsrcR;

  //NOTE: This format is pretty much the same as DMR EMB GPS, but has a few extra elements,
  //so I got lazy and just lifted most of the code from there, also assuming same lat/lon calcs
  //since those have been tested to work in DMR EMB GPS and units are same in LIP

  fprintf (stderr, "Location Information Protocol; ");

  // uint8_t service_type = (uint8_t)ConvertBitIntoBytes(&input[0], 4); //checked before arrival here
  uint8_t time_elapsed = (uint8_t)ConvertBitIntoBytes(&input[6], 2);
  uint8_t lon_sign = input[8];
  uint32_t lon = (uint32_t)ConvertBitIntoBytes(&input[9], 24); //8, 25
  uint8_t lat_sign = input[33];
  uint32_t lat = (uint32_t)ConvertBitIntoBytes(&input[34], 23); //33, 24
  uint8_t pos_err = (uint8_t)ConvertBitIntoBytes(&input[57], 2);
  uint8_t hor_vel = (uint8_t)ConvertBitIntoBytes(&input[59], 7);
  uint8_t dir_tra = (uint8_t)ConvertBitIntoBytes(&input[66], 4);
  uint8_t reason = (uint8_t)ConvertBitIntoBytes(&input[70], 3);
  uint8_t add_hash = (uint8_t)ConvertBitIntoBytes(&input[73], 8); //MS Source Address Hash

  //NOTE: May need to use double instead of float to avoid rounding errors
  double latitude, longitude = 0.0f;
  double lat_unit = (double)180/ pow (2.0, 24); //180 divided by 2^24 -- 6.3.30
  double lon_unit = (double)360/ pow (2.0, 25); //360 divided by 2^25 -- 6.3.50
  double lon_sf = 1.0f; //float value we can multiple longitude with
  double lat_sf = 1.0f; //float value we can multiple latitude with

  char latstr[3];
  char lonstr[3];
  sprintf (latstr, "%s", "N");
  sprintf (lonstr, "%s", "E");

  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");

  //lat and lon calculations
  if (lat_sign)
  {
    lat = 0x800001 - lat;
    sprintf (latstr, "%s", "S");
    lat_sf = -1.0f;
  }
  latitude = ((double)lat * lat_unit);

  if (lon_sign)
  {
    lon = 0x1000001 - lon;
    sprintf (lonstr, "%s", "W");
    lon_sf = -1.0f;
  }
  longitude = ((double)lon * lon_unit);

  //6.3.63 Position Error
  //6.3.17 Horizontal velocity
  /*
    Horizontal velocity shall be encoded for speeds 0 km/h to 28 km/h in 1 km/h steps and
    from 28 km/h onwards using equation: v = C × (1 + x)^(K-A) + B where:
  */
  float v = 0.0f;
  float C = 16.0f;
  float x = 0.038f;
  float A = 13.0f;
  float K = (float)hor_vel;
  float B = 0.0f;

  if (hor_vel > 28)
    v = C * (pow(1+x, K-A)) + B; //I think this pow function is set up correctly
  else v = (float)hor_vel;

  float dir = (((float)dir_tra + 11.25f)/22.5f); //page 68, Table 6.45

  //truncated and rounded forms
  int vt = (int)v;
  int dt = (int)dir;

  //sanity check
  if (fabs (latitude) < 90 && fabs(longitude) < 180)
  {
    fprintf (stderr, "Src(Hash); %03d;  Lat: %.5lf%s%s Lon: %.5lf%s%s (%.5lf, %.5lf); Spd: %d km/h; Dir: %d%s",add_hash, latitude, deg_glyph, latstr, longitude, deg_glyph, lonstr, lat_sf * latitude, lon_sf * longitude, vt, dt, deg_glyph);

    //6.3.63 Position Error
    uint16_t position_error = 2 * pow(10, pos_err); //2 * 10^pos_err
    if (pos_err == 0x7 ) fprintf (stderr, "\n  Position Error: Unknown or Invalid;");
    else fprintf (stderr, "\n  Position Error: Less than %dm;", position_error);

    //Reason For Sending 6.6.11.3.3 Table 6.80
    if (reason) fprintf (stderr, " Reserved: %d;", reason);
    else fprintf (stderr, " Request Response; ");

    //6.3.78 Time elapsed
    if (time_elapsed == 0) fprintf (stderr, " TE: < 5s;");
    if (time_elapsed == 1) fprintf (stderr, " TE: < 5m;");
    if (time_elapsed == 2) fprintf (stderr, " TE: < 30m;");
    if (time_elapsed == 3) fprintf (stderr, " TE: NA or UNK;"); //not applicable or unknown

    //save to array for ncurses
    if (pos_err != 0x7)
    {
      sprintf (state->dmr_embedded_gps[slot], "%03d; LIP: %.5lf%s%s %.5lf%s%s; Err: %dm; Spd: %d km/h; Dir: %d%s", add_hash, latitude, deg_glyph, latstr, longitude, deg_glyph, lonstr, position_error, vt, dt, deg_glyph);
    }
    else sprintf (state->dmr_embedded_gps[slot], "%03d; LIP: %.5lf%s%s %.5lf%s%s Unknown Pos Err; Spd: %d km/h; Dir %d%s", add_hash, latitude, deg_glyph, latstr, longitude, deg_glyph, lonstr, vt, dt, deg_glyph);

    //save to event history string
    sprintf (state->event_history_s[slot].Event_History_Items[0].gps_s, "%s", state->dmr_embedded_gps[slot]);

    //save to LRRP report for mapping/logging
    FILE * pFile; //file pointer
    if (opts->lrrp_file_output == 1)
    {

      char * datestr = getDate();
      char * timestr = getTime();

      //open file by name that is supplied in the ncurses terminal, or cli
      pFile = fopen (opts->lrrp_out_file, "a");
      fprintf (pFile, "%s\t", datestr );
      fprintf (pFile, "%s\t", timestr );
      fprintf (pFile, "%08d\t", add_hash);
      fprintf (pFile, "%.5lf\t", latitude);
      fprintf (pFile, "%.5lf\t", longitude);
      fprintf (pFile, "%d\t", vt); //speed in km/h
      fprintf (pFile, "%d\t", dt); //direction of travel
      fprintf (pFile, "\n");
      fclose (pFile);

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

  }
  else fprintf (stderr, " Position Calculation Error;");

}

void nmea_iec_61162_1 (dsd_opts * opts, dsd_state * state, uint8_t * input, uint32_t src, int type)
{
  int slot = state->currentslot;

  //NOTE: The Only difference between Short (type == 1) and Long Format (type == 2)
  //is the utc_ss3 on short vs utc_ss6 and inclusion of COG value on long

  //NOTE: MFID Specific Formats are not handled here, unknown, could be added if worked out
  //they could share most of the same elements and use the large spare bits in block 2 for extra
  // uint8_t mfid = (uint8_t)ConvertBitIntoBytes(&input[88], 8); //on type 3, last octet of first block carries MFID

  // uint8_t nmea_c = input[0];  //encrypted -- checked before we get here
  uint8_t nmea_ns = input[1]; //north/south (lat sign)
  uint8_t nmea_ew = input[2]; //east/west (lon sign)
  uint8_t nmea_q = input[3]; //Quality Indicator (no fix or fix valid)
  uint8_t nmea_speed = (uint8_t)ConvertBitIntoBytes(&input[4], 7); //speed in knots (127 = greater than 126 knots)

  //Latitude Bits
  uint8_t nmea_ndeg = (uint8_t)ConvertBitIntoBytes(&input[11], 7); //Latitude Degrees
  uint8_t nmea_nmin = (uint8_t)ConvertBitIntoBytes(&input[18], 6); //Latitude Minutes
  uint16_t nmea_nminf = (uint16_t)ConvertBitIntoBytes(&input[24], 14); //Latitude Fractions of Minutes

  //Longitude Bits
  uint8_t nmea_edeg = (uint8_t)ConvertBitIntoBytes(&input[38], 8); //Longitude Degrees
  uint8_t nmea_emin = (uint8_t)ConvertBitIntoBytes(&input[46], 6); //Longitude Minutes
  uint16_t nmea_eminf = (uint16_t)ConvertBitIntoBytes(&input[52], 14); //Longitude Fractions of Minutes

  //UTC Time and COG
  uint8_t nmea_utc_hh  = (uint8_t)ConvertBitIntoBytes(&input[66], 5);
  uint8_t nmea_utc_mm  = (uint8_t)ConvertBitIntoBytes(&input[71], 6);
  //seconds and the addition of COG is the difference between short and long formats
  uint8_t nmea_utc_ss3 = (uint8_t)ConvertBitIntoBytes(&input[77], 3) * 10; //seconds in 10s
  uint8_t nmea_utc_ss6 = (uint8_t)ConvertBitIntoBytes(&input[77], 6);     //seconds in 1s
  uint16_t nmea_cog = (uint16_t)ConvertBitIntoBytes(&input[103], 9);     //course over ground in degrees

  //lat and lon conversion
  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");
  float latitude = 0.0f;
  float longitude = 0.0f;
  // float m_unit = 1.0f / 60.0f;     //unit to convert min into decimal value - (1/60)*60 minutes = 1 degree
  // float mm_unit = 1.0f / 10000.0f;  //unit to convert minf into decimal value - (0000 - 9999) 0.0001×9999 = .9999 minutes, so its sub 1 minute decimal

  //testing w/ Harris NMEA like values (ran tests over there with this code, this seems to work on those values)
  float m_unit = 1.0f / 60.0f;     //unit to convert min into decimal value - (1/60)*60 minutes = 1 degree
  float mm_unit = 1.0f / 600000.0f;  //unit to convert minf into decimal value - (0000 - 9999) 0.0001×9999 = .9999 minutes, so its sub 1 minute decimal

  //speed conversion
  float fmps, fmph, fkph = 0.0f; //conversion of knots to mps, kph, and mph values
  fmps = (float)nmea_speed * 0.514444; UNUSED(fmps);
  fmph = (float)nmea_speed * 1.15078f; UNUSED(fmph);
  fkph = (float)nmea_speed * 1.852f;

  //calculate decimal representation of latidude and longitude (need some samples to test)
  latitude  = ( (float)nmea_ndeg + ((float)nmea_nmin*m_unit) + ((float)nmea_nminf*mm_unit) );
  longitude = ( (float)nmea_edeg + ((float)nmea_emin*m_unit) + ((float)nmea_eminf*mm_unit) );

  if (!nmea_ns) latitude  *= -1.0f; //0 is South, 1 is North
  if (!nmea_ew) longitude *= -1.0f; //0 is West, 1 is East

  fprintf (stderr, " GPS: %f%s, %f%s;", latitude, deg_glyph, longitude, deg_glyph);

  //Speed in Knots
  if (nmea_speed > 126)
    fprintf (stderr, " SPD > 126 knots or %f kph;", fkph);
  else
    fprintf (stderr, " SPD: %d knots; %f kph;", nmea_speed, fkph);

  //Print UTC Time and COG according to Format Type
  if (type == 1)
    fprintf (stderr, " FIX: %d; %02d:%02d:%02d UTC; Short Format;", nmea_q, nmea_utc_hh, nmea_utc_mm, nmea_utc_ss3);
  if (type == 2)
    fprintf (stderr, " FIX: %d; %02d:%02d:%02d UTC; COG: %d%s; Long Format;", nmea_q, nmea_utc_hh, nmea_utc_mm, nmea_utc_ss6, nmea_cog, deg_glyph);

  //save to ncurses string
  sprintf (state->dmr_embedded_gps[slot], "GPS: (%f%s, %f%s)", latitude, deg_glyph, longitude, deg_glyph);

  //save to event history string
  sprintf (state->event_history_s[slot].Event_History_Items[0].gps_s, "(%f%s, %f%s)", latitude, deg_glyph, longitude, deg_glyph);

  //save to LRRP report for mapping/logging
  FILE * pFile; //file pointer
  if (opts->lrrp_file_output == 1)
  {

    char * datestr = getDate();
    char * timestr = getTime();

    int s = (int)fkph; //rounded interger format for the log report
    int a = 0;
    if (type == 2)
      a = nmea_cog; //long format only
    //open file by name that is supplied in the ncurses terminal, or cli
    pFile = fopen (opts->lrrp_out_file, "a");
    fprintf (pFile, "%s\t", datestr );
    fprintf (pFile, "%s\t", timestr ); //could switch to UTC time if desired, but would require local user offset
    fprintf (pFile, "%08d\t", src);
    fprintf (pFile, "%.6lf\t", latitude);
    fprintf (pFile, "%.6lf\t", longitude);
    fprintf (pFile, "%d\t ", s);
    fprintf (pFile, "%d\t ", a);
    fprintf (pFile, "\n");
    fclose (pFile);

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

}

//harris location on PTT (MAC) 3.100
void harris_lptt (dsd_opts * opts, dsd_state * state, uint8_t * input, uint32_t src, int slot, int phase)
{

  //I really wonder about the engineers who laid out this message

  //NOTE: I have only seen the MAC variation of this message description, I cannot 100% confirm
  //the two block LCW format is identical, but it does carry the same amount of relevant bytes
  //observation shows the GPS coordinates are accurate on LCW format, but I cannot
  //vouch for the COG,SPEED,TIME, EMG and GPS Quality elements when in LCW format.

  //starting at relevant information, bytes 0,1,2,3
  uint16_t lat_min_dec = (uint16_t)ConvertBitIntoBytes(input+0, 16);
  uint16_t lat_ns = input[16]; //0 == N, 1 == S
  uint16_t lat_min_int = (uint16_t)ConvertBitIntoBytes(input+17, 7);
  uint16_t lat_deg_int = (uint16_t)ConvertBitIntoBytes(input+24, 8);

  //starting at relevant information, bytes 4,5,6,7
  uint16_t lon_min_dec = (uint16_t)ConvertBitIntoBytes(input+32, 16);
  uint16_t lon_ew = input[48]; //0 == N, 1 == S
  uint16_t lon_min_int = (uint16_t)ConvertBitIntoBytes(input+49, 7);
  uint16_t lon_deg_int = (uint16_t)ConvertBitIntoBytes(input+56, 8);

  //starting at relevant information, bytes 8,9
  uint32_t time_secs_low = (uint32_t)ConvertBitIntoBytes(input+64, 16);

  //starting at relevant information, bytes 10,11,12,13
  uint16_t course_low  = (uint16_t)ConvertBitIntoBytes(input+80, 8);
  uint16_t speed_low   = (uint16_t)ConvertBitIntoBytes(input+88, 8);
  uint16_t course_high = (uint16_t)ConvertBitIntoBytes(input+96+4, 4); //says 3,2,1,0,
  uint16_t speed_high  = (uint16_t)ConvertBitIntoBytes(input+96+0, 4); //says 7,6,5,4

  //starting at relevant information, byte  14
  uint32_t time_secs_high = input[104];
  uint8_t status_emergency = input[105];
  uint8_t gps_quality = (uint16_t)ConvertBitIntoBytes(input+106, 2);
  uint8_t gps_sat_num = (uint16_t)ConvertBitIntoBytes(input+108, 4);

  //start calculating things
  float lat_DDMMmm = (float)lat_deg_int + ((float)lat_min_int / 60.0f) + ((float)lat_min_dec / 600000.0f);
  float lon_DDMMmm = (float)lon_deg_int + ((float)lon_min_int / 60.0f) + ((float)lon_min_dec / 600000.0f);

  if (lat_ns)
    lat_DDMMmm *= -1.0f;
  if (lon_ew)
    lon_DDMMmm *= -1.0f;

  //course conversion
  uint32_t course = (course_high << 4) | course_low; //tenths of degrees
  float course_deg = (float)course / 10.0f; //actual value

  //speed conversion
  uint32_t speed_knots = (speed_high << 4) | speed_low; //tenths of degrees
  float fmps, fmph, fkph = 0.0f; //conversion of knots to mps, kph, and mph values
  fmps = ((float)speed_knots / 10.0f) * 0.514444; UNUSED(fmps);
  fmph = ((float)speed_knots / 10.0f) * 1.15078f; UNUSED(fmph);
  fkph = ((float)speed_knots / 10.0f) * 1.852f;   UNUSED(fkph);

  //seconds since midnight as a 17-bit representation and calculation to HHMMSS format
  uint32_t sec_m = (time_secs_high << 16) | time_secs_low;
  uint32_t thour = sec_m / 3600;
  uint32_t tmin  = (sec_m % 3600) / 60;
  uint32_t tsec  = (sec_m % 3600) % 60;

  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");

  //print results
  fprintf (stderr, "\n");
  if (phase == 2)
  {
    fprintf (stderr, " LCH: %d;", slot);
    if (src != 0)
      fprintf (stderr, " SRC: %08d;", src);
    else
      fprintf (stderr, " SRC: UNK;");

    if (gps_quality < 3) //GPS hardware present
    {
      fprintf (stderr, " GPS: %f%s, %f%s;", lat_DDMMmm, deg_glyph, lon_DDMMmm, deg_glyph);
      if (gps_quality == 0)
        fprintf (stderr, " Last Fix;"); //No fix still holds last values
      else if (gps_quality == 1)
        fprintf (stderr, " Curr Fix;");
      else if (gps_quality == 2)
        fprintf (stderr, " Diff Fix;"); //presumably either lat or lon only changed, not both?

      fprintf (stderr, " #Sats: %d;", gps_sat_num);
    }
    else if (gps_quality == 3)
      fprintf (stderr, " GPS: Not Enabled / No Hardware Present;");

    fprintf (stderr, " COG: %f%s;", course_deg, deg_glyph);
    fprintf (stderr, " SPD: %f k/h;", fkph);

    if (sec_m != 0x1FFFF) // && sec_m != 0 (so says the doc, but midnight is a valid time)
      fprintf (stderr, " T: %02d:%02d:%02d;", thour, tmin, tsec);

    if (status_emergency)
      fprintf (stderr, " Emergency;");
  }
  else if (phase == 1)
  {
    if (src != 0)
      fprintf (stderr, " SRC: %08d;", src);
    else
      fprintf (stderr, " SRC: UNK;");
    fprintf (stderr, " GPS: %f%s, %f%s;", lat_DDMMmm, deg_glyph, lon_DDMMmm, deg_glyph);

    //again, can't vouch 100%, so just going to dump the GPS Coordinates and zero out other values for LRRP file
    fkph = 0.0f;
    course_deg = 0.0f;
  }

  //save to ncurses string
  sprintf (state->dmr_embedded_gps[slot], "(%f%s, %f%s)", lat_DDMMmm, deg_glyph, lon_DDMMmm, deg_glyph);

  //save to event history string
  if (state->event_history_s[slot].Event_History_Items[0].source_id == src && src != 0)
    sprintf (state->event_history_s[slot].Event_History_Items[0].gps_s, "%s", state->dmr_embedded_gps[slot]);

  //save to LRRP report for mapping/logging
  if (opts->lrrp_file_output == 1 && src != 0 && gps_quality != 3)
  {

    char * datestr = getDate();
    char * timestr = getTime();

    //rounded interger formats for the log report
    int s = (int)fkph;
    int a = (int)course_deg;

    //open file by name that is supplied in the ncurses terminal, or cli
    FILE * pFile; //file pointer
    pFile = fopen (opts->lrrp_out_file, "a");
    fprintf (pFile, "%s\t", datestr );
    fprintf (pFile, "%s\t", timestr );
    fprintf (pFile, "%08d\t", src);
    fprintf (pFile, "%.6lf\t", lat_DDMMmm);
    fprintf (pFile, "%.6lf\t", lon_DDMMmm);
    fprintf (pFile, "%d\t ", s);
    fprintf (pFile, "%d\t ", a);
    fprintf (pFile, "\n");
    fclose (pFile);

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

  //debugging dump of this message
  // if (opts->payload == 1)
  // {
  //   fprintf (stderr, "\n ");
  //   for (int i = 0; i < 14; i++)
  //     fprintf (stderr, "%02X ", (uint8_t)ConvertBitIntoBytes(input+(i*8), 8));
  // }

}

//externalize embedded GPS - Confirmed working now on NE, NW, SE, and SW coordinates
void dmr_embedded_gps (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[])
{
  UNUSED(opts);

  fprintf (stderr, "%s", KYEL);
  fprintf (stderr, " Embedded GPS:");
  uint8_t slot = state->currentslot;
  uint8_t pf = lc_bits[0];
  uint8_t res_a = lc_bits[1];
  uint8_t res_b = (uint8_t)ConvertBitIntoBytes(&lc_bits[16], 4);
  uint8_t pos_err = (uint8_t)ConvertBitIntoBytes(&lc_bits[20], 3);
  UNUSED2(res_a, res_b);

  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");

  uint32_t lon_sign = lc_bits[23];
  uint32_t lon = (uint32_t)ConvertBitIntoBytes(&lc_bits[24], 24);
  uint32_t lat_sign = lc_bits[48];
  uint32_t lat = (uint32_t)ConvertBitIntoBytes(&lc_bits[49], 23);
  double lon_sf = 1.0f; //float value we can multiple longitude with
  double lat_sf = 1.0f; //float value we can multiple latitude with

  double lat_unit = (double)180/ pow (2.0, 24); //180 divided by 2^24
  double lon_unit = (double)360/ pow (2.0, 25); //360 divided by 2^25

  char latstr[3];
  char lonstr[3];
  sprintf (latstr, "%s", "N");
  sprintf (lonstr, "%s", "E");

  //run calculations and print
  //7.2.16 and 7.2.17 (two's compliment)

  double latitude = 0;
  double longitude = 0;

  if (pf) fprintf (stderr, " Protected");
  else
  {
    if (lat_sign)
    {
      lat = 0x800001 - lat;
      sprintf (latstr, "%s", "S");
      lat_sf = -1.0f;
    }
    latitude = ((double)lat * lat_unit);

    if (lon_sign)
    {
      lon = 0x1000001 - lon;
      sprintf (lonstr, "%s", "W");
      lon_sf = -1.0f;
    }
    longitude = ((double)lon * lon_unit);

    //sanity check
    if (fabs(latitude) < 90 && fabs(longitude) < 180)
    {
      fprintf (stderr, " Lat: %.5lf%s%s Lon: %.5lf%s%s (%.5lf, %.5lf)", latitude, deg_glyph, latstr, longitude, deg_glyph, lonstr, lat_sf * latitude, lon_sf * longitude);

      //7.2.15 Position Error
      uint16_t position_error = 2 * pow(10, pos_err); //2 * 10^pos_err
      if (pos_err == 0x7 ) fprintf (stderr, "\n  Position Error: Unknown or Invalid");
      else fprintf (stderr, "\n  Position Error: Less than %dm", position_error);

      //save to array for ncurses
      if (pos_err != 0x7)
      {
        sprintf (state->dmr_embedded_gps[slot], "GPS: %.5lf%s%s %.5lf%s%s Err: %dm", latitude, deg_glyph, latstr, longitude, deg_glyph, lonstr, position_error);
      }
      else sprintf (state->dmr_embedded_gps[slot], "GPS: %.5lf%s%s %.5lf%s%s Unknown Pos Err", latitude, deg_glyph, latstr, longitude, deg_glyph, lonstr);

      uint32_t src = 0;
      if (slot == 0) src = state->lasttg;
      if (slot == 1) src = state->lasttgR;

      //save to event history string
      if (state->event_history_s[slot].Event_History_Items[0].source_id == src)
        sprintf (state->event_history_s[slot].Event_History_Items[0].gps_s, "%s", state->dmr_embedded_gps[slot]);

      //save to LRRP report for mapping/logging
      FILE * pFile; //file pointer
      if (opts->lrrp_file_output == 1)
      {

        char * datestr = getDate();
        char * timestr = getTime();

        //open file by name that is supplied in the ncurses terminal, or cli
        pFile = fopen (opts->lrrp_out_file, "a");
        fprintf (pFile, "%s\t", datestr );
        fprintf (pFile, "%s\t", timestr );
        fprintf (pFile, "%08d\t", src);
        fprintf (pFile, "%.5lf\t", latitude);
        fprintf (pFile, "%.5lf\t", longitude);
        fprintf (pFile, "0\t " ); //zero for velocity
        fprintf (pFile, "0\t " ); //zero for azimuth
        fprintf (pFile, "\n");
        fclose (pFile);

      }
    }
  }

  fprintf (stderr, "%s", KNRM);
}

//This Function needs testing, is tested working for NW and NE lat and
//long coordinates, but not for SE and SW coordinates
void apx_embedded_gps (dsd_opts * opts, dsd_state * state, uint8_t lc_bits[])
{

  fprintf (stderr, "%s", KYEL);
  fprintf (stderr, " GPS:");
  uint8_t slot = state->currentslot;
  uint8_t pf = lc_bits[0];
  uint8_t res_a = lc_bits[1];
  uint8_t res_b = (uint8_t)ConvertBitIntoBytes(&lc_bits[16], 7);
  uint8_t expired = lc_bits[23]; //this bit seems to indicate that the GPS coordinates are out of date or fresh

  char deg_glyph[4];
  sprintf (deg_glyph, "%s", "°");

  uint32_t lon_sign = lc_bits[48];
  uint32_t lon = (uint32_t)ConvertBitIntoBytes(&lc_bits[49], 23);
  uint32_t lat_sign = lc_bits[24];
  uint32_t lat = (uint32_t)ConvertBitIntoBytes(&lc_bits[25], 23);

  double lat_unit = 90.0f / 0x7FFFFF;
  double lon_unit = 180.0f / 0x7FFFFF;

  char latstr[3];
  char lonstr[3];
  char valid[12];
  sprintf (latstr, "%s", "N");
  sprintf (lonstr, "%s", "E");
  sprintf (valid, "%s", "Current Fix");

  double latitude = 0;
  double longitude = 0;

  if (pf) fprintf (stderr, " Protected");
  else
  {

    latitude = ((double)lat * lat_unit);
    if (lat_sign)
    {
      latitude -= 90.0f;
      sprintf (latstr, "%s", "S");
    }

    longitude = ((double)lon * lon_unit);
    if (lon_sign)
    {
      longitude -= 180.0f;
      sprintf (lonstr, "%s", "W");
    }

    //sanity check
    if (fabs ((float)latitude) < 90 && fabs((float)longitude) < 180)
    {
      fprintf (stderr, " Lat: %.5lf%s%s Lon: %.5lf%s%s (%.5lf, %.5lf) ", latitude, deg_glyph, latstr, longitude, deg_glyph, lonstr, latitude, longitude);

      if (expired)
      {
        fprintf (stderr, "Last Fix; ");
        sprintf (valid, "%s", "Last Fix");
      }
      else if (!expired)
        fprintf (stderr, "Current Fix; ");

      if (res_a)
        fprintf (stderr, "RES_A: %d; ", res_a);

      if (res_b)
        fprintf (stderr, "RES_B: %02X; ", res_b);

      uint32_t src = 0;
      if (slot == 0) src = state->lastsrc;
      if (slot == 1) src = state->lastsrcR;

      //save to array for ncurses
      sprintf (state->dmr_embedded_gps[slot], "GPS: %lf%s%s %lf%s%s (%lf, %lf) %s", latitude, deg_glyph, latstr, longitude, deg_glyph, lonstr, latitude, longitude, valid);

      //save to event history string
      if (state->event_history_s[slot].Event_History_Items[0].source_id == src)
        sprintf (state->event_history_s[slot].Event_History_Items[0].gps_s, "%s", state->dmr_embedded_gps[slot]);

      //save to LRRP report for mapping/logging
      FILE * pFile; //file pointer
      if (opts->lrrp_file_output == 1)
      {
        char * datestr = getDate();
        char * timestr = getTime();

        //open file by name that is supplied in the ncurses terminal, or cli
        pFile = fopen (opts->lrrp_out_file, "a");
        fprintf (pFile, "%s\t", datestr );
        fprintf (pFile, "%s\t", timestr );
        fprintf (pFile, "%08d\t", src);
        fprintf (pFile, "%.5lf\t", latitude);
        fprintf (pFile, "%.5lf\t", longitude);
        fprintf (pFile, "0\t " ); //zero for velocity
        fprintf (pFile, "0\t " ); //zero for azimuth
        fprintf (pFile, "\n");
        fclose (pFile);

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

    }
  }

  fprintf (stderr, "%s", KNRM);
}

void decode_cellocator(dsd_opts * opts, dsd_state * state, uint8_t * input, int len)
{
  //TODO: This
  UNUSED(opts);
  UNUSED(state);
  UNUSED(input);
  UNUSED(len);

  //UTF8 Text: MCGP (0x4D434750) followed by data values
  //(0417D1050000F45FD1DD00010000000097BDD56C81000009AAAABF12864C)
  utf8_to_text(state, 0, 4, input);

  fprintf (stderr, " Cellocator:");

  uint8_t type = input[4];
  if      (type == 1)  fprintf (stderr, " Platform Manifest Data;");
  else if (type == 2)  fprintf (stderr, " CAN Data;");
  else if (type == 3)  fprintf (stderr, " CAN Trigger Data;");
  else if (type == 4)  fprintf (stderr, " Time and Location Data;");
  else if (type == 5)  fprintf (stderr, " Accelerometer Data;");
  else if (type == 6)  fprintf (stderr, " PSP Alarm System Data;");
  else if (type == 7)  fprintf (stderr, " Usage Counter Data;");
  else if (type == 8)  fprintf (stderr, " Command Authentication Table Data;");
  else if (type == 9)  fprintf (stderr, " GSM Neighbor List Data;");
  else if (type == 10) fprintf (stderr, " Maintenance Server Platform Manifest Data;");
  else fprintf (stderr, " Unknown Data;");

  //Data afterwards appears to be an arbitrary len so has variable reporting data
  //will need to establish a len value for data and contents

}
void decode_ars(dsd_opts * opts, dsd_state * state, uint8_t * input, int len)
{
  //TODO: This
  UNUSED(opts);
  UNUSED(state);
  UNUSED(input);
  UNUSED(len);
}
