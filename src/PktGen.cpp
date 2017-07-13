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
  delete [] cur_vnum;
  delete [] cur_qnum;
  delete [] num_pkts_sent_per_queue;
  delete [] num_bytes_sent_per_queue;
  delete [] pkt_gen_queue;
}

///////////////////////////////////////////////////////////////////////
// Function: init 
///////////////////////////////////////////////////////////////////////
//void PktGen::init(int num_vlans, int* num_queues, std::vector <PktInfo> **wait_queue, OutputHandler *ref_output_handler) {
void PktGen::init(int num_vlans, int* num_queues, int num_priorities, std::vector <PktInfo> **wait_queue, int **wait_queue_bytes, QueueInfo **ref_queue_info, OutputHandler *ref_output_handler) {

  // Set pointers
  m_scheduler_pkt_wait_queue = wait_queue;
  m_scheduler_pkt_wait_queue_bytes = wait_queue_bytes;
  output_handler = ref_output_handler;
  queue_info = ref_queue_info;

  knob_num_vlans = num_vlans;
  knob_num_queues = num_queues;
  knob_num_priorities = num_priorities;

  knob_iqueue_rate = new double*[knob_num_vlans];
  knob_min_pkt_size = new int*[knob_num_vlans];
  knob_max_pkt_size = new int*[knob_num_vlans];

  cur_vnum = new int[knob_num_priorities];
  cur_qnum = new int*[knob_num_priorities];
  cur_iqueue_rate = new double*[knob_num_vlans];

  num_pkts_sent_per_queue = new int*[knob_num_vlans];
  num_bytes_sent_per_queue = new int*[knob_num_vlans];

  pkt_gen_queue = new std::vector<PktInfo>*[knob_num_vlans];

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    knob_iqueue_rate[vnum] = new double[knob_num_queues[vnum]];
    knob_min_pkt_size[vnum] = new int[knob_num_queues[vnum]];
    knob_max_pkt_size[vnum] = new int[knob_num_queues[vnum]];

    cur_iqueue_rate[vnum] = new double[knob_num_queues[vnum]];
    num_pkts_sent_per_queue[vnum] = new int[knob_num_queues[vnum]];
    num_bytes_sent_per_queue[vnum] = new int[knob_num_queues[vnum]];

    pkt_gen_queue[vnum] = new std::vector<PktInfo>[knob_num_queues[vnum]];
  }

  for(int priority=0; priority<knob_num_priorities; priority++) {
    cur_qnum[priority] = new int[knob_num_vlans];
  }

  for(int priority=0; priority<knob_num_priorities; priority++) {
    cur_vnum[priority] = 0;
    for(int vnum=0; vnum<knob_num_vlans; vnum++) {
      cur_qnum[priority][vnum] = 0;
    }
  }

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
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

