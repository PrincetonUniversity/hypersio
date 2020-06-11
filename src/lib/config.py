import os
import common
import dbglib as dbg
from dir_config import *


SIOPERF_EXEC_NAME = "hypersio-perf"

###############################################################################
# Qemu Config
##############################################################################

L1_VM_NAME = "l1vm"

QEMU_DIR = {
    "localhost":LOCALQEMU_DIR,
    L1_VM_NAME:DEFAULTQEMU_DIR
}
QEMU_SYSTEM = "qemu-system-x86_64"
DRIVE_DIR = {
    "localhost":VMSTORAGE_DIR,
    L1_VM_NAME:"/home/hypersio/hypersio/vm_images"
}

SIOPERF_RUN_LOG_DIR = os.path.join(BUILD_DIR, "perf_logs")

###############################################################################
# VM Config
##############################################################################
DEFAULT_ISO         = "ubuntu-18.04.4-live-server-amd64.iso"
DEFAULT_FWD_PORT    = 10024

# Client parameters
CLIENT_BASEIMG      = "client_base.img"
CLIENT_VMPREFIX     = "client"
CLIENT_FWDPORT_BASE = 10130

VM_USERNAME = "hypersio"

VMTYPE_PREFIX_D = {
    "l1":"l1vm",
    "l2":"l2vm",
    "client":"client"
}

VMTYPE_BASEIMG_D = {t:p+"_base.img" for (t,p) in VMTYPE_PREFIX_D.items()}

VMTYPE_IMGSIZEGB_D = {
    "l1":64,
    "l2":8,
    "client":16,
}

VMTYPE_FWDPORTBASE_D = {
    "l1":10025,
    "l2":10030,
    "client":10130,
}

VMTYPE_CORES_D = {
    "l1":8,
    "l2":2,
    "client":2
}

VMTYPE_MEMORY_D = {
    "l1":32,
    "l2":16,
    "client":16
}

# L1 VM parameters
L1_HOST_NAME        = "l1vm"
L1_BASEIMG          = "l1vm_base.img"
L1_FWDPORT_BASE     = 10025
L1_TRACE_EVENTS_FILE = os.path.join(SRC_DIR, "trace_events")
L1_TRACE_FILE       = "l1_vm_trace.log"
L1_TRACE_PATH       = os.path.join(LOG_DIR, L1_TRACE_FILE)
L1_CFGNIC_PCIE_ADDR = "1c.00"
L1_LOG_FILE_NAME    = "l1_vm.log"

# L2 VM parameters
L2_BASEIMG          = VMTYPE_BASEIMG_D["l2"]
L2_VMPREFIX         = VMTYPE_PREFIX_D["l2"]
L2_FWDPORT_BASE     = 10030
L2_VM_BOOTWAIT      = 90

FIRST_NIC_PCIEDEV   = 2

###############################################################################
# HW Config
##############################################################################
DEFAULT_IOTLB_SIZE = 64
DEFAULT_IOTLB_ASSOC = 8
DEFAULT_TENANT_L = [4, 8, 16, 32, 64, 128, 256, 512, 1024]
DEFAULT_BENCHMARK_L = ["iperf3", "mediastream", "websearch"]

TRANSLATIONS_PER_PKT = 3

DEFAULT_LINK_BW_GBPS = 200
DEFAULT_TENANT_ORDER = "rr"
DEFAULT_BURST_PKTNUM = 1
DEFAULT_CONSEC_TRANSL = DEFAULT_BURST_PKTNUM*TRANSLATIONS_PER_PKT


X540_VF_PER_PF = 63
