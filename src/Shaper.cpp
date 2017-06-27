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
bool Shaper::init_shaper(std::string shaper_name, double shaper_rate, double clk_period, int shaper_refresh_interval) {

  name = shaper_name;

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
  bytes_per_clk = shaper_rate * clk_period/8; 
  bytes_per_refresh = bytes_per_clk * refresh_interval;

  // Initialize token bucket
  cur_num_tokens = bytes_per_refresh;
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
// sign cur_tokens decr  subtract to do 
//    0         >           cur_tokens -= decr
//    0         <           decr - cur_tokens, ~sign
//    1         >           cur_token + decr
//    1         <           cur_token + decr
bool Shaper::check_shaper(int num_pkt_bytes, double & wait_clks) {
  if((cur_num_tokens >= num_pkt_bytes) && !sign) {
    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: transmit %0d bytes, cur_num_tokens:%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
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
    wait_clks = 0;
    return 1; // immediate send pkt
  }
  else {
    int num_tokens_needed;
    if(sign == 1) {
      num_tokens_needed = num_pkt_bytes + cur_num_tokens;
    }
    else {
      num_tokens_needed = num_pkt_bytes - cur_num_tokens;
    }

    if(num_tokens_needed > bytes_per_refresh){
      wait_clks = clks_before_refresh + ceil(((num_tokens_needed-bytes_per_refresh)/bytes_per_refresh) * refresh_interval);
    }
    else {
      wait_clks = clks_before_refresh; 
    }

    if(sign == 1) {
      logger::dv_debug(DV_DEBUG1, "check_shaper: %s: delay %0d bytes, cur_num_tokens:-%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
    }
    else {
      logger::dv_debug(DV_DEBUG1, "check_shaper: %s: delay %0d bytes, cur_num_tokens:%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
    }
    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: wait_clks: %2f clks\n", name.c_str(), wait_clks);
    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: frac_num_tokens_needed:%2f \n", name.c_str(), num_tokens_needed/bytes_per_refresh);
    logger::dv_debug(DV_DEBUG1, "check_shaper: %s: clks_before_refresh: %0d num_tokens_needed:%0d bytes_per_refresh:%2f\n", name.c_str(), clks_before_refresh, num_tokens_needed, bytes_per_refresh);

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
void Shaper::decr_shaper(int num_pkt_bytes) {
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
    logger::dv_debug(DV_DEBUG1,"decr_shaper: %s: num_pkt_bytes:%0d cur_num_tokens:-%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
  }
  else {
    logger::dv_debug(DV_DEBUG1,"decr_shaper: %s: num_pkt_bytes:%0d cur_num_tokens:%2f\n", name.c_str(), num_pkt_bytes, cur_num_tokens);
  }
}

