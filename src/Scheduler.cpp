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

#include "Scheduler.hpp"
#include "logger.hpp"

int unsigned logger::m_clk_count = 0;
int unsigned logger::m_debug_level = 0;

/////////////////////////////////////////////////////////
// Function: Scheduler Constructor
/////////////////////////////////////////////////////////
Scheduler::Scheduler(int argc, char* argv[]) {
  total_time_in_ns = 0;
  clk_period = 0;

  max_wait_time_in_clks = 0;
  total_wait_time_in_ns = 0;
  num_pkts_imm_send = 0;
  num_bytes_imm_send = 0;
  num_pkts_delayed_send = 0;
  num_bytes_delayed_send = 0;

  knob_clk_period = 0;

  // Initialize sknobs and read knob file indicated by -f
  sknobs_init(argc, argv);
  logger::dv_debug(DV_INFO,"Initializing knobs\n");
  get_knobs(argc, argv);

  // Initialize Packet Generator
  pkt_gen.init(knob_num_vlans, knob_num_queues, pkt_wait_queue, pkt_wait_queue_bytes, &output_handler);
//  pkt_gen.init(knob_num_vlans, knob_num_queues, pkt_wait_queue, &output_handler);
//  pkt_gen.init(knob_num_vlans, knob_num_queues, &output_handler);

  // Initialize Output Handler
  output_handler.init(knob_num_vlans, knob_num_queues, knob_num_total_pkts, clk_period, knob_oport_rate, knob_num_ofifo_entries, knob_gather_bytes_per_clk);

  // Initialize Queue Shaper
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      std::stringstream vnum_str;
      std::stringstream qnum_str;
      vnum_str << vnum;
      qnum_str << qnum;

      std::string shaper_name = "V" + vnum_str.str() + "_Q" + qnum_str.str() + "_shaper";
      
      logger::dv_debug(DV_INFO,"Initializing Vlan%0d Queue%0d Shaper\n", vnum, qnum);
      logger::dv_debug(DV_INFO,"shaper_name: %s\n", shaper_name.c_str());
      logger::dv_debug(DV_INFO,"shaper_rate: %2f\n", knob_qshaper_rate[vnum][qnum]);
      logger::dv_debug(DV_INFO,"shaper_max_burst_size: %2f\n", knob_qshaper_max_burst_size[vnum][qnum]);
      logger::dv_debug(DV_INFO,"clk_period: %2f\n", clk_period);
      logger::dv_debug(DV_INFO,"refresh_interval: %0d\n", knob_qshaper_refresh_interval[vnum][qnum]);
      queue_shaper[vnum][qnum].init_shaper(shaper_name, knob_qshaper_rate[vnum][qnum], clk_period, knob_qshaper_refresh_interval[vnum][qnum], knob_qshaper_max_burst_size[vnum][qnum]);
    }
  }

  // Initialize Vlan Shaper
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    std::stringstream vnum_str;
    vnum_str << vnum;

    std::string shaper_name = "V" + vnum_str.str() + "_shaper";

    logger::dv_debug(DV_INFO,"Initializing Vlan%0d Shaper\n", vnum);
    logger::dv_debug(DV_INFO,"shaper_name: %s\n", shaper_name.c_str());
    logger::dv_debug(DV_INFO,"shaper_rate: %2f\n", knob_vshaper_rate[vnum]);
    logger::dv_debug(DV_INFO,"shaper_max_burst_size: %2f\n", knob_vshaper_max_burst_size[vnum]);
    logger::dv_debug(DV_INFO,"clk_period: %2f\n", clk_period);
    logger::dv_debug(DV_INFO,"refresh_interval: %0d\n", knob_vshaper_refresh_interval[vnum]);
    vlan_shaper[vnum].init_shaper(shaper_name, knob_vshaper_rate[vnum], clk_period, knob_vshaper_refresh_interval[vnum], knob_vshaper_max_burst_size[vnum]);
  }
}

/////////////////////////////////////////////////////////
// Function: Scheduler Destructor
/////////////////////////////////////////////////////////
Scheduler::~Scheduler() {
  delete [] queue_shaper;
  delete [] pkt_wait_queue;
  delete [] pkt_wait_queue_bytes;
  delete [] pkt_wait_queue_max;

  delete [] max_wait_time_in_clks_per_queue;
  delete [] total_wait_time_in_ns_per_queue;
  delete [] num_pkts_imm_send_per_queue;
  delete [] num_bytes_imm_send_per_queue;
  delete [] num_pkts_delayed_send_per_queue;
  delete [] num_bytes_delayed_send_per_queue;

  sknobs_save((char*) "Scheduler_sknobs.saved");
  sknobs_close();
}

