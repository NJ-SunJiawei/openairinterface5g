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

/*! \file gNB_scheduler_uci.c
 * \brief MAC procedures related to UCI
 * \date 2020
 * \version 1.0
 * \company Eurecom
 */

#include "LAYER2/MAC/mac.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "NR_MAC_COMMON/nr_mac_extern.h"
#include "NR_MAC_gNB/mac_proto.h"
#include "common/ran_context.h"

extern RAN_CONTEXT_t RC;

void nr_schedule_pucch(int Mod_idP,
                       int UE_id,
                       int nr_ulmix_slots,
                       frame_t frameP,
                       sub_frame_t slotP) {

  uint16_t O_csi, O_ack, O_uci;
  uint8_t O_sr = 0; // no SR in PUCCH implemented for now
  NR_ServingCellConfigCommon_t *scc = RC.nrmac[Mod_idP]->common_channels->ServingCellConfigCommon;
  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  AssertFatal(UE_info->active[UE_id],"Cannot find UE_id %d is not active\n",UE_id);

  NR_CellGroupConfig_t *secondaryCellGroup = UE_info->secondaryCellGroup[UE_id];
  int bwp_id=1;
  NR_BWP_Uplink_t *ubwp=secondaryCellGroup->spCellConfig->spCellConfigDedicated->uplinkConfig->uplinkBWP_ToAddModList->list.array[bwp_id-1];
  nfapi_nr_ul_tti_request_t *UL_tti_req = &RC.nrmac[Mod_idP]->UL_tti_req[0];

  NR_sched_pucch *curr_pucch;

  for (int k=0; k<nr_ulmix_slots; k++) {
    for (int l=0; l<2; l++) {
      curr_pucch = &UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][l];
      O_ack = curr_pucch->dai_c;
      O_csi = curr_pucch->csi_bits;
      O_uci = O_ack + O_csi + O_sr;
      if ((O_uci>0) && (frameP == curr_pucch->frame) && (slotP == curr_pucch->ul_slot)) {
        UL_tti_req->SFN = curr_pucch->frame;
        UL_tti_req->Slot = curr_pucch->ul_slot;
        UL_tti_req->pdus_list[UL_tti_req->n_pdus].pdu_type = NFAPI_NR_UL_CONFIG_PUCCH_PDU_TYPE;
        UL_tti_req->pdus_list[UL_tti_req->n_pdus].pdu_size = sizeof(nfapi_nr_pucch_pdu_t);
        nfapi_nr_pucch_pdu_t  *pucch_pdu = &UL_tti_req->pdus_list[UL_tti_req->n_pdus].pucch_pdu;
        memset(pucch_pdu,0,sizeof(nfapi_nr_pucch_pdu_t));
        UL_tti_req->n_pdus+=1;

        LOG_D(MAC,"Scheduling pucch reception for frame %d slot %d with (%d, %d, %d) (SR ACK, CSI) bits\n",
              frameP,slotP,O_sr,O_ack,curr_pucch->csi_bits);

        nr_configure_pucch(pucch_pdu,
                           scc,
                           ubwp,
                           UE_info->rnti[UE_id],
                           curr_pucch->resource_indicator,
                           O_csi,
                           O_ack,
                           O_sr);

        memset((void *) &UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][l],
               0,
               sizeof(NR_sched_pucch));
      }
    }
  }
}


