#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ip.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "flow.h"

#include <rte_hash.h>
#include <rte_jhash.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 128
#define NUM_MBUFS 8192
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define AGING_INTERVAL_SEC 10
#define FLOW_TIMEOUT_SEC 5
#define MAX_FLOWS 1024

static uint64_t flow_timeout_tsc;
static uint64_t aging_interval_tsc;

static struct rte_hash *flow_hash;

static struct flow_stats flow_stats[MAX_FLOWS];

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .mq_mode = RTE_ETH_MQ_RX_NONE,
    },
};


static inline void
age_flows(struct rte_hash *flow_hash)
{
    uint64_t now = rte_rdtsc();

    for (int i = 0; i < MAX_FLOWS; i++) {

        if (!flow_stats[i].valid)
            continue;

        uint64_t diff = now - flow_stats[i].last_seen_tsc;

        if (diff > flow_timeout_tsc) {

            // Remove from hash

         uint64_t idle_sec = diff / rte_get_tsc_hz();

         printf("FLOW AGED OUT (idx=%d proto=%u packets=%lu idle=%lus)\n",
                i,
                flow_stats[i].key.proto,
                flow_stats[i].packets,
                idle_sec);

            rte_hash_del_key(flow_hash, &flow_stats[i].key);
            // Clear entry
            memset(&flow_stats[i], 0, sizeof(struct flow_stats));
        }
    }
}