/////////////////////////////////////////////////////////
// Function: get_knobs
/////////////////////////////////////////////////////////
void Scheduler::get_knobs(int argc, char* argv[]) {
  int knobfile_found = 0;

  // Set debug level
  knob_debug_level = sknobs_get_string((char*) "debug_level", "DV_INFO");
  debug_level_e debug_level;

  if(knob_debug_level == "DV_DEBUG1") {
    debug_level = DV_DEBUG1;
  }
  else if(knob_debug_level == "DV_DEBUG2") {
    debug_level = DV_DEBUG2;
  }
  else if(knob_debug_level == "DV_DEBUG3") {
    debug_level = DV_DEBUG3;
  }
  else {
    debug_level = DV_INFO;
  }
  logger::dv_set_debug_level(debug_level);

  // Set clock frequency/clock period
  knob_clk_freq = sknobs_get_value((char*) "clkfreq", 1.0); 

  knob_clk_period = sknobs_get_value((char*) "clkperiod", 0);
  if(knob_clk_period == 0) {
    clk_period = 1/knob_clk_freq;
    logger::dv_debug(DV_INFO,"Assign Clk Period from freq knob: %2fns\n", clk_period);
  }
  else {
    clk_period = knob_clk_period;
    logger::dv_debug(DV_INFO,"Assign Clk Period from period knob: %2fns \n", clk_period);
  }

  // Set number of vlans
  knob_num_vlans = sknobs_get_value((char*) "nvlans", 2);
  logger::dv_debug(DV_INFO,"Number of VLANs is: %0d\n", knob_num_vlans);

  knob_num_queues = new int[knob_num_vlans];
  knob_qshaper_rate = new double*[knob_num_vlans];
  knob_vshaper_rate = new double[knob_num_vlans];
  knob_qshaper_max_burst_size = new double*[knob_num_vlans];
  knob_vshaper_max_burst_size = new double[knob_num_vlans];
  knob_qshaper_refresh_interval = new int*[knob_num_vlans];
  knob_vshaper_refresh_interval = new int[knob_num_vlans];

  queue_shaper = new Shaper*[knob_num_vlans];
  vlan_shaper = new Shaper[knob_num_vlans];
  pkt_wait_queue = new std::vector<PktInfo>*[knob_num_vlans];
  pkt_wait_queue_bytes = new int*[knob_num_vlans];
  pkt_wait_queue_max = new int*[knob_num_vlans];

  total_wait_time_in_ns_per_queue = new double*[knob_num_vlans];
  num_pkts_imm_send_per_queue = new int*[knob_num_vlans];
  num_pkts_delayed_send_per_queue = new int*[knob_num_vlans];
  num_bytes_imm_send_per_queue = new int*[knob_num_vlans];
  num_bytes_delayed_send_per_queue = new int*[knob_num_vlans];
  pcent_imm_sends_per_queue = new double*[knob_num_vlans];

  max_wait_time_in_clks_per_queue = new double*[knob_num_vlans];

  // Set number of queues
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    std::stringstream vnum_str;
    vnum_str << vnum;

    std::string qnum_knob = "vlan" + vnum_str.str() + "_nqueues";
    knob_num_queues[vnum] = sknobs_get_value((char*) qnum_knob.c_str(), 1);
    logger::dv_debug(DV_INFO,"Vlan%0d Number of Queues is: %0d\n", vnum, knob_num_queues[vnum]);
  }

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    knob_qshaper_rate[vnum] = new double[knob_num_queues[vnum]];
    knob_qshaper_max_burst_size[vnum] = new double[knob_num_queues[vnum]];
    knob_qshaper_refresh_interval[vnum] = new int[knob_num_queues[vnum]];

    queue_shaper[vnum] = new Shaper[knob_num_queues[vnum]];
    pkt_wait_queue[vnum] = new std::vector<PktInfo>[knob_num_queues[vnum]];
    pkt_wait_queue_bytes[vnum] = new int[knob_num_queues[vnum]];
    pkt_wait_queue_max[vnum] = new int [knob_num_queues[vnum]];

    total_wait_time_in_ns_per_queue[vnum] = new double[knob_num_queues[vnum]];
    num_pkts_imm_send_per_queue[vnum] = new int[knob_num_queues[vnum]];
    num_pkts_delayed_send_per_queue[vnum] = new int[knob_num_queues[vnum]];
    num_bytes_imm_send_per_queue[vnum] = new int[knob_num_queues[vnum]];
    num_bytes_delayed_send_per_queue[vnum] = new int[knob_num_queues[vnum]];
    pcent_imm_sends_per_queue[vnum] = new double[knob_num_queues[vnum]];
    max_wait_time_in_clks_per_queue[vnum] = new double[knob_num_queues[vnum]];
  }

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      total_wait_time_in_ns_per_queue[vnum][qnum] = 0;
      num_pkts_imm_send_per_queue[vnum][qnum] = 0;
      num_pkts_delayed_send_per_queue[vnum][qnum] = 0;
      num_bytes_imm_send_per_queue[vnum][qnum] = 0;
      num_bytes_delayed_send_per_queue[vnum][qnum] = 0;
      pcent_imm_sends_per_queue[vnum][qnum] = 0;
      max_wait_time_in_clks_per_queue[vnum][qnum] = 0;
      pkt_wait_queue_bytes[vnum][qnum] = 0;
      pkt_wait_queue_max[vnum][qnum] = 0;
    }
  }

  // Set number of packets
  knob_num_total_pkts = sknobs_get_value((char*) "npkt", 50);
  logger::dv_debug(DV_INFO,"Num Total Pkts is: %0d\n", knob_num_total_pkts);

  knob_stop_on_rcvd_pkts = sknobs_get_value((char*) "stop_on_rcvd_pkt", 1);
  knob_num_rcvd_pkts = sknobs_get_value((char*) "nrcvd_pkt", 25);

  // Set Queue Shaper Rate
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      std::stringstream vnum_str;
      std::stringstream qnum_str;
      vnum_str << vnum;
      qnum_str << qnum;

      std::string qshaper_rate_knob = "vlan" + vnum_str.str() + "_qnum" + qnum_str.str()  + "_qshaper_rate";
      knob_qshaper_rate[vnum][qnum] = sknobs_get_value((char*) qshaper_rate_knob.c_str(), 100);
      logger::dv_debug(DV_INFO,"VLAN%0d Queue%0d Shaper Rate is: %2fGbps\n", vnum, qnum, knob_qshaper_rate[vnum][qnum]);
    }
  }

  // Set Vlan Shaper Rate
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    std::stringstream vnum_str;
    vnum_str << vnum;

    std::string vshaper_rate_knob = "vlan" + vnum_str.str() + "_vshaper_rate";
    knob_vshaper_rate[vnum] = sknobs_get_value((char*) vshaper_rate_knob.c_str(), 100);
    logger::dv_debug(DV_INFO,"VLAN%0d Shaper Rate is: %2fGbps\n", vnum, knob_vshaper_rate[vnum]);
  }

  // Set Queue Shaper Max Burst Size
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      std::stringstream vnum_str;
      std::stringstream qnum_str;
      vnum_str << vnum;
      qnum_str << qnum;

      std::string qshaper_max_burst_size_knob = "vlan" + vnum_str.str() + "_qnum" + qnum_str.str()  + "_qshaper_max_burst_size";
      knob_qshaper_max_burst_size[vnum][qnum] = sknobs_get_value((char*) qshaper_max_burst_size_knob.c_str(), 100);
      logger::dv_debug(DV_INFO,"VLAN%0d Queue%0d Shaper Max Burst Size is: %2fB\n", vnum, qnum, knob_qshaper_max_burst_size[vnum][qnum]);
    }
  }

  // Set Vlan Shaper Max Burst Size
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    std::stringstream vnum_str;
    vnum_str << vnum;

    std::string vshaper_max_burst_size_knob = "vlan" + vnum_str.str() + "_vshaper_max_burst_size";
    knob_vshaper_max_burst_size[vnum] = sknobs_get_value((char*) vshaper_max_burst_size_knob.c_str(), 100);
    logger::dv_debug(DV_INFO,"VLAN%0d Shaper Max Burst Size is: %2fGbps\n", vnum, knob_vshaper_max_burst_size[vnum]);
  }

  // Set Queue Shaper Refresh Rate
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      std::stringstream vnum_str;
      std::stringstream qnum_str;
      vnum_str << vnum;
      qnum_str << qnum;

      std::string qshaper_refresh_interval_knob = "vlan" + vnum_str.str() + "_qnum" + qnum_str.str()  + "_qshaper_refresh_interval";
      knob_qshaper_refresh_interval[vnum][qnum] = sknobs_get_value((char*) qshaper_refresh_interval_knob.c_str(), 100);
      logger::dv_debug(DV_INFO, "Vlan%0d Queue%0d Shaper Refresh Period is: %0d cycles\n", vnum, qnum, knob_qshaper_refresh_interval[vnum][qnum]);
    }
  }

  // Set Vlan Shaper Refresh Rate
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    std::stringstream vnum_str;
    vnum_str << vnum;

    std::string vshaper_refresh_interval_knob = "vlan" + vnum_str.str() + "_vshaper_refresh_interval";
    knob_vshaper_refresh_interval[vnum] = sknobs_get_value((char*) vshaper_refresh_interval_knob.c_str(), 100);
    logger::dv_debug(DV_INFO, "Vlan%0d Shaper Refresh Period is: %0d cycles\n", vnum, knob_vshaper_refresh_interval[vnum]);
  }

  // Set Output Port Rate
  knob_oport_rate = sknobs_get_value((char*) "orate", 150);
  logger::dv_debug(DV_INFO, "Output Port Rate is: %2fGbps\n", knob_oport_rate);

  // Set Output Fifo Rate
  knob_num_ofifo_entries = sknobs_get_value((char*) "num_ofifo_entries", 8);
  logger::dv_debug(DV_INFO, "Number of Output Fifo Entries: %0d\n", knob_num_ofifo_entries);

  // Set Output Gather Bytes Rate
  knob_gather_bytes_per_clk = sknobs_get_value((char*) "gather_bytes_per_clk", 15);
  logger::dv_debug(DV_INFO, "Number of Gather Bytes Per Clk: %0d\n", knob_gather_bytes_per_clk);

  // Set Shaper Decrement Type
  knob_shaper_pre_decr = sknobs_get_value((char*) "shaper_pre_decr", 1);
  logger::dv_debug(DV_INFO, "Shaper Pre-Decrement: %0d\n", knob_shaper_pre_decr);
  knob_shaper_pre_decr_both = sknobs_get_value((char*) "shaper_pre_decr_both", 0);
  logger::dv_debug(DV_INFO, "Shaper Pre-Decrement Both Queue/Vlan: %0d\n", knob_shaper_pre_decr_both);
  knob_shaper_pre_decr_on_type = sknobs_get_value((char*) "shaper_pre_decr_on_type", 0);
  logger::dv_debug(DV_INFO, "Shaper Pre-Decrement Fail Type: %0d\n", knob_shaper_pre_decr_on_type);

  // Set Shaper Query Time - query on generate or query on head of queue
  knob_shaper_query_on_generate = sknobs_get_value((char*) "shaper_query_on_generate", 0);
  logger::dv_debug(DV_INFO, "Shaper Query on Generate: %0d\n", knob_shaper_query_on_generate);
}