//!TODO : same function can be written to handle csi_resources
void compute_csi_bitlen (NR_CellGroupConfig_t *secondaryCellGroup, NR_UE_info_t *UE_info, int UE_id) {
  uint8_t csi_report_id = 0;
  uint8_t csi_resourceidx =0;
  uint8_t csi_ssb_idx =0;

  NR_CSI_MeasConfig_t *csi_MeasConfig = secondaryCellGroup->spCellConfig->spCellConfigDedicated->csi_MeasConfig->choice.setup;
  NR_CSI_ResourceConfigId_t csi_ResourceConfigId;
  for (csi_report_id=0; csi_report_id < csi_MeasConfig->csi_ReportConfigToAddModList->list.count; csi_report_id++){
    csi_ResourceConfigId=csi_MeasConfig->csi_ReportConfigToAddModList->list.array[csi_report_id]->resourcesForChannelMeasurement;
    UE_info->csi_report_template[UE_id][csi_report_id].reportQuantity_type = csi_MeasConfig->csi_ReportConfigToAddModList->list.array[csi_report_id]->reportQuantity.present;

    for ( csi_resourceidx = 0; csi_resourceidx < csi_MeasConfig->csi_ResourceConfigToAddModList->list.count; csi_resourceidx++) {
      if ( csi_MeasConfig->csi_ResourceConfigToAddModList->list.array[csi_resourceidx]->csi_ResourceConfigId != csi_ResourceConfigId)
	continue;
      else {
      //Finding the CSI_RS or SSB Resources
        UE_info->csi_report_template[UE_id][csi_report_id].CSI_Resource_type= csi_MeasConfig->csi_ResourceConfigToAddModList->list.array[csi_resourceidx]->csi_RS_ResourceSetList.present;
        if (NR_CSI_ResourceConfig__csi_RS_ResourceSetList_PR_nzp_CSI_RS_SSB ==UE_info->csi_report_template[UE_id][csi_report_id].CSI_Resource_type){
          struct NR_CSI_ResourceConfig__csi_RS_ResourceSetList__nzp_CSI_RS_SSB * nzp_CSI_RS_SSB = csi_MeasConfig->csi_ResourceConfigToAddModList->list.array[csi_resourceidx]->csi_RS_ResourceSetList.choice.nzp_CSI_RS_SSB;

          UE_info->csi_report_template[UE_id][csi_report_id].nb_of_nzp_csi_report = nzp_CSI_RS_SSB->nzp_CSI_RS_ResourceSetList!=NULL ? nzp_CSI_RS_SSB->nzp_CSI_RS_ResourceSetList->list.count:0;
          UE_info->csi_report_template[UE_id][csi_report_id].nb_of_csi_ssb_report = nzp_CSI_RS_SSB->csi_SSB_ResourceSetList!=NULL ? nzp_CSI_RS_SSB->csi_SSB_ResourceSetList->list.count:0;
        }

        if (0 != UE_info->csi_report_template[UE_id][csi_report_id].nb_of_csi_ssb_report){
	  uint8_t nb_ssb_resources =0;
          for ( csi_ssb_idx = 0; csi_ssb_idx < csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.count; csi_ssb_idx++) {
            if (csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.array[csi_ssb_idx]->csi_SSB_ResourceSetId ==
                *(csi_MeasConfig->csi_ResourceConfigToAddModList->list.array[csi_resourceidx]->csi_RS_ResourceSetList.choice.nzp_CSI_RS_SSB->csi_SSB_ResourceSetList->list.array[0])) { 
              ///We can configure only one SSB resource set from spec 38.331 IE CSI-ResourceConfig
              if (NR_CSI_ReportConfig__groupBasedBeamReporting_PR_disabled ==
                csi_MeasConfig->csi_ReportConfigToAddModList->list.array[csi_report_id]->groupBasedBeamReporting.present ) {
	        if (NULL != csi_MeasConfig->csi_ReportConfigToAddModList->list.array[csi_report_id]->groupBasedBeamReporting.choice.disabled->nrofReportedRS)
                  UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].nb_ssbri_cri = *(csi_MeasConfig->csi_ReportConfigToAddModList->list.array[csi_report_id]->groupBasedBeamReporting.choice.disabled->nrofReportedRS)+1;
                else
                  /*! From Spec 38.331
                  * nrofReportedRS
                  * The number (N) of measured RS resources to be reported per report setting in a non-group-based report. N <= N_max, where N_max is either 2 or 4 depending on UE
                  * capability. FFS: The signaling mechanism for the gNB to select a subset of N beams for the UE to measure and report.
                  * When the field is absent the UE applies the value 1
                  */
                  UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].nb_ssbri_cri= 1;
              } else
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].nb_ssbri_cri= 2;

              nb_ssb_resources=  csi_MeasConfig->csi_SSB_ResourceSetToAddModList->list.array[csi_ssb_idx]->csi_SSB_ResourceList.list.count;
              if (nb_ssb_resources){
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].cri_ssbri_bitlen =ceil(log2 (nb_ssb_resources));
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].rsrp_bitlen = 7; //From spec 38.212 Table 6.3.1.1.2-6: CRI, SSBRI, and RSRP 
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].diff_rsrp_bitlen =4; //From spec 38.212 Table 6.3.1.1.2-6: CRI, SSBRI, and RSRP
              }
              else{
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].cri_ssbri_bitlen =0;
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].rsrp_bitlen = 0;
                UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].diff_rsrp_bitlen =0;
              }

              LOG_I (MAC, "UCI: CSI_bit len : ssbri %d, rsrp: %d, diff_rsrp: %d",
                     UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].cri_ssbri_bitlen,
                     UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].rsrp_bitlen,
                     UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0].diff_rsrp_bitlen);
              break ;
            }
          }
        }
        if (0 != UE_info->csi_report_template[UE_id][csi_report_id].nb_of_nzp_csi_report)
          AssertFatal(1==0,"Currently configuring only SSB beamreporting.");
        break;
      }
    }
  }
}


