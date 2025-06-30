sudo dpdk-devbind.py -u 06:00.0

sudo dpdk-devbind.py -u 06:00.1

echo 1 | sudo tee /sys/bus/pci/devices/0000:06:00.0/remove

echo 1 | sudo tee /sys/bus/pci/devices/0000:06:00.1/remove

echo 1 | sudo tee /sys/bus/pci/rescan

sudo setpci -s 06:00.0 COMMAND=0x02;

sudo setpci -s 06:00.1 COMMAND=0x02;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0x1000 w 0x8;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0x2000 w 0x00400008;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0x8014 w 0x1;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0x8014 w 0x1;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0x800c w 0x1;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0x800c w 0x1;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0xC014 w 0x00000001;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0xC00c w 0x00000001;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0x8204;

sudo ./pcimem/pcimem /sys/devices/pci0000:00/0000:00:03.1/0000:06:00.0/resource2 0xc204;

sudo dpdk-20.11/usertools/dpdk-devbind.py -b vfio-pci 06:00.0

sudo pktgen-dpdk-pktgen-20.11.3/usr/local/bin/pktgen -a 06:00.0 -d librte_net_qdma.so -l 4-14 -n 4 --proc-type auto --log-level 7 -a 00:03.1 -- -N -P -T -m [5:6-13].0