/////////////////////////////////////////////////////////
// Function: check_for_delayed_pkt_ready
/////////////////////////////////////////////////////////
bool Scheduler::check_for_delayed_pkt_ready(check_type_e check_type) {
  PktInfo delay_pkt_to_shape;
  double wait_time_in_clks = 0;

  if (!pkt_wait_heap.empty()) {
    const PktInfo delayed_pkt = pkt_wait_heap.top();
    PktInfo exp_delayed_pkt; // Expected pkt at top of heap (popped from wait_queue) 

    // Delayed pkt ready
    if (delayed_pkt.send_time_in_clks <= logger::m_clk_count) {            
      logger::dv_debug(DV_INFO, "check_for_delayed_pkt_ready: top element ready to send: pkt_id=%0d vlan=%d, qnum=%d, size=%d, send_time_in_clks=%d\n", delayed_pkt.pkt_id, delayed_pkt.vlan, delayed_pkt.qnum, delayed_pkt.num_pkt_bytes, delayed_pkt.send_time_in_clks);

      // Increment counters
      num_pkts_delayed_send++;
      num_pkts_delayed_send_per_queue[delayed_pkt.vlan][delayed_pkt.qnum]++;

      num_bytes_delayed_send+=delayed_pkt.num_pkt_bytes;
      num_bytes_delayed_send_per_queue[delayed_pkt.vlan][delayed_pkt.qnum]+=delayed_pkt.num_pkt_bytes;

      // Decrement shaper when delayed pkt sent
      switch (delayed_pkt.m_shaper_wait) 
      {
	case SHAPER_QUEUE_WAIT:
          if(knob_shaper_pre_decr) { 
            if(knob_shaper_pre_decr_on_type) {
  	      vlan_shaper[new_pkt.vlan].decr_shaper(logger::m_clk_count, new_pkt.num_pkt_bytes);
	      queue_shaper[delayed_pkt.vlan][delayed_pkt.qnum].decr_shaper(logger::m_clk_count, delayed_pkt.num_pkt_bytes);
            }
            else {
              if(!knob_shaper_pre_decr_both) {
	        queue_shaper[delayed_pkt.vlan][delayed_pkt.qnum].decr_shaper(logger::m_clk_count, delayed_pkt.num_pkt_bytes);
              }
            }
          }
	  break;
	case SHAPER_VLAN_WAIT:
          if(knob_shaper_pre_decr) { 
            if(knob_shaper_pre_decr_on_type) {
	      queue_shaper[delayed_pkt.vlan][delayed_pkt.qnum].decr_shaper(logger::m_clk_count, delayed_pkt.num_pkt_bytes);
            }
            else {
              if(!knob_shaper_pre_decr_both) {
	        queue_shaper[delayed_pkt.vlan][delayed_pkt.qnum].decr_shaper(logger::m_clk_count, delayed_pkt.num_pkt_bytes);
              }
            }
          }
//	  vlan_shaper[delayed_pkt.vlan].decr_shaper(logger::m_clk_count, delayed_pkt.num_pkt_bytes);
	  break;
	case SHAPER_VLAN_QUEUE_WAIT:
          if(knob_shaper_pre_decr) { 
            if(knob_shaper_pre_decr_on_type) {
	      queue_shaper[delayed_pkt.vlan][delayed_pkt.qnum].decr_shaper(logger::m_clk_count, delayed_pkt.num_pkt_bytes);
            }
            else {
              if(!knob_shaper_pre_decr_both) {
	        queue_shaper[delayed_pkt.vlan][delayed_pkt.qnum].decr_shaper(logger::m_clk_count, delayed_pkt.num_pkt_bytes);
              }
            }
          }
//	  vlan_shaper[delayed_pkt.vlan].decr_shaper(logger::m_clk_count, delayed_pkt.num_pkt_bytes);
	  break;
	default:
	  logger::dv_debug(DV_INFO,"ERROR: Invalid value for shaper_wait: %d\n", delayed_pkt.m_shaper_wait);
      }

      // Remove pkt from heap 
      pkt_wait_heap.pop();
      logger::dv_debug(DV_INFO, "check_for_delayed_pkt_ready: popped pkt from heap pkt_id:%0d vlan:%0d qnum:%0d\n", delayed_pkt.pkt_id, delayed_pkt.vlan, delayed_pkt.qnum);

      // Remove pkt from pkt_wait_queue 
      if(pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].size()) {
        exp_delayed_pkt = pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].front();
        if((delayed_pkt.vlan != exp_delayed_pkt.vlan) ||
           (delayed_pkt.qnum != exp_delayed_pkt.qnum) ||
           (delayed_pkt.pkt_id != exp_delayed_pkt.pkt_id)) {
           logger::dv_debug(DV_INFO, "ERROR: check_for_delayed_pkt_ready: popped pkt from heap is not expected from wait queue\n");
           logger::dv_debug(DV_INFO, "ERROR: check_for_delayed_pkt_ready: heap_pkt pkt_id:%0d vlan:%0d, qnum:%0d\n", delayed_pkt.pkt_id, delayed_pkt.vlan, delayed_pkt.qnum);
           logger::dv_debug(DV_INFO, "ERROR: check_for_delayed_pkt_ready: exp_pkt pkt_id:%0d vlan:%0d, qnum:%0d\n", exp_delayed_pkt.pkt_id, exp_delayed_pkt.vlan, exp_delayed_pkt.qnum);
          return 1;
        } 
        else {
          logger::dv_debug(DV_INFO, "check_for_delayed_pkt_ready: popped pkt from pkt_wait_queue pkt_id:%0d vlan:%0d qnum:%0d\n", exp_delayed_pkt.pkt_id, exp_delayed_pkt.vlan, exp_delayed_pkt.qnum);
        }
        pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].erase(pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].begin());
        pkt_wait_queue_bytes[delayed_pkt.vlan][delayed_pkt.qnum] -= delayed_pkt.num_pkt_bytes;