uint16_t nr_get_csi_bitlen(int Mod_idP,
                           int UE_id,
                           uint8_t csi_report_id) {

  uint16_t csi_bitlen =0;
  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  CRI_SSBRI_RSRP_bitlen_t * CSI_report_bitlen = NULL;

  CSI_report_bitlen = &(UE_info->csi_report_template[UE_id][csi_report_id].CSI_report_bitlen[0]);
  csi_bitlen = ((CSI_report_bitlen->cri_ssbri_bitlen * CSI_report_bitlen->nb_ssbri_cri) +
               CSI_report_bitlen->rsrp_bitlen +(CSI_report_bitlen->diff_rsrp_bitlen *
               (CSI_report_bitlen->nb_ssbri_cri -1 )) *UE_info->csi_report_template[UE_id][csi_report_id].nb_of_csi_ssb_report);

  return csi_bitlen;
}


void nr_csi_meas_reporting(int Mod_idP,
                           int UE_id,
                           frame_t frame,
                           sub_frame_t slot,
                           int slots_per_tdd,
                           int ul_slots,
                           int n_slots_frame) {

  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  NR_sched_pucch *curr_pucch;
  NR_PUCCH_ResourceSet_t *pucchresset;
  NR_CSI_ReportConfig_t *csirep;
  NR_CellGroupConfig_t *secondaryCellGroup = UE_info->secondaryCellGroup[UE_id];
  NR_CSI_MeasConfig_t *csi_measconfig = secondaryCellGroup->spCellConfig->spCellConfigDedicated->csi_MeasConfig->choice.setup;
  NR_BWP_Uplink_t *ubwp=secondaryCellGroup->spCellConfig->spCellConfigDedicated->uplinkConfig->uplinkBWP_ToAddModList->list.array[0];
  NR_PUCCH_Config_t *pucch_Config = ubwp->bwp_Dedicated->pucch_Config->choice.setup;

  AssertFatal(csi_measconfig->csi_ReportConfigToAddModList->list.count>0,"NO CSI report configuration available");

  for (int csi_report_id = 0; csi_report_id < csi_measconfig->csi_ReportConfigToAddModList->list.count; csi_report_id++){

    csirep = csi_measconfig->csi_ReportConfigToAddModList->list.array[csi_report_id];

    AssertFatal(csirep->reportConfigType.choice.periodic!=NULL,"Only periodic CSI reporting is implemented currently");
    int period, offset, sched_slot;
    csi_period_offset(csirep,&period,&offset);
    sched_slot = (period+offset)%n_slots_frame;
    // prepare to schedule csi measurement reception according to 5.2.1.4 in 38.214
    // preparation is done in first slot of tdd period
    if ( (frame%(period/n_slots_frame)==(offset/n_slots_frame)) && (slot==((sched_slot/slots_per_tdd)*slots_per_tdd))) {

      // we are scheduling pucch for csi in the first pucch occasion (this comes before ack/nack)
      curr_pucch = &UE_info->UE_sched_ctrl[UE_id].sched_pucch[sched_slot-slots_per_tdd+ul_slots][0];

      NR_PUCCH_CSI_Resource_t *pucchcsires = csirep->reportConfigType.choice.periodic->pucch_CSI_ResourceList.list.array[0];

      int found = -1;
      pucchresset = pucch_Config->resourceSetToAddModList->list.array[1]; // set with formats >1
      int n_list = pucchresset->resourceList.list.count;
      for (int i=0; i<n_list; i++) {
        if (*pucchresset->resourceList.list.array[i] == pucchcsires->pucch_Resource)
          found = i;
      }
      AssertFatal(found>-1,"CSI resource not found among PUCCH resources");

      curr_pucch->resource_indicator = found;

      n_list = pucch_Config->resourceToAddModList->list.count;

      // going through the list of PUCCH resources to find the one indexed by resource_id
      for (int i=0; i<n_list; i++) {
        NR_PUCCH_Resource_t *pucchres = pucch_Config->resourceToAddModList->list.array[i];
        if (pucchres->pucch_ResourceId == *pucchresset->resourceList.list.array[found]) {
          switch(pucchres->format.present){
            case NR_PUCCH_Resource__format_PR_format2:
              if (pucch_Config->format2->choice.setup->simultaneousHARQ_ACK_CSI == NULL)
                curr_pucch->simultaneous_harqcsi = false;
              else
                curr_pucch->simultaneous_harqcsi = true;
              break;
            case NR_PUCCH_Resource__format_PR_format3:
              if (pucch_Config->format3->choice.setup->simultaneousHARQ_ACK_CSI == NULL)
                curr_pucch->simultaneous_harqcsi = false;
              else
                curr_pucch->simultaneous_harqcsi = true;
              break;
            case NR_PUCCH_Resource__format_PR_format4:
              if (pucch_Config->format4->choice.setup->simultaneousHARQ_ACK_CSI == NULL)
                curr_pucch->simultaneous_harqcsi = false;
              else
                curr_pucch->simultaneous_harqcsi = true;
              break;
          default:
            AssertFatal(1==0,"Invalid PUCCH format type");
          }
        }
      }
      curr_pucch->csi_bits += nr_get_csi_bitlen(Mod_idP,UE_id,csi_report_id); // TODO function to compute CSI meas report bit size
      curr_pucch->frame = frame;
      curr_pucch->ul_slot = sched_slot;
    }
  }
}


