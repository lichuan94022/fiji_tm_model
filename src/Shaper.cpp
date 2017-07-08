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

#include "Shaper.hpp"

////////////////////////////////////////////////////////////////////////////
//  Function: init_shaper
////////////////////////////////////////////////////////////////////////////
bool Shaper::init_shaper(std::string shaper_name, double shaper_rate, double clk_period, int shaper_refresh_interval, int shaper_max_burst_size) {

  name = shaper_name;
  total_sent_bytes = 0;
  total_added_bytes = 0;

  if(name == "") {
    logger::dv_debug(DV_INFO, "init_shaper failed: name is null\n");
  }

  logger::dv_debug(DV_INFO, "init_shaper %s\n", name.c_str());
  
  if(shaper_refresh_interval > 0) {
    refresh_interval = shaper_refresh_interval;
  }
  else {
    logger::dv_debug(DV_INFO, "init_shaper %s failed: refresh_interval(%0d) <= 0\n", name.c_str(), shaper_refresh_interval);
    return 0;
  }

  if(shaper_rate <= 0) {
    logger::dv_debug(DV_INFO, "init_shaper %s failed: shaper_rate(%2f) <= 0\n", name.c_str(), shaper_rate);
    return 0;
  }
  if(clk_period <= 0) {
    logger::dv_debug(DV_INFO, "init_shaper %s failed: clk_period(%2f) <= 0\n", name.c_str(), clk_period);
    return 0;
  }

  if(shaper_max_burst_size > 0) {
    max_burst_size = shaper_max_burst_size;
  }
  else {
    logger::dv_debug(DV_INFO, "init_shaper %s failed: max_burst_size(%0d) <= 0\n", name.c_str(), shaper_max_burst_size);
    return 0;
  }

  last_upd_clk_count = 0;

  bytes_per_clk = shaper_rate * clk_period/8; 
  bytes_per_refresh = bytes_per_clk * refresh_interval;

  // Initialize token bucket
//  cur_num_tokens = bytes_per_refresh;
  cur_num_tokens = 0;
//  cur_num_tokens = 1600;
  sign = 0; // 0 = positive number, 1 = negative number
  clks_before_refresh = refresh_interval;

  logger::dv_debug(DV_INFO, "init_shaper %s: shaper_rate: %2f\n", name.c_str(), shaper_rate);
  logger::dv_debug(DV_INFO, "init_shaper %s: clk_period: %2f\n", name.c_str(), clk_period);
  logger::dv_debug(DV_INFO, "init_shaper %s: shaper_refresh_interval: %0d\n", name.c_str(), shaper_refresh_interval);
  logger::dv_debug(DV_INFO, "init_shaper %s: bytes_per_clk: %2f\n", name.c_str(), bytes_per_clk);
  logger::dv_debug(DV_INFO, "init_shaper %s: bytes_per_refresh: %2f\n", name.c_str(), bytes_per_refresh);
}

////////////////////////////////////////////////////////////////////////////
//  Function: update_shaper
////////////////////////////////////////////////////////////////////////////
// sign cur_tokens refresh  add to do 
//    0         >           cur_tokens += refresh
//    0         <           cur_tokens += refresh
//    1         >           cur_token-refresh
//    1         <           refresh-cur_tokens, ~sign
void Shaper::update_shaper() {
  if(clks_before_refresh == 0) {
    if(sign == 1) {
      if(cur_num_tokens > bytes_per_refresh) {
        cur_num_tokens -= bytes_per_refresh;
      }   
      else {
        cur_num_tokens = bytes_per_refresh - cur_num_tokens;
        sign = 0;
      }
    }
    else {
        cur_num_tokens += bytes_per_refresh;
    }
    if(cur_num_tokens > max_burst_size) {
      cur_num_tokens = max_burst_size;
    }

    logger::dv_debug(DV_DEBUG1, "update_shaper: %s: add %2f tokens to shaper\n", name.c_str(), bytes_per_refresh);
    if(sign == 1) {
      logger::dv_debug(DV_DEBUG1, "update_shaper: %s: cur_num_tokens:-%2f in shaper\n", name.c_str(), cur_num_tokens);
    }
    else {
      logger::dv_debug(DV_DEBUG1, "update_shaper: %s: cur_num_tokens:%2f in shaper\n", name.c_str(), cur_num_tokens);
    }
  }

  if(clks_before_refresh == 0) {
    clks_before_refresh = refresh_interval - 1;
  }
  else {
    clks_before_refresh -= 1;
    if(sign == 1) {
      logger::dv_debug(DV_DEBUG1, "update_shaper: %s: clks before refresh %0d cur_num_tokens:-%2f\n", name.c_str(), clks_before_refresh, cur_num_tokens);
    }
    else {
      logger::dv_debug(DV_DEBUG1, "update_shaper: %s: clks before refresh %0d cur_num_tokens:%2f\n", name.c_str(), clks_before_refresh, cur_num_tokens);
    }
  }
}

