Tool **`hypersio-perf`** is a C++ performance model located in a *src/sioperf* folder.

# Building
To build the tool, navigate to *$HYPERSIO_HOME/src/sioperf* folder and run `make`. If you have multiple cores on a machine,
you can run `make -j<N>` to speed up a compilation, where *N* is the number of available cores.

# Description
`hypersio-perf` can generate a hyper-tenant trace and execute performance simulations. The corresponding Python wrappers
for these actions are `hypersio-trace-gen` and `hypersio-perf-explore`. Using wrapper tools is not required, and `hypersio-perf` can be
run with `--build` option to generate a trace, and with `--run` option to get performance results.

The tool saves generated traces to a *build/hypersio_traces* folder. Logs from performance simulations are written to a *build/perf_logs* folder.
Architectural configuration files for performance simulation are written to *build/perf_configs* folder.

# Options
Parameters and their description can be checked by running `hypersio-perf -h`. Either `--build` or `--run` option is required.

When using `--build` option, log base name, the total number of tenants, maximum devices per single log file, log file type (text or binary),
tenant order, the number of events (**not** packets) in tenant's burst, and a benchmark name must be provided. In case there is an option for a wrapper tool
`hypersio-trace-gen`, the option is passed to `hypersio-perf`, otherwise, default one is chosen (e.g., *txt* for log file type).

When using `--run` option, a trace prefix, depth of pending translation buffer, the total number of tenants, DevTLB replacement policy, 
a link bandwidth, and a file path with architecture configuration options are required.

# Examples
## Generating a hyper-tenant trace
Command
```
hypersio-trace-gen --benchmark iperf3 --tenants 3 --tenantorder rr --consectransl 1 --loggroupprefix hypersio_iperf3_vm_1_log
```
corresponds to
```
$HYPERSIO_HOME/src/sioperf/hypersio-perf --lbase hypersio_iperf3_vm_1_log --tenantnum 3 --maxdevperlog 1 --ltype txt --build --order rr --consec 3 --tname iperf3
```
The maximum number of devices per log is determined by `hypersio-trace-gen` based on the number of VM used for log collecting.

## Running performance simulation
Command 
```
hypersio-perf-explore --benchmark iperf3 --tenants 3 --tenantorder rr --tenantburstpkt 1
```
corresponds to
```
$HYPERSIO_HOME/src/sioperf/hypersio-perf --run --config $HYPERSIO_HOME/build/perf_configs/iperf3_tnt_3_rr3_base_tq_1_ooo_iotlb_64_8_lru_l2_512_16_l3_1024_16_pref_0_2_32.cfg --prefix iperf3_tnt_3_rr3 --tqueue 1 
--tenants 3 --iotlbrepl lru --linkbwgbps 200
```
