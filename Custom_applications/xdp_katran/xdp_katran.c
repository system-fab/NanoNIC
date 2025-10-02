/*
 * Copyright 2004-present Facebook. All Rights Reserved.
 * This is main balancer's application code
 */

#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <stddef.h>
#include <stdbool.h>

#include "balancer_consts.h"
#include "balancer_helpers.h"
#include "balancer_structs.h"
#include "balancer_maps.h"
#include "bpf.h"
#include "bpf_helpers.h"

#include "pckt_encap.h"
#include "pckt_parsing.h"
#include "handle_icmp.h"


__attribute__((__always_inline__))
static inline __u32 get_packet_hash(struct packet_description *pckt,
                                    bool hash_16bytes) {
  // Simplified hash to avoid intrinsics - just use simple XOR
  if (hash_16bytes) {
    return (pckt->flow.srcv6[0] ^ pckt->flow.srcv6[1] ^ pckt->flow.ports) & 0xFFFF;
  } else {
    return (pckt->flow.src ^ pckt->flow.ports) & 0xFFFF;
  }
}

__attribute__((__always_inline__))
static inline bool is_under_flood(__u64 *cur_time) {
  // Simplified flood detection - just get current time
  // Remove complex statistics that cause read-after-write issues
  *cur_time = bpf_ktime_get_ns();
  return false; // Never consider under flood for simplicity
}

__attribute__((__always_inline__))
static inline bool get_packet_dst(struct real_definition **real,
                                  struct packet_description *pckt,
                                  struct vip_meta *vip_info,
                                  bool is_ipv6,
                                  void *lru_map) {

  // to update lru w/ new connection
  struct real_pos_lru new_dst_lru = {};
  bool under_flood = false;
  bool src_found = false;
  __u32 *real_pos;
  __u64 cur_time = 0;
  __u32 hash;
  __u32 key;

  under_flood = is_under_flood(&cur_time);

  #ifdef LPM_SRC_LOOKUP
  if ((vip_info->flags & F_SRC_ROUTING) && !under_flood) {
    __u32 *lpm_val;
    if (is_ipv6) {
      struct v6_lpm_key lpm_key_v6 = {};
      lpm_key_v6.prefixlen = 128;
      memcpy(lpm_key_v6.addr, pckt->flow.srcv6, 16);
      lpm_val = bpf_map_lookup_elem(&lpm_src_v6, &lpm_key_v6);
    } else {
      struct v4_lpm_key lpm_key_v4 = {};
      lpm_key_v4.addr = pckt->flow.src;
      lpm_key_v4.prefixlen = 32;
      lpm_val = bpf_map_lookup_elem(&lpm_src_v4, &lpm_key_v4);
    }
    if (lpm_val) {
      src_found = true;
      key = *lpm_val;
    }
    // Simplified: Remove complex statistics that cause read-after-write issues
  }
  #endif
  if (!src_found) {
    bool hash_16bytes = is_ipv6;

    if (vip_info->flags & F_HASH_DPORT_ONLY) {
      // service which only use dst port for hash calculation
      // e.g. if packets has same dst port -> they will go to the same real.
      // usually VoIP related services.
      pckt->flow.port16[0] = pckt->flow.port16[1];
      memset(pckt->flow.srcv6, 0, 16);
    }
    hash = get_packet_hash(pckt, hash_16bytes) % RING_SIZE;
    key = RING_SIZE * (vip_info->vip_num) + hash;

    real_pos = bpf_map_lookup_elem(&ch_rings, &key);
    if(!real_pos) {
      return false;
    }
    key = *real_pos;
  }
  pckt->real_index = key;
  *real = bpf_map_lookup_elem(&reals, &key);
  if (!(*real)) {
    return false;
  }
  if (!(vip_info->flags & F_LRU_BYPASS) && !under_flood) {
    if (pckt->flow.proto == IPPROTO_UDP) {
      new_dst_lru.atime = cur_time;
    }
    new_dst_lru.pos = key;
    bpf_map_update_elem(lru_map, &pckt->flow, &new_dst_lru, BPF_ANY);
  }
  return true;
}

