/*
 ============================================================================
 Name        : nxdn_element.c (formerly nxdn_lib)
 Author      :
 Version     : 1.0
 Date        : 2018 December 26
 Copyright   : No copyright
 Description : NXDN decoding source lib - modified from nxdn_lib
 Origin      : Originally found at - https://github.com/LouisErigHerve/dsd
 ============================================================================
 */

#include "dsd.h"

#define   NXDN_VCALL           0b000001  /* VCALL = VCALL_REQ = VCALL_RESP */
#define   NXDN_VCALL_IV        0b000011
#define   NXDN_DCALL_HDR       0b001001  /* Header Format - NXDN_DCALL_HDR = NXDN_DCALL_REQ = NXDN_DCALL_RESP */
#define   NXDN_DCALL_USR       0b001011  /* User Data Format */
#define   NXDN_DCALL_ACK       0b001100
#define   NXDN_REL_EX          0b000111
#define   NXDN_HEAD_DLY        0b001111
#define   NXDN_SDCALL_REQ_HDR  0b111000  /* Header Format */
#define   NXDN_SDCALL_REQ_USR  0b111001  /* User Data Format */
#define   NXDN_SDCALL_RESP     0b111011
#define   NXDN_SDCALL_IV       0b111010
#define   NXDN_STAT_INQ_REQ    0b110000
#define   NXDN_STAT_INQ_RESP   0b110001
#define   NXDN_STAT_REQ        0b110010
#define   NXDN_STAT_RESP       0b110011
#define   NXDN_REM_CON_REQ     0b110100
#define   NXDN_REM_CON_RESP    0b110101
#define   NXDN_REM_CON_E_REQ   0b110110
#define   NXDN_REM_CON_E_RESP  0b110111
//NXDN_VCALL_REQ       = 0b000001,  /* VCALL = VCALL_REQ = VCALL_RESP */
//NXDN_VCALL_RESP      = 0b000001,  /* VCALL = VCALL_REQ = VCALL_RESP */
#define   NXDN_VCALL_REC_REQ   = 0b000010  /* NXDN_VCALL_REC_REQ = NXDN_VCALL_REC_RESP */
//NXDN_VCALL_REC_RESP  = 0b000010,
#define   NXDN_VCALL_CONN_REQ  = 0b000011  /* NXDN_VCALL_CONN_REQ = NXDN_VCALL_CONN_RESP */
//NXDN_VCALL_CONN_RESP = 0b000011,  /* NXDN_VCALL_CONN_REQ = NXDN_VCALL_CONN_RESP */
#define   NXDN_VCALL_ASSGN_DUP = 0b000101
//NXDN_DCALL_REQ       = 0b001001,  /* NXDN_DCALL_HDR = NXDN_DCALL_REQ = NXDN_DCALL_RESP */
//NXDN_DCALL_RESP      = 0b001001,  /* NXDN_DCALL_HDR = NXDN_DCALL_REQ = NXDN_DCALL_RESP */
#define   NXDN_DCALL_REC_REQ   = 0b001010  /* NXDN_DCALL_REC_REQ = NXDN_DCALL_REC_RESP */
//NXDN_DCALL_REC_RESP  = 0b001010,  /* NXDN_DCALL_REC_REQ = NXDN_DCALL_REC_RESP */
#define   NXDN_DCALL_ASSGN     = 0b001110
#define   NXDN_DCALL_ASSGN_DUP = 0b001101
#define   NXDN_IDLE            0b010000
#define   NXDN_DISC_REQ        = 0b010001  /* NXDN_DISC_REQ = NXDN_DISC */
#define   NXDN_DISC            = 0b010001   /* NXDN_DISC_REQ = NXDN_DISC */

/*
 * @brief : This function decodes the full SACCH (when 4 voice frame parts
 *          have been successfully received)
 *
 * @param opts : Option structure parameters pointer
 *
 * @param state : State structure parameters pointer
 *
 * @return None
 *
 */