void nr_rx_acknack(nfapi_nr_uci_pusch_pdu_t *uci_pusch,
                   nfapi_nr_uci_pucch_pdu_format_0_1_t *uci_01,
                   nfapi_nr_uci_pucch_pdu_format_2_3_4_t *uci_234,
                   NR_UL_IND_t *UL_info, NR_UE_sched_ctrl_t *sched_ctrl, NR_mac_stats_t *stats) {

  // TODO
  int max_harq_rounds = 4; // TODO define macro

  if (uci_01 != NULL) {
    // handle harq
    int harq_idx_s = 0;

    // iterate over received harq bits
    for (int harq_bit = 0; harq_bit < uci_01->harq->num_harq; harq_bit++) {
      // search for the right harq process
      for (int harq_idx = harq_idx_s; harq_idx < NR_MAX_NB_HARQ_PROCESSES; harq_idx++) {
        // if the gNB received ack with a good confidence
        if ((UL_info->slot-1) == sched_ctrl->harq_processes[harq_idx].feedback_slot) {
          sched_ctrl->harq_processes[harq_idx].feedback_slot = -1;
          if ((uci_01->harq->harq_list[harq_bit].harq_value == 1) &&
              (uci_01->harq->harq_confidence_level == 0)) {
            // toggle NDI and reset round
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
          }
          else
            sched_ctrl->harq_processes[harq_idx].round++;
          sched_ctrl->harq_processes[harq_idx].is_waiting = 0;
          harq_idx_s = harq_idx + 1;
          // if the max harq rounds was reached
          if (sched_ctrl->harq_processes[harq_idx].round == max_harq_rounds) {
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
            stats->dlsch_errors++;
          }
          break;
        }
        // if feedback slot processing is aborted
        else if (sched_ctrl->harq_processes[harq_idx].feedback_slot != -1
                 && (UL_info->slot-1) > sched_ctrl->harq_processes[harq_idx].feedback_slot
                 && sched_ctrl->harq_processes[harq_idx].is_waiting) {
          sched_ctrl->harq_processes[harq_idx].feedback_slot = -1;
          sched_ctrl->harq_processes[harq_idx].round++;
          if (sched_ctrl->harq_processes[harq_idx].round == max_harq_rounds) {
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
          }
          sched_ctrl->harq_processes[harq_idx].is_waiting = 0;
        }
      }
    }
  }


  if (uci_234 != NULL) {
    int harq_idx_s = 0;
    int acknack;

    // iterate over received harq bits
    for (int harq_bit = 0; harq_bit < uci_234->harq.harq_bit_len; harq_bit++) {
      acknack = ((uci_234->harq.harq_payload[harq_bit>>3])>>harq_bit)&0x01;
      for (int harq_idx = harq_idx_s; harq_idx < NR_MAX_NB_HARQ_PROCESSES-1; harq_idx++) {
        // if the gNB received ack with a good confidence or if the max harq rounds was reached
        if ((UL_info->slot-1) == sched_ctrl->harq_processes[harq_idx].feedback_slot) {
          // TODO add some confidence level for when there is no CRC
          sched_ctrl->harq_processes[harq_idx].feedback_slot = -1;
          if ((uci_234->harq.harq_crc != 1) && acknack) {
            // toggle NDI and reset round
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
          }
          else
            sched_ctrl->harq_processes[harq_idx].round++;
          sched_ctrl->harq_processes[harq_idx].is_waiting = 0;
          harq_idx_s = harq_idx + 1;
          // if the max harq rounds was reached
          if (sched_ctrl->harq_processes[harq_idx].round == max_harq_rounds) {
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
            stats->dlsch_errors++;
          }
          break;
        }
        // if feedback slot processing is aborted
        else if (sched_ctrl->harq_processes[harq_idx].feedback_slot != -1
                 && (UL_info->slot-1) > sched_ctrl->harq_processes[harq_idx].feedback_slot
                 && sched_ctrl->harq_processes[harq_idx].is_waiting) {
          sched_ctrl->harq_processes[harq_idx].feedback_slot = -1;
          sched_ctrl->harq_processes[harq_idx].round++;
          if (sched_ctrl->harq_processes[harq_idx].round == max_harq_rounds) {
            sched_ctrl->harq_processes[harq_idx].ndi ^= 1;
            sched_ctrl->harq_processes[harq_idx].round = 0;
          }
          sched_ctrl->harq_processes[harq_idx].is_waiting = 0;
        }
      }
    }
  }
}


