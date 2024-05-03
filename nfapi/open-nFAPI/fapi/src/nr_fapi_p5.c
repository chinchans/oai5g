/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */
/*! \file nfapi/open-nFAPI/fapi/src/nr_fapi_p5.c
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */
#include "nr_fapi.h"
#include "nr_fapi_p5.h"
#include "nr_fapi_p5_utils.h"
#include "debug.h"

bool isFAPIMessageIDValid(uint16_t id){
  // SCF 222.10.04 Table 3-5 PHY API message types
  return ((id >= NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST && id <= 0xFF)
          || id == NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE
          || id == NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC
          || id == NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC
          || id == NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO
          );
}

int fapi_nr_p5_message_header_unpack(uint8_t **pMessageBuf,
                                     uint32_t messageBufLen,
                                     void *pUnpackedBuf,
                                     uint32_t unpackedBufLen,
                                     nfapi_p4_p5_codec_config_t *config)
{
  uint8_t **pReadPackedMessage = pMessageBuf;
  nfapi_p4_p5_message_header_t *header = pUnpackedBuf;
  fapi_message_header_t fapi_msg;

  AssertFatal(pMessageBuf != NULL && pUnpackedBuf != NULL, "P5 header unpack supplied pointers are null");
  uint8_t *end = *pMessageBuf + messageBufLen;
  AssertFatal(messageBufLen >= NFAPI_HEADER_LENGTH && unpackedBufLen >= sizeof(nfapi_p4_p5_message_header_t),
              "P5 header unpack supplied message buffer is too small %d, %d\n",
              messageBufLen,
              unpackedBufLen);
  // process the header
  int result =
      (pull8(pReadPackedMessage, &fapi_msg.num_msg, end) && pull8(pReadPackedMessage, &fapi_msg.opaque_handle, end)
       && pull16(pReadPackedMessage, &header->message_id, end) && pull32(pReadPackedMessage, &fapi_msg.message_length, end));
  DevAssert(fapi_msg.message_length <= 0xFFFF);
  header->message_length = fapi_msg.message_length;
  return (result);
}