//        pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].pop_front();
      }
      else {
        logger::dv_debug(DV_INFO, "ERROR: check_for_delayed_pkt_ready: popped pkt from heap does not have corresponding pkt in pkt_wait_queue\n");
        return 1;
      }

      // If not in query on generate mode and another pkt is waiting,
      // check shaper to calculate wait time and add to heap
      if(pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].size() > 0) {
        if(!knob_shaper_query_on_generate) {
          logger::dv_debug(DV_INFO, "check_for_delayed_pkt_ready: check shapers for delayed pkt \n");
          delay_pkt_to_shape = pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].front();
          if(check_shapers_n_send_queue(delay_pkt_to_shape, NON_HEAD_OF_QUEUE)) {
            return 1;
          }
        }
        else {
          PktInfo delay_pkt = pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].front();
          queue_delay_pkt(delay_pkt, PUSH_TO_HEAP);
        }

      } // end if(pkt_wait_queue[delayed_pkt.vlan][delayed_pkt.qnum].size() > 0) 
 
      // Send current pkt to output_handler
      output_handler.add_pkt(exp_delayed_pkt);
      logger::dv_debug(DV_INFO, "check_for_delayed_pkt_ready: push new delayed pkt to output handler pkt_id:%0d vlan:%0d, qnum:%0d\n", exp_delayed_pkt.pkt_id, exp_delayed_pkt.vlan, exp_delayed_pkt.qnum);
      output_handler.print_fifo(total_time_in_ns);
    }
    else { // no delayed pkt ready
      if(check_type == NEW_PKT_DELAYED) {
        logger::dv_debug(DV_DEBUG1, "check_for_delayed_pkt_ready: no delayed pkt ready - new pkt delayed cycle\n");
      }
      else if(check_type == NO_NEW_PKT) {
        logger::dv_debug(DV_DEBUG1, "check_for_delayed_pkt_ready: no delayed pkt ready - no new pkt cycle\n");
      }
      else {
        logger::dv_debug(DV_DEBUG1, "check_for_delayed_pkt_ready: no delayed pkt ready - no more pkts cycle\n");
      }
    }
  }  
  else {
    logger::dv_debug(DV_INFO, "check_for_delayed_pkt_ready: no more pkts in heap\n");
  }
  return 0;
}