void NXDN_SACCH_Full_decode(dsd_opts * opts, dsd_state * state)
{
  uint8_t SACCH[72];
  uint32_t i;
  uint8_t CrcCorrect = 1;

  /* Consider all SACCH CRC parts as corrects */
  CrcCorrect = 1;

  /* Reconstitute the full 72 bits SACCH */
  for(i = 0; i < 4; i++)
  {
    memcpy(&SACCH[i * 18], state->nxdn_sacch_frame_segment[i], 18);

    /* Check CRC */ 
    if (state->nxdn_sacch_frame_segcrc[i] != 0) CrcCorrect = 0;
  }

  /* Decodes the element content */
  //run it under crccorrect, or under payload (if incorrect, hide bad data unless payload enabled)
  if (CrcCorrect == 1 || opts->payload == 1) NXDN_Elements_Content_decode(opts, state, CrcCorrect, SACCH);

} /* End NXDN_SACCH_Full_decode() */


void NXDN_Elements_Content_decode(dsd_opts * opts, dsd_state * state,
                                  uint8_t CrcCorrect, uint8_t * ElementsContent)
{
  uint32_t i;
  uint8_t MessageType;
  uint64_t CurrentIV = 0;
  unsigned long long int FullMessage = 0;
  /* Get the "Message Type" field */
  MessageType  = (ElementsContent[2] & 1) << 5;
  MessageType |= (ElementsContent[3] & 1) << 4;
  MessageType |= (ElementsContent[4] & 1) << 3;
  MessageType |= (ElementsContent[5] & 1) << 2;
  MessageType |= (ElementsContent[6] & 1) << 1;
  MessageType |= (ElementsContent[7] & 1) << 0;

  //only run message type if good CRC? 
  if (CrcCorrect == 1) nxdn_message_type (opts, state, MessageType);

  /* Save the "F1" and "F2" flags */
  state->NxdnElementsContent.F1 = ElementsContent[0];
  state->NxdnElementsContent.F2 = ElementsContent[1];

  /* Save the "Message Type" field */
  state->NxdnElementsContent.MessageType = MessageType;

  /* Decode the right "Message Type" */
  switch(MessageType)
  {
    //Alias 0x3F
    case 0x3F:
    {
      NXDN_decode_Alias(opts, state, ElementsContent);
    }
    break;

    //Idle
    case NXDN_IDLE:
    {
      //sprintf (state->nxdn_call_type, "NXDN IDLE ");
      break;
    }
    

    /* VCALL */
    case NXDN_VCALL:
    {

      /* Set the CRC state */
      state->NxdnElementsContent.VCallCrcIsGood = CrcCorrect;

      /* Decode the "VCALL" message */
      NXDN_decode_VCALL(opts, state, ElementsContent);

      /* Check the "Cipher Type" and the "Key ID" validity */
      if(CrcCorrect)
      {
        state->NxdnElementsContent.CipherParameterValidity = 1;
      }
      else state->NxdnElementsContent.CipherParameterValidity = 0;
      break;
    } /* End case NXDN_VCALL: */

    /* VCALL_IV */
    case NXDN_VCALL_IV:
    {

      /* Set the CRC state */
      state->NxdnElementsContent.VCallIvCrcIsGood = CrcCorrect;

      /* Decode the "VCALL_IV" message */
      NXDN_decode_VCALL_IV(opts, state, ElementsContent);

      if(CrcCorrect)
      {
        /* CRC is correct, copy the next theorical IV to use directly from the
         * received VCALL_IV */
        memcpy(state->NxdnElementsContent.NextIVComputed, state->NxdnElementsContent.IV, 8);
      }
      else
      {
        /* CRC is incorrect, compute the next IV to use */
        CurrentIV = 0;

        /* Convert the 8 bytes buffer into a 64 bits integer */
        for(i = 0; i < 8; i++)
        {
          CurrentIV |= state->NxdnElementsContent.NextIVComputed[i];
          CurrentIV = CurrentIV << 8;
        }

      }
      break;
    } /* End case NXDN_VCALL_IV: */

    /* Unknown Message Type */
    default:
    {
      //fprintf(stderr, "Unknown Message type ");
      break;
    }
  } /* End switch(MessageType) */

} /* End NXDN_Elements_Content_decode() */

