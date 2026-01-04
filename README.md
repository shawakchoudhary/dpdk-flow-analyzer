🚀 DPDK Flow Analyzer

DPDK Flow Analyzer is a high-performance user-space networking application built on DPDK that performs real-time packet parsing, UDP flow classification, and time-based flow aging using the CPU Time Stamp Counter (TSC).
The application is hardware-independent and runs using TAP PMD, making it ideal for development, learning, and testing on standard Linux systems without requiring physical NICs.

✨ Features
•	High-speed packet reception using the DPDK RX burst API
•	Ethernet and IPv6 packet parsing
•	5-tuple flow classification
•	(source IP, destination IP, source port, destination port, protocol)
•	Fast flow lookup using rte_hash
•	Per-flow statistics:
•	Packet count
•	Byte count
•	Time-based flow aging using CPU TSC (no system calls)
•	Automatic cleanup of inactive flows
•	Works without physical NICs using TAP PMD
•	Clear separation between fast path (packet processing) and slow path (flow aging)


🧠 Architecture Overview
TAP PMD (vdev)
     |
rte_eth_rx_burst()
     |
mbuf RX
     |
Packet Parsing
(Ethernet → IPv6 → UDP)
     |
Flow Key Creation (5-tuple)
     |
rte_hash Lookup
     |
+----------------------+----------------------+
| Existing Flow        | New Flow             |
| - Update stats       | - Create entry       |
| - Refresh TSC        | - Initialize stats   |
+----------------------+----------------------+

Periodic Aging (Slow Path)
--------------------------------
- Scan flow table
- Compare TSC timestamps
- Expire inactive flows

🛠️ Build Requirements

Linux (tested on Ubuntu)
GCC
DPDK (tested with dpdk-stable-24.11.3)
Hugepages configured
TAP support enabled

🔧 Build Instructions

  Run the following command from the project root:
  make
  
  This produces the executable "dpdk-flow-analyzer"

▶️ Run Instructions

  Example command using TAP PMD:
  sudo ./dpdk-flow-analyzer \
  -l 0-1 \
  -n 2 \
  --huge-dir=/mnt/huge \
  --vdev=net_tap0


Make sure that:
 Hugepages are allocated
 The TAP interface exists and is up

📊 Sample Output
Received 1 packets (total=25)
ETH SRC 06:0F:C4:FD:AE:5E -> DST 33:33:00:00:00:FB
IPv6 SRC fe80::40f:c4ff:fefd:ae5e -> DST ff02::fb
FLOW proto=17 sport=5353 dport=5353

NEW FLOW added (idx=3 proto=17)

FLOW AGED OUT (idx=3 proto=17 packets=13 idle=15s)

⏱️ Flow Aging Design

Each flow maintains a last_seen_tsc timestamp
The CPU Time Stamp Counter (TSC) is used for precise time measurement
Aging runs periodically in the slow path
A flow is expired when:
now - last_seen_tsc > FLOW_TIMEOUT
Using TSC avoids system calls and keeps the RX fast path extremely efficient.

🎯 Design Choices & Rationale

TAP PMD
Enables development and testing without specialized hardware.
Static flow table with rte_hash
Deterministic memory usage
Cache-friendly access patterns
No runtime heap allocation on the fast path
TSC-based timing
Nanosecond-level precision
Zero kernel involvement
UDP-only flow tracking
Avoids noisy control traffic (ICMPv6, ND, etc.)
Aligns with real firewall and dataplane behavior

📁 Project Structure
dpdk-flow-analyzer/
├── src/
│   └── main.c
├── Makefile
├── README.md
└── .gitignore