__attribute__((__always_inline__))
static inline void connection_table_lookup(struct real_definition **real,
                                           struct packet_description *pckt,
                                           void *lru_map) {

  struct real_pos_lru *dst_lru;
  __u64 cur_time;
  __u32 key;
  dst_lru = bpf_map_lookup_elem(lru_map, &pckt->flow);
  if (!dst_lru) {
    return;
  }
  if (pckt->flow.proto == IPPROTO_UDP) {
    cur_time = bpf_ktime_get_ns();
    if (cur_time - dst_lru->atime > LRU_UDP_TIMEOUT) {
      return;
    }
    dst_lru->atime = cur_time;
  }
  key = dst_lru->pos;
  pckt->real_index = key;
  *real = bpf_map_lookup_elem(&reals, &key);
  return;
}

__attribute__((__always_inline__))
static inline int process_l3_headers(struct packet_description *pckt,
                                     __u8 *protocol, __u64 off,
                                     __u16 *pkt_bytes, void *data,
                                     void *data_end, bool is_ipv6) {
  __u64 iph_len;
  int action;
  struct iphdr *iph;
  struct ipv6hdr *ip6h;
  if (is_ipv6) {
    ip6h = data + off;
    if (ip6h + 1 > data_end) {
      return XDP_DROP;
    }

    iph_len = sizeof(struct ipv6hdr);
    *protocol = ip6h->nexthdr;
    pckt->flow.proto = *protocol;

    // Simplified TOS extraction to avoid intrinsics
    pckt->tos = 0; // Simplified - just use 0 instead of complex bit operations

    // Manual byte swap to avoid intrinsics
    __u16 payload_len = ip6h->payload_len;
    *pkt_bytes = ((payload_len & 0xff) << 8) | ((payload_len >> 8) & 0xff);
    off += iph_len;
    if (*protocol == IPPROTO_FRAGMENT) {
      // we drop fragmented packets
      return XDP_DROP;
    } else if (*protocol == IPPROTO_ICMPV6) {
      action = parse_icmpv6(data, data_end, off, pckt);
      if (action >= 0) {
        return action;
      }
    } else {
      memcpy(pckt->flow.srcv6, ip6h->saddr.s6_addr32, 16);
      memcpy(pckt->flow.dstv6, ip6h->daddr.s6_addr32, 16);
    }
  } else {
    iph = data + off;
    if (iph + 1 > data_end) {
      return XDP_DROP;
    }
    //ihl contains len of ipv4 header in 32bit words
    if (iph->ihl != 5) {
      // if len of ipv4 hdr is not equal to 20bytes that means that header
      // contains ip options, and we dont support em
      return XDP_DROP;
    }
    pckt->tos = iph->tos;
    *protocol = iph->protocol;
    pckt->flow.proto = *protocol;
    // Manual byte swap to avoid intrinsics
    __u16 tot_len = iph->tot_len;
    *pkt_bytes = ((tot_len & 0xff) << 8) | ((tot_len >> 8) & 0xff);
    off += IPV4_HDR_LEN_NO_OPT;

    if (iph->frag_off & PCKT_FRAGMENTED) {
      // we drop fragmented packets.
      return XDP_DROP;
    }
    if (*protocol == IPPROTO_ICMP) {
      action = parse_icmp(data, data_end, off, pckt);
      if (action >= 0) {
        return action;
      }
    } else {
      pckt->flow.src = iph->saddr;
      pckt->flow.dst = iph->daddr;
    }
  }
  return FURTHER_PROCESSING;
}