////////////////////////////////////////////////////////////////////////////
//  Function: check_shaper
////////////////////////////////////////////////////////////////////////////
// sign cur_tokens refresh  add to do 
//    0         >           cur_tokens += refresh
//    0         <           cur_tokens += refresh
//    1         >           cur_token-refresh
//    1         <           refresh-cur_tokens, ~sign
// sign cur_tokens decr  subtract to do 
//    0         >           cur_tokens -= decr
//    0         <           decr - cur_tokens, ~sign
//    1         >           cur_token + decr
//    1         <           cur_token + decr
bool Shaper::check_shaper(int unsigned cur_clk_count, int num_pkt_bytes, double & wait_clks) {
  // Update cur_num_tokens since last check
  if(sign) {
    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: initial cur_num_tokens:-%2f\n", name.c_str(), cur_num_tokens);
  }
  else {
    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: initial cur_num_tokens:%2f\n", name.c_str(), cur_num_tokens);
  }
  logger::dv_debug(DV_DEBUG1, "check_shaper: %s: adding num_tokens:%2f\n", name.c_str(), ((cur_clk_count - last_upd_clk_count) * bytes_per_clk));
  logger::dv_debug(DV_DEBUG1, "check_shaper: %s: cur_clk_count:%0d last_upd_clk_count:%0d\n", name.c_str(), cur_clk_count, last_upd_clk_count);
  logger::dv_debug(DV_DEBUG1, "check_shaper: %s: bytes_per_clk:%2f \n", name.c_str(), bytes_per_clk);

  if(sign == 1) {
    if(cur_num_tokens > ((cur_clk_count - last_upd_clk_count) * bytes_per_clk)) {
      cur_num_tokens -= ((cur_clk_count - last_upd_clk_count) * bytes_per_clk);
    }   
    else {
      cur_num_tokens = ((cur_clk_count - last_upd_clk_count) * bytes_per_clk) - cur_num_tokens;
      sign = 0;
    }
  }
  else {
    cur_num_tokens += ((cur_clk_count - last_upd_clk_count) * bytes_per_clk);
  }
// TEMP
/* 
  if(cur_num_tokens > max_burst_size) {
    cur_num_tokens = max_burst_size;
  }
*/

  total_added_bytes += ((cur_clk_count - last_upd_clk_count) * bytes_per_clk);
  if(sign == 1) {
    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: update cur_num_tokens:-%2f \n", name.c_str(), cur_num_tokens);
  }
  else {
    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: update cur_num_tokens:%2f \n", name.c_str(), cur_num_tokens);
  }

  last_upd_clk_count = cur_clk_count;

  // Check if shaper has tokens for transmit
  if((cur_num_tokens >= num_pkt_bytes) && !sign) {
    logger::dv_debug(DV_INFO, "check_shaper: %s: transmit %0d bytes\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
    wait_clks = 0;
    double exp_rate = total_added_bytes * 8/cur_clk_count;
    double act_rate = total_sent_bytes * 8/cur_clk_count;
/*
    logger::dv_debug(DV_DEBUG1,"check_shaper: %s: total_added_bytes:%2f\n", name.c_str(), total_added_bytes);
    logger::dv_debug(DV_DEBUG1,"check_shaper: %s: total_sent_bytes:%02fn", name.c_str(), total_sent_bytes);
    if(sign) {
      logger::dv_debug(DV_DEBUG1,"check_shaper: %s: cur_num_tokens:-%2f\n", name.c_str(), cur_num_tokens);
      if(total_sent_bytes-total_added_bytes != cur_num_tokens) {
        logger::dv_debug(DV_DEBUG1,"check_shaper: %s: ERROR cur_num_tokens(-%2f) != total_sent_bytes(%2f) = total_added_bytes(%2f)\n", name.c_str(), cur_num_tokens, total_sent_bytes, total_added_bytes);
      }
    }
    else {
      logger::dv_debug(DV_DEBUG1,"check_shaper: %s: cur_num_tokens:%2f\n", name.c_str(), cur_num_tokens);
      if(total_added_bytes - total_sent_bytes != cur_num_tokens) {
        logger::dv_debug(DV_DEBUG1,"check_shaper: %s: ERROR cur_num_tokens(%2f) != total_added_bytes(%2f) - total_sent_bytes(%2f) \n", name.c_str(), cur_num_tokens, total_added_bytes, total_sent_bytes);
      }
    }
    logger::dv_debug(DV_DEBUG1,"check_shaper: %s: exp_rate:%2f\n", name.c_str(), exp_rate);
    logger::dv_debug(DV_DEBUG1,"check_shaper: %s: act_rate:%2f\n", name.c_str(), act_rate);
*/
    return 1; // immediate send pkt
  }
  else {
    double num_tokens_needed;

    if(sign == 1) {
      num_tokens_needed = num_pkt_bytes + cur_num_tokens;
    }
    else {
      num_tokens_needed = num_pkt_bytes - cur_num_tokens;
    }

    wait_clks = num_tokens_needed/bytes_per_clk;

/*
    if(num_tokens_needed > bytes_per_refresh){
      wait_clks = clks_before_refresh + ceil(((num_tokens_needed-bytes_per_refresh)/bytes_per_refresh) * refresh_interval);
    }
    else {
      wait_clks = clks_before_refresh; 
    }
*/

    if(sign == 1) {
      logger::dv_debug(DV_INFO, "check_shaper: %s: delay %0d bytes, not_decremented cur_num_tokens:-%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
    }
    else {
      logger::dv_debug(DV_INFO, "check_shaper: %s: delay %0d bytes, not_decremented cur_num_tokens:%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
    }
    logger::dv_debug(DV_INFO, "check_shaper: %s: wait_clks: %2f clks\n", name.c_str(), wait_clks);
//    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: frac_num_tokens_needed:%2f \n", name.c_str(), num_tokens_needed/bytes_per_refresh);
//    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: clks_before_refresh: %0d num_tokens_needed:%0d bytes_per_refresh:%2f\n", name.c_str(), clks_before_refresh, num_tokens_needed, bytes_per_refresh);

    return 0; // delayed pkt
  }
}

////////////////////////////////////////////////////////////////////////////
//  Function: decr_shaper
////////////////////////////////////////////////////////////////////////////
// sign cur_tokens decr  subtract to do 
//    0         >           cur_tokens -= decr
//    0         <           decr - cur_tokens, ~sign
//    1         >           cur_token + decr
//    1         <           cur_token + decr
void Shaper::decr_shaper(int unsigned cur_clk_count, int num_pkt_bytes) {
  total_sent_bytes += num_pkt_bytes;
  logger::dv_debug(DV_DEBUG1,"  decr_shaper: %s: initial_cur_tokens:%2f cur_clk_count:%0d total_sent_bytes:%2f\n", name.c_str(), cur_num_tokens, cur_clk_count, total_sent_bytes);
  if(sign == 1) {
    cur_num_tokens += num_pkt_bytes;
  }
  else {
    if(cur_num_tokens > num_pkt_bytes) {
      cur_num_tokens -= num_pkt_bytes;
    }
    else {
      cur_num_tokens = num_pkt_bytes - cur_num_tokens;
       sign = 1;
    }
  }
  if(sign == 1) {
    logger::dv_debug(DV_INFO,"  decr_shaper: %s: num_pkt_bytes decr:%0d cur_num_tokens:-%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
  }
  else {
    logger::dv_debug(DV_INFO,"  decr_shaper: %s: num_pkt_bytes decr:%0d cur_num_tokens:%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
  }
  double exp_rate = total_added_bytes * 8 /cur_clk_count;
  double act_rate = total_sent_bytes * 8 /cur_clk_count;
  logger::dv_debug(DV_DEBUG1,"  decr_shaper: %s: total_added_bytes:%2f\n", name.c_str(), total_added_bytes);
  logger::dv_debug(DV_DEBUG1,"  decr_shaper: %s: total_sent_bytes:%2f\n", name.c_str(), total_sent_bytes);
  if(sign) {
    logger::dv_debug(DV_DEBUG1,"  decr_shaper: %s: cur_num_tokens:-%2f\n", name.c_str(), cur_num_tokens);
    if(total_sent_bytes-total_added_bytes != cur_num_tokens) {
      logger::dv_debug(DV_INFO,"  decr_shaper: %s: ERROR cur_num_tokens(-%2f) != total_sent_bytes(%2f) = total_added_bytes(%2f)\n", name.c_str(), cur_num_tokens, total_sent_bytes, total_added_bytes);
    }
  }
  else {
    logger::dv_debug(DV_DEBUG1,"  decr_shaper: %s: cur_num_tokens:%2f\n", name.c_str(), cur_num_tokens);
    if(total_added_bytes - total_sent_bytes != cur_num_tokens) {
      logger::dv_debug(DV_INFO,"  decr_shaper: %s: ERROR cur_num_tokens(%2f) != total_added_bytes(%2f) - total_sent_bytes(%2f) \n", name.c_str(), cur_num_tokens, total_added_bytes, total_sent_bytes);
    }
  }
  logger::dv_debug(DV_DEBUG1,"  decr_shaper: %s: exp_rate:%2f\n", name.c_str(), exp_rate);
  logger::dv_debug(DV_DEBUG1,"  decr_shaper: %s: act_rate:%2f\n", name.c_str(), act_rate);
}