// function to update pucch scheduling parameters in UE list when a USS DL is scheduled
void nr_acknack_scheduling(int Mod_idP,
                           int UE_id,
                           frame_t frameP,
                           sub_frame_t slotP,
                           int slots_per_tdd,
                           int *pucch_id,
                           int *pucch_occ) {

  NR_ServingCellConfigCommon_t *scc = RC.nrmac[Mod_idP]->common_channels->ServingCellConfigCommon;
  NR_UE_info_t *UE_info = &RC.nrmac[Mod_idP]->UE_info;
  NR_sched_pucch *curr_pucch;
  int max_acknacks,pucch_res,first_ul_slot_tdd,k,i,l;
  uint8_t pdsch_to_harq_feedback[8];
  int found = 0;
  int nr_ulmix_slots = scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofUplinkSlots;
  if (scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofUplinkSymbols!=0)
    nr_ulmix_slots++;

  bool csi_pres=false;
  for (k=0; k<nr_ulmix_slots; k++) {
    if(UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][0].csi_bits>0)
      csi_pres=true;
  }

  // As a preference always schedule ack nacks in PUCCH0 (max 2 per slots)
  // Unless there is CSI meas reporting scheduled in the period to avoid conflicts in the same slot
  if (csi_pres)
    max_acknacks=10;
  else
    max_acknacks=2;

  // this is hardcoded for now as ue specific
  NR_SearchSpace__searchSpaceType_PR ss_type = NR_SearchSpace__searchSpaceType_PR_ue_Specific;
  get_pdsch_to_harq_feedback(Mod_idP,UE_id,ss_type,pdsch_to_harq_feedback);

  // for each possible ul or mixed slot
  for (k=0; k<nr_ulmix_slots; k++) {
    for (l=0; l<1; l++) { // scheduling 2 PUCCH in a single slot does not work with the phone, currently
      curr_pucch = &UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][l];
      //if it is possible to schedule acknack in current pucch (no exclusive csi pucch)
      if ((curr_pucch->csi_bits == 0) || (curr_pucch->simultaneous_harqcsi==true)) {
        // if there is free room in current pucch structure
        if (curr_pucch->dai_c<max_acknacks) {
          pucch_res = get_pucch_resource(UE_info,UE_id,k,l);
          if (pucch_res>-1){
            curr_pucch->resource_indicator = pucch_res;
            curr_pucch->frame = frameP;
            // first pucch occasion in first UL or MIXED slot
            first_ul_slot_tdd = scc->tdd_UL_DL_ConfigurationCommon->pattern1.nrofDownlinkSlots;
            i = 0;
            while (i<8 && found == 0)  {  // look if timing indicator is among allowed values
              if (pdsch_to_harq_feedback[i]==(first_ul_slot_tdd+k)-(slotP % slots_per_tdd))
                found = 1;
              if (found == 0) i++;
            }
            if (found == 1) {
              // computing slot in which pucch is scheduled
              curr_pucch->dai_c++;
              curr_pucch->ul_slot = first_ul_slot_tdd + k + (slotP - (slotP % slots_per_tdd));
              curr_pucch->timing_indicator = i; // index in the list of timing indicators
              *pucch_id = k;
              *pucch_occ = l;
              return;
            }
          }
        }
      }
    }
  }
  AssertFatal(1==0,"No Uplink slot available in accordance to allowed timing indicator\n");
}