#ifdef INLINE_DECAP_GENERIC
__attribute__((__always_inline__))
static inline int check_decap_dst(struct packet_description *pckt,
                                  bool is_ipv6, bool *pass) {
    struct address dst_addr = {};
    struct lb_stats *data_stats;

    if (is_ipv6) {
      memcpy(dst_addr.addrv6, pckt->flow.dstv6, 16);
    } else {
      dst_addr.addr = pckt->flow.dst;
    }
    __u32 *decap_dst_flags = bpf_map_lookup_elem(&decap_dst, &dst_addr);

    if (decap_dst_flags) {
      *pass = false;
      // Simplified: Remove statistics operations that cause read-after-write issues
    }
    return FURTHER_PROCESSING;
}

#endif // of INLINE_DECAP_GENERIC

#ifdef INLINE_DECAP_IPIP
__attribute__((__always_inline__))
static inline int process_encaped_ipip_pckt(void **data, void **data_end,
                                            struct xdp_md *xdp, bool *is_ipv6,
                                            __u8 *protocol, bool pass) {
  int action;
  if (*protocol == IPPROTO_IPIP) {
    if (*is_ipv6) {
      int offset = sizeof(struct ipv6hdr) + sizeof(struct eth_hdr);
      if ((*data + offset) > *data_end) {
        return XDP_DROP;
      }
      action = decrement_ttl(*data, *data_end, offset, false);
      if (!decap_v6(xdp, data, data_end, true)) {
        return XDP_DROP;
      }
      *is_ipv6 = false;
    } else {
      int offset = sizeof(struct iphdr) + sizeof(struct eth_hdr);
      if ((*data + offset) > *data_end) {
        return XDP_DROP;
      }
      action = decrement_ttl(*data, *data_end, offset, false);
      if (!decap_v4(xdp, data, data_end)) {
        return XDP_DROP;
      }
    }
  } else if (*protocol == IPPROTO_IPV6) {
    int offset = sizeof(struct ipv6hdr) + sizeof(struct eth_hdr);
    if ((*data + offset) > *data_end) {
      return XDP_DROP;
    }
    action = decrement_ttl(*data, *data_end, offset, true);
    if (!decap_v6(xdp, data, data_end, false)) {
      return XDP_DROP;
    }
  }
  if (action >= 0) {
    return action;
  }
  if (pass) {
    // pass packet to kernel after decapsulation
    return XDP_PASS;
  }
  return recirculate(xdp);
}
#endif // of INLINE_DECAP_IPIP

#ifdef INLINE_DECAP_GUE
__attribute__((__always_inline__))
static inline int process_encaped_gue_pckt(void **data, void **data_end,
                                           struct xdp_md *xdp, bool is_ipv6,
                                           bool pass) {
  int offset = 0;
  int action;
  if (is_ipv6) {
    __u8 v6 = 0;
    offset = sizeof(struct ipv6hdr) + sizeof(struct eth_hdr) +
      sizeof(struct udphdr);
    // 1 byte for gue v1 marker to figure out what is internal protocol
    if ((*data + offset + 1) > *data_end) {
      return XDP_DROP;
    }
    v6 = ((__u8*)(*data))[offset];
    v6 &= GUEV1_IPV6MASK;
    if (v6) {
      // inner packet is ipv6 as well
      action = decrement_ttl(*data, *data_end, offset, true);
      if (!gue_decap_v6(xdp, data, data_end, false)) {
        return XDP_DROP;
      }
    } else {
      // inner packet is ipv4
      action = decrement_ttl(*data, *data_end, offset, false);
      if (!gue_decap_v6(xdp, data, data_end, true)) {
        return XDP_DROP;
      }
    }
  } else {
    offset = sizeof(struct iphdr) + sizeof(struct eth_hdr) +
      sizeof(struct udphdr);
    if ((*data + offset) > *data_end) {
      return XDP_DROP;
    }
    action = decrement_ttl(*data, *data_end, offset, false);
    if (!gue_decap_v4(xdp, data, data_end)) {
        return XDP_DROP;
    }
  }
  if (action >= 0) {
    return action;
  }
  if (pass) {
    return XDP_PASS;
  }
  return recirculate(xdp);
}
#endif // of INLINE_DECAP_GUE