void NXDN_decode_Alias(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  uint8_t Alias1 = 0x0; //value of an ascii 'NULL' character
  uint8_t Alias2 = 0x0;
  uint8_t Alias3 = 0x0;
  uint8_t Alias4 = 0x0;
  uint8_t blocknumber = 0; 
  uint8_t CrcCorrect = 1;
  state->NxdnElementsContent.VCallCrcIsGood = CrcCorrect;

  //FACCH Payload [3F][68][82][04][2 <- block number4] "[69][6F][6E][20]" <- 4 alias octets [00][7F][1C]
  blocknumber = (uint8_t)ConvertBitIntoBytes(&Message[32], 4) & 0x7; // & 0x7, might just be three bits, unsure
  Alias1 = (uint8_t)ConvertBitIntoBytes(&Message[40], 8);
  Alias2 = (uint8_t)ConvertBitIntoBytes(&Message[48], 8);
  Alias3 = (uint8_t)ConvertBitIntoBytes(&Message[56], 8);
  Alias4 = (uint8_t)ConvertBitIntoBytes(&Message[64], 8);
  //sanity check to prevent OOB array assignment
  if (blocknumber > 0 && blocknumber < 5)
  {
    //assign to index -1, since block number conveyed here is 1,2,3,4, and index values are 0,1,2,3
    //only assign if within valid range of ascii characters (not including diacritical extended alphabet)
    //else assign "null" ascii character

    //since we are zeroing out the blocks on tx_rel and other conditions, better to just set nothing to bad Alias bytes
    //tends to zero out otherwise already good blocks set in a previous repitition.
    if (Alias1 > 0x19 && Alias1 < 0x7F) sprintf (state->nxdn_alias_block_segment[blocknumber-1][0], "%c", Alias1);
    else ;// sprintf (state->nxdn_alias_block_segment[blocknumber-1][0], "%c", 0);

    if (Alias2 > 0x19 && Alias2 < 0x7F) sprintf (state->nxdn_alias_block_segment[blocknumber-1][1], "%c", Alias2);
    else ; //sprintf (state->nxdn_alias_block_segment[blocknumber-1][1], "%c", 0);

    if (Alias3 > 0x19 && Alias3 < 0x7F) sprintf (state->nxdn_alias_block_segment[blocknumber-1][2], "%c", Alias3);
    else ; //sprintf (state->nxdn_alias_block_segment[blocknumber-1][2], "%c", 0);

    if (Alias4 > 0x19 && Alias4 < 0x7F) sprintf (state->nxdn_alias_block_segment[blocknumber-1][3], "%c", Alias4);
    else ; //sprintf (state->nxdn_alias_block_segment[blocknumber-1][3], "%c", 0);
  }

  if (CrcCorrect) //was by blocknumber being 4, but wasn't as frequent as I would like in the console
  { 
    fprintf (stderr, " "); //probably just want a space instead
    for (int i = 0; i < 4; i++)
    {
      for (int j = 0; j < 4; j++)
      {
        fprintf (stderr, "%s", state->nxdn_alias_block_segment[i][j]); 
      }
    }
    fprintf (stderr, " ");
  }
  else fprintf (stderr, " CRC ERR "); //redundant print? or okay?
}

void NXDN_decode_cch_info(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  uint8_t location_id = 0;
  uint8_t channel_ifno = 0;
}

/*
 * @brief : This function decodes the VCALL message
 *
 * @param opts : Option structure parameters pointer
 *
 * @param state : State structure parameters pointer
 *
 * @param Message : A 64 bit buffer containing the VCALL message to decode
 *
 * @return None
 *
 */
