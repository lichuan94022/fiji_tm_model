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

#include "PktGen.hpp"

///////////////////////////////////////////////////////////////////////
// PktGen Constructor
///////////////////////////////////////////////////////////////////////
PktGen::PktGen() {
  cur_vnum = 0;
  cur_pkt_id = 0; 
  total_bytes = 0;
  num_pkts_sent = 0;
  num_bytes_sent = 0;
}

///////////////////////////////////////////////////////////////////////
// PktGen Destructor
///////////////////////////////////////////////////////////////////////
PktGen::~PktGen() {
  delete [] knob_min_pkt_size;
  delete [] knob_max_pkt_size;
  delete [] knob_iqueue_rate;
  delete [] cur_iqueue_rate;
  delete [] cur_qnum;
  delete [] num_pkts_sent_per_queue;
  delete [] num_bytes_sent_per_queue;
}

///////////////////////////////////////////////////////////////////////
// Function: init 
///////////////////////////////////////////////////////////////////////
void PktGen::init(int num_vlans, int* num_queues, std::deque <PktInfo> **wait_queue, OutputHandler *ref_output_handler) {

  // Set pointers
  m_scheduler_pkt_wait_queue = wait_queue;
  output_handler = ref_output_handler;

  knob_num_vlans = num_vlans;
  knob_num_queues = num_queues;

  knob_oport_rate = sknobs_get_value((char*) "orate", 100);
  logger::dv_debug(DV_INFO, "Output Port Rate is: %2fGbps\n", knob_oport_rate);

  knob_iqueue_rate = new double*[knob_num_vlans];
  knob_min_pkt_size = new int*[knob_num_vlans];
  knob_max_pkt_size = new int*[knob_num_vlans];

  cur_qnum = new int[knob_num_vlans];
  cur_iqueue_rate = new double*[knob_num_vlans];

  num_pkts_sent_per_queue = new int*[knob_num_vlans];
  num_bytes_sent_per_queue = new int*[knob_num_vlans];

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    knob_iqueue_rate[vnum] = new double[knob_num_queues[vnum]];
    knob_min_pkt_size[vnum] = new int[knob_num_queues[vnum]];
    knob_max_pkt_size[vnum] = new int[knob_num_queues[vnum]];

    cur_iqueue_rate[vnum] = new double[knob_num_queues[vnum]];
    num_pkts_sent_per_queue[vnum] = new int[knob_num_queues[vnum]];
    num_bytes_sent_per_queue[vnum] = new int[knob_num_queues[vnum]];
  }

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    cur_qnum[vnum] = 0;

    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      cur_iqueue_rate[vnum][qnum] = 0;
      num_pkts_sent_per_queue[vnum][qnum] = 0;
      num_bytes_sent_per_queue[vnum][qnum] = 0;
    }
  }

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      std::stringstream vnum_str;
      std::stringstream qnum_str;
      vnum_str << vnum;
      qnum_str << qnum;

      std::string irate_knob = "vlan" + vnum_str.str() + "_qnum" + qnum_str.str()  + "_irate";
      knob_iqueue_rate[vnum][qnum] = sknobs_get_value((char*) irate_knob.c_str(), 100);
      logger::dv_debug(DV_INFO, "Vlan%0d Queue%0d Input Port Rate is: %2fGbps\n", vnum, qnum, knob_iqueue_rate[vnum][qnum]);
    }
  }

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      std::stringstream vnum_str;
      std::stringstream qnum_str;
      vnum_str << vnum;
      qnum_str << qnum;

      std::string minsize_knob = "vlan" + vnum_str.str() + "_qnum" + qnum_str.str()  + "_minsize";
      knob_min_pkt_size[vnum][qnum] = sknobs_get_value((char *) minsize_knob.c_str(), 64);
      logger::dv_debug(DV_INFO, "Vlan%0d Queue%0d Min Pkt Size (%s) is: %0dB\n", vnum, qnum, minsize_knob.c_str(), knob_min_pkt_size[vnum][qnum]);
    }
  }

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      std::stringstream vnum_str;
      std::stringstream qnum_str;
      vnum_str << vnum;
      qnum_str << qnum;

      std::string maxsize_knob = "vlan" + vnum_str.str() + "_qnum" + qnum_str.str()  + "_maxsize";
      knob_max_pkt_size[vnum][qnum] = sknobs_get_value((char*) maxsize_knob.c_str(), 256);
      logger::dv_debug(DV_INFO, "Vlan%0d Queue%0d Max Pkt Size (%s) is: %0dB\n", vnum, qnum, maxsize_knob.c_str(), knob_max_pkt_size[vnum][qnum]);
    }
  }
}