void csi_period_offset(NR_CSI_ReportConfig_t *csirep,
                       int *period, int *offset) {

    NR_CSI_ReportPeriodicityAndOffset_PR p_and_o = csirep->reportConfigType.choice.periodic->reportSlotConfig.present;

    switch(p_and_o){
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots4:
        *period = 4;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots4;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots5:
        *period = 5;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots5;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots8:
        *period = 8;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots8;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots10:
        *period = 10;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots10;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots16:
        *period = 16;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots16;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots20:
        *period = 20;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots20;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots40:
        *period = 40;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots40;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots80:
        *period = 80;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots80;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots160:
        *period = 160;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots160;
        break;
      case NR_CSI_ReportPeriodicityAndOffset_PR_slots320:
        *period = 320;
        *offset = csirep->reportConfigType.choice.periodic->reportSlotConfig.choice.slots320;
        break;
    default:
      AssertFatal(1==0,"No periodicity and offset resource found in CSI report");
    }
}


int get_pucch_resource(NR_UE_info_t *UE_info,int UE_id,int k,int l) {

  // to be updated later, for now simple implementation
  // use the second allocation just in case there is csi in the first
  // in that case use second resource (for a different symbol) see 9.2 in 38.213
  if (l==1) {
    if (UE_info->UE_sched_ctrl[UE_id].sched_pucch[k][0].csi_bits==0)
      return -1;
    else
      return 1;
  }
  else
    return 0;

}


uint16_t compute_pucch_prb_size(uint8_t format,
                                uint8_t nr_prbs,
                                uint16_t O_tot,
                                uint16_t O_csi,
                                NR_PUCCH_MaxCodeRate_t *maxCodeRate,
                                uint8_t Qm,
                                uint8_t n_symb,
                                uint8_t n_re_ctrl) {

  uint16_t O_crc;

  if (O_tot<12)
    O_crc = 0;
  else{
    if (O_tot<20)
      O_crc = 6;
    else {
      if (O_tot<360)
        O_crc = 11;
      else
        AssertFatal(1==0,"Case for segmented PUCCH not yet implemented");
    }
  }

  int rtimes100;
  switch(*maxCodeRate){
    case NR_PUCCH_MaxCodeRate_zeroDot08 :
      rtimes100 = 8;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot15 :
      rtimes100 = 15;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot25 :
      rtimes100 = 25;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot35 :
      rtimes100 = 35;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot45 :
      rtimes100 = 45;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot60 :
      rtimes100 = 60;
      break;
    case NR_PUCCH_MaxCodeRate_zeroDot80 :
      rtimes100 = 80;
      break;
  default :
    AssertFatal(1==0,"Invalid MaxCodeRate");
  }

  float r = (float)rtimes100/100;

  if (O_csi == O_tot) {
    if ((O_tot+O_csi)>(nr_prbs*n_re_ctrl*n_symb*Qm*r))
      AssertFatal(1==0,"MaxCodeRate %.2f can't support %d UCI bits and %d CRC bits with %d PRBs",
                  r,O_tot,O_crc,nr_prbs);
    else
      return nr_prbs;
  }

  if (format==2){
    // TODO fix this for multiple CSI reports
    for (int i=1; i<=nr_prbs; i++){
      if((O_tot+O_crc)<=(i*n_symb*Qm*n_re_ctrl*r) &&
         (O_tot+O_crc)>((i-1)*n_symb*Qm*n_re_ctrl*r))
        return i;
    }
    AssertFatal(1==0,"MaxCodeRate %.2f can't support %d UCI bits and %d CRC bits with at most %d PRBs",
                r,O_tot,O_crc,nr_prbs);
  }
  else{
    AssertFatal(1==0,"Not yet implemented");
  }

}
