#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_lcore.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 128
#define NUM_MBUFS 8192
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static volatile int keep_running = 1;
void int_handler(int dummy) { keep_running = 0; }

// Max ports supported
#define MAX_PORTS 2

struct lcore_stats {
    uint64_t rx_packets;
    uint64_t tx_packets;
} __rte_cache_aligned;

struct lcore_stats stats[RTE_MAX_LCORE];

// Forward packets from src_port to dst_port
static inline void forward_packets(uint16_t src_port, uint16_t dst_port, struct rte_mbuf **pkts, uint16_t nb_pkts) {
    uint16_t nb_tx = rte_eth_tx_burst(dst_port, 0, pkts, nb_pkts);

    // Free any packets not sent
    if (nb_tx < nb_pkts) {
        for (int i = nb_tx; i < nb_pkts; i++) {
            rte_pktmbuf_free(pkts[i]);
        }
    }

    stats[rte_lcore_id()].rx_packets += nb_pkts;
    stats[rte_lcore_id()].tx_packets += nb_tx;
}

// Worker function per lcore
int lcore_main(__rte_unused void *arg) {
    uint16_t ports[MAX_PORTS] = {0, 1};

    while (keep_running) {
        for (int i = 0; i < MAX_PORTS; i++) {
            struct rte_mbuf *pkts[BURST_SIZE];
            uint16_t nb_rx = rte_eth_rx_burst(ports[i], 0, pkts, BURST_SIZE);
            if (nb_rx > 0) {
                // Forward to other port
                forward_packets(ports[i], ports[1 - i], pkts, nb_rx);
            }
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    int ret;
    uint16_t nb_ports;
    struct rte_mempool *mbuf_pool;

    signal(SIGINT, int_handler);

    // 1. Initialize EAL
    ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "EAL init failed\n");

    nb_ports = rte_eth_dev_count_avail();
    if (nb_ports < MAX_PORTS)
        rte_exit(EXIT_FAILURE, "Need at least %d ports\n", MAX_PORTS);

    // 2. Create mbuf pool
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
                                        MBUF_CACHE_SIZE, 0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (!mbuf_pool)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    // 3. Configure each port
    struct rte_eth_conf port_conf = {0};
    for (uint16_t port_id = 0; port_id < MAX_PORTS; port_id++) {
        rte_eth_dev_configure(port_id, 1, 1, &port_conf);
        rte_eth_rx_queue_setup(port_id, 0, RX_RING_SIZE, rte_eth_dev_socket_id(port_id), NULL, mbuf_pool);
        rte_eth_tx_queue_setup(port_id, 0, TX_RING_SIZE, rte_eth_dev_socket_id(port_id), NULL);
        rte_eth_dev_start(port_id);
        printf("Port %d started\n", port_id);
    }

    printf("DPDK Multi-core Multi-port switch running...\n");

    // 4. Launch per-core workers
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        rte_eal_remote_launch(lcore_main, NULL, lcore_id);
    }

    // Run main lcore too
    lcore_main(NULL);

    // Wait for all lcores to finish
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        rte_eal_wait_lcore(lcore_id);
    }

    // 5. Stop ports
    for (uint16_t port_id = 0; port_id < MAX_PORTS; port_id++) {
        rte_eth_dev_stop(port_id);
        rte_eth_dev_close(port_id);
    }

    printf("Exiting switch...\n");
    for (int i = 0; i < RTE_MAX_LCORE; i++)
        printf("Lcore %d: RX %" PRIu64 ", TX %" PRIu64 "\n",
               i, stats[i].rx_packets, stats[i].tx_packets);

    return 0;
}