///////////////////////////////////////////////////////////////////////
// Function: need_new_pkt 
///////////////////////////////////////////////////////////////////////
bool PktGen::need_new_pkt(PktInfo &new_pkt, double total_time_in_ns) {
  int has_new_pkt = 0;

  // Look for new pkt if needed
  if(!output_handler->is_full()) {
    output_handler->print_fifo(total_time_in_ns);
    if(select_new_pkt(new_pkt)) {
      has_new_pkt = 1;
    }
  }

  // Update input queue rates
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      logger::dv_debug(DV_DEBUG1, "need_new_pkt: num_bytes_sent_per_queue: %0d\n", num_bytes_sent_per_queue[vnum][qnum]);
      logger::dv_debug(DV_DEBUG1, "need_new_pkt: total_time_in_ns: %2f\n", total_time_in_ns);
      cur_iqueue_rate[vnum][qnum] = (num_bytes_sent_per_queue[vnum][qnum] * 8)/total_time_in_ns;
      logger::dv_debug(DV_DEBUG1, "need_new_pkt: cur_iqueue_rate: %2f\n", cur_iqueue_rate[vnum][qnum]);
    }
  }

  logger::dv_debug(DV_DEBUG1, "need_new_pkt: New Pkt: %0d\n", has_new_pkt);
  if(has_new_pkt) {
    logger::dv_debug(DV_DEBUG1, "need_new_pkt:  vlan: %0d\n", new_pkt.vlan);
    logger::dv_debug(DV_DEBUG1, "need_new_pkt:  qnum: %0d\n", new_pkt.qnum);
    logger::dv_debug(DV_DEBUG1, "need_new_pkt:  num_pkt_bytes: %0d\n", new_pkt.num_pkt_bytes);
  }
  logger::dv_debug(DV_DEBUG1, "need_new_pkt: Total Bytes : %2f\n", total_bytes);
  logger::dv_debug(DV_DEBUG1, "need_new_pkt: Total Time: %2f\n", total_time_in_ns);

  return has_new_pkt;
}