uint8_t fapi_nr_p5_message_body_pack(nfapi_p4_p5_message_header_t *header,
                                     uint8_t **ppWritePackedMsg,
                                     uint8_t *end,
                                     nfapi_p4_p5_codec_config_t *config)
{
  uint8_t result = 0;

  // look for the specific message
  switch (header->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
      result = 0;
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE:
      result = pack_nr_param_response(header, ppWritePackedMsg, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
      result = pack_nr_config_request(header, ppWritePackedMsg, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE:
      result = pack_nr_config_response(header, ppWritePackedMsg, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
      result = pack_nr_start_request(header, ppWritePackedMsg, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE:
      result = pack_nr_start_response(header, ppWritePackedMsg, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
      result = pack_nr_stop_request(header, ppWritePackedMsg, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION:
      result = pack_nr_stop_indication(header, ppWritePackedMsg, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION:
      result = pack_nr_error_indication(header, ppWritePackedMsg, end, config);
      break;

    default: {
      AssertFatal(header->message_id >= 0x00 && header->message_id <= 0xFF,
                  "FAPI message IDs are defined between 0x00 and 0xFF the message provided 0x%02x, which is not a FAPI message",
                  header->message_id);
      break;
    }
  }
  return result;
}

int fapi_nr_p5_message_pack(void *pMessageBuf,
                            uint32_t messageBufLen,
                            void *pPackedBuf,
                            uint32_t packedBufLen,
                            nfapi_p4_p5_codec_config_t *config)
{
  nfapi_p4_p5_message_header_t *pMessageHeader = pMessageBuf;
  uint8_t *pWritePackedMessage = pPackedBuf;
  AssertFatal(isFAPIMessageIDValid(pMessageHeader->message_id),
              "FAPI message IDs are defined between 0x00 and 0xFF the message provided 0x%02x, which is not a FAPI message",
              pMessageHeader->message_id);
  uint32_t packedMsgLen;
  uint32_t packedBodyLen;
  uint16_t packedMsgLen16;
  AssertFatal(pMessageBuf != NULL && pPackedBuf != NULL, "P5 Pack supplied pointers are null");

  uint8_t *pPackMessageEnd = pPackedBuf + packedBufLen;
  uint8_t *pPackedLengthField = &pWritePackedMessage[4];
  uint8_t *pPacketBodyField = &pWritePackedMessage[8];
  uint8_t *pPacketBodyFieldStart = &pWritePackedMessage[8];

  uint8_t res = fapi_nr_p5_message_body_pack(pMessageHeader, &pPacketBodyField, pPackMessageEnd, config);
  AssertFatal(res >= 0, "fapi_nr_p5_message_body_pack error packing message body %d\n", res);

  // PHY API message header
  push8(1, &pWritePackedMessage, pPackMessageEnd); // Number of messages
  push8(0, &pWritePackedMessage, pPackMessageEnd); // Opaque handle

  // PHY API Message structure
  push16(pMessageHeader->message_id, &pWritePackedMessage, pPackMessageEnd); // Message type ID

  // check for a valid message length
  packedMsgLen = get_packed_msg_len((uintptr_t)pPackedBuf, (uintptr_t)pPacketBodyField);
  packedBodyLen = get_packed_msg_len((uintptr_t)pPacketBodyFieldStart, (uintptr_t)pPacketBodyField);
  packedMsgLen16 = (uint16_t)packedBodyLen;
  if (pMessageHeader->message_id == NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST
      || pMessageHeader->message_id == NFAPI_NR_PHY_MSG_TYPE_START_REQUEST
      || pMessageHeader->message_id == NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST
      || pMessageHeader->message_id == NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION) {
    // These messages don't have a body, length is 0
    packedMsgLen16 = 0;
  }
  AssertFatal(packedMsgLen <= 0xFFFF && packedMsgLen <= packedBufLen,
              "Packed message 0x%02x length error %d, buffer supplied %d\n",
              pMessageHeader->message_id,
              packedMsgLen,
              packedBufLen);
  // Update the message length in the header
  if (!push32(packedMsgLen16, &pPackedLengthField, pPackMessageEnd))
    return -1;

  // return the packed length
  return (int)(packedMsgLen);
}

int fapi_nr_p5_message_unpack(void *pMessageBuf,
                              uint32_t messageBufLen,
                              void *pUnpackedBuf,
                              uint32_t unpackedBufLen,
                              nfapi_p4_p5_codec_config_t *config)
{
  fapi_message_header_t *pMessageHeader = pUnpackedBuf;
  uint8_t *pReadPackedMessage = pMessageBuf;

  AssertFatal(pMessageBuf != NULL && pUnpackedBuf != NULL, "P5 unpack supplied pointers are null");
  uint8_t *end = (uint8_t *)pMessageBuf + messageBufLen;
  AssertFatal(messageBufLen >= NFAPI_HEADER_LENGTH && unpackedBufLen >= sizeof(fapi_message_header_t),
              "P5 unpack supplied message buffer is too small %d, %d\n",
              messageBufLen,
              unpackedBufLen);
#if DEBUG_FAPI_NFAPI_MSGS
  uint8_t *ptr = pReadPackedMessage;
  printf("\n Read NR message unpack: ");

  while (ptr < end) {
    printf(" %02x ", *ptr);
    ptr++;
  }

  printf("\n");
#endif
  // clean the supplied buffer for - tag value blanking
  (void)memset(pUnpackedBuf, 0, unpackedBufLen);
  if (fapi_nr_p5_message_header_unpack(&pReadPackedMessage, NFAPI_HEADER_LENGTH, pMessageHeader, sizeof(fapi_message_header_t), 0)
      < 0) {
    // failed to read the header
    return -1;
  }

  int result = -1;

  if (check_nr_fapi_unpack_length(pMessageHeader->message_id, unpackedBufLen) == 0) {
    // the unpack buffer is not big enough for the struct
    return -1;
  }

  // look for the specific message
  switch (pMessageHeader->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
      // PARAM request has no body;
      result = 0;
      break;

    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE:
      result = unpack_nr_param_response(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
      result = unpack_nr_config_request(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE:
      result = unpack_nr_config_response(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
      result = unpack_nr_start_request(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE:
      result = unpack_nr_start_response(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
      result = unpack_nr_stop_request(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION:
      result = unpack_nr_stop_indication(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION:
      result = unpack_nr_error_indication(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    default:
      if (pMessageHeader->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && pMessageHeader->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        NFAPI_TRACE(NFAPI_TRACE_ERROR,
                    "%s VE NFAPI message ID %d. No ve decoder provided\n",
                    __FUNCTION__,
                    pMessageHeader->message_id);
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown P5 message ID %d\n", __FUNCTION__, pMessageHeader->message_id);
      }

      break;
  }

  return result;
}

int check_nr_fapi_unpack_length(nfapi_nr_phy_msg_type_e msgId, uint32_t unpackedBufLen)
{
  int retLen = 0;
  /**  NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST=  0x00,
    NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE= 0x01,
    NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST= 0x02,
    NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE=0X03,
    NFAPI_NR_PHY_MSG_TYPE_START_REQUEST=  0X04,
    NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST=   0X05,
    NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION=0X06,
    NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION=0X07
    */
  // check for size of nFAPI struct without the nFAPI specific parameters
  switch (msgId) {
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_param_request_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE:
      if (unpackedBufLen >= sizeof(nfapi_nr_param_response_scf_t) - sizeof(nfapi_vendor_extension_tlv_t) - sizeof(nfapi_nr_nfapi_t))
        retLen = sizeof(nfapi_nr_param_request_scf_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_config_request_scf_t) - sizeof(nfapi_vendor_extension_tlv_t) - sizeof(nfapi_nr_nfapi_t))
        retLen = sizeof(nfapi_nr_config_request_scf_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE:
      if (unpackedBufLen >= sizeof(nfapi_nr_config_response_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(nfapi_nr_config_response_scf_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_start_request_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE:
      if (unpackedBufLen >= sizeof(nfapi_nr_start_response_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_stop_request_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_stop_indication_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_error_indication_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    default:
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s Unknown message ID %d\n", __FUNCTION__, msgId);
      break;
  }

  return retLen;
}


uint8_t pack_nr_param_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *config) {
  nfapi_nr_param_request_scf_t *pNfapiMsg = (nfapi_nr_param_request_scf_t *)msg;
  return (pack_vendor_extension_tlv(pNfapiMsg->vendor_extension, ppWritePackedMsg, end, config));
}

uint8_t unpack_nr_param_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p4_p5_codec_config_t *config)
{
  nfapi_nr_param_request_scf_t *pNfapiMsg = (nfapi_nr_param_request_scf_t *)msg;
  return unpack_nr_tlv_list(NULL, 0, ppReadPackedMsg, end, config, &(pNfapiMsg->vendor_extension));
}

uint8_t pack_nr_param_response(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *config)
{
  printf("\nRUNNING pack_param_response\n");
  nfapi_nr_param_response_scf_t *pNfapiMsg = (nfapi_nr_param_response_scf_t *)msg;
  uint8_t retval = push8(pNfapiMsg->error_code, ppWritePackedMsg, end) && push8(pNfapiMsg->num_tlv, ppWritePackedMsg, end);
  retval &= pack_nr_tlv(NFAPI_NR_PARAM_TLV_RELEASE_CAPABILITY_TAG,
                        &(pNfapiMsg->cell_param.release_capability),
                        ppWritePackedMsg,
                        end,
                        &pack_uint16_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PHY_STATE_TAG,
                           &(pNfapiMsg->cell_param.phy_state),
                           ppWritePackedMsg,
                           end,
                           &pack_uint16_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_SKIP_BLANK_DL_CONFIG_TAG,
                           &(pNfapiMsg->cell_param.skip_blank_dl_config),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_SKIP_BLANK_UL_CONFIG_TAG,
                           &(pNfapiMsg->cell_param.skip_blank_ul_config),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_NUM_CONFIG_TLVS_TO_REPORT_TAG,
                           &(pNfapiMsg->cell_param.num_config_tlvs_to_report),
                           ppWritePackedMsg,
                           end,
                           &pack_uint16_tlv_value);

  for (int i = 0; i < pNfapiMsg->cell_param.num_config_tlvs_to_report.value; ++i) {
    /*retval &= pack_nr_tlv(pNfapiMsg->cell_param.config_tlvs_to_report_list[i].tl.tag,
                          &(pNfapiMsg->cell_param.config_tlvs_to_report_list[i]),
                          ppWritePackedMsg,
                          end,
                          &pack_uint8_tlv_value);*/
    retval &= push16(pNfapiMsg->cell_param.config_tlvs_to_report_list[i].tl.tag, ppWritePackedMsg, end);
    retval &= push8(pNfapiMsg->cell_param.config_tlvs_to_report_list[i].tl.length, ppWritePackedMsg, end);
    retval &= push8(pNfapiMsg->cell_param.config_tlvs_to_report_list[i].value, ppWritePackedMsg, end);
    // Add padding that ensures multiple of 4 bytes (SCF 225 Section 2.3.2.1)
    int padding = get_tlv_padding(pNfapiMsg->cell_param.config_tlvs_to_report_list[i].tl.length);
    NFAPI_TRACE(NFAPI_TRACE_DEBUG,
                "TLV 0x%x with padding of %d bytes\n",
                pNfapiMsg->cell_param.config_tlvs_to_report_list[i].tl.tag,
                padding);
    if (padding != 0) {
      memset(*ppWritePackedMsg, 0, padding);
      (*ppWritePackedMsg) += padding;
    }
  }

  retval &= pack_nr_tlv(NFAPI_NR_PARAM_TLV_CYCLIC_PREFIX_TAG,
                        &(pNfapiMsg->carrier_param.cyclic_prefix),
                        ppWritePackedMsg,
                        end,
                        &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_SUPPORTED_SUBCARRIER_SPACINGS_DL_TAG,
                           &(pNfapiMsg->carrier_param.supported_subcarrier_spacings_dl),
                           ppWritePackedMsg,
                           end,
                           &pack_uint16_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_SUPPORTED_BANDWIDTH_DL_TAG,
                           &(pNfapiMsg->carrier_param.supported_bandwidth_dl),
                           ppWritePackedMsg,
                           end,
                           &pack_uint16_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_SUPPORTED_SUBCARRIER_SPACINGS_UL_TAG,
                           &(pNfapiMsg->carrier_param.supported_subcarrier_spacings_ul),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_SUPPORTED_BANDWIDTH_UL_TAG,
                           &(pNfapiMsg->carrier_param.supported_bandwidth_ul),
                           ppWritePackedMsg,
                           end,
                           &pack_uint16_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_CCE_MAPPING_TYPE_TAG,
                           &(pNfapiMsg->pdcch_param.cce_mapping_type),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_CORESET_OUTSIDE_FIRST_3_OFDM_SYMS_OF_SLOT_TAG,
                           &(pNfapiMsg->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PRECODER_GRANULARITY_CORESET_TAG,
                           &(pNfapiMsg->pdcch_param.coreset_precoder_granularity_coreset),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDCCH_MU_MIMO_TAG,
                           &(pNfapiMsg->pdcch_param.pdcch_mu_mimo),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDCCH_PRECODER_CYCLING_TAG,
                           &(pNfapiMsg->pdcch_param.pdcch_precoder_cycling),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_PDCCHS_PER_SLOT_TAG,
                           &(pNfapiMsg->pdcch_param.max_pdcch_per_slot),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUCCH_FORMATS_TAG,
                           &(pNfapiMsg->pucch_param.pucch_formats),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_PUCCHS_PER_SLOT_TAG,
                           &(pNfapiMsg->pucch_param.max_pucchs_per_slot),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_MAPPING_TYPE_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_mapping_type),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_ALLOCATION_TYPES_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_allocation_types),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_VRB_TO_PRB_MAPPING_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_vrb_to_prb_mapping),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_CBG_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_cbg),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_DMRS_CONFIG_TYPES_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_dmrs_config_types),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_DMRS_MAX_LENGTH_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_dmrs_max_length),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_DMRS_ADDITIONAL_POS_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_dmrs_additional_pos),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_PDSCH_S_YBS_PER_SLOT_TAG,
                           &(pNfapiMsg->pdsch_param.max_pdsch_tbs_per_slot),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_NUMBER_MIMO_LAYERS_PDSCH_TAG,
                           &(pNfapiMsg->pdsch_param.max_number_mimo_layers_pdsch),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_MU_MIMO_USERS_DL_TAG,
                           &(pNfapiMsg->pdsch_param.max_mu_mimo_users_dl),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_DATA_IN_DMRS_SYMBOLS_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_data_in_dmrs_symbols),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PREMPTION_SUPPORT_TAG,
                           &(pNfapiMsg->pdsch_param.premption_support),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PDSCH_NON_SLOT_SUPPORT_TAG,
                           &(pNfapiMsg->pdsch_param.pdsch_non_slot_support),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_UCI_MUX_ULSCH_IN_PUSCH_TAG,
                           &(pNfapiMsg->pusch_param.uci_mux_ulsch_in_pusch),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_UCI_ONLY_PUSCH_TAG,
                           &(pNfapiMsg->pusch_param.uci_only_pusch),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_FREQUENCY_HOPPING_TAG,
                           &(pNfapiMsg->pusch_param.pusch_frequency_hopping),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_DMRS_CONFIG_TYPES_TAG,
                           &(pNfapiMsg->pusch_param.pusch_dmrs_config_types),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_DMRS_MAX_LEN_TAG,
                           &(pNfapiMsg->pusch_param.pusch_dmrs_max_len),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_DMRS_ADDITIONAL_POS_TAG,
                           &(pNfapiMsg->pusch_param.pusch_dmrs_additional_pos),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_CBG_TAG,
                           &(pNfapiMsg->pusch_param.pusch_cbg),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_MAPPING_TYPE_TAG,
                           &(pNfapiMsg->pusch_param.pusch_mapping_type),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_ALLOCATION_TYPES_TAG,
                           &(pNfapiMsg->pusch_param.pusch_allocation_types),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_VRB_TO_PRB_MAPPING_TAG,
                           &(pNfapiMsg->pusch_param.pusch_vrb_to_prb_mapping),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_MAX_PTRS_PORTS_TAG,
                           &(pNfapiMsg->pusch_param.pusch_max_ptrs_ports),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_PDUSCHS_TBS_PER_SLOT_TAG,
                           &(pNfapiMsg->pusch_param.max_pduschs_tbs_per_slot),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_NUMBER_MIMO_LAYERS_NON_CB_PUSCH_TAG,
                           &(pNfapiMsg->pusch_param.max_number_mimo_layers_non_cb_pusch),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_SUPPORTED_MODULATION_ORDER_UL_TAG,
                           &(pNfapiMsg->pusch_param.supported_modulation_order_ul),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_MU_MIMO_USERS_UL_TAG,
                           &(pNfapiMsg->pusch_param.max_mu_mimo_users_ul),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_DFTS_OFDM_SUPPORT_TAG,
                           &(pNfapiMsg->pusch_param.dfts_ofdm_support),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PUSCH_AGGREGATION_FACTOR_TAG,
                           &(pNfapiMsg->pusch_param.pusch_aggregation_factor),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PRACH_LONG_FORMATS_TAG,
                           &(pNfapiMsg->prach_param.prach_long_formats),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PRACH_SHORT_FORMATS_TAG,
                           &(pNfapiMsg->prach_param.prach_short_formats),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_PRACH_RESTRICTED_SETS_TAG,
                           &(pNfapiMsg->prach_param.prach_restricted_sets),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_MAX_PRACH_FD_OCCASIONS_IN_A_SLOT_TAG,
                           &(pNfapiMsg->prach_param.max_prach_fd_occasions_in_a_slot),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_PARAM_TLV_RSSI_MEASUREMENT_SUPPORT_TAG,
                           &(pNfapiMsg->measurement_param.rssi_measurement_support),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            &&
            // config:
            pack_nr_tlv(NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV4_TAG,
                        &(pNfapiMsg->nfapi_config.p7_vnf_address_ipv4),
                        ppWritePackedMsg,
                        end,
                        &pack_ipv4_address_value)
            && pack_nr_tlv(NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV6_TAG,
                           &(pNfapiMsg->nfapi_config.p7_vnf_address_ipv6),
                           ppWritePackedMsg,
                           end,
                           &pack_ipv6_address_value)
            && pack_nr_tlv(NFAPI_NR_NFAPI_P7_VNF_PORT_TAG,
                           &(pNfapiMsg->nfapi_config.p7_vnf_port),
                           ppWritePackedMsg,
                           end,
                           &pack_uint16_tlv_value)
            && pack_nr_tlv(NFAPI_NR_NFAPI_P7_PNF_ADDRESS_IPV4_TAG,
                           &(pNfapiMsg->nfapi_config.p7_pnf_address_ipv4),
                           ppWritePackedMsg,
                           end,
                           &pack_ipv4_address_value)
            && pack_nr_tlv(NFAPI_NR_NFAPI_P7_PNF_ADDRESS_IPV6_TAG,
                           &(pNfapiMsg->nfapi_config.p7_pnf_address_ipv6),
                           ppWritePackedMsg,
                           end,
                           &pack_ipv6_address_value)
            && pack_nr_tlv(NFAPI_NR_NFAPI_P7_PNF_PORT_TAG,
                           &(pNfapiMsg->nfapi_config.p7_pnf_port),
                           ppWritePackedMsg,
                           end,
                           &pack_uint16_tlv_value)
            && pack_nr_tlv(NFAPI_NR_NFAPI_TIMING_WINDOW_TAG,
                           &(pNfapiMsg->nfapi_config.timing_window),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_NFAPI_TIMING_INFO_MODE_TAG,
                           &(pNfapiMsg->nfapi_config.timing_info_mode),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_nr_tlv(NFAPI_NR_NFAPI_TIMING_INFO_PERIOD_TAG,
                           &(pNfapiMsg->nfapi_config.timing_info_period),
                           ppWritePackedMsg,
                           end,
                           &pack_uint8_tlv_value)
            && pack_vendor_extension_tlv(pNfapiMsg->vendor_extension, ppWritePackedMsg, end, config);
  return retval;
}

static uint8_t unpack_config_tlvs_to_report(void *tlv, uint8_t **ppReadPackedMsg, uint8_t *end)
{
  uint8_t *pStartOfValue = *ppReadPackedMsg;
  nfapi_nr_cell_param_t *cellParamTable = (nfapi_nr_cell_param_t* )tlv;
  uint8_t retval = pull16(ppReadPackedMsg, &cellParamTable->num_config_tlvs_to_report.value, end);

  // check if the length was right;
  if (cellParamTable->num_config_tlvs_to_report.tl.length != (*ppReadPackedMsg - pStartOfValue)) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR,
                "Warning tlv tag 0x%x length %d not equal to unpack %ld\n",
                cellParamTable->num_config_tlvs_to_report.tl.tag,
                cellParamTable->num_config_tlvs_to_report.tl.length,
                (*ppReadPackedMsg - pStartOfValue));
  }
  // Remove padding that ensures multiple of 4 bytes (SCF 225 Section 2.3.2.1)
  int padding = get_tlv_padding(cellParamTable->num_config_tlvs_to_report.tl.length);
  if (padding != 0) {
    (*ppReadPackedMsg) += padding;
  }

  //after this value, get the tlv list
  cellParamTable->config_tlvs_to_report_list = calloc(cellParamTable->num_config_tlvs_to_report.value, sizeof(nfapi_uint8_tlv_t*) );

  for (int i = 0; i < cellParamTable->num_config_tlvs_to_report.value; ++i) {

    pull16(ppReadPackedMsg, &cellParamTable->config_tlvs_to_report_list[i].tl.tag , end);
    pull8(ppReadPackedMsg, (uint8_t *)&cellParamTable->config_tlvs_to_report_list[i].tl.length, end);
    pull8(ppReadPackedMsg, &cellParamTable->config_tlvs_to_report_list[i].value, end);
    // Remove padding that ensures multiple of 4 bytes (SCF 225 Section 2.3.2.1)
    padding = get_tlv_padding(cellParamTable->config_tlvs_to_report_list[i].tl.length);
    if (padding != 0) {
      (*ppReadPackedMsg) += padding;
    }

  }
  return retval;
}

uint8_t unpack_nr_param_response(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p4_p5_codec_config_t *config)
{
  nfapi_nr_param_response_scf_t *pNfapiMsg = (nfapi_nr_param_response_scf_t *)msg;
  unpack_tlv_t unpack_fns[] = {
      {NFAPI_NR_PARAM_TLV_RELEASE_CAPABILITY_TAG, &(pNfapiMsg->cell_param.release_capability), &unpack_uint16_tlv_value},
      {NFAPI_NR_PARAM_TLV_PHY_STATE_TAG, &(pNfapiMsg->cell_param.phy_state), &unpack_uint16_tlv_value},
      {NFAPI_NR_PARAM_TLV_SKIP_BLANK_DL_CONFIG_TAG, &(pNfapiMsg->cell_param.skip_blank_dl_config), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_SKIP_BLANK_UL_CONFIG_TAG, &(pNfapiMsg->cell_param.skip_blank_ul_config), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_NUM_CONFIG_TLVS_TO_REPORT_TAG, &(pNfapiMsg->cell_param), &unpack_config_tlvs_to_report},

      {NFAPI_NR_PARAM_TLV_CYCLIC_PREFIX_TAG, &(pNfapiMsg->carrier_param.cyclic_prefix), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_SUPPORTED_SUBCARRIER_SPACINGS_DL_TAG,
       &(pNfapiMsg->carrier_param.supported_subcarrier_spacings_dl),
       &unpack_uint16_tlv_value},
      {NFAPI_NR_PARAM_TLV_SUPPORTED_BANDWIDTH_DL_TAG, &(pNfapiMsg->carrier_param.supported_bandwidth_dl), &unpack_uint16_tlv_value},
      {NFAPI_NR_PARAM_TLV_SUPPORTED_SUBCARRIER_SPACINGS_UL_TAG,
       &(pNfapiMsg->carrier_param.supported_subcarrier_spacings_ul),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_SUPPORTED_BANDWIDTH_UL_TAG, &(pNfapiMsg->carrier_param.supported_bandwidth_ul), &unpack_uint16_tlv_value},

      {NFAPI_NR_PARAM_TLV_CCE_MAPPING_TYPE_TAG, &(pNfapiMsg->pdcch_param.cce_mapping_type), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_CORESET_OUTSIDE_FIRST_3_OFDM_SYMS_OF_SLOT_TAG,
       &(pNfapiMsg->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PRECODER_GRANULARITY_CORESET_TAG,
       &(pNfapiMsg->pdcch_param.coreset_precoder_granularity_coreset),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDCCH_MU_MIMO_TAG, &(pNfapiMsg->pdcch_param.pdcch_mu_mimo), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDCCH_PRECODER_CYCLING_TAG, &(pNfapiMsg->pdcch_param.pdcch_precoder_cycling), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_PDCCHS_PER_SLOT_TAG, &(pNfapiMsg->pdcch_param.max_pdcch_per_slot), &unpack_uint8_tlv_value},

      {NFAPI_NR_PARAM_TLV_PUCCH_FORMATS_TAG, &(pNfapiMsg->pucch_param.pucch_formats), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_PUCCHS_PER_SLOT_TAG, &(pNfapiMsg->pucch_param.max_pucchs_per_slot), &unpack_uint8_tlv_value},

      {NFAPI_NR_PARAM_TLV_PDSCH_MAPPING_TYPE_TAG, &(pNfapiMsg->pdsch_param.pdsch_mapping_type), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDSCH_ALLOCATION_TYPES_TAG, &(pNfapiMsg->pdsch_param.pdsch_allocation_types), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDSCH_VRB_TO_PRB_MAPPING_TAG,
       &(pNfapiMsg->pdsch_param.pdsch_vrb_to_prb_mapping),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDSCH_CBG_TAG, &(pNfapiMsg->pdsch_param.pdsch_cbg), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDSCH_DMRS_CONFIG_TYPES_TAG, &(pNfapiMsg->pdsch_param.pdsch_dmrs_config_types), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDSCH_DMRS_MAX_LENGTH_TAG, &(pNfapiMsg->pdsch_param.pdsch_dmrs_max_length), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDSCH_DMRS_ADDITIONAL_POS_TAG,
       &(pNfapiMsg->pdsch_param.pdsch_dmrs_additional_pos),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_PDSCH_S_YBS_PER_SLOT_TAG, &(pNfapiMsg->pdsch_param.max_pdsch_tbs_per_slot), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_NUMBER_MIMO_LAYERS_PDSCH_TAG,
       &(pNfapiMsg->pdsch_param.max_number_mimo_layers_pdsch),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_MU_MIMO_USERS_DL_TAG, &(pNfapiMsg->pdsch_param.max_mu_mimo_users_dl), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDSCH_DATA_IN_DMRS_SYMBOLS_TAG,
       &(pNfapiMsg->pdsch_param.pdsch_data_in_dmrs_symbols),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PREMPTION_SUPPORT_TAG, &(pNfapiMsg->pdsch_param.premption_support), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PDSCH_NON_SLOT_SUPPORT_TAG, &(pNfapiMsg->pdsch_param.pdsch_non_slot_support), &unpack_uint8_tlv_value},

      {NFAPI_NR_PARAM_TLV_UCI_MUX_ULSCH_IN_PUSCH_TAG, &(pNfapiMsg->pusch_param.uci_mux_ulsch_in_pusch), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_UCI_ONLY_PUSCH_TAG, &(pNfapiMsg->pusch_param.uci_only_pusch), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_FREQUENCY_HOPPING_TAG, &(pNfapiMsg->pusch_param.pusch_frequency_hopping), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_DMRS_CONFIG_TYPES_TAG, &(pNfapiMsg->pusch_param.pusch_dmrs_config_types), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_DMRS_MAX_LEN_TAG, &(pNfapiMsg->pusch_param.pusch_dmrs_max_len), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_DMRS_ADDITIONAL_POS_TAG,
       &(pNfapiMsg->pusch_param.pusch_dmrs_additional_pos),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_CBG_TAG, &(pNfapiMsg->pusch_param.pusch_cbg), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_MAPPING_TYPE_TAG, &(pNfapiMsg->pusch_param.pusch_mapping_type), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_ALLOCATION_TYPES_TAG, &(pNfapiMsg->pusch_param.pusch_allocation_types), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_VRB_TO_PRB_MAPPING_TAG,
       &(pNfapiMsg->pusch_param.pusch_vrb_to_prb_mapping),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_MAX_PTRS_PORTS_TAG, &(pNfapiMsg->pusch_param.pusch_max_ptrs_ports), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_PDUSCHS_TBS_PER_SLOT_TAG,
       &(pNfapiMsg->pusch_param.max_pduschs_tbs_per_slot),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_NUMBER_MIMO_LAYERS_NON_CB_PUSCH_TAG,
       &(pNfapiMsg->pusch_param.max_number_mimo_layers_non_cb_pusch),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_SUPPORTED_MODULATION_ORDER_UL_TAG,
       &(pNfapiMsg->pusch_param.supported_modulation_order_ul),
       &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_MU_MIMO_USERS_UL_TAG, &(pNfapiMsg->pusch_param.max_mu_mimo_users_ul), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_DFTS_OFDM_SUPPORT_TAG, &(pNfapiMsg->pusch_param.dfts_ofdm_support), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PUSCH_AGGREGATION_FACTOR_TAG,
       &(pNfapiMsg->pusch_param.pusch_aggregation_factor),
       &unpack_uint8_tlv_value},

      {NFAPI_NR_PARAM_TLV_PRACH_LONG_FORMATS_TAG, &(pNfapiMsg->prach_param.prach_long_formats), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PRACH_SHORT_FORMATS_TAG, &(pNfapiMsg->prach_param.prach_short_formats), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_PRACH_RESTRICTED_SETS_TAG, &(pNfapiMsg->prach_param.prach_restricted_sets), &unpack_uint8_tlv_value},
      {NFAPI_NR_PARAM_TLV_MAX_PRACH_FD_OCCASIONS_IN_A_SLOT_TAG,
       &(pNfapiMsg->prach_param.max_prach_fd_occasions_in_a_slot),
       &unpack_uint8_tlv_value},

      {NFAPI_NR_PARAM_TLV_RSSI_MEASUREMENT_SUPPORT_TAG,
       &(pNfapiMsg->measurement_param.rssi_measurement_support),
       &unpack_uint8_tlv_value},
      // config
      {NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV4_TAG, &pNfapiMsg->nfapi_config.p7_vnf_address_ipv4, &unpack_ipv4_address_value},
      {NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV6_TAG, &pNfapiMsg->nfapi_config.p7_vnf_address_ipv6, &unpack_ipv6_address_value},
      {NFAPI_NR_NFAPI_P7_VNF_PORT_TAG, &pNfapiMsg->nfapi_config.p7_vnf_port, &unpack_uint16_tlv_value},
      {NFAPI_NR_NFAPI_P7_PNF_ADDRESS_IPV4_TAG, &pNfapiMsg->nfapi_config.p7_pnf_address_ipv4, &unpack_ipv4_address_value},
      {NFAPI_NR_NFAPI_P7_PNF_ADDRESS_IPV6_TAG, &pNfapiMsg->nfapi_config.p7_pnf_address_ipv6, &unpack_ipv6_address_value},
      {NFAPI_NR_NFAPI_P7_PNF_PORT_TAG, &pNfapiMsg->nfapi_config.p7_pnf_port, &unpack_uint16_tlv_value},
      {NFAPI_NR_NFAPI_TIMING_WINDOW_TAG, &pNfapiMsg->nfapi_config.timing_window, &unpack_uint8_tlv_value},
      {NFAPI_NR_NFAPI_TIMING_INFO_MODE_TAG, &pNfapiMsg->nfapi_config.timing_info_mode, &unpack_uint8_tlv_value},
      {NFAPI_NR_NFAPI_TIMING_INFO_PERIOD_TAG, &pNfapiMsg->nfapi_config.timing_info_period, &unpack_uint8_tlv_value},
  };
  // print ppReadPackedMsg
  uint8_t *ptr = *ppReadPackedMsg;
  printf("\n Read message unpack_param_response:   ");

  while (ptr < end) {
    printf(" 0x%02x", *ptr);
    ptr++;
  }

  printf("\n");
  return (pull8(ppReadPackedMsg, &pNfapiMsg->error_code, end) && pull8(ppReadPackedMsg, &pNfapiMsg->num_tlv, end)
          && unpack_nr_tlv_list(unpack_fns,
                                sizeof(unpack_fns) / sizeof(unpack_tlv_t),
                                ppReadPackedMsg,
                                end,
                                config,
                                &pNfapiMsg->vendor_extension));
}
