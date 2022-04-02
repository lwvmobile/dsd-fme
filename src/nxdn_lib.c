/*
 ============================================================================
 Name        : nxdn_lib
 Author      :
 Version     : 1.0
 Date        : 2018 December 26
 Copyright   : No copyright
 Description : NXDN decoding source lib
 ============================================================================
 */


/* Include ------------------------------------------------------------------*/
#include "dsd.h"
#include "nxdn_const.h"


/* Global variables ---------------------------------------------------------*/


/* Functions ----------------------------------------------------------------*/


/*
 * @brief : This function decodes the LICH
 *
 * @param InputLich : A 8 ASCII bytes buffer
 *
 * @param RFChannelType : Output of RF Channel Type
 *        parameter
 *
 * @param FunctionnalChannelType : Output of Functional
 *        Channel Type parameter
 *
 * @param Option : Output of Option parameter
 *
 * @param : CompleteLichBinaryFormat : The constitued LICH (7 bits)
 *
 * @param Direction : Output of Direction parameter
 *
 * @param Inverted : 1 = Inverted frame ; 0 = Normal frame
 *
 * @return 1 when LICH parity is good
 *         0 when LICH parity is bad
 *
 */
 //borrowed from LEH my_lib.c file
 uint32_t ConvertBitIntoBytes(uint8_t * BufferIn, uint32_t BitLength)
 {
   uint32_t Output = 0;
   uint32_t i;

   for(i = 0; i < BitLength; i++)
   {
     Output <<= 1;
     Output |= (uint32_t)(BufferIn[i] & 1);
   }

   return Output;
 } /* End ConvertBitIntoBytes() */


uint8_t NXDN_decode_LICH(uint8_t   InputLich[8],
                         uint8_t * RFChannelType,
                         uint8_t * FunctionnalChannelType,
                         uint8_t * Option,
                         uint8_t * Direction,
                         uint8_t * CompleteLichBinaryFormat,
                         uint8_t   Inverted)
{
  uint8_t  GoodParity = 0;
  uint32_t i;
  uint8_t  LichDibits[8] = {0};
  uint8_t  LichBits[8] = {0};
  uint8_t  EvenParityComputed = 0;
  uint8_t *pr;
  uint8_t  dibit;
  uint8_t  InvertionDibitValue;
  uint8_t  CompleteLich = 0;

  if(Inverted) InvertionDibitValue = 0x02;
  else InvertionDibitValue = 0x00;

  pr = (uint8_t *)(&nxdnpr[0]);

  /* Recreate a 8 bits field
   * 7 bits of LICH + 1 parity bit */
  for(i = 0; i < 8; i++)
  {
    /* Get the dibit value (convert ASCII to bin value) */
    dibit = ((InputLich[i] - '0') & 0x03) ^ InvertionDibitValue;

    /* Descramble the dibit */
    LichDibits[i] = dibit ^ ((*pr & 0x01) << 1);

    /* Increment pseudo-random pointer */
    pr++;

    /* Decode LICH dibit */
    switch(LichDibits[i])
    {
      case 0:
      {
        LichBits[i] = 0;
        break;
      }
      case 1:
      {
        LichBits[i] = 0;
        break;
      }
      case 2:
      {
        LichBits[i] = 1;
        break;
      }
      default:
      {
        LichBits[i] = 1;
        break;
      }
    } /* End switch(LichDibits[i]) */

    /* Parity should be even */
    EvenParityComputed += LichBits[i];

  } /* End for(i = 0; i < 8; i++) */

  /* Even parity = Good */
  if((EvenParityComputed % 2) == 0)
  {
    GoodParity = 1;
  }
  else
  {
    /* Odd parity = Wrong */
  }

  *RFChannelType = ((LichBits[0] & 0x01) << 1) | (LichBits[1] & 0x01);
  *FunctionnalChannelType = ((LichBits[2] & 0x01) << 1) | (LichBits[3] & 0x01);
  *Option        = ((LichBits[4] & 0x01) << 1) | (LichBits[5] & 0x01);
  *Direction     = LichBits[6] & 0x01;

  CompleteLich = 0;
  for(i = 0; i < 7; i++)
  {
    CompleteLich = ((CompleteLich << 1) | LichBits[i]) & 0x7F;
  }
  *CompleteLichBinaryFormat = CompleteLich;

  return GoodParity;
} /* End NXDN_decode_LICH() */