/////////////////////////////////////////////////////////
// Function: check_shapers_n_send_queue
/////////////////////////////////////////////////////////
bool Scheduler::check_shapers_n_send_queue(PktInfo& pkt, query_type_e query_type) {
  double queue_shaper_wait_clks = 0;
  double vlan_shaper_wait_clks  = 0;

  bool queue_shaper_pass = queue_shaper[pkt.vlan][pkt.qnum].check_shaper(logger::m_clk_count, pkt.num_pkt_bytes, queue_shaper_wait_clks);
  bool vlan_shaper_pass  = vlan_shaper[pkt.vlan].check_shaper(logger::m_clk_count, pkt.num_pkt_bytes, vlan_shaper_wait_clks);


  // If pkt passes both shapers, send immediately to output handler
  if (queue_shaper_pass && vlan_shaper_pass) {
    if((knob_shaper_query_on_generate && (query_type == HEAD_OF_QUEUE)) ||
        !knob_shaper_query_on_generate ) {

      total_wait_time_in_ns += 0;
      total_wait_time_in_ns_per_queue[pkt.vlan][pkt.qnum] += 0;
 
      pkt.wait_time_in_clks = 0;
      pkt.send_time_in_clks = logger::m_clk_count;

      if(query_type == HEAD_OF_QUEUE) {
        logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: HEAD_OF_QUEUE: shaper result: both pass\n");
        num_pkts_imm_send++;
        num_pkts_imm_send_per_queue[pkt.vlan][pkt.qnum]++;
 
        num_bytes_imm_send+=pkt.num_pkt_bytes;
        num_bytes_imm_send_per_queue[pkt.vlan][pkt.qnum]+=pkt.num_pkt_bytes;

        output_handler.add_pkt(pkt);
        logger::dv_debug(DV_INFO, "check_shapers_n_send_queue: New Pkt IMM Send pkt_id:%0d vlan:%0d, qnum:%0d pkt_bytes:%0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum, pkt.num_pkt_bytes);
        logger::dv_debug(DV_DEBUG1, "check_shapers_n_send_queue: total_time_in_ns:%2f\n", total_time_in_ns);
        output_handler.print_fifo(total_time_in_ns);
      }
      else {
        logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: NON_HEAD_OF_QUEUE: shaper result: both pass\n");
        queue_delay_pkt(pkt, PUSH_TO_HEAP);
      }
      logger::dv_debug(DV_DEBUG1, "check_shapers_n_send_queue: pkt_id:%0d vlan:%0d, qnum:%0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum);
 	  
      // Decrement both shapers
      queue_shaper[pkt.vlan][pkt.qnum].decr_shaper(logger::m_clk_count, pkt.num_pkt_bytes);
      vlan_shaper[pkt.vlan].decr_shaper(logger::m_clk_count, pkt.num_pkt_bytes);
    }
    else {
      logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: ERROR: unexpected condition - shapers all pass for non-head of queue\n");
    }
  }
  else { // Pkt delayed
  
    // Determine wait time
    double wait_time_in_clks = vlan_shaper_wait_clks;
  
    if (queue_shaper_pass) {
      wait_time_in_clks = vlan_shaper_wait_clks;
      pkt.m_shaper_wait = SHAPER_VLAN_WAIT;
      logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: shaper result: vlan_shaper only fail\n");
    }
    else if (vlan_shaper_pass) {
      wait_time_in_clks = queue_shaper_wait_clks;
      pkt.m_shaper_wait = SHAPER_QUEUE_WAIT;
      logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: shaper result: queue_shaper only fail\n");
    }
    else {
      wait_time_in_clks = vlan_shaper_wait_clks;
      pkt.m_shaper_wait = SHAPER_VLAN_QUEUE_WAIT;
      if (queue_shaper_wait_clks > vlan_shaper_wait_clks) {
        wait_time_in_clks = queue_shaper_wait_clks;
      }
      logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: shaper result: both shaper fail\n");
    }
    logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: pkt_id:%0d vlan:%0d qnum:%0d num_pkt_bytes:%0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum, pkt.num_pkt_bytes);
  	  
    pkt.wait_time_in_clks = wait_time_in_clks;
//  pkt.send_time_in_clks = (ceil(wait_time_in_clks) + logger::m_clk_count);
    pkt.send_time_in_clks = (wait_time_in_clks + logger::m_clk_count);
  
    if(query_type == HEAD_OF_QUEUE) {
      // Push to wait_queue and heap
      queue_delay_pkt(pkt, PUSH_TO_BOTH);
      logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: New Head Pkt Placed on Heap/Wait queue - pkt_id:%0d vlan:%0d, qnum:%0d pkt_bytes:%0d wait_clks=%0d send_clks=%0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum, pkt.num_pkt_bytes, pkt.wait_time_in_clks, pkt.send_time_in_clks);
    }
    else {

      if(!knob_shaper_query_on_generate) {
        queue_delay_pkt(pkt, PUSH_TO_HEAP);
        logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: New Head Pkt Placed on Heap - pkt_id:%0d vlan:%0d, qnum:%0d pkt_bytes:%0d wait_clks=%0d send_clks=%0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum, pkt.num_pkt_bytes, pkt.wait_time_in_clks, pkt.send_time_in_clks);
      }
      else {
        queue_delay_pkt(pkt, PUSH_TO_WAIT_QUEUE);
        logger::dv_debug(DV_INFO,"check_shapers_n_send_queue: New Non-Head Pkt Placed in Wait queue - pkt_id:%0d vlan:%0d, qnum:%0d pkt_bytes:%0d wait_clks=%0d send_clks=%0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum, pkt.num_pkt_bytes, pkt.wait_time_in_clks, pkt.send_time_in_clks);
      }
    }
  
    // Decrement shaper depending on configuration
    if(knob_shaper_pre_decr) { 
      if(knob_shaper_pre_decr_on_type) {
        if((pkt.m_shaper_wait == SHAPER_VLAN_WAIT) ||
           (pkt.m_shaper_wait == SHAPER_VLAN_QUEUE_WAIT)) {
        }
        vlan_shaper[pkt.vlan].decr_shaper(logger::m_clk_count, pkt.num_pkt_bytes);
      }
      else {
        if(knob_shaper_pre_decr_both) {
          queue_shaper[pkt.vlan][pkt.qnum].decr_shaper(logger::m_clk_count,pkt.num_pkt_bytes);
        }
        vlan_shaper[pkt.vlan].decr_shaper(logger::m_clk_count, pkt.num_pkt_bytes);
      }
    }
  
    // Update statistical data tracking
    total_wait_time_in_ns += wait_time_in_clks * clk_period;
    total_wait_time_in_ns_per_queue[pkt.vlan][pkt.qnum] += wait_time_in_clks * clk_period;
  
    if(max_wait_time_in_clks < wait_time_in_clks) {
      max_wait_time_in_clks = wait_time_in_clks;
    }
    if(max_wait_time_in_clks_per_queue[pkt.vlan][pkt.qnum] < wait_time_in_clks) {
      max_wait_time_in_clks_per_queue[pkt.vlan][pkt.qnum] = wait_time_in_clks;
    }
  
    // Check to see if delayed pkt is ready in check_shapers_on_generate mode
    // or if checking shapers on a new pkt at the head of the queue
    if((!knob_shaper_query_on_generate && (query_type == HEAD_OF_QUEUE)) ||
       knob_shaper_query_on_generate) {
      // Check for delayed packet to send if output handler is not full
      logger::dv_debug(DV_DEBUG1, "check_shapers_n_send_queue: New Pkt Delayed - check for delayed pkt to send\n");
      if(check_for_delayed_pkt_ready(NEW_PKT_DELAYED)) {
        return 1;
      }
    }
  } // if pkt delayed	  
  return 0;
}

