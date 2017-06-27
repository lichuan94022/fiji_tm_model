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
#ifndef __PKT_INFO_HPP__
#define __PKT_INFO_HPP__

#include "logger.hpp"

typedef enum {SHAPER_NO_WAIT, SHAPER_VLAN_WAIT, SHAPER_QUEUE_WAIT, SHAPER_VLAN_QUEUE_WAIT} pkt_wait_e;


//////////////////////////////////
// Class: PktInfo
// 
// Description:
//    Pkt described by vlan, queue number, number of bytes, and packet id.
//    Packet status is also kept in this class:
//      wait_time - if shaper delays packet, it also gives an amount of time
//                  to wait before pkt can be sent.
//      send_time - wait time + current time 
//      m_shaper_wait - records which shapers delayed the packet
//      num_bytes_to_gather - once a packet is sent to the output, the
//                            bytes of the pkt are gathered at a configurable rate
//      gathered - set to 1 when all bytes have been gathered
//      num_bytes_transmit - once all bytes of a pkt has been gathered, the packet 
//                           is "transmitted" at a configurable rate 
//      transmitted - set to 1 when all bytes have been transmitted
//////////////////////////////////
class PktInfo {

public:

  // Pkt Variables
  int vlan;
  int qnum;
  int num_pkt_bytes;
  int pkt_id;

  // Pkt Status Variables
  int wait_time_in_clks;
  int send_time_in_clks;
  pkt_wait_e m_shaper_wait;

  double num_bytes_to_gather;
  double num_bytes_to_transmit;
  int gathered;
  int transmitted;

  ////////////////////////////////////////////////////////////////////////
  // Function: Reset
  // 
  // Description: 
  //   Set status to initial state
  ////////////////////////////////////////////////////////////////////////
  void reset(int new_pkt_id);

  ////////////////////////////////////////////////////////////////////////
  // Function: update_gather_status
  // 
  // Description: 
  //   For each packet called, subtract some configurable amount of bytes
  //   to represent gathering of packet data for output.  When all bytes
  //   have been gathered, set "gathered" status bit.
  ////////////////////////////////////////////////////////////////////////
  void update_gather_status(double gather_bytes_per_clk);

  ////////////////////////////////////////////////////////////////////////
  // Function: decr_num_bytes_to_transmit
  // 
  // Description: 
  //   For each packet called, subtract some configurable amount of bytes
  //   to represent transmit of packet data to output.  When all bytes
  //   have been transmitted, set "transmitted" status bit.
  ////////////////////////////////////////////////////////////////////////
  void decr_num_bytes_to_transmit(int num_bytes, double &total_output_bytes);

  ////////////////////////////////////////////////////////////////////////
  // Function: is_gathered
  //
  // Description:
  //   Set to 1 when gather has completed
  ////////////////////////////////////////////////////////////////////////
  bool is_gathered();

  ////////////////////////////////////////////////////////////////////////
  // Function: is_transmitted
  //
  // Description:
  //   Set to 1 when transmit has completed
  ////////////////////////////////////////////////////////////////////////
  bool is_transmitted();

  ////////////////////////////////////////////////////////////////////////
  // Function: operator<
  //  
  // Description: used by the pkt_wait_head (priority queue) to determine
  //   heap structure 
  ////////////////////////////////////////////////////////////////////////
  bool operator< (const PktInfo& pkt) const {
    return (send_time_in_clks > pkt.send_time_in_clks);
  }

  ////////////////////////////////////////////////////////////////////////
  // Function: print
  ////////////////////////////////////////////////////////////////////////
  void print() const {
    logger::dv_debug(DV_INFO, "PktInfo: vlan=%d, qnum=%d, num_bytes=%d, wait_time_in_clks=%0d, send_time_in_clks=%d\n",
	                      vlan, qnum, num_pkt_bytes, wait_time_in_clks,  send_time_in_clks);
  }

};

#endif
