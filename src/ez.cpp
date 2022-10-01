/*-------------------------------------------------------------------------------
 * ez.cpp
 * EZPWD R-S bridge and ISCH map lookup
 *
 * original copyrights for portions used below (OP25, EZPWD)
 *
 * LWVMOBILE
 * 2022-09 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include "ezpwd/rs"
#include <map>

std::vector<uint8_t> ESS_A(28,0); // ESS_A and ESS_B are hexbits vectors
std::vector<uint8_t> ESS_B(16,0);
ezpwd::RS<63,35> rs28;
std::vector<uint8_t> HB(63,0);
std::vector<uint8_t> HBS(63,0);
std::vector<uint8_t> HBL(63,0);
std::vector<int> Erasures;

//Reed-Solomon Correction of ESS section
int ez_rs28_ess (int payload[96], int parity[168])
{
  //do something

  uint8_t a, b, i, j, k;
  k = 0;
  for (i = 0; i < 16; i++)
  {
    b = 0;
    for (j = 0; j < 6; j++)
    {
      b = b << 1;
      b = b + payload[k]; //convert bits to hexbits.
      k++;
    }
    ESS_B[i] = b;
    // fprintf (stderr, " %X", ESS_B[i]);
  }

  k = 0;
  for (i = 0; i < 28; i++)
  {
    a = 0;
    for (j = 0; j < 6; j++)
    {
      a = a << 1;
      a = a + parity[k]; //convert bits to hexbits.
      k++;
    }
    ESS_A[i] = a;
    // fprintf (stderr, " %X ", ESS_A[i]);
  }

  int ec;

  ec = rs28.decode (ESS_B, ESS_A);
  // fprintf (stderr, "\n EC = %d \n", ec);

  //convert ESS_B back to bits
  k = 0;
  for (i = 0; i < 16; i++)
  {
    b = 0;
    for (j = 0; j < 6; j++)
    {
      b = (ESS_B[i] >> (5-j) & 0x1);
      payload[k] = b;
      k++;
    }
    // fprintf (stderr, " %X", ESS_B[i]);
  }

  return(ec);

}

//Reed-Solomon Correction of FACCH section
int ez_rs28_facch (int payload[156], int parity[114])
{
  //do something!
  int ec = -2;
  int i, j, k, b;

  //init HB 
  for (i = 0; i < 64; i++)
  {
	HB[i] = 0;
  }

  //Erasures for FACCH
  Erasures = {0,1,2,3,4,5,6,7,8,54,55,56,57,58,59,60,61,62};

  //convert bits to hexbits, 156 for payload, 114 parity
  j = 9; //starting position according to OP25
  for (i = 0; i < 156; i += 6)
  {
		HB[j] = (payload[i] << 5) + (payload[i+1] << 4) + (payload[i+2] << 3) + (payload[i+3] << 2) + (payload[i+4] << 1) + payload[i+5];
		j++;
	}
  //j should continue from its last increment
  for (i = 0; i < 114; i += 6)
  {
		HB[j] = (parity[i] << 5) + (parity[i+1] << 4) + (parity[i+2] << 3) + (parity[i+3] << 2) + (parity[i+4] << 1) + parity[i+5];
		j++;
	}

  ec = rs28.decode(HB, Erasures);

  //convert HB back to bits
  //fprintf (stderr, "\n");
  k = 0;
  for (i = 0; i < 26; i++) //26*6=156 bits
  {
    b = 0;
    for (j = 0; j < 6; j++)
    {
      b = (HB[i+9] >> (5-j) & 0x1); //+9 to mach our starting position
      payload[k] = b;
      //fprintf (stderr, "%d", payload[k]);
      k++;
    }

  }

  return (ec);
}

//Reed-Solomon Correction of SACCH section
int ez_rs28_sacch (int payload[180], int parity[132])
{
  //do something!
  int ec = -2;
  int i, j, k, b;

  //init HBS
  for (i = 0; i < 64; i++)
  {
	HBS[i] = 0;
  }

  //Erasures for SACCH
  Erasures = {0,1,2,3,4,57,58,59,60,61,62};

  //convert bits to hexbits, 156 for payload, 114 parity
  j = 5; //starting position according to OP25
  for (i = 0; i < 180; i += 6)
  {
		HBS[j] = (payload[i] << 5) + (payload[i+1] << 4) + (payload[i+2] << 3) + (payload[i+3] << 2) + (payload[i+4] << 1) + payload[i+5];
		j++;
	}
  //j should continue from its last increment
  for (i = 0; i < 132; i += 6)
  {
		HBS[j] = (parity[i] << 5) + (parity[i+1] << 4) + (parity[i+2] << 3) + (parity[i+3] << 2) + (parity[i+4] << 1) + parity[i+5];
		j++;
	}

  ec = rs28.decode(HBS, Erasures);

  //convert HBS back to bits
  // fprintf (stderr, "\n");
  k = 0;
  for (i = 0; i < 30; i++) //30*6=180 bits
  {
    b = 0;
    for (j = 0; j < 6; j++)
    {
      b = (HBS[i+5] >> (5-j) & 0x1); //+5 to mach our starting position
      payload[k] = b;
      // fprintf (stderr, "%d", payload[k]);
      k++;
    }

  }
  return (ec);
}

//I-ISCH Lookup section, borrowed from OP25
std::map <std::string, int> isch_map;
void map_isch()
{
	isch_map["184229d461"] = 0;
	isch_map["18761451f6"] = 1;
	isch_map["181ae27e2f"] = 2;
	isch_map["182edffbb8"] = 3;
	isch_map["18df8a7510"] = 4;
	isch_map["18ebb7f087"] = 5;
	isch_map["188741df5e"] = 6;
	isch_map["18b37c5ac9"] = 7;
	isch_map["1146a44f13"] = 8;
	isch_map["117299ca84"] = 9;
	isch_map["111e6fe55d"] = 10;
	isch_map["112a5260ca"] = 11;
	isch_map["11db07ee62"] = 12;
	isch_map["11ef3a6bf5"] = 13;
	isch_map["1183cc442c"] = 14;
	isch_map["11b7f1c1bb"] = 15;
	isch_map["1a4a2e239e"] = 16;
	isch_map["1a7e13a609"] = 17;
	isch_map["1a12e589d0"] = 18;
	isch_map["1a26d80c47"] = 19;
	isch_map["1ad78d82ef"] = 20;
	isch_map["1ae3b00778"] = 21;
	isch_map["1a8f4628a1"] = 22;
	isch_map["1abb7bad36"] = 23;
	isch_map["134ea3b8ec"] = 24;
	isch_map["137a9e3d7b"] = 25;
	isch_map["13166812a2"] = 26;
	isch_map["1322559735"] = 27;
	isch_map["13d300199d"] = 28;
	isch_map["13e73d9c0a"] = 29;
	isch_map["138bcbb3d3"] = 30;
	isch_map["13bff63644"] = 31;
	isch_map["1442f705ef"] = 32;
	isch_map["1476ca8078"] = 33;
	isch_map["141a3cafa1"] = 34;
	isch_map["142e012a36"] = 35;
	isch_map["14df54a49e"] = 36;
	isch_map["14eb692109"] = 37;
	isch_map["14879f0ed0"] = 38;
	isch_map["14b3a28b47"] = 39;
	isch_map["1d467a9e9d"] = 40;
	isch_map["1d72471b0a"] = 41;
	isch_map["1d1eb134d3"] = 42;
	isch_map["1d2a8cb144"] = 43;
	isch_map["1ddbd93fec"] = 44;
	isch_map["1defe4ba7b"] = 45;
	isch_map["1d831295a2"] = 46;
	isch_map["1db72f1035"] = 47;
	isch_map["164af0f210"] = 48;
	isch_map["167ecd7787"] = 49;
	isch_map["16123b585e"] = 50;
	isch_map["162606ddc9"] = 51;
	isch_map["16d7535361"] = 52;
	isch_map["16e36ed6f6"] = 53;
	isch_map["168f98f92f"] = 54;
	isch_map["16bba57cb8"] = 55;
	isch_map["1f4e7d6962"] = 56;
	isch_map["1f7a40ecf5"] = 57;
	isch_map["1f16b6c32c"] = 58;
	isch_map["1f228b46bb"] = 59;
	isch_map["1fd3dec813"] = 60;
	isch_map["1fe7e34d84"] = 61;
	isch_map["1f8b15625d"] = 62;
	isch_map["1fbf28e7ca"] = 63;
	isch_map["84d62c339"] = 64;
	isch_map["8795f46ae"] = 65;
	isch_map["815a96977"] = 66;
	isch_map["82194ece0"] = 67;
	isch_map["8d0c16248"] = 68;
	isch_map["8e4fce7df"] = 69;
	isch_map["8880ac806"] = 70;
	isch_map["8bc374d91"] = 71;
	isch_map["149ef584b"] = 72;
	isch_map["17dd2dddc"] = 73;
	isch_map["11124f205"] = 74;
	isch_map["125197792"] = 75;
	isch_map["1d44cf93a"] = 76;
	isch_map["1e0717cad"] = 77;
	isch_map["18c875374"] = 78;
	isch_map["1b8bad6e3"] = 79;
	isch_map["a456534c6"] = 80;
	isch_map["a7158b151"] = 81;
	isch_map["a1dae9e88"] = 82;
	isch_map["a29931b1f"] = 83;
	isch_map["ad8c695b7"] = 84;
	isch_map["aecfb1020"] = 85;
	isch_map["a800d3ff9"] = 86;
	isch_map["ab430ba6e"] = 87;
	isch_map["341e8afb4"] = 88;
	isch_map["375d52a23"] = 89;
	isch_map["3192305fa"] = 90;
	isch_map["32d1e806d"] = 91;
	isch_map["3dc4b0ec5"] = 92;
	isch_map["3e8768b52"] = 93;
	isch_map["38480a48b"] = 94;
	isch_map["3b0bd211c"] = 95;
	isch_map["44dbc12b7"] = 96;
	isch_map["479819720"] = 97;
	isch_map["41577b8f9"] = 98;
	isch_map["4214a3d6e"] = 99;
	isch_map["4d01fb3c6"] = 100;
	isch_map["4e4223651"] = 101;
	isch_map["488d41988"] = 102;
	isch_map["4bce99c1f"] = 103;
	isch_map["d493189c5"] = 104;
	isch_map["d7d0c0c52"] = 105;
	isch_map["d11fa238b"] = 106;
	isch_map["d25c7a61c"] = 107;
	isch_map["dd49228b4"] = 108;
	isch_map["de0afad23"] = 109;
	isch_map["d8c5982fa"] = 110;
	isch_map["db864076d"] = 111;
	isch_map["645bbe548"] = 112;
	isch_map["6718660df"] = 113;
	isch_map["61d704f06"] = 114;
	isch_map["6294dca91"] = 115;
	isch_map["6d8184439"] = 116;
	isch_map["6ec25c1ae"] = 117;
	isch_map["680d3ee77"] = 118;
	isch_map["6b4ee6be0"] = 119;
	isch_map["f41367e3a"] = 120;
	isch_map["f750bfbad"] = 121;
	isch_map["f19fdd474"] = 122;
	isch_map["f2dc051e3"] = 123;
	isch_map["fdc95df4b"] = 124;
	isch_map["fe8a85adc"] = 125;
	isch_map["f845e7505"] = 126;
	isch_map["fb063f092"] = 127;
}

unsigned long long int isch_lookup (unsigned long long int isch)
{
  map_isch(); //initialize the lookup map
  char s[64];
  unsigned long long int decoded = -2; //initialize lookup to an invalid number
  sprintf(s, "%llx", isch);
  decoded = isch_map[s];
  return(decoded);
}