void NXDN_decode_VCALL(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  //uint32_t i;
  uint8_t  CCOption = 0;
  uint8_t  CallType = 0;
  uint8_t  VoiceCallOption = 0;
  uint16_t SourceUnitID = 0;
  uint16_t DestinationID = 0;
  uint8_t  CipherType = 0;
  uint8_t  KeyID = 0;
  uint8_t  DuplexMode[32] = {0};
  uint8_t  TransmissionMode[32] = {0};
  unsigned long long int FullMessage = 0;

  /* Message[0..7] contains :
   * - The "F1" and "F2" flags
   * - The "Message Type" (already decoded before calling this function)
   *
   * So no need to decode it a second time
   */

  /* Decode "CC Option" */
  CCOption = (uint8_t)ConvertBitIntoBytes(&Message[8], 8);
  state->NxdnElementsContent.CCOption = CCOption;

  /* Decode "Call Type" */
  CallType = (uint8_t)ConvertBitIntoBytes(&Message[16], 3);
  state->NxdnElementsContent.CallType = CallType;

  /* Decode "Voice Call Option" */
  VoiceCallOption = (uint8_t)ConvertBitIntoBytes(&Message[19], 5);
  state->NxdnElementsContent.VoiceCallOption = VoiceCallOption;

  /* Decode "Source Unit ID" */
  SourceUnitID = (uint16_t)ConvertBitIntoBytes(&Message[24], 16);
  state->NxdnElementsContent.SourceUnitID = SourceUnitID;

  /* Decode "Destination ID" */
  DestinationID = (uint16_t)ConvertBitIntoBytes(&Message[40], 16);
  state->NxdnElementsContent.DestinationID = DestinationID;

  /* Decode the "Cipher Type" */
  CipherType = (uint8_t)ConvertBitIntoBytes(&Message[56], 2);
  state->NxdnElementsContent.CipherType = CipherType;

  /* Decode the "Key ID" */
  KeyID = (uint8_t)ConvertBitIntoBytes(&Message[58], 6);
  state->NxdnElementsContent.KeyID = KeyID;

  /* Print the "CC Option" */
  if(CCOption & 0x80) fprintf(stderr, "Emergency ");
  if(CCOption & 0x40) fprintf(stderr, "Visitor ");
  if(CCOption & 0x20) fprintf(stderr, "Priority Paging ");

  if((CipherType == 2) || (CipherType == 3))
  {
    state->NxdnElementsContent.PartOfCurrentEncryptedFrame = 1;
    state->NxdnElementsContent.PartOfNextEncryptedFrame    = 2;
  }
  else
  {
    state->NxdnElementsContent.PartOfCurrentEncryptedFrame = 1;
    state->NxdnElementsContent.PartOfNextEncryptedFrame    = 1;
  }

  /* Print the "Call Type" */
  fprintf (stderr, "%s", KGRN);
  fprintf(stderr, "\n %s - ", NXDN_Call_Type_To_Str(CallType)); 
  sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType)); 

  /* Print the "Voice Call Option" */
  NXDN_Voice_Call_Option_To_Str(VoiceCallOption, DuplexMode, TransmissionMode);
  fprintf(stderr, "%s %s - ", DuplexMode, TransmissionMode);

  /* Print Source ID and Destination ID (Talk Group or Unit ID) */
  //fprintf (stderr, "%s", KGRN);
  fprintf(stderr, "Src=%u - Dst/TG=%u ", SourceUnitID & 0xFFFF, DestinationID & 0xFFFF);
  fprintf (stderr, "%s", KNRM);

  /* Print the "Cipher Type" */
  if(CipherType != 0)
  {
    fprintf (stderr, "\n  %s", KYEL);
    fprintf(stderr, "%s - ", NXDN_Cipher_Type_To_Str(CipherType));
    //state->nxdn_cipher_type = CipherType;
  }

  /* Print the Key ID */
  if(CipherType != 0)
  {

    fprintf(stderr, "Key ID %u - ", KeyID & 0xFF);
    fprintf (stderr, "%s", KNRM);
    //state->nxdn_key = (KeyID & 0xFF);
  }

  if (state->nxdn_cipher_type == 0x01 && state->R > 0) //scrambler key value
  {
    fprintf (stderr, "%s", KYEL);
    fprintf(stderr, "Value: %05lld", state->R);
    fprintf (stderr, "%s", KNRM);
  }

  if(state->NxdnElementsContent.VCallCrcIsGood)
  {
    if ( (SourceUnitID & 0xFFFF) > 0 )
    {
      state->nxdn_last_rid = SourceUnitID & 0xFFFF;   //only grab if CRC is okay
      state->nxdn_last_tg = (DestinationID & 0xFFFF);
      state->nxdn_key = (KeyID & 0xFF);
      state->nxdn_cipher_type = CipherType; //will this set to zero if no enc?
    }
  }
  else
  {
    fprintf (stderr, "%s", KRED);
    fprintf(stderr, "(CRC ERR) ");
    fprintf (stderr, "%s", KNRM);
  }

  //set enc bit here so we can tell playSynthesizedVoice whether or not to play enc traffic
  if (state->nxdn_cipher_type != 0)
  {
    state->dmr_encL = 1;
  }
  if (state->nxdn_cipher_type == 0 || state->R != 0)
  {
    state->dmr_encL = 0;
  }
} /* End NXDN_decode_VCALL() */