/*
 * @brief : This function decodes the RAW part of SACCH received when
 *          each voice frame starts.
 *          This is NOT the full SACCH but only 18 bits on 72.
 *
 * @param Input : A 60 bit (60 bytes) buffer pointer of input data
 *
 * @param Output : A 32 bit (26 bytes data + 6 bytes CRC) buffer where write output data
 *
 * @return 1 when CRC is good
 *         0 when CRC is bad
 *
 */
uint8_t NXDN_SACCH_raw_part_decode(uint8_t * Input, uint8_t * Output)
{
  uint32_t i;
  uint8_t sacchRaw[60]; /* SACCH bits retrieved from RF channel */
  uint8_t sacch[72] = {0};
  uint8_t s0;
  uint8_t s1;
  uint8_t m_data[5] = {0}; /* SACCH bytes after de-convolution (36 bits) */
  uint8_t CRCComputed = 0;
  uint8_t CRCExtracted = 0;
  uint32_t index = 0;
  uint32_t punctureIndex = 0;
  uint8_t  temp[90] = {0}; /* SACCH working area */
  //uint8_t  RAN = 0;
  //uint8_t  StructureField = 0;
  uint8_t  GoodCrc = 0;

  //UNUSED_VARIABLE(sacch[0]);

  /* De-interleave */
  for(i = 0; i < 60; i++)
  {
    sacchRaw[DSDNXDN_SACCH_m_Interleave[i]] = Input[i];
  }

  /* Un-Punture */
  for (index = 0, punctureIndex = 0, i = 0; i < 60; i++)
  {
    if (index == (uint32_t)DSDNXDN_SACCH_m_PunctureList[punctureIndex])
    {
      temp[index++] = 1;
      punctureIndex++;
    }

    temp[index++] = sacchRaw[i]<<1; // 0->0, 1->2
  }

  for (i = 0; i < 8; i++)
  {
    temp[index++] = 0;
  }

  /* Convolutional decoding */
  CNXDNConvolution_start();
  for (i = 0U; i < 40U; i++)
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 36U);


  for(i = 0; i < 4; i++)
  {
    Output[(i*8)+0] = (m_data[i] >> 7) & 1;
    Output[(i*8)+1] = (m_data[i] >> 6) & 1;
    Output[(i*8)+2] = (m_data[i] >> 5) & 1;
    Output[(i*8)+3] = (m_data[i] >> 4) & 1;
    Output[(i*8)+4] = (m_data[i] >> 3) & 1;
    Output[(i*8)+5] = (m_data[i] >> 2) & 1;
    Output[(i*8)+6] = (m_data[i] >> 1) & 1;
    Output[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

  /* Extract the 6 bit CRC */
  CRCExtracted = m_data[3] & 0x3F;

  /* Compute the 6 bit CRC */
  CRCComputed = CRC6BitNXDN(Output, 26);

  /* Compute the RAN (6 last bits of the SR Information in the SACCH) */
  //RAN = m_data[0] & 0x3F;

  /* Compute the Structure Field (remove 2 first bits of the SR Information in the SACCH) */
  //StructureField = (m_data[0] >> 6) & 0x03;

  //fprintf(stderr, "RAN=%u ; StructureField=%u ; ", RAN, StructureField);

  //fprintf(stderr, "SACCH=0x%02X-0x%02X-0x%02X-0x%02X-0x%02X ; ",
  //       m_data[0], m_data[1], m_data[2], m_data[3], m_data[4]);

  //fprintf(stderr, "CRC Extracted=0x%02X ; CRC computed=0x%02X ; ", CRCExtracted, CRCComputed);

  /* Check CRCs */
  if(CRCExtracted == CRCComputed)
  {
    //fprintf(stderr, "OK !\n");
    GoodCrc = 1;
  }
  else
  {
    //fprintf(stderr, "ERROR !\n");
    GoodCrc = 0;
  }

  return GoodCrc;
} /* NXDN_SACCH_raw_part_decode() */


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
    memcpy(&SACCH[i * 18], state->NxdnSacchRawPart[i].Data, 18);

    /* Check CRC */
    if(state->NxdnSacchRawPart[i].CrcIsGood == 0) CrcCorrect = 0;
  }

  /* Decodes the element content */
  NXDN_Elements_Content_decode(opts, state, CrcCorrect, SACCH);

} /* End NXDN_SACCH_Full_decode() */


/*
 * @brief : This function decodes the RAW part of FACCH1 received when
 *          each voice frame starts.
 *
 * @param Input : A 144 bits (144 bytes) buffer pointer of input data
 *
 * @param Output : A 96 bits (80 bytes data + 12 bytes CRC + 4 bytes "Tail" all "0") buffer where write output data
 *
 * @return 1 when CRC is good
 *         0 when CRC is bad
 *
 */