__attribute__((__always_inline__))
static inline int process_packet(void *data, __u64 off, void *data_end,
                                 bool is_ipv6, struct xdp_md *xdp) {

  struct ctl_value *cval;
  struct real_definition *dst = NULL;
  struct packet_description pckt = {};
  struct vip_definition vip = {};
  struct vip_meta *vip_info;
  struct lb_stats *data_stats;
  __u64 iph_len;
  __u8 protocol;

  int action;
  __u32 vip_num;
  __u32 mac_addr_pos = 0;
  __u16 pkt_bytes;
  action = process_l3_headers(
    &pckt, &protocol, off, &pkt_bytes, data, data_end, is_ipv6);
  if (action >= 0) {
    return action;
  }
  protocol = pckt.flow.proto;

  #ifdef INLINE_DECAP_IPIP
  if (protocol == IPPROTO_IPIP || protocol == IPPROTO_IPV6) {
    bool pass = true;
    action = check_decap_dst(&pckt, is_ipv6, &pass);
    if (action >= 0) {
      return action;
    }
    return process_encaped_ipip_pckt(
        &data, &data_end, xdp, &is_ipv6, &protocol, pass);
  }
#endif // INLINE_DECAP_IPIP

  if (protocol == IPPROTO_TCP) {
    if (!parse_tcp(data, data_end, is_ipv6, &pckt)) {
      return XDP_DROP;
    }
  } else if (protocol == IPPROTO_UDP) {
    if (!parse_udp(data, data_end, is_ipv6, &pckt)) {
      return XDP_DROP;
    }
  #ifdef INLINE_DECAP_GUE
    // Manual byte swap to avoid intrinsics  
    __u16 gue_port = ((GUE_DPORT & 0xff) << 8) | ((GUE_DPORT >> 8) & 0xff);
    if (pckt.flow.port16[1] == gue_port) {
      bool pass = true;
      action = check_decap_dst(&pckt, is_ipv6, &pass);
      if (action >= 0) {
        return action;
      }
      return process_encaped_gue_pckt(&data, &data_end, xdp, is_ipv6, pass);
    }
  #endif // of INLINE_DECAP_GUE
  } else {
    // send to tcp/ip stack
    return XDP_PASS;
  }

  if (is_ipv6) {
    memcpy(vip.vipv6, pckt.flow.dstv6, 16);
  } else {
    vip.vip = pckt.flow.dst;
  }

  vip.port = pckt.flow.port16[1];
  vip.proto = pckt.flow.proto;
  vip_info = bpf_map_lookup_elem(&vip_map, &vip);
  if (!vip_info) {
    vip.port = 0;
    vip_info = bpf_map_lookup_elem(&vip_map, &vip);
    if (!vip_info) {
      return XDP_PASS;
    }

    if (!(vip_info->flags & F_HASH_DPORT_ONLY)) {
      // VIP, which doesnt care about dst port (all packets to this VIP w/ diff
      // dst port but from the same src port/ip must go to the same real
      pckt.flow.port16[1] = 0;
    }
  }

  if (data_end - data > MAX_PCKT_SIZE) {
#ifdef ICMP_TOOBIG_GENERATION
    // Simplified: Remove ICMP too big statistics to avoid read-after-write issues
    return send_icmp_too_big(xdp, is_ipv6, data_end - data);
#else
    return XDP_DROP;
#endif
  }

  // Simplified: Remove LRU statistics to avoid read-after-write issues
  
  // totall packets - removed stats update

  if ((vip_info->flags & F_QUIC_VIP)) {
    int real_index;
    real_index = parse_quic(data, data_end, is_ipv6, &pckt);
    if (real_index > 0) {
      __u32 key = real_index;
      __u32 *real_pos = bpf_map_lookup_elem(&quic_mapping, &key);
      if (real_pos) {
        key = *real_pos;
        pckt.real_index = key;
        dst = bpf_map_lookup_elem(&reals, &key);
        if (!dst) {
          return XDP_DROP;
        }
      }
    }
  }

  if (!dst) {
    if ((vip_info->flags & F_HASH_NO_SRC_PORT)) {
      // service, where diff src port, but same ip must go to the same real,
      // e.g. gfs
      pckt.flow.port16[0] = 0;
    }
#ifndef NANOTUBE_SIMPLE
    __u32 cpu_num = bpf_get_smp_processor_id();
    void *lru_map = bpf_map_lookup_elem(&lru_maps_mapping, &cpu_num);
#else //!NANOTUBE_SIMPLE
    void *lru_map = &single_lru_cache;
#endif //!NANOTUBE_SIMPLE
    if (!lru_map) {
      lru_map = &fallback_lru_cache;
      __u32 lru_stats_key = MAX_VIPS + FALLBACK_LRU_CNTR;
      struct lb_stats *lru_stats = bpf_map_lookup_elem(&stats, &lru_stats_key);
      if (!lru_stats) {
        return XDP_DROP;
      }
      // we weren't able to retrieve per cpu/core lru and falling back to
      // default one. this counter should never be anything except 0 in prod.
      // we are going to use it for monitoring.
      // Simplified: Remove LRU stats to avoid read-after-write issues
    }

    if (!(pckt.flags & F_SYN_SET) &&
        !(vip_info->flags & F_LRU_BYPASS)) {
      connection_table_lookup(&dst, &pckt, lru_map);
    }
    if (!dst) {
      if (pckt.flow.proto == IPPROTO_TCP) {
        __u32 lru_stats_key = MAX_VIPS + LRU_MISS_CNTR;
        struct lb_stats *lru_stats = bpf_map_lookup_elem(
          &stats, &lru_stats_key);
        if (!lru_stats) {
          return XDP_DROP;
        }
        if (pckt.flags & F_SYN_SET) {
          // miss because of new tcp session
          // Simplified: Remove LRU stats to avoid read-after-write issues
        } else {
          // miss of non-syn tcp packet. could be either because of LRU trashing
          // or because another katran is restarting and all the sessions
          // have been reshuffled
#ifdef KATRAN_INTROSPECTION
          __u32 size = data_end - data;
          submit_event(xdp, &event_pipe, TCP_NONSYN_LRUMISS, data, size);
#endif
          // Simplified: Remove LRU stats to avoid read-after-write issues
        }
      }
      if(!get_packet_dst(&dst, &pckt, vip_info, is_ipv6, lru_map)) {
        return XDP_DROP;
      }
      // lru misses (either new connection or lru is full and starts to trash)
      // Simplified: Remove stats update to avoid read-after-write issues
    }
  }

  cval = bpf_map_lookup_elem(&ctl_array, &mac_addr_pos);

  if (!cval) {
    return XDP_DROP;
  }

  if (dst->flags & F_IPV6) {
    if(!PCKT_ENCAP_V6(xdp, cval, is_ipv6, &pckt, dst, pkt_bytes)) {
      return XDP_DROP;
    }
  } else {
    if(!PCKT_ENCAP_V4(xdp, cval, &pckt, dst, pkt_bytes)) {
      return XDP_DROP;
    }
  }
  vip_num = vip_info->vip_num;
  // Simplified: Remove VIP statistics to avoid read-after-write issues

  // per real statistics - also simplified (removed to avoid read-after-write issues)

  return XDP_TX;
}