/////////////////////////////////////////////////////////
// Function: run_scheduler
/////////////////////////////////////////////////////////
bool Scheduler::run_scheduler() {
  while(((num_pkts_imm_send+num_pkts_delayed_send) < knob_num_total_pkts) ||
        !pkt_wait_heap.empty() ||
        !output_handler.is_empty()) {

    logger::m_clk_count++;
    
    // Print scheduling snapshot
    logger::dv_debug(DV_DEBUG1, "\n");
    logger::dv_debug(DV_DEBUG1, "-- Simulation Status Begin --\n");
    logger::dv_debug(DV_DEBUG1, "run_scheduler: num_pkts_sent by pktgen: %0d\n", pkt_gen.num_pkts_sent);
    logger::dv_debug(DV_DEBUG1, "run_scheduler: num_bytes_sent by pktgen: %0d\n", pkt_gen.num_bytes_sent);
    logger::dv_debug(DV_DEBUG1, "run_scheduler: num_pkts sent (immediate): %0d\n", num_pkts_imm_send);
    logger::dv_debug(DV_DEBUG1, "run_scheduler: num_pkts sent (delayed): %0d\n", num_pkts_delayed_send);
    logger::dv_debug(DV_DEBUG1, "run_scheduler: num_total_pkts: %0d\n", knob_num_total_pkts);

    total_time_in_ns += clk_period;
    logger::dv_debug(DV_DEBUG1, "run_scheduler: clk_count: %0d\n", logger::m_clk_count);
    logger::dv_debug(DV_DEBUG1, "run_scheduler: clk_period: %2f\n", clk_period);
    logger::dv_debug(DV_DEBUG1, "run_scheduler: total_time_in_ns: %2fns\n", total_time_in_ns);

    for(int vnum=0; vnum<knob_num_vlans; vnum++) {
      for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
        if(pkt_wait_queue[vnum][qnum].size() > 0) {
          print_pkt_wait_queue(vnum, qnum);
        }
        else {
          logger::dv_debug(DV_DEBUG1, "pkt_wait_queue[%0d][%0d] empty\n", vnum, qnum);
        }
      }
    }
    if(!pkt_wait_heap.empty()) {      
      print_pkt_heap_top();
    }
    else {
      logger::dv_debug(DV_DEBUG1, "pkt_wait_heap empty\n");
    }

    output_handler.print_fifo(total_time_in_ns);
    logger::dv_debug(DV_DEBUG1, "-- Simulation Status End --\n");
    logger::dv_debug(DV_DEBUG1, "\n");

    // Check to see if new pkt should be sent by pktgen
    logger::dv_debug(DV_DEBUG1,"run_scheduler: check if need new pkt\n");
    if(!output_handler.is_full()) {
      if (pkt_gen.num_pkts_sent < knob_num_total_pkts) {
  
        // If new pkt created
        if(pkt_gen.need_new_pkt(new_pkt, total_time_in_ns)) {
  	
  
  	  if(pkt_wait_queue[new_pkt.vlan][new_pkt.qnum].size() > 0) {
            if(knob_shaper_query_on_generate) {
  	      logger::dv_debug(DV_INFO,"run_scheduler: push to existing wait queue after checking shapers\n");
              // Check shapers (and send/queue)
              if( check_shapers_n_send_queue(new_pkt, NON_HEAD_OF_QUEUE)) {
                return 1;
              } 
            }
            else {
              // Queue pkt without calling shapers or calculating wait time 
              // if associated wait queue is non-zero
  	      new_pkt.wait_time_in_clks = 0; // no wait time assigned until at top of fifo
  	      new_pkt.send_time_in_clks = 0; // no send time assigned until at top of fifo
              queue_delay_pkt(new_pkt, PUSH_TO_WAIT_QUEUE);
  	      logger::dv_debug(DV_INFO,"run_scheduler: push to existing wait queue without checking shapers\n");
            }

//            print_pkt_wait_queue(new_pkt.vlan, new_pkt.qnum);
          }
          else { // wait queue == 0
  	
            // Check shapers (and send/queue)
            if(check_shapers_n_send_queue(new_pkt, HEAD_OF_QUEUE)) {
              return 1;
            }
  	
          }
        }
        else { // No new pkt this cycle
          logger::dv_debug(DV_DEBUG1, "run_scheduler: NO New Pkt - Check wait queues\n");
          if(check_for_delayed_pkt_ready(NO_NEW_PKT)) {
            return 1;
          }
        }
      }
      else { // No more new pkts
        logger::dv_debug(DV_DEBUG1, "run_scheduler: NO More New Pkts - Check wait queues\n");
        if(check_for_delayed_pkt_ready(NO_MORE_PKTS)){
          return 1;
        }
      }
    }
    else {
      logger::dv_debug(DV_DEBUG1, "run_scheduler: no pkts sent - output handler full\n");
    }

/*
    // Update shapers
    logger::dv_debug(DV_DEBUG1, "run_scheduler: update shapers\n");
    for(int vnum=0; vnum<knob_num_vlans; vnum++) {
      for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
        queue_shaper[vnum][qnum].update_shaper();
      }
      vlan_shaper[vnum].update_shaper();	
    }
*/

    // Update output handler
    logger::dv_debug(DV_DEBUG1, "run_scheduler: update output handler\n");
    output_handler.update_output_handler(total_time_in_ns);

    if(knob_stop_on_rcvd_pkts) {
      logger::dv_debug(DV_DEBUG1, "run_scheduler: check on rcvd_pkt stop\n");
      if(output_handler.num_rcvd_pkts >= knob_num_rcvd_pkts) {
        logger::dv_debug(DV_INFO, "run_scheduler: stopping - rcvd: %0d pkts\n", output_handler.num_rcvd_pkts);
        return 0;
      }
    }
  }
  return 0;
}