uint8_t NXDN_FACCH1_decode(uint8_t * Input, uint8_t * Output)
{
  uint32_t i;
  uint8_t  GoodCrc = 0;
  uint16_t CRCComputed = 0;
  uint16_t CRCExtracted = 0;
  uint8_t  facch1Raw[144]; /* FACCH1 bits retrieved from RF channel */
  uint8_t  s0;
  uint8_t  s1;
  uint8_t  m_data[12] = {0}; /* FACCH1 bytes after de-convolution (96 bits) */
  uint32_t index = 0;
  uint32_t punctureIndex = 0;
  uint8_t  temp[210] = {0}; /* FACCH1 working area */

  /* De-interleave */
  for(i = 0; i < 144; i++)
  {
    facch1Raw[DSDNXDN_FACCH1_m_Interleave[i]] = Input[i];
  }

  /* Un-Punture */
  for (index = 0, punctureIndex = 0, i = 0; i < 144; i++)
  {
    if (index == (uint32_t)DSDNXDN_FACCH1_m_PunctureList[punctureIndex])
    {
      temp[index++] = 1;
      punctureIndex++;
    }

    temp[index++] = facch1Raw[i]<<1; // 0->0, 1->2
  }

  for (i = 0; i < 8; i++)
  {
    temp[index++] = 0;
  }

  /* Convolutional decoding */
  CNXDNConvolution_start();
  for (i = 0U; i < 100U; i++)
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }
  /*
  fprintf (stderr, "\n");
  for (i = 0U; i < 100U; i++)
  {
    fprintf (stderr, "temp=%X", temp[i]);
  }
  fprintf (stderr, "\n");
  */
  CNXDNConvolution_chainback(m_data, 96U);

  /* Reconstitute L3 Data (Data 80 bits + CRC 12 bits + 4 tail bits = Total 96 bits = 12 bytes) */
  for(i = 0; i < 12; i++)
  {
    Output[(i*8)+0] = (m_data[i] >> 7) & 1;
    Output[(i*8)+1] = (m_data[i] >> 6) & 1;
    Output[(i*8)+2] = (m_data[i] >> 5) & 1;
    Output[(i*8)+3] = (m_data[i] >> 4) & 1;
    Output[(i*8)+4] = (m_data[i] >> 3) & 1;
    Output[(i*8)+5] = (m_data[i] >> 2) & 1;
    Output[(i*8)+6] = (m_data[i] >> 1) & 1;
    Output[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

  /* Extract the 12 bits CRC */
  CRCExtracted = ((uint16_t)(m_data[10] << 4) | (uint16_t)(m_data[11] >> 4)) & 0x0FFF;

  /* Compute the 12 bits CRC */
  CRCComputed = CRC12BitNXDN(Output, 80);

  //fprintf(stderr, "\n");

  //fprintf(stderr, "SACCH1 Data = ");
  //for(i = 0; i < 12; i++)
  //{
  //  fprintf(stderr, "0x%02X - ", m_data[i]);
  //}
  //fprintf(stderr, "\n");

  //fprintf(stderr, "CRC computed = 0x%03X - CRC extracted = 0x%03X\n", CRCComputed, CRCExtracted);

  /* Check CRCs */
  if(CRCExtracted == CRCComputed)
  {
    //fprintf(stderr, "OK !\n");
    GoodCrc = 1;
  }
  else
  {
    //fprintf(stderr, "ERROR !\n");
    GoodCrc = 0;
  }

  return GoodCrc;
} /* End NXDN_FACCH1_decode() */



/*
 * @brief : This function decodes the UDCH/FACCH2 received.
 *
 * @param Input : A 348 bits (348 bytes) buffer pointer of input data
 *
 * @param Output : A 203 bits (184 bytes data + 15 bytes CRC + 4 bytes "Tail" all "0") buffer where write output data
 *
 * @return 1 when CRC is good
 *         0 when CRC is bad
 *
 */
uint8_t NXDN_UDCH_decode(uint8_t * Input, uint8_t * Output)
{
  uint32_t i;
  uint8_t  GoodCrc = 0;
  uint16_t CRCComputed = 0;
  uint16_t CRCExtracted = 0;
  uint8_t  UdchRaw[348]; /* UDCH bits retrieved from RF channel */
  uint8_t  s0;
  uint8_t  s1;
  uint8_t  m_data[26] = {0}; /* UDCH bytes after de-convolution (203 bits) */
  uint32_t index = 0;
  uint32_t punctureIndex = 0;
  uint8_t  temp[420] = {0}; /* UDCH working area */

  /* De-interleave */
  for(i = 0; i < 348; i++)
  {
    UdchRaw[DSDNXDN_UDCH_m_Interleave[i]] = Input[i];
  }

  /* Un-Punture */
  for (index = 0, punctureIndex = 0, i = 0; i < 348; i++)
  {
    if (index == (uint32_t)DSDNXDN_UDCH_m_PunctureList[punctureIndex])
    {
      temp[index++] = 1;
      punctureIndex++;
    }

    temp[index++] = UdchRaw[i]<<1; // 0->0, 1->2
  }

  for (i = 0; i < 8; i++)
  {
    temp[index++] = 0;
  }

  /* Convolutional decoding */
  CNXDNConvolution_start();
  for (i = 0U; i < 207U; i++)
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 203U);

  /* Reconstitute L3 Data (Data 184 bits + CRC 15 bits + 4 tail bits = Total 203 bits = 26 bytes) */
  for(i = 0; i < 25; i++)
  {
    Output[(i*8)+0] = (m_data[i] >> 7) & 1;
    Output[(i*8)+1] = (m_data[i] >> 6) & 1;
    Output[(i*8)+2] = (m_data[i] >> 5) & 1;
    Output[(i*8)+3] = (m_data[i] >> 4) & 1;
    Output[(i*8)+4] = (m_data[i] >> 3) & 1;
    Output[(i*8)+5] = (m_data[i] >> 2) & 1;
    Output[(i*8)+6] = (m_data[i] >> 1) & 1;
    Output[(i*8)+7] = (m_data[i] >> 0) & 1;
  }
  /* Get the 3 last bits */
  Output[200] = (m_data[25] >> 7) & 1;
  Output[201] = (m_data[25] >> 6) & 1;
  Output[202] = (m_data[25] >> 5) & 1;

  /* Extract the 15 bits CRC */
  CRCExtracted = ((uint16_t)(m_data[23] << 7) | (uint16_t)(m_data[24] >> 1)) & 0x7FFF;

  /* Compute the 15 bits CRC */
  CRCComputed = CRC15BitNXDN(Output, 184);

  //fprintf(stderr, "\n");

  //fprintf(stderr, "FACCH2 Data = ");
  //for(i = 0; i < 26; i++)
  //{
  //  fprintf(stderr, "0x%02X - ", m_data[i]);
  //}
  //fprintf(stderr, "\n");

  //fprintf(stderr, "CRC computed = 0x%04X - CRC extracted = 0x%04X\n", CRCComputed, CRCExtracted);

  /* Check CRCs */
  if(CRCExtracted == CRCComputed)
  {
    //fprintf(stderr, "OK !\n");
    GoodCrc = 1;
  }
  else
  {
    //fprintf(stderr, "ERROR !\n");
    GoodCrc = 0;
  }

  return GoodCrc;
} /* End NXDN_UDCH_decode() */