// Specific IP to monitor (in network byte order)
#define MONITOR_IP 0x6401A8C0  // 192.168.1.100

// Packet counter map
struct bpf_map_def SEC("maps") packet_count_map = {
    .type = BPF_MAP_TYPE_ARRAY,
    .key_size = sizeof(__u32),
    .value_size = sizeof(__u64),
    .max_entries = 1,
};
BPF_ANNOTATE_KV_PAIR(packet_count_map, __u32, __u64);

SEC("xdp-balancer")
static
int balancer_ingress(struct xdp_md *ctx) {
  void *data = (void *)(long)ctx->data;
  void *data_end = (void *)(long)ctx->data_end;
  struct eth_hdr *eth = data;
  __u32 eth_proto;
  __u32 nh_off;
  nh_off = sizeof(struct eth_hdr);

  struct iphdr *ip;
  struct icmphdr *icmp;
  __u32 map_key = 0;
  __u64 *counter_value;
  __u64 new_count = 1;

  ip = (struct iphdr *)(eth + 1);
  
  // Only process ICMP packets from monitored IP
  if (ip->saddr == MONITOR_IP && ip->protocol == IPPROTO_ICMP) {

      // Get current counter value and increment
      counter_value = bpf_map_lookup_elem(&packet_count_map, &map_key);
      if (counter_value) {
          new_count = *counter_value + 100;
      }
  
      // Get ICMP header (handle variable IP header length)
      icmp = (struct icmphdr *)((void *)ip + (ip->ihl * 4));
      
      // Get pointer to first 8 bytes of ICMP payload
      __u8 *payload = (__u8 *)((void *)icmp + sizeof(*icmp));
      
      // Save original 8 bytes for checksum calculation
      __u8 original_bytes[8];
      #pragma unroll
      for (int i = 0; i < 8; i++) {
          original_bytes[i] = payload[i];
      }
      
      // Split 64-bit counter into high and low 32-bit parts
      __u32 counter_high = (__u32)(new_count >> 32);
      __u32 counter_low  = (__u32)(new_count & 0xFFFFFFFF);
      
      // Write counter in big-endian format (most significant byte first)
      // Bytes 0-3: high 32 bits
      payload[0] = (counter_high >> 24) & 0xFF;  // Most significant byte
      payload[1] = (counter_high >> 16) & 0xFF;
      payload[2] = (counter_high >> 8)  & 0xFF;
      payload[3] = (counter_high >> 0)  & 0xFF;
      
      // Bytes 4-7: low 32 bits
      payload[4] = (counter_low >> 24) & 0xFF;
      payload[5] = (counter_low >> 16) & 0xFF;
      payload[6] = (counter_low >> 8)  & 0xFF;
      payload[7] = (counter_low >> 0)  & 0xFF;   // Least significant byte
      
      // === ICMP Checksum Recalculation ===
      
      // Convert original bytes back to 32-bit values (big-endian to host)
      __u32 old_high = (original_bytes[0] << 24) | (original_bytes[1] << 16) |
                      (original_bytes[2] << 8)  | (original_bytes[3]);
      __u32 old_low  = (original_bytes[4] << 24) | (original_bytes[5] << 16) |
                      (original_bytes[6] << 8)  | (original_bytes[7]);
      
      // Start with current checksum
      __u32 checksum = icmp->checksum;
      
      // Remove old values from checksum (treat each 32-bit word as two 16-bit words)
      checksum += (~old_high & 0xFFFF) + (~old_high >> 16);
      checksum += (~old_low  & 0xFFFF) + (~old_low  >> 16);
      
      // Add new values to checksum
      checksum += (counter_high & 0xFFFF) + (counter_high >> 16);
      checksum += (counter_low  & 0xFFFF) + (counter_low  >> 16);
      
      // Fold carries into 16-bit result
      checksum = (checksum & 0xFFFF) + (checksum >> 16);
      checksum = (checksum & 0xFFFF) + (checksum >> 16);
      
      // Update ICMP checksum
      icmp->checksum = (__u16)checksum;

      // Update counter in map
      bpf_map_update_elem(&packet_count_map, &map_key, &new_count, BPF_ANY);

      return XDP_PASS;
  }

  if (data + nh_off > data_end) {
    // bogus packet, len less than minimum ethernet frame size
    return XDP_DROP;
  }

  eth_proto = eth->eth_proto;

  if (eth_proto == BE_ETH_P_IP) {
    return process_packet(data, nh_off, data_end, false, ctx);
  } else if (eth_proto == BE_ETH_P_IPV6) {
    return process_packet(data, nh_off, data_end, true, ctx);
  } else {
    // pass to tcp/ip stack
    return XDP_PASS;
  }
}

char _license[] SEC("license") = "GPL";