/////////////////////////////////////////////////////////
// Function: queue_delay_pkt
/////////////////////////////////////////////////////////
void Scheduler::queue_delay_pkt(PktInfo& pkt, delay_type_e delay_type) {
  if((delay_type == PUSH_TO_WAIT_QUEUE) || (delay_type == PUSH_TO_BOTH)) {
    pkt_wait_queue[pkt.vlan][pkt.qnum].push_back(pkt);
    pkt_wait_queue_bytes[pkt.vlan][pkt.qnum] += pkt.num_pkt_bytes;
    if(pkt_wait_queue_max[pkt.vlan][pkt.qnum] < pkt_wait_queue[pkt.vlan][pkt.qnum].size()) {
      pkt_wait_queue_max[pkt.vlan][pkt.qnum] = pkt_wait_queue[pkt.vlan][pkt.qnum].size();
    }
    logger::dv_debug(DV_DEBUG1, "queue_delay_pkt: push_to_wait_queue pkt_id:%0d vlan:%0d, qnum:%0d pkt_size: %0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum, pkt.num_pkt_bytes);
  }

  if((delay_type == PUSH_TO_HEAP) || (delay_type == PUSH_TO_BOTH)) {
    pkt_wait_heap.push(pkt);
    logger::dv_debug(DV_DEBUG1, "queue_delay_pkt: push_to_heap pkt_id:%0d vlan:%0d, qnum:%0d pkt_size:%0d send_time_in_clks:%0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum, pkt.num_pkt_bytes, pkt.send_time_in_clks);
  }

//  pkt.print();
//  print_pkt_wait_queue(pkt.vlan, pkt.qnum); 
//  print_pkt_heap_top(); 

}

/////////////////////////////////////////////////////////
// Function: report_stats
/////////////////////////////////////////////////////////
void Scheduler::report_stats() {

  // Calculate average wait time for total number of packets
  double avg_wait_per_pkt = total_wait_time_in_ns/knob_num_total_pkts;
  logger::dv_debug(DV_INFO,"Average wait time per pkt: %2f\n", avg_wait_per_pkt);

  // Calculate average wait time for total number of bytes
  double avg_wait_per_byte = total_wait_time_in_ns/pkt_gen.total_bytes;
  logger::dv_debug(DV_INFO,"Average wait time per byte: %2f\n", avg_wait_per_byte);
    
  // Calculate average pkt wait time per queue
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      if(pkt_gen.num_pkts_sent_per_queue[vnum][qnum] > 0) {
        double avg_wait_per_queue = total_wait_time_in_ns_per_queue[vnum][qnum]/pkt_gen.num_pkts_sent_per_queue[vnum][qnum]; 
        logger::dv_debug(DV_INFO,"VLAN%0d Queue%0d Average wait time per pkt per queue: %2f\n", vnum, qnum, avg_wait_per_queue);
      }
      else {
        logger::dv_debug(DV_INFO,"No pkts sent to vlan: %0d  queue: %0d\n", vnum, qnum);
      }
    }
  }

  // Maximum wait time for total number of packets
  logger::dv_debug(DV_INFO, "Maximum wait time per pkt: %2fns\n", max_wait_time_in_clks);

  // Maximum pkt wait time per queue
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      if(pkt_gen.num_pkts_sent_per_queue[vnum][qnum] > 0) {
        logger::dv_debug(DV_INFO, "Vlan%0d Queue%0d Maximum wait time per pkt per queue: %2fns\n", vnum, qnum, max_wait_time_in_clks_per_queue[vnum][qnum]);
      }
      else {
        logger::dv_debug(DV_INFO, "No pkts sent to vlan: %0d  queue: %0d\n", vnum, qnum);
      }
    }
  }
  
  // Calculate port throughput during simulation
  double port_thrpt = (pkt_gen.total_bytes *8 ) / total_time_in_ns;
  logger::dv_debug(DV_INFO, "Total Bytes: %2f\n", pkt_gen.total_bytes);
  logger::dv_debug(DV_INFO, "Total Time: %2f\n", total_time_in_ns);
  logger::dv_debug(DV_INFO, "Input Port Throughput: %2fGbps\n", port_thrpt);

  port_thrpt = (output_handler.total_output_bytes *8 ) / output_handler.total_output_time;
  logger::dv_debug(DV_INFO, "Total Bytes: %2f\n", output_handler.total_output_bytes);
  logger::dv_debug(DV_INFO, "Total Time: %2f\n", output_handler.total_output_time);
  logger::dv_debug(DV_INFO, "Output Port Throughput: %2fGbps\n", port_thrpt);

  // Report input queue rate per queue
  pkt_gen.report_stats(total_time_in_ns);


   
  // Report num imm sends vs delayed sends vs total sends
  logger::dv_debug(DV_INFO, "num_pkt_imm_send: %0d\n", num_pkts_imm_send);
  logger::dv_debug(DV_INFO, "num_bytes_imm_send %f\n", num_bytes_imm_send);

  logger::dv_debug(DV_INFO, "num_pkt_delayed_send: %0d\n", num_pkts_delayed_send);
  logger::dv_debug(DV_INFO, "num_bytes_delayed_send %f\n", num_bytes_delayed_send);

  logger::dv_debug(DV_INFO, "num_total_pkts: %0d\n", knob_num_total_pkts);
  pcent_imm_send = (num_pkts_imm_send *100)/ knob_num_total_pkts;
  logger::dv_debug(DV_INFO, "Percentage of Immediate Sends: %2f\n", pcent_imm_send);

  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      logger::dv_debug(DV_INFO,"Max wait queue length vlan%0d queue %0d: %0d\n", vnum, qnum, pkt_wait_queue_max[vnum][qnum]);
    }
  }

  // Report num imm sends vs total sends per queue
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      if(pkt_gen.num_pkts_sent_per_queue[vnum][qnum]) {
        double pcent_imm_sends_per_queue = (num_pkts_imm_send_per_queue[vnum][qnum]*100)/pkt_gen.num_pkts_sent_per_queue[vnum][qnum] ;
        logger::dv_debug(DV_INFO, "Percentage of Immediate Sends for Vlan %0d Queue %0d: %2f\n", vnum, qnum, pcent_imm_sends_per_queue);
      }
      else {
        logger::dv_debug(DV_INFO,"No pkts sent to Vlan %0d Queue %0d: %2f\n", vnum, qnum);
      }
    }
  }

  double pcent_full_time = output_handler.full_time/output_handler.total_output_time;
  logger::dv_debug(DV_INFO, "Percentage Time Output Fifo Full: %2f\n", pcent_full_time);

  // Report output queue rate per queue
  output_handler.print_stats();