///////////////////////////////////////////////////////////////////////
// Function: select_new_pkt 
///////////////////////////////////////////////////////////////////////
bool PktGen::select_new_pkt(PktInfo &new_pkt) {
  int has_new_pkt = 0;
  int tried_all_vlans_once = 0;
  int start_vnum = cur_vnum;
  int start_qnum = cur_qnum[cur_vnum];

  while(!has_new_pkt) {

    logger::dv_debug(DV_DEBUG1, "select_new_pkt: vlan%0d queue%0d cur_iqueue_rate:%2f iqueue_rate:%2f, scheduler_pkt_wait_queue size = %d\n", cur_vnum, cur_qnum[cur_vnum], cur_iqueue_rate[cur_vnum][cur_qnum[cur_vnum]], knob_iqueue_rate[cur_vnum][cur_qnum[cur_vnum]], m_scheduler_pkt_wait_queue[cur_vnum][cur_qnum[cur_vnum]].size());
    logger::dv_debug(DV_DEBUG1, "select_new_pkt: min_pkt_size=%0d max_pkt_size=%0d\n", knob_min_pkt_size[cur_vnum][cur_qnum[cur_vnum]], knob_max_pkt_size[cur_vnum][cur_qnum[cur_vnum]]);

    // Generate new pkt if current queue is below configured rate and
    // nothing is in the wait queue
    if(cur_iqueue_rate[cur_vnum][cur_qnum[cur_vnum]] < knob_iqueue_rate[cur_vnum][cur_qnum[cur_vnum]] && (m_scheduler_pkt_wait_queue[cur_vnum][cur_qnum[cur_vnum]].size() == 0)) {
      new_pkt.reset(cur_pkt_id);
      cur_pkt_id++;

      if (knob_min_pkt_size[cur_vnum][cur_qnum[cur_vnum]] > knob_max_pkt_size[cur_vnum][cur_qnum[cur_vnum]]) {
	new_pkt.num_pkt_bytes = 0;
        logger::dv_debug(DV_DEBUG1, "WARNING: min_pkt_size is > max_pkt_size\n");
        logger::dv_debug(DV_DEBUG1, "WARNING: generated pkts will have 0 pkt bytes");
      }
      else {
	new_pkt.num_pkt_bytes = rand() % (knob_max_pkt_size[cur_vnum][cur_qnum[cur_vnum]]-knob_min_pkt_size[cur_vnum][cur_qnum[cur_vnum]] + 1);
      }      
      new_pkt.num_pkt_bytes += knob_min_pkt_size[cur_vnum][cur_qnum[cur_vnum]];
      new_pkt.vlan = cur_vnum;
      new_pkt.qnum = cur_qnum[cur_vnum];

      has_new_pkt = 1;

      logger::dv_debug(DV_DEBUG1, "select_new_pkt: Generating new pkt\n");
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: vlan%0d queue%0d pkt_id:%0d bytes:%0d\n", new_pkt.vlan, new_pkt.qnum, new_pkt.pkt_id, new_pkt.num_pkt_bytes);
      cur_qnum[cur_vnum] = (cur_qnum[cur_vnum]+1) % knob_num_queues[cur_vnum];
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_qnum[%0d]:%d\n", cur_vnum, cur_qnum[cur_vnum]);
      cur_vnum = (cur_vnum+1) % knob_num_vlans;
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_vnum:%d\n", cur_vnum);

    }
    // Round-Robin Search for queue below configured rate
    // RR on vlan first then RR on queue
    else {
      cur_vnum = (cur_vnum+1) % knob_num_vlans;
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: testing for below rate vlan/queue\n", cur_vnum);
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_vnum:%0d \n", cur_vnum);

      if(tried_all_vlans_once) {
	cur_qnum[cur_vnum] = (cur_qnum[cur_vnum]+1) % knob_num_queues[cur_vnum];
        logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_qnum[%0d]:%0d \n", cur_vnum, cur_qnum[cur_vnum]);
        logger::dv_debug(DV_DEBUG1, "select_new_pkt: start_vnum:%0d\n", start_vnum);
        logger::dv_debug(DV_DEBUG1, "select_new_pkt: start_qnum:%0d\n", start_qnum);

	if (cur_vnum == start_vnum) {
	  if(cur_qnum[cur_vnum] == start_qnum) {
            logger::dv_debug(DV_DEBUG1, "select_new_pkt: No valid queues found\n");
            return 0;
          }
	}
	
      }
      else {
        if(cur_vnum == start_vnum) {
          tried_all_vlans_once = 1;
          cur_qnum[cur_vnum] = (cur_qnum[cur_vnum]+1) % knob_num_queues[cur_vnum];
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: tried all vlans\n");
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating cur_qnum[%0d]:%0d, start_qnum=%d \n", cur_vnum, cur_qnum[cur_vnum], start_qnum);
          if(cur_qnum[cur_vnum] == start_qnum) {
            logger::dv_debug(DV_DEBUG1, "select_new_pkt: No valid queues found\n");
            return 0;
          }
        }
        else {
          // (tried_all_vlans_once == 0) && (cur_vnum != start_vnum)
          // Try with updated cur_vnum and not modified cur_qnum
        }
      }
    }
  }

  total_bytes += new_pkt.num_pkt_bytes;

  num_pkts_sent++;
  num_pkts_sent_per_queue[new_pkt.vlan][new_pkt.qnum]++;

  num_bytes_sent += new_pkt.num_pkt_bytes;
  num_bytes_sent_per_queue[new_pkt.vlan][new_pkt.qnum] += new_pkt.num_pkt_bytes;

  return has_new_pkt;
}


///////////////////////////////////////////////////////////////////////
// Function: report_stats 
///////////////////////////////////////////////////////////////////////
void PktGen::report_stats(double total_time_in_ns) {
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      double output_rate = (num_bytes_sent_per_queue[vnum][qnum]*8)/total_time_in_ns;
      logger::dv_debug(DV_INFO, "Vlan: %0d Queue: %0d:  Exp Input Queue Rate: %2f Act Input Queue Rate: %2f\n", vnum, qnum, knob_iqueue_rate[vnum][qnum], output_rate);
    }
  }
  logger::dv_debug(DV_INFO, "Number of Packets Sent: %0d\n", num_pkts_sent);
  logger::dv_debug(DV_INFO, "Number of Bytes Sent: %0d\n", num_bytes_sent);
}