/*
 * @brief : This function decodes the VCALL_IV message
 *
 * @param opts : Option structure parameters pointer
 *
 * @param state : State structure parameters pointer
 *
 * @param Message : A 72 bit buffer containing the VCALL_IV message to decode
 *
 * @return None
 *
 */
void NXDN_decode_VCALL_IV(dsd_opts * opts, dsd_state * state, uint8_t * Message)
{
  uint32_t i;

  /* Message[0..7] contains :
   * - The "F1" and "F2" flags
   * - The "Message Type" (already decoded before calling this function)
   *
   * So no need to decode it a second time
   *
   * Message[8..71] contains : The 64 bits IV
   */
   state->payload_miN = 0; //zero out
  /* Extract the IV from the VCALL_IV message */
  for(i = 0; i < 8; i++)
  {
    state->NxdnElementsContent.IV[i] = (uint8_t)ConvertBitIntoBytes(&Message[(i + 1) * 8], 8);

    state->payload_miN = state->payload_miN << 8 | state->NxdnElementsContent.IV[i];
  }

  state->NxdnElementsContent.PartOfCurrentEncryptedFrame = 2;
  state->NxdnElementsContent.PartOfNextEncryptedFrame    = 1;

} /* End NXDN_decode_VCALL_IV() */


/*
 * @brief : This function decodes the "Call Type" and return the
 *          ASCII string corresponding to it.
 *
 * @param CallType : The call type parameter to decode
 *
 * @return An ASCII string of the "Call Type"
 *
 */
char * NXDN_Call_Type_To_Str(uint8_t CallType)
{
  char * Ptr = NULL;

  switch(CallType)
  {
    case 0:  Ptr = "Broadcast Call";    break;
    case 1:  Ptr = "Group Call";        break;
    case 2:  Ptr = "Unspecified Call";  break;
    case 3:  Ptr = "Reserved";          break;
    case 4:  Ptr = "Individual Call";   break;
    case 5:  Ptr = "Reserved";          break;
    case 6:  Ptr = "Interconnect Call"; break;
    case 7:  Ptr = "Speed Dial Call";   break;
    default: Ptr = "Unknown Call Type"; break;
  }

  return Ptr;
} /* End NXDN_Call_Type_To_Str() */


/*
 * @brief : This function decodes the "Voice Call Option" and return the
 *          ASCII string corresponding to it.
 *
 * @param VoiceCallOption : The call type parameter to decode
 *
 * @param Duplex : A 32 bytes ASCII buffer pointer where store
 *                 the Duplex/Half duplex mode
 *
 * @param TransmissionMode : A 32 bytes ASCII buffer pointer where store
 *                           the transmission mode (bit rate)
 *
 * @return An ASCII string of the "Voice Call Option"
 *
 */
void NXDN_Voice_Call_Option_To_Str(uint8_t VoiceCallOption, uint8_t * Duplex, uint8_t * TransmissionMode)
{
  char * Ptr = NULL;

  Duplex[0] = 0;
  TransmissionMode[0] = 0;

  if(VoiceCallOption & 0x10) strcpy((char *)Duplex, "Duplex");
  else strcpy((char *)Duplex, "Half Duplex");

  switch(VoiceCallOption & 0x17)
  {
    case 0: Ptr = "4800bps/EHR"; break;
    case 2: Ptr = "9600bps/EHR"; break;
    case 3: Ptr = "9600bps/EFR"; break;
    default: Ptr = "Reserved Voice Call Option";  break;
  }

  strcpy((char *)TransmissionMode, Ptr);
} /* End NXDN_Voice_Call_Option_To_Str() */

/*
 * @brief : This function decodes the "Cipher Type" and return the
 *          ASCII string corresponding to it.
 *
 * @param CipherType : The cipher type parameter to decode
 *
 * @return An ASCII string of the "Cipher Type"
 *
 */
char * NXDN_Cipher_Type_To_Str(uint8_t CipherType)
{
  char * Ptr = NULL;

  switch(CipherType)
  {
    case 0:  Ptr = "";          break;  /* Non-ciphered mode / clear call */
    case 1:  Ptr = "Scrambler"; break;
    case 2:  Ptr = "DES";       break;
    case 3:  Ptr = "AES";       break;
    default: Ptr = "Unknown Cipher Type"; break;
  }

  return Ptr;
} /* End NXDN_Cipher_Type_To_Str() */