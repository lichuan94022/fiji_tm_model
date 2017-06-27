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

#include "OutputHandler.hpp"

///////////////////////////////////////////////////////////////////////
// Function: OutputHandler Destructor
///////////////////////////////////////////////////////////////////////
OutputHandler::~OutputHandler() {
  delete [] output_fifo_per_queue;
}

///////////////////////////////////////////////////////////////////////
// Function: init
///////////////////////////////////////////////////////////////////////
void OutputHandler::init(int num_vlans, int* num_queues, double new_clk_period,
     double oport_rate, int num_ofifo_entries, int gather_bytes_per_clk) {
  cur_oport_rate = 0;
  full_time = 0;
  total_output_time =0;
  total_output_bytes = 0;

  clk_period = new_clk_period;

  output_fifo_per_queue = new std::deque<PktInfo>*[num_vlans];

  for(int vnum=0; vnum<num_vlans; vnum++) {
    output_fifo_per_queue[vnum] = new std::deque<PktInfo>[num_queues[vnum]];
  }

  knob_gather_bytes_per_clk = gather_bytes_per_clk;
  knob_oport_rate = oport_rate;
  knob_num_ofifo_entries = num_ofifo_entries;

  obytes_per_clk = knob_oport_rate * clk_period/8;

  logger::dv_debug(DV_INFO, "Output Handler init: oport_rate:%2f\n", knob_oport_rate);
  for(int vnum=0; vnum<num_vlans; vnum++) {
    logger::dv_debug(DV_INFO, "Output Handler init: vlans:%0d num_queues[0]:%0d\n", vnum, num_queues[vnum]);
  }
}

///////////////////////////////////////////////////////////////////////
// Function: add_pkt
///////////////////////////////////////////////////////////////////////
void OutputHandler::add_pkt(PktInfo &new_pkt) {
  if(output_fifo.size() <= knob_num_ofifo_entries) {
    logger::dv_debug(DV_DEBUG1, "output_handler add_pkt: pkt_id:%0d vlan:%0d qnum:%0d\n", new_pkt.pkt_id, new_pkt.vlan, new_pkt.qnum);
    output_fifo.push_back(new_pkt); 
    output_fifo_per_queue[new_pkt.vlan][new_pkt.qnum].push_back(new_pkt);
  }
  else {
    logger::dv_debug(DV_INFO, "output_handler: ERROR: Pushing onto full output fifo\n");
    logger::dv_debug(DV_INFO, "output_handler: ERROR: output fifo size:%0d knob_num_ofifo_entries:%0d\n", output_fifo.size(), knob_num_ofifo_entries);
  }
}

///////////////////////////////////////////////////////////////////////
// Function: update_output_handler
///////////////////////////////////////////////////////////////////////
void OutputHandler::update_output_handler(double total_time) {

    PktInfo act_out_pkt;
    PktInfo exp_out_pkt;

    // Update transmit status if rate allows, do not adjust output fifo contents
    if(cur_oport_rate < knob_oport_rate) {
      if(output_fifo[0].is_gathered()) {
        output_fifo[0].decr_num_bytes_to_transmit(obytes_per_clk, total_output_bytes);
        logger::dv_debug(DV_DEBUG1, "update_output_handler: decr_num_bytes_to_transmit pkt_id: %0d pkt_size:%0d, num_bytes_to_transmit:%2f obytes_per_clk:%2f\n", output_fifo[0].pkt_id, output_fifo[0].num_pkt_bytes, output_fifo[0].num_bytes_to_transmit, obytes_per_clk);
      }
    }

    // Update gather state for all in fifo
    for(int i=0; i<output_fifo.size();i++) {
      output_fifo[i].update_gather_status(knob_gather_bytes_per_clk);
    }

    // Pop output fifo if pkt has been completely transmitted
    if(output_fifo.size() && (output_fifo[0].is_transmitted())) {
       logger::dv_debug(DV_DEBUG1, "update_output_handler: pop output fifo pkt_id: %0d pkt_size:%0d\n", output_fifo[0].pkt_id, output_fifo[0].num_pkt_bytes);
      act_out_pkt = output_fifo[0];
      output_fifo.pop_front();
      if(output_fifo_per_queue[act_out_pkt.vlan][act_out_pkt.qnum].size()) {
        exp_out_pkt = output_fifo_per_queue[act_out_pkt.vlan][act_out_pkt.qnum][0];
        output_fifo_per_queue[act_out_pkt.vlan][act_out_pkt.qnum].pop_front();
        if(act_out_pkt.pkt_id != exp_out_pkt.pkt_id) {
          logger::dv_debug(DV_INFO, "ERROR: output pkt_id(%0d) != exp_pkt_id(%-d): vlan:%0d qnum:%0d\n", act_out_pkt.pkt_id, exp_out_pkt.pkt_id, act_out_pkt.vlan, act_out_pkt.qnum);
        }
      }
      else {
          logger::dv_debug(DV_INFO, "ERROR: output fifo has no corresponding per fifo entry: vlan:%0d qnum:%0d\n");
      }

      logger::dv_debug(DV_DEBUG1, "Output FIFO\n");
      print_fifo(total_time);
      logger::dv_debug(DV_DEBUG1, "Output FIFO for vlan:%0d queue:%0d\n", act_out_pkt.vlan, act_out_pkt.qnum);
      print_fifo_per_queue(act_out_pkt.vlan, act_out_pkt.qnum, total_time);
    } // end if(output_fifo.size() && (output_fifo[0].is_transmitted()))

    // Update output port rate
    cur_oport_rate = (total_output_bytes*8) / total_time;
    logger::dv_debug(DV_DEBUG1, "update_output_handler: cur_oport_rate:%2f total_output_bytes:%2f total_time:%2f knob_oport_rate:%2f\n", cur_oport_rate, total_output_bytes, total_time, knob_oport_rate);
    
    total_output_time++; 

    if(is_full()) {
      full_time++;
    } 
}