//  std::string rate_cntl_on_bytes_knob = "rate_cntl_on_bytes";
//  knob_rate_cntl_on_bytes = sknobs_get_value((char *) rate_cntl_on_bytes_knob.c_str(), 0);
//  logger::dv_debug(DV_INFO, "Rate control on bytes (instead of queue size): %0d\n", knob_rate_cntl_on_bytes);

  std::string max_wait_queue_size_knob = "max_wait_queue_size";
  knob_max_wait_queue_size = sknobs_get_value((char *) max_wait_queue_size_knob.c_str(), 32);
  logger::dv_debug(DV_INFO, "Max Queue Size is: %0d entries\n", knob_max_wait_queue_size);

  std::string max_wait_queue_bytes_knob = "max_wait_queue_bytes";
  knob_max_wait_queue_bytes = sknobs_get_value((char *) max_wait_queue_bytes_knob.c_str(), 4096);
  logger::dv_debug(DV_INFO, "Max Queue bytes is: %0d B\n", knob_max_wait_queue_bytes);
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
      logger::dv_debug(DV_DEBUG1, "need_new_pkt: cur_iqueue_rate: %2fGbps\n", cur_iqueue_rate[vnum][qnum]);
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
// Function: generate_pkts 
///////////////////////////////////////////////////////////////////////
void PktGen::generate_pkts() {
  PktInfo new_pkt;

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      if(cur_iqueue_rate[vnum][qnum] <= knob_iqueue_rate[vnum][qnum]) {

          new_pkt.reset(cur_pkt_id);
          cur_pkt_id++;
    
          if (knob_min_pkt_size[vnum][qnum] > knob_max_pkt_size[vnum][qnum]) {
    	    new_pkt.num_pkt_bytes = 0;
            logger::dv_debug(DV_DEBUG1, "WARNING: min_pkt_size is > max_pkt_size\n");
            logger::dv_debug(DV_DEBUG1, "WARNING: generated pkts will have 0 pkt bytes");
          }
          else {
    	    new_pkt.num_pkt_bytes = rand() % (knob_max_pkt_size[vnum][qnum]-knob_min_pkt_size[vnum][qnum] + 1);
          }      
          new_pkt.vlan = vnum;
          new_pkt.qnum = qnum;
          new_pkt.num_pkt_bytes += knob_min_pkt_size[vnum][qnum];
          pkt_gen_queue[vnum][qnum].push_back(new_pkt);
          logger::dv_debug(DV_INFO, "select_new_pkt: vlan%0d, queue%0d: Generating new pkt -  pkt_id:%0d bytes:%0d\n", new_pkt.vlan, new_pkt.qnum, new_pkt.pkt_id, new_pkt.num_pkt_bytes);
      } 
      logger::dv_debug(DV_INFO, "select_new_pkt: pkt_gen_queue[%0d][%0d]=%0d\n", vnum, qnum, pkt_gen_queue[vnum][qnum].size());
    }
  }
}