int main(int argc, char *argv[])
{
    int ret;
    uint16_t port_id = 0;
    uint16_t nb_ports;
    struct rte_mempool *mbuf_pool;
    uint64_t last_aging_tsc = rte_rdtsc();
    /* Initialize EAL */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "EAL init failed\n");

    argc -= ret;
    argv += ret;

    /* Check available ports */
    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0)
        rte_exit(EXIT_FAILURE, "No Ethernet ports available\n");

    printf("Found %u port(s)\n", nb_ports);

    /* Create mbuf pool */
    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",
        NUM_MBUFS,
        MBUF_CACHE_SIZE,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id()
    );

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Configure port */
    ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf_default);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Cannot configure port\n");

    /* Setup RX queue */
    ret = rte_eth_rx_queue_setup(
        port_id,
        0,
        RX_RING_SIZE,
        rte_eth_dev_socket_id(port_id),
        NULL,
        mbuf_pool
    );
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "RX queue setup failed\n");

    /* Setup TX queue */
    ret = rte_eth_tx_queue_setup(
        port_id,
        0,
        TX_RING_SIZE,
        rte_eth_dev_socket_id(port_id),
        NULL
    );
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "TX queue setup failed\n");

    /* Start device */
    ret = rte_eth_dev_start(port_id);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Device start failed\n");

    printf("Port %u started. Waiting for packets...\n", port_id);


    struct rte_hash_parameters hash_params = {
    .name = "flow_table_v6",
    .entries = MAX_FLOWS,
    .key_len = sizeof(struct flow6_key),
    .hash_func = rte_jhash,
    .hash_func_init_val = 0,
    .socket_id = rte_socket_id(),
    };

    flow_hash = rte_hash_create(&hash_params);
    if (flow_hash == NULL) {
        rte_exit(EXIT_FAILURE, "Failed to create flow hash table\n");
    }


   flow_timeout_tsc = FLOW_TIMEOUT_SEC * rte_get_tsc_hz();
   aging_interval_tsc = AGING_INTERVAL_SEC * rte_get_tsc_hz();

    struct rte_mbuf *rx_pkts[BURST_SIZE];
    uint64_t total_rx = 0;
    
    while (1) {
        uint16_t nb_rx = rte_eth_rx_burst(
            port_id,
            0,
            rx_pkts,
            BURST_SIZE
        );

        if (nb_rx > 0) {
            total_rx += nb_rx;
            printf("Received %u packets (total=%lu)\n", nb_rx, total_rx);
            
            

            for (uint16_t i = 0; i < nb_rx; i++) {
		struct rte_mbuf *m = rx_pkts[i];
                uint8_t *data = rte_pktmbuf_mtod(m, uint8_t *);
                
		if (rte_pktmbuf_data_len(m) < sizeof(struct rte_ether_hdr)) {
                    rte_pktmbuf_free(m);
                    continue;
                }
                struct rte_ether_hdr *eth = (struct rte_ether_hdr *)data;
                
		struct rte_ether_addr *src = &eth->src_addr;
                struct rte_ether_addr *dst = &eth->dst_addr;

                printf("ETH SRC %02X:%02X:%02X:%02X:%02X:%02X -> ",
                            src->addr_bytes[0], src->addr_bytes[1],
                            src->addr_bytes[2], src->addr_bytes[3],
                            src->addr_bytes[4], src->addr_bytes[5]);

                printf("DST %02X:%02X:%02X:%02X:%02X:%02X\n",
                            dst->addr_bytes[0], dst->addr_bytes[1],
                            dst->addr_bytes[2], dst->addr_bytes[3],
                            dst->addr_bytes[4], dst->addr_bytes[5]);
                
               
		//accessing IPv6 address

               // uint16_t ether_type = rte_be_to_cpu_16(eth->ether_type);

		   
                   struct rte_ipv6_hdr *ipv6 = rte_pktmbuf_mtod_offset(m, struct rte_ipv6_hdr *, sizeof(struct rte_ether_hdr));

                   char src_ip[INET6_ADDRSTRLEN];
                   char dst_ip[INET6_ADDRSTRLEN];

                   inet_ntop(AF_INET6, ipv6->src_addr.a, src_ip, sizeof(src_ip));
                   inet_ntop(AF_INET6, ipv6->dst_addr.a, dst_ip, sizeof(dst_ip));

                   printf("IPv6 SRC %s -> DST %s\n",
                          src_ip, dst_ip); 
                   
                   if(ipv6->proto == 0 || (ipv6->proto != IPPROTO_TCP && ipv6->proto != IPPROTO_UDP)){
			   rte_pktmbuf_free(m);
			   continue;
		   }
                if (rte_pktmbuf_data_len(m) <
                    sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr)) {
                    rte_pktmbuf_free(m);
                    continue;
                   }

		
	        printf("PROTO = %u\n",ipv6->proto);

		uint16_t src_port = 0;
                uint16_t dst_port = 0;
                
                if (ipv6->proto == IPPROTO_UDP) {
                    struct rte_udp_hdr *udp = rte_pktmbuf_mtod_offset(m, struct rte_udp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv6_hdr));

                    src_port = rte_be_to_cpu_16(udp->src_port);
                    dst_port = rte_be_to_cpu_16(udp->dst_port);
                }
                else if (ipv6->proto == IPPROTO_TCP) {
                struct rte_tcp_hdr *tcp = rte_pktmbuf_mtod_offset(m, struct rte_tcp_hdr *, sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv6_hdr));

                src_port = rte_be_to_cpu_16(tcp->src_port);
                dst_port = rte_be_to_cpu_16(tcp->dst_port);
                }
                
                
               struct flow6_key key;
               memset(&key, 0, sizeof(key));

               memcpy(key.src_ip, ipv6->src_addr.a, 16);
               memcpy(key.dst_ip, ipv6->dst_addr.a, 16);
               key.src_port = src_port;
               key.dst_port = dst_port;
               key.proto = ipv6->proto;
		
               printf("FLOW proto=%u sport=%u dport=%u\n",
                      key.proto, key.src_port, key.dst_port);
               
               
               int ret = rte_hash_lookup(flow_hash, &key);

               if (ret >= 0) {
                   // Existing flow
                   struct flow_stats *stats = &flow_stats[ret];
                   stats->packets++;
                   stats->bytes += rte_pktmbuf_pkt_len(m);
                   stats->last_seen_tsc = rte_rdtsc();
                   } else {
                   // New flow
                   int idx = rte_hash_add_key(flow_hash, &key);
                   if (idx >= 0) {
                       struct flow_stats *stats = &flow_stats[idx];
                       memset(stats, 0, sizeof(*stats));
		       stats->key = key;
                       stats->packets = 1;
                       stats->bytes = rte_pktmbuf_pkt_len(m);
                       stats->last_seen_tsc = rte_rdtsc();
		       stats->valid = 1;

                       printf("NEW FLOW added (idx=%d proto=%u)\n", idx, key.proto);
                  }
              }
              rte_pktmbuf_free(rx_pkts[i]);
            }
            uint64_t now = rte_rdtsc();
            if ((now - last_aging_tsc) >= aging_interval_tsc) {
                age_flows(flow_hash);
		last_aging_tsc = now;
            }
        }
    }

    return 0;
}