/*
  logger::dv_debug(DV_INFO, "Imm Send Pkts = %0d\n", num_pkts_imm_send);
  logger::dv_debug(DV_INFO, "Delayed Send Pkts = %0d\n", num_pkts_delayed_send);
  for(int vnum=0; vnum<knob_num_vlans; vnum++) {
    for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
      double rate = ((num_bytes_imm_send_per_queue[vnum][qnum] + num_bytes_delayed_send_per_queue[vnum][qnum]) * 8) / (logger::m_clk_count * clk_period);
      logger::dv_debug(DV_INFO, "Imm Send Bytes queue[%d][%d] = %0d\n", vnum, qnum, num_bytes_imm_send_per_queue[vnum][qnum]);
      logger::dv_debug(DV_INFO, "Delayed Send Bytes queue[%d][%d] = %0d\n", vnum, qnum, num_bytes_delayed_send_per_queue[vnum][qnum]);
      logger::dv_debug(DV_INFO, "Time queue[%d][%d] = %0d\n", vnum, qnum, logger::m_clk_count);
      logger::dv_debug(DV_INFO, "Output Rate for queue[%d][%d] = %f\n", vnum, qnum, rate);
    }
  }

  // Report total output rate
  double rate = ((num_bytes_imm_send + num_bytes_delayed_send) * 8) / (logger::m_clk_count * clk_period);
  logger::dv_debug(DV_INFO, "Total Output Rate %f\n", rate);
*/


  logger::dv_debug(DV_INFO, "Done Reporting TM Output Scheduler Model Statistics!\n");


}

void Scheduler::post_run_checks() {
  logger::dv_debug(DV_INFO, "Start post run checks\n");

  if(!knob_stop_on_rcvd_pkts) {
    if(!pkt_wait_heap.empty()) {  
      logger::dv_debug(DV_INFO, "ERROR: Pkt heap not empty!\n");
    }
    while (!pkt_wait_heap.empty()) {
      const PktInfo& pkt = pkt_wait_heap.top();
      pkt.print();
      pkt_wait_heap.pop();
    }
    for(int vnum=0; vnum<knob_num_vlans; vnum++) {
      for(int qnum=0; qnum<knob_num_queues[vnum]; qnum++) {
        if(pkt_wait_queue[vnum][qnum].size()) {  
          logger::dv_debug(DV_INFO, "ERROR: Pkt wait queue[%0d][%0d] not empty!\n", vnum, qnum);
          print_pkt_wait_queue(vnum, qnum);
        }
      }
    }
  }

  logger::dv_debug(DV_INFO, "Completed post run checks\n");
}

/////////////////////////////////////////////////////////
// Function: print_pkt_wait_queue
/////////////////////////////////////////////////////////
void Scheduler::print_pkt_wait_queue(int vlan, int qnum) {
  logger::dv_debug(DV_DEBUG1, "pkt_wait_queue[%0d][%0d]\n", vlan, qnum);
  for(int i=0; i<pkt_wait_queue[vlan][qnum].size(); i++) {
    logger::dv_debug(DV_DEBUG1, "pkt_wait_queue[%0d][%0d][%0d].pkt_id=%0d\n", vlan, qnum, i, pkt_wait_queue[vlan][qnum][i].pkt_id);
    logger::dv_debug(DV_DEBUG1, "pkt_wait_queue[%0d][%0d][%0d].num_pkt_bytes=%0d\n", vlan, qnum, i, pkt_wait_queue[vlan][qnum][i].num_pkt_bytes);
    logger::dv_debug(DV_DEBUG1, "pkt_wait_queue[%0d][%0d][%0d].wait_time_in_clks=%0d\n", vlan, qnum, i, pkt_wait_queue[vlan][qnum][i].wait_time_in_clks);
    logger::dv_debug(DV_DEBUG1, "pkt_wait_queue[%0d][%0d][%0d].send_time_in_clks=%0d\n", vlan, qnum, i, pkt_wait_queue[vlan][qnum][i].send_time_in_clks);
  }
}

/////////////////////////////////////////////////////////
// Function: print_pkt_heap_top
/////////////////////////////////////////////////////////
void Scheduler::print_pkt_heap_top() {
  const PktInfo& pkt = pkt_wait_heap.top();
  logger::dv_debug(DV_DEBUG1, "pkt_heap_top pkt_id:%0d vlan:%0d qnum:%0d\n", pkt.pkt_id, pkt.vlan, pkt.qnum);
}