/*
 * @brief : This function decodes the content of elements in :
 *          - Full SACCH (when 4 voice frame parts have been
 *            successfully received)
 *
 * @param opts : Option structure parameters pointer
 *
 * @param state : State structure parameters pointer
 *
 * @param CrcCorrect : The CRC status of the last element received
 *                     0 = CRC incorrect
 *                     Oher = CRC correct
 *
 * @param ElementsContent : A 64 to 144 bits buffer containing the
 *                         element content
 *
 * @return None
 *
 */
void NXDN_Elements_Content_decode(dsd_opts * opts, dsd_state * state,
                                  uint8_t CrcCorrect, uint8_t * ElementsContent)
{
  uint32_t i;
  uint8_t MessageType;
  uint64_t CurrentIV = 0;

  /* Get the "Message Type" field */
  MessageType  = (ElementsContent[2] & 1) << 5;
  MessageType |= (ElementsContent[3] & 1) << 4;
  MessageType |= (ElementsContent[4] & 1) << 3;
  MessageType |= (ElementsContent[5] & 1) << 2;
  MessageType |= (ElementsContent[6] & 1) << 1;
  MessageType |= (ElementsContent[7] & 1) << 0;

  /* Save the "F1" and "F2" flags */
  state->NxdnElementsContent.F1 = ElementsContent[0];
  state->NxdnElementsContent.F2 = ElementsContent[1];

  /* Save the "Message Type" field */
  state->NxdnElementsContent.MessageType = MessageType;

  //fprintf(stderr, "Message Type = 0x%02X ", MessageType & 0xFF);

  /* Decode the right "Message Type" */
  switch(MessageType)
  {
    //Idle
    case NXDN_IDLE:
    {
      //fprintf(stderr, "NXDN IDLE\n");
      sprintf (state->nxdn_call_type, "NXDN IDLE ");
    }
    break;

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

  //UNUSED_VARIABLE(opts);
  //UNUSED_VARIABLE(state);
  //UNUSED_VARIABLE(DuplexMode[0]);
  //UNUSED_VARIABLE(TransmissionMode[0]);

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

  /* On an AES or DES encrypted frame one IV is used on two
   * superframe.
   * The first superframe contains a VCALL message inside
   * the SACCH and the second superframe contains the next IV
   * to use.
   *
   * Set the correct part of encrypted frame flag.
   */
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
  fprintf(stderr, "\n %s - ", NXDN_Call_Type_To_Str(CallType)); //line break and 1 space
  //sprintf (state->nxdn_call_type, NXDN_Call_Type_To_Str(CallType)); //fix warning below
  //warning: format not a string literal and no format arguments [-Wformat-security]
  sprintf (state->nxdn_call_type, "%s", NXDN_Call_Type_To_Str(CallType)); //fix warning below

  /* Print the "Voice Call Option" */
  NXDN_Voice_Call_Option_To_Str(VoiceCallOption, DuplexMode, TransmissionMode);
  fprintf(stderr, "%s %s - ", DuplexMode, TransmissionMode);

  //state->nxdn_key = (KeyID & 0xFF);
  //state->nxdn_cipher_type = CipherType;
  /* Print the "Cipher Type" */
  if(CipherType != 0)
  {
    fprintf(stderr, "%s - ", NXDN_Cipher_Type_To_Str(CipherType));
    //state->nxdn_cipher_type = CipherType;
  }

  /* Print the Key ID */
  if(CipherType != 0)
  {
    fprintf(stderr, "Key ID %u - ", KeyID & 0xFF);
    //state->nxdn_key = (KeyID & 0xFF);
  }

  /* Print Source ID and Destination ID (Talk Group or Unit ID) */
  fprintf(stderr, "Src=%u - Dst/TG=%u ", SourceUnitID & 0xFFFF, DestinationID & 0xFFFF);


  if(state->NxdnElementsContent.VCallCrcIsGood)
  {
    if ( (SourceUnitID & 0xFFFF) > 0 )
    {
      state->nxdn_last_rid = SourceUnitID & 0xFFFF;   //only grab if CRC is okay
      state->nxdn_last_tg = (DestinationID & 0xFFFF);
      state->nxdn_key = (KeyID & 0xFF);
      state->nxdn_cipher_type = CipherType;
    }
    fprintf(stderr, "(CRC OK) ");
  }
  //fprintf(stderr, "   (OK)   - ");
  else fprintf(stderr, "(CRC ERR) ");

  //fprintf(stderr, "\nVCALL = ");

  //for(i = 0; i < 8; i++)
  //{
  //  fprintf(stderr, "0x%02X, ", ConvertBitIntoBytes(&Message[i * 8], 8) & 0xFF);
  //}
  //fprintf(stderr, "\n");

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

  //UNUSED_VARIABLE(opts);
  //UNUSED_VARIABLE(state);

  /* Message[0..7] contains :
   * - The "F1" and "F2" flags
   * - The "Message Type" (already decoded before calling this function)
   *
   * So no need to decode it a second time
   *
   * Message[8..71] contains : The 64 bits IV
   */

  /* Extract the IV from the VCALL_IV message */
  for(i = 0; i < 8; i++)
  {
    state->NxdnElementsContent.IV[i] = (uint8_t)ConvertBitIntoBytes(&Message[(i + 1) * 8], 8);
  }

  /* On an AES or DES encrypted frame one IV is used on two
   * superframe.
   * The first superframe contains a VCALL message inside
   * the SACCH and the second superframe contains the next IV
   * to use.
   *
   * Set the correct part of encrypted frame flag.
   */
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


/* CRC 15 bits computation with the following polynomial :
 * X^15 + X^14 + X^11 + X^10 + X^7 + X^6 + X^2 + 1
 *
 * X^15 + (1X^14 + 0X^13 + 0X^12 + 1X^11 + 1X^10 + 0X^9 + 0X^8 + 1X^7 + 1X^6 + 0X^5 + 0X^4 + 0X^3 + 1X^2 + 0X^1 + 1X^0)
 * => Polynomial = 0b100110011000101 = 0x4CC5
 */
uint16_t CRC15BitNXDN(uint8_t * BufferIn, uint32_t BitLength)
{
  uint16_t CRC = 0x7FFF;      /* Initial value = All bit to '1' (only 15 LSBit are used) */
  uint16_t Polynome = 0x4CC5;  /* X^15 + X^14 + X^11 + X^10 + X^7 + X^6 + X^2 + 1 */
  uint32_t i;

  for(i = 0; i < BitLength; i++)
  {
    if(((CRC >> 14) & 1) ^ (BufferIn[i] & 1))
    {
      CRC = ((CRC << 1) ^ Polynome) & 0x7FFF;
    }
    else
    {
      CRC = (CRC << 1) & 0x7FFF;
    }
  }

  return CRC;
} /* End CRC15BitNXDN() */


/* CRC 12 bits computation with the following polynomial :
 * X^12 + X^11 + X^3 + X^2 + X + 1
 *
 * X^12 + (1X^11 + 0X^10 + 0X^9 + 0X^8 + 0X^7 + 0X^6 + 0X^5 + 0X^4 + 1X^3 + 1X^2 + 1X^1 + 1X^0)
 * => Polynomial = 0b100000001111 = 0x80F
 */
uint16_t CRC12BitNXDN(uint8_t * BufferIn, uint32_t BitLength)
{
  uint16_t CRC = 0x0FFF;      /* Initial value = All bit to '1' (only 12 LSBit are used) */
  uint16_t Polynome = 0x80F;  /* X^12 + X^11 + X^3 + X^2 + X + 1 */
  uint32_t i;

  for(i = 0; i < BitLength; i++)
  {
    if(((CRC >> 11) & 1) ^ (BufferIn[i] & 1))
    {
      CRC = ((CRC << 1) ^ Polynome) & 0x0FFF;
    }
    else
    {
      CRC = (CRC << 1) & 0x0FFF;
    }
  }

  return CRC;
} /* End CRC12BitNXDN() */


/* CRC 6 bit computation with the following
 * polynomial : X^6 + X^5 + X^2 + X + 1
 *
 * X^6 + (1X^5 + 0X^4 + 0X^3 + 1X^2 + 1X^1 + 1X^0)
 * => Polynomial = 0b100111 = 0x27
 */
uint8_t CRC6BitNXDN(uint8_t * BufferIn, uint32_t BitLength)
{
  uint8_t  CRC = 0x3F;      /* Initial value = All bit to '1' (only 6 LSBit are used) */
  uint8_t  Polynome = 0x27; /* X^6 + X^5 + X^2 + X + 1 */
  uint32_t i;

  for(i = 0; i < BitLength; i++)
  {
    if(((CRC >> 5) & 1) ^ (BufferIn[i] & 1))
    {
      CRC = ((CRC << 1) ^ Polynome) & 0x3F;
    }
    else
    {
      CRC = (CRC << 1) & 0x3F;
    }
  }

  return CRC;
} /* End CRC6BitNXDN() */


/* Scrambler used for NXDN transmission (different of the
 * voice encryption scrambler), see NXDN Part1-A Common Air Interface chapter
 * 4.6 for the polynomial description.
 * It is a X^9 + X^4 + 1 polynomial used as pseudo-random generator */
void ScrambledNXDNVoiceBit(int * LfsrValue, char * BufferIn, char * BufferOut, int NbOfBitToScramble)
{
  uint32_t i;
  uint32_t LFSR;
  uint8_t  bit;

  /* Load the initial LFSR value
   * LFSR = 0x0E4 at starting value */
  LFSR = (uint32_t)*LfsrValue;

  for(i = 0; i < (uint32_t)NbOfBitToScramble; i++)
  {
    /* Bit 0 of LFSR is used to scramble data */
    BufferOut[i] = (BufferIn[i] ^ (LFSR & 1)) & 1;

    /* Compute bit = X^0 + X^4 */
    bit = ((LFSR & 1) ^ ((LFSR >> 4) & 1)) & 1;

    /* Insert bit on the 9th bit (X^9) */
    LFSR = ((LFSR >> 1) & 0xFF) | ((bit << 8) & 0x100);
  }

  *LfsrValue = (int)LFSR;
} /* End ScrambledNXDNVoiceBit() */



/* End of file */
