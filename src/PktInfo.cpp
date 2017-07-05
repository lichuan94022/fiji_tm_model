/*********************************************************************
 * ZYZYX CONFIDENTIAL
 * __________________
 * 
 * 2017 Zyzyx Incorporated 
 * All Rights Reserved.
 * 
 * NOTICE:  All information contained herein is, and remains
 * the property of Zyzyx Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Zyzyx Incorporated and its suppliers 
 * and may be covered by U.S. and Foreign Patents, patents in process, 
 * and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Zyzyx Incorporated.
 *
 * File: $File$
 * Revision: $Revision$
 * Author: $Author$
 * Creation Date: $Date$
 *
 * Description:
 *
*********************************************************************/

#include "PktInfo.hpp"

///////////////////////////////////////////////////////////////////
// Function: reset
///////////////////////////////////////////////////////////////////
void PktInfo::reset(int new_pkt_id) {
  vlan = 0;
  qnum = 0;
  num_pkt_bytes = 0;
  pkt_id = new_pkt_id;

  wait_time_in_clks = 0;
  send_time_in_clks = 0;
  m_shaper_wait = SHAPER_NO_WAIT;

  num_bytes_to_gather = 0;
  num_bytes_to_transmit = 0;
  gathered = 0;
  transmitted = 0;
}

///////////////////////////////////////////////////////////////////
// Function: update_gather_status
///////////////////////////////////////////////////////////////////
void PktInfo::update_gather_status(double gather_bytes_per_clk){
  double num_bytes_to_subtract;

  if(!is_gathered()) {
    if(num_bytes_to_gather == 0) { // Gather has not commenced
      num_bytes_to_gather = num_pkt_bytes;
    }
    num_bytes_to_subtract = gather_bytes_per_clk;
    if(gather_bytes_per_clk > num_bytes_to_gather) {
      num_bytes_to_subtract = num_bytes_to_gather;
    }
    num_bytes_to_gather -= num_bytes_to_subtract;
    logger::dv_debug(DV_DEBUG1, "update gather: gathering pkt_id %0d pkt_bytes:%0d num_bytes_to_gather:%2f num_bytes_to_subtract:%2f\n", pkt_id, num_pkt_bytes, num_bytes_to_gather, num_bytes_to_subtract);
    logger::dv_debug(DV_DEBUG1, "update gather: gather_bytes_per_clk:%2f num_bytes_to_subtract:%2f\n", gather_bytes_per_clk, num_bytes_to_subtract);

    if(num_bytes_to_gather == 0) { // Gather is done
      gathered = 1;
      logger::dv_debug(DV_DEBUG1, "update gather: gather done - pkt_id %0d pkt_bytes:%0d num_bytes_to_gather:%2f\n", pkt_id, num_pkt_bytes, num_bytes_to_gather);
    }
  }
}

///////////////////////////////////////////////////////////////////
// Function: decr_num_bytes_to_transmit
///////////////////////////////////////////////////////////////////
void PktInfo::decr_num_bytes_to_transmit(int num_bytes, double &num_bytes_transmitted) {
  
  if(!transmitted && (num_bytes_to_transmit == 0)) { // Transmit has not commenced
    num_bytes_to_transmit = num_pkt_bytes;
  }

  double num_bytes_to_subtract = num_bytes;
  if(num_bytes > num_bytes_to_transmit) {
    num_bytes_to_subtract = num_bytes_to_transmit;
  }
  num_bytes_to_transmit -= num_bytes_to_subtract;
  num_bytes_transmitted = num_bytes_to_subtract; 
  logger::dv_debug(DV_DEBUG1, "decr_num_bytes_to_transmit: transmitting pkt_id %0d pkt_bytes:%0d num_bytes_to_transmit:%2f num_bytes_subtracted:%2f\n", pkt_id, num_pkt_bytes, num_bytes_to_transmit, num_bytes_to_subtract);

  if(num_bytes_to_transmit == 0) { // Transmit is done
    transmitted = 1;
    logger::dv_debug(DV_DEBUG1, "decr_num_bytes_to_transmit: transmit done - pkt_id %0d pkt_bytes:%0d\n", pkt_id, num_pkt_bytes);
  }
}

bool PktInfo::is_gathered() {
  if(gathered == 1) {
    return 1;
  } 
  return 0;
}

bool PktInfo::is_transmitted() {
  if(transmitted == 1) {
    return 1;
  } 
  return 0;
}
