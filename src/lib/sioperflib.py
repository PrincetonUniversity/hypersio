import config as cfg
import re

class HWConfig:
    def __init__(self):
        # Default values. Do not change here!
        self.iotlb_size_l = [4, 8, 16]
        self.iotlb_fullassoc = False

class WorkloadConfig:
    def __init__(self):
        # Default values. Do not change here!
        self.test_name_l = cfg.DEFAULT_BENCHMARK_L
        self.tenant_num_l = cfg.DEFAULT_TENANT_L
        self.tenant_order = cfg.DEFAULT_TENANT_ORDER
        self.consec_transl = cfg.DEFAULT_CONSEC_TRANSL
        self.link_gbps_l = [cfg.DEFAULT_LINK_BW_GBPS]
        self.log_group_prefix = None

    def setLogGroupPrefix(self, log_group_prefix):
        self.log_group_prefix = log_group_prefix

def constructWorkloadConfigFromArgs(args):
    workload_cfg = WorkloadConfig()
    workload_cfg.test_name_l    = args.benchmark
    workload_cfg.tenant_num_l   = args.tenants
    workload_cfg.tenant_order   = args.tenantorder
    workload_cfg.consec_transl  = args.tenantburstpkt*cfg.TRANSLATIONS_PER_PKT
    workload_cfg.link_gbps_l    = args.linkgbps

    return workload_cfg

def getVMNumFromLogBase(log_base):
    m = re.match(r".*_vm_(\d+)_.*", log_base)
    if m:
        return int(m.group(1))
    else:
        return None

def constructHWConfigFromArgs(args):
    hw_cfg = HWConfig()
    hw_cfg.iotlb_size_l = args.iotlbsize

    return hw_cfg

def addWorkloadOptionsToParser(parser):
    required_args = parser.add_argument_group("Required named trace arguments")
    required_args.add_argument("--benchmark", dest="benchmark", nargs="*", type=str, required=True, help="Name of a benchmark(s)")
    required_args.add_argument("--tenants", dest="tenants", nargs="*", type=int, required=True, help="Number of tenants - single or list of values")
    required_args.add_argument("--tenantorder", dest="tenantorder", type=str, required=True, help="Tenant order")
    required_args.add_argument("--tenantburstpkt", dest="tenantburstpkt", type=int, required=True, help="Number of consecutive packets from a single tenant - burst size")
    return parser

def addHWOptionsToParser(parser):
    parser.add_argument("--iotlbsize", dest="iotlbsize", nargs="*", type=int, default=[cfg.DEFAULT_IOTLB_SIZE],  help="IOTLB size - single or list of values")
    parser.add_argument("--linkgbps", dest="linkgbps", type=int, nargs="*", default=[cfg.DEFAULT_LINK_BW_GBPS], help="Link Bandwidth Gb/s")
    return parser


def addSIOPerfOptionsToParser(parser):
    parser = addWorkloadOptionsToParser(parser)
    parser = addHWOptionsToParser(parser)
    return parser