///////////////////////////////////////////////////////////////////////
// Function: select_new_pkt 
///////////////////////////////////////////////////////////////////////
bool PktGen::select_new_pkt(PktInfo &new_pkt) {
  int has_new_pkt = 0;
  int loop = 0;


  for(int priority=knob_num_priorities-1; priority>=0 ; priority--) {
    int tried_all_vlans_once = 0;
    int start_vnum = cur_vnum[priority];
    int start_qnum = cur_qnum[priority][cur_vnum[priority]];

    while(!has_new_pkt) {

      logger::dv_debug(DV_DEBUG1, "\n");
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: Begin --\n");
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: cur_priority:%0d queue_priority:%0d cur_vnum%0d cur_qnum%0d cur_iqueue_rate:%2f knob_iqueue_rate:%2f\n", priority, queue_info[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].priority, cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]], cur_iqueue_rate[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]], knob_iqueue_rate[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]]);
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: min_pkt_size=%0d max_pkt_size=%0d\n", knob_min_pkt_size[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]], knob_max_pkt_size[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]]);
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: queue_priority:%0d current priority:%0d\n", queue_info[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].priority, priority);
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: start_vnum:%0d start_qnum:%0d\n", start_vnum, start_qnum);
  
      // Generate new pkt if current queue is below configured rate and
      // nothing is in the wait queue
  // For debug - see reason for pkt generation slow down 
  /*
      if(cur_iqueue_rate[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]] > knob_iqueue_rate[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]]) {
          logger::dv_debug(DV_DEBUG1, "  select_new_pkt: vlan%0d, queue%0d : rate(%2f) too high to generate\n", cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]], cur_iqueue_rate[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]]);
      }
      if(m_scheduler_pkt_wait_queue[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].size() > knob_max_wait_queue_size) {
          logger::dv_debug(DV_DEBUG1, "  select_new_pkt: vlan%0d, queue%0d : queue(%0d>%0d) too long to generate\n", cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]], m_scheduler_pkt_wait_queue[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].size(), knob_max_wait_queue_size);
      }
      if(m_scheduler_pkt_wait_queue_bytes[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]] > knob_max_wait_queue_bytes) {
          logger::dv_debug(DV_DEBUG1, "  select_new_pkt: vlan%0d, queue%0d : queue(%0d>%0d) too large to generate\n", cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]], m_scheduler_pkt_wait_queue_bytes[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]], knob_max_wait_queue_bytes);
      }
  */

      // Consider queue only if it is at the current priority
      if(queue_info[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].priority == priority) {
      logger::dv_debug(DV_DEBUG1, "select_new_pkt: vlan%0d queue%0d is at current priority:%0d\n", cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]], priority);
        // Generate new pkt if current queue is below configured rate 
        if(pkt_gen_queue[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].size() && 
           (((m_scheduler_pkt_wait_queue[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].size() <= knob_max_wait_queue_size)) && 
            ((m_scheduler_pkt_wait_queue_bytes[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]] <= knob_max_wait_queue_bytes)))) 
        {
          new_pkt = pkt_gen_queue[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].front();
          pkt_gen_queue[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].erase(pkt_gen_queue[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].begin());
    
          has_new_pkt = 1;
    
          cur_qnum[priority][cur_vnum[priority]] = (cur_qnum[priority][cur_vnum[priority]]+1) % knob_num_queues[cur_vnum[priority]];
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_qnum[%0d]:%d\n", cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]]);
          cur_vnum[priority] = (cur_vnum[priority]+1) % knob_num_vlans;
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_vnum:%d\n", cur_vnum[priority]);
    
          break;
        }
        // Round-Robin Search for queue below configured rate
        // RR on vlan first then RR on queue
        else {
          cur_vnum[priority] = (cur_vnum[priority]+1) % knob_num_vlans;
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: testing for below rate vlan/queue\n");
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_vnum:%0d \n", cur_vnum[priority]);
    
          if(tried_all_vlans_once) {
    	    cur_qnum[priority][cur_vnum[priority]] = (cur_qnum[priority][cur_vnum[priority]]+1) % knob_num_queues[cur_vnum[priority]];
            logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_qnum[%0d]:%0d \n", cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]]);
            logger::dv_debug(DV_DEBUG1, "select_new_pkt: start_vnum:%0d\n", start_vnum);
            logger::dv_debug(DV_DEBUG1, "select_new_pkt: start_qnum:%0d\n", start_qnum);
    
    	    if (cur_vnum[priority] == start_vnum) {
    	      if(cur_qnum[priority][cur_vnum[priority]] == start_qnum) {
                logger::dv_debug(DV_DEBUG1, "select_new_pkt: No valid queues found for priority:%0d\n", priority);
                break;
              }
    	    }
          }
          else {
            if(cur_vnum[priority] == start_vnum) {
              tried_all_vlans_once = 1;
              cur_qnum[priority][cur_vnum[priority]] = (cur_qnum[priority][cur_vnum[priority]]+1) % knob_num_queues[cur_vnum[priority]];
              logger::dv_debug(DV_DEBUG1, "select_new_pkt: tried all vlans\n");
              logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating cur_qnum[%0d][%0d]:%0d, start_qnum=%d \n", priority,  cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]], start_qnum);
              if(cur_qnum[priority][cur_vnum[priority]] == start_qnum) {
                logger::dv_debug(DV_DEBUG1, "select_new_pkt: No valid queues found for priority:%0d\n", priority);
                break;
              }
            }
            else {
              // (tried_all_vlans_once == 0) && (cur_vnum[priority] != start_vnum)
              // Try with updated cur_vnum[priority] and not modified cur_qnum[priority]
              logger::dv_debug(DV_DEBUG1, "select_new_pkt: using cur_qnum:%0d \n", cur_qnum[cur_vnum[priority]]);
            }
          }
        }
        loop++;
        logger::dv_debug(DV_DEBUG1, "select_new_pkt: loop:%0d\n", loop);
      } // end if queue in priority
      else {
        cur_vnum[priority] = (cur_vnum[priority]+1) % knob_num_vlans;
        logger::dv_debug(DV_DEBUG1, "select_new_pkt: skipping to next queue to check priority\n");
        logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_vnum:%0d \n", cur_vnum[priority]);
    
        if(tried_all_vlans_once) {
    	  cur_qnum[priority][cur_vnum[priority]] = (cur_qnum[priority][cur_vnum[priority]]+1) % knob_num_queues[cur_vnum[priority]];
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating nxt_qnum[%0d]:%0d \n", cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]]);
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: start_vnum:%0d\n", start_vnum);
          logger::dv_debug(DV_DEBUG1, "select_new_pkt: start_qnum:%0d\n", start_qnum);
    
    	  if (cur_vnum[priority] == start_vnum) {
    	    if(cur_qnum[priority][cur_vnum[priority]] == start_qnum) {
              logger::dv_debug(DV_DEBUG1, "select_new_pkt: No valid queues found for priority:%0d\n", priority);
              break;
            }
    	  }
        }
        else {
          if(cur_vnum[priority] == start_vnum) {
            tried_all_vlans_once = 1;
            cur_qnum[priority][cur_vnum[priority]] = (cur_qnum[priority][cur_vnum[priority]]+1) % knob_num_queues[cur_vnum[priority]];
            logger::dv_debug(DV_DEBUG1, "select_new_pkt: tried all vlans\n");
            logger::dv_debug(DV_DEBUG1, "select_new_pkt: updating cur_qnum[%0d][%0d]:%0d, start_qnum=%d \n", priority,  cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]], start_qnum);
            if(cur_qnum[priority][cur_vnum[priority]] == start_qnum) {
              logger::dv_debug(DV_DEBUG1, "select_new_pkt: No valid queues found for priority:%0d\n", priority);
              break;
            }
          }
          else {
            // (tried_all_vlans_once == 0) && (cur_vnum[priority] != start_vnum)
            // Try with updated cur_vnum[priority] and not modified cur_qnum[priority][cur_vnum[priority]]
            logger::dv_debug(DV_DEBUG1, "select_new_pkt: using cur_qnum:%0d\n", cur_qnum[priority][cur_vnum[priority]]);
          }
        }
        logger::dv_debug(DV_DEBUG1, "select_new_pkt: current priority:%0d is not vlan:%0d qnum:%0d priority:%0d \n", priority, cur_vnum[priority], cur_qnum[priority][cur_vnum[priority]], queue_info[cur_vnum[priority]][cur_qnum[priority][cur_vnum[priority]]].priority);

      }
    } // end while

    if(has_new_pkt) {
      total_bytes += new_pkt.num_pkt_bytes;

      num_pkts_sent++;
      num_pkts_sent_per_queue[new_pkt.vlan][new_pkt.qnum]++;

      num_bytes_sent += new_pkt.num_pkt_bytes;
      num_bytes_sent_per_queue[new_pkt.vlan][new_pkt.qnum] += new_pkt.num_pkt_bytes;

      return has_new_pkt;
    }

  } // end for priority

  logger::dv_debug(DV_DEBUG1, "select_new_pkt: No valid queues found\n");

}


///////////////////////////////////////////////////////////////////////
// Function: report_stats 
///////////////////////////////////////////////////////////////////////
void PktGen::report_stats(double total_time_in_ns) {
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      double output_rate = (num_bytes_sent_per_queue[vnum][qnum]*8)/total_time_in_ns;
      logger::dv_debug(DV_INFO, "Vlan: %0d Queue: %0d:  Exp Input Queue Rate: %2f Act Input Queue Rate: %2f Cur Iqueue Rate: %2f\n", vnum, qnum, knob_iqueue_rate[vnum][qnum], output_rate, cur_iqueue_rate[vnum][qnum]);
    }
  }
  logger::dv_debug(DV_INFO, "Number of Packets Sent: %0d\n", num_pkts_sent);
  logger::dv_debug(DV_INFO, "Number of Bytes Sent: %0d\n", num_bytes_sent);
}
