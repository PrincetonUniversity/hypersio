Tool **hypersio-log-collector** is a python script located in a *src/tools* folder.

# Description

The tool launches an L1 VM and one or multiple L2 VMs/Client VMs pairs, which run a test. The L1 VM produces an access log to IOMMU.
Events recorded in a log are specified in a *src/trace_events* file. After a test is finished, QEMU log is saved into *build/qemu_logs* folder.
In addition to a QEMU log, console outputs from the L1 VM and the Client VM are saved in *build/qemu_logs* folder.

During execution, **hypersio-log-collector** creates tap interfaces and bridges on a host,
and attaches L1 VM's I/O devices (e1000 NICs) to a tap interface on one side, and a Client VM's NIC on the other. L2 VM and Client VM pairs
communicate with each other through a dedicated tap interface. IPs used for tap interfaces are 172.16.X.Y/24,
where X is ID of an L2 VM/Client VM, and Y depends on a device. For example, a *tap0* host interface has IP address 172.16.0.10/24,
a Client VM 0 has IP 172.16.0.21, and an L2 VM 0 has IP 172.16.0.20.

The maximum number of L2 VMs and clients which can be run concurrently is 24. This limitation is imposed by PCIe model constraints of L1 VM.
Due to this limitation, the same configuration must be run several times to construct a hyper-tenant trace. The number of runs for a single configuration is specified using `--runs` command line option. Logs from each run are recorded into separate files in the form <log_prefix>_\<X\>.txt, where log_prefix is the same for all runs for a given configuration, and X is a run number. The logs can be later read by **hypersio-trace-gen** tool.

# Parameters
Parameters and their description can be checked by running `hypersio-log-collector -h`. There are two required paramets - test name
required with `--test` option, and the number of L2 VMs (which also equals to the number of Client VMs) - required with `--l2vm` option.

# Examples
To run an *iperf3* test with one client-server pair, run the next command:
```
hypersio-log-collector --test iperf3 --l2vm 1
```
By default, duration of *iperf3* test is 10 seconds. To run the test for 60 seconds for 2 client-server connections execute a command:
```
hypersio-log-collector --test iperf3 --l2vm 2 --iperf3time 60
```
By default, only a single run for a given configuration is done. `--runs` allows to perform multiple runs for the same configuration:
```
hypersio-log-collector --test iperf3 --l2vm 12 --runs 3
```
The above command performs a full simulation of 12 *iperf3* connections three times. All VMs will be shut down between runs and brought up before the next one.

# Notes
1. Error message `[ERROR] vmlib.py:376: 'poweroff' failed on client_0` just indicates that non-zero code was returned
after executing `poweroff` command on a Client VM. The command correctly powers off the VM.