///////////////////////////////////////////////////////////////////////
// Function: is_full
///////////////////////////////////////////////////////////////////////
bool OutputHandler::is_full() {
  if(output_fifo.size() == knob_num_ofifo_entries) {
    logger::dv_debug(DV_INFO, "WARNING: Output fifo is full: %0d\n", knob_num_ofifo_entries);
    return 1;
  }
  else if (output_fifo.size() == knob_num_ofifo_entries-1) {
    logger::dv_debug(DV_INFO, "WARNING: Output fifo is almost full: %0d\n", output_fifo.size());
    return 0;
  }
  else if (output_fifo.size() > knob_num_ofifo_entries) {
    logger::dv_debug(DV_INFO, "ERROR: Output fifo > full: %0d\n", output_fifo.size());
    return 1;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////
// Function: is_empty
///////////////////////////////////////////////////////////////////////
bool OutputHandler::is_empty() {
  if(output_fifo.size() == 0) {
    logger::dv_debug(DV_INFO, "INFO: Output fifo is empty\n");
    return 1;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////
// Function: print_fifo
///////////////////////////////////////////////////////////////////////
void OutputHandler::print_fifo(double total_time) {

  logger::dv_debug(DV_DEBUG1, "Output fifo print\n");
  logger::dv_debug(DV_DEBUG1, "total_time: %2f\n", total_time);

  for(int i=0; i<output_fifo.size(); i++) {
    logger::dv_debug(DV_DEBUG1, "Output fifo[%0d]: pkt_id:%0d vlan:%0d qnum:%0d num_pkt_bytes:%0d\n", i, output_fifo[i].pkt_id, output_fifo[i].vlan, output_fifo[i].qnum, output_fifo[i].num_pkt_bytes);
    logger::dv_debug(DV_DEBUG1, "Output fifo[%0d]:   wait_time_in_clks:%2f\n", i, output_fifo[i].wait_time_in_clks);
    logger::dv_debug(DV_DEBUG1, "Output fifo[%0d]:   cur_time:%2f\n", i, total_time);
    logger::dv_debug(DV_DEBUG1, "Output fifo[%0d]:   gathered:%0d num_bytes_to_gather:%2f\n", i, output_fifo[i].gathered, output_fifo[i].num_bytes_to_gather);
    logger::dv_debug(DV_DEBUG1, "Output fifo[%0d]:   transmitted:%0d num_bytes_to_transmit:%2f\n", i, output_fifo[i].transmitted, output_fifo[i].num_bytes_to_transmit);
  }
}

///////////////////////////////////////////////////////////////////////
// Function: print_fifo_per_queue
///////////////////////////////////////////////////////////////////////
void OutputHandler::print_fifo_per_queue(int vlan, int qnum, double total_time) {
  logger::dv_debug(DV_DEBUG1, "Output fifo vlan:%0d qnum:%0d print\n", vlan, qnum);

  for(int i=0; i<output_fifo_per_queue[vlan][qnum].size(); i++) {
    logger::dv_debug(DV_DEBUG1, "Output fifo_per_queue[vlan][qnum][%0d]: pkt_id:%0d vlan:%0d qnum:%0d \n", i, output_fifo_per_queue[vlan][qnum][i].pkt_id, output_fifo_per_queue[vlan][qnum][i].vlan, output_fifo_per_queue[vlan][qnum][i].qnum);
    logger::dv_debug(DV_DEBUG1, "Output fifo_per_queue[%0d]: wait_time_in_clks:%2f cur_time:%2f\n", i, output_fifo_per_queue[vlan][qnum][i].wait_time_in_clks, total_time);
    logger::dv_debug(DV_DEBUG1, "Output fifo_per_queue[%0d]: gathered:%0d transmitted:%0d\n", i, output_fifo[i].gathered, output_fifo[i].transmitted);
  }
}
