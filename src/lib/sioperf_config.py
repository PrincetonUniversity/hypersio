#!/usr/bin/env python2

import sys, os, copy

# Local Modules
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
LIB_DIR = os.path.join(SCRIPT_DIR, "../../lib")
sys.path.insert(0, LIB_DIR)

import config
import dbglib as dbg
from sioperflib import *

IMPORT_TNT_NUM_L = [1024]

TEST_NAME_L = [
                "iperf3",
                "mediastream",
                "websearch",
              ]

##############################################################
# Configuration for building traces
QEMU_LOG_EXT = "txt"
FIRST_DEV = 2
TNAME_DEVPERLOG_D = {
    "mediastream":24,
    "iperf3":24,
    "websearch":12,
}
TNAME_MAXLOG_D = {
    "mediastream":48,
    "iperf3":44,
    "websearch":92,
}
TNAME_QLOGBASE_D = {
    "mediastream":"hypersio_mediastream_vm_24_log",
    "iperf3":"hypersio_iperf3_vm_24_log",
    "websearch":"hypersio_websearch_vm_12_log",
}
##############################################################


class CacheConfig:
    def __init__(self, name, s, a, g):
        self.name = name
        self.size = s
        self.assoc = a
        self.groups = g
        self.histlen = 0

    def __str__(self):
        s = self.name + '\n'
        s += "size:%d\n" % self.size
        s += "assoc:%d\n" % self.assoc
        s += "groups:%d\n" % self.groups
        s += "histlen:%d\n" % self.histlen
        return s

class SimConfig:
    def __init__(self, name, tq_size, tq_ooo, salloc, iotlbrepl, arch_type):
        self.cfg_l = dict()
        self.name = name
        self.tq_size = tq_size
        self.tq_ooo = tq_ooo;
        self.salloc = salloc
        self.iotlbrepl = iotlbrepl
        self.testname = None
        self.tnt_num = 0
        self.did_order = None
        self.consec_transl = cfg.DEFAULT_CONSEC_TRANSL
        self.arch_type = arch_type
        self.link_gbps = cfg.DEFAULT_LINK_BW_GBPS
        if arch_type == "base":
            self.tq_ooo = True
            self.salloc = False
        elif arch_type == "part_io":
            self.tq_ooo = False
            self.salloc = False
        elif arch_type == "part_ooo":
            self.tq_ooo = True
            self.salloc = False
        self.log_dir = cfg.SIOPERF_RUN_LOG_DIR

    def AddConfig(self, cache_cfg):
        self.cfg_l[cache_cfg.name] = cache_cfg

    def ReplicateForWorkloads(self, workload_cfg):
        new_cfg_l = list()
        for tnt_num in workload_cfg.tenant_num_l:
            for bw in workload_cfg.link_gbps_l:
                for test_name in workload_cfg.test_name_l:
                    new_cfg = copy.deepcopy(self)
                    new_cfg.did_order = workload_cfg.tenant_order
                    new_cfg.tnt_num = tnt_num
                    new_cfg.testname = test_name
                    new_cfg.consec_transl = workload_cfg.consec_transl
                    new_cfg.link_gbps = bw
                    new_cfg_l.append(new_cfg)
        dbg.print_info("Replicated HW Configuration for %d SW configuration" % len(new_cfg_l))
        return new_cfg_l

    def setLogDir(self, log_dir):
        self.log_dir = log_dir

    def GetCacheCfgSuff(self):
        suff = "_iotlb_" + str(self.cfg_l["IOTLB"].size) + "_" + str(self.cfg_l["IOTLB"].assoc) + "_" + self.iotlbrepl
        suff += "_l2_" + str(self.cfg_l["L2PAGE"].size)  + "_" + str(self.cfg_l["L2PAGE"].assoc)
        suff += "_l3_" + str(self.cfg_l["L3PAGE"].size) + "_" + str(self.cfg_l["L3PAGE"].assoc)
        suff += "_pref_" + str(self.cfg_l["PREFETCH"].size) + "_" + str(self.cfg_l["PREFETCH"].histlen) + "_" + str(self.cfg_l["PREFETCH"].assoc)
        return suff

    def GetName(self):
        name = self.name
        name += self.GetCacheCfgSuff()
        name += "_%s" % self.GetDIDOrderSuff()
        return name

    def GetDIDOrderSuff(self):
        consec_transl_suff = "%d" % self.consec_transl
        order_suff = "%s%s" % (self.did_order, consec_transl_suff)

        return order_suff

    def GetDIDOrderSuffPaper(self):
        consec_transl_suff = "%d" % (self.consec_transl/3)
        order_suff = "%s%s" % (self.did_order.upper(), consec_transl_suff)

        return order_suff

        

    def GetTracePrefix(self):
        pref = "%s_tnt_%d_%s" % (self.testname, self.tnt_num, self.GetDIDOrderSuff())
        
        return pref

    def GetUniqueName(self):
        name = self.GetTracePrefix()
        name += "_%s_tq_%d" % (self.arch_type, self.tq_size)
        order_cfg = "ooo" if self.tq_ooo else "io"
        name += "_%s" % order_cfg
        name += self.GetCacheCfgSuff()
        
        if self.link_gbps != cfg.DEFAULT_LINK_BW_GBPS:
            name += "_%dgbps" % self.link_gbps

        return name
        
    def GetLogName(self):
        return self.GetUniqueName() + ".log"

    def GetLogPath(self):
        return os.path.join(self.log_dir, self.GetLogName())

    def GetConfigPath(self):
        cfgname = self.GetUniqueName() + ".cfg"
        return os.path.join(config.SIOPERF_CONFIG_DIR, cfgname)

    def GetLabel(self, ispaper):
        if ispaper:
            # For the paper
            lbl = ""
            for cname in self.cfg_l:
                c_cfg = self.cfg_l[cname]
                if c_cfg.name == "IOTLB":
                    lbl += "IOTLB: %s entries, %d ways, %s" % (c_cfg.size, c_cfg.assoc, self.iotlbrepl.upper())
                else:
                    continue
        else:
            lbl = ""
            for cname in self.cfg_l:
                c_cfg = self.cfg_l[cname]
                if cname == "PREFETCH":
                    continue
                lbl += "%s %d:%d:%d" % (c_cfg.name, c_cfg.size, c_cfg.assoc, c_cfg.groups)
                if c_cfg.name == "IOTLB":
                    lbl += " %s" % self.iotlbrepl
                lbl += ", "
            lbl += "Pref: %d-%d-%d, " % (self.cfg_l["PREFETCH"].size, self.cfg_l["PREFETCH"].histlen, self.cfg_l["PREFETCH"].assoc)
            lbl += "TQ: %d " % self.tq_size
            if self.tq_ooo:
                lbl += "ooo "
            else:
                lbl += "io"

            if self.salloc:
                lbl += ", salloc"

        return lbl

    def WriteConfig(self):
        f = open(self.GetConfigPath(), 'w')
        for cname in self.cfg_l:
            f.write(str(self.cfg_l[cname]))
            print >> f, "//" + 32*'-'
        f.close()

class SimConfigGroup:
    def __init__(self):
        self.cfg_l = list()
        self.name_l = list()


    def AddConfig(self, cfg):
        if not self.InGroup(cfg):
            self.cfg_l.append(cfg)
            self.name_l.append(cfg.GetUniqueName())
        else:
            pass

    def AddListOfConfig(self, cfg_l):
        for cfg in cfg_l:
            self.AddConfig(cfg)

    def InGroup(self, cfg):
        return cfg.GetUniqueName() in self.name_l

    def GetConfigList(self):
        return self.cfg_l

    def ReplicateForWorkloads(self, workload_cfg):
        new_cfg_l = list()
        for cfg in self.cfg_l:
            new_cfg_l += cfg.ReplicateForWorkloads(workload_cfg)
        new_name_l = [cfg.GetUniqueName() for cfg in new_cfg_l]
        
        self.cfg_l = new_cfg_l    
        self.name_l = new_name_l

    def setLinkBW(self, gbps):
        for cfg in self.cfg_l:
            cfg.link_gbps = gbps

    def setLogDir(self, log_dir):
        [cfg.setLogDir(log_dir) for cfg in self.cfg_l]


def GenIOTLBConfigList(hwconfig):
    # Configurations
    tq_size = 1
    base_tq_size = tq_size
    repl = "lru"
    iotlb_size_l = hwconfig.iotlb_size_l
    cfg_l = list()
    for iotlb_s in iotlb_size_l:
        base_cfg = SimConfig("base_tq_%d_ooo" % base_tq_size, base_tq_size,
                                    tq_ooo=True, salloc=False, iotlbrepl=repl, arch_type="base")
        base_cfg.AddConfig(CacheConfig("CC",     1536, 1536, 1))
        if hwconfig.iotlb_fullassoc:
            iotlb_assoc = iotlb_s
        else:
            iotlb_assoc = config.DEFAULT_IOTLB_ASSOC
        base_cfg.AddConfig(CacheConfig("IOTLB",  iotlb_s, iotlb_assoc, 1))
        base_cfg.AddConfig(CacheConfig("L2PAGE", 512, 16, 1))
        base_cfg.AddConfig(CacheConfig("L3PAGE", 1024, 16, 1))
        pb_size = 0
        prefetch_cfg = CacheConfig("PREFETCH", pb_size, 0, 0)
        prefetch_cfg.assoc = 32     # doesn't matter since PB size is 0
        prefetch_cfg.histlen = 2
        base_cfg.AddConfig(prefetch_cfg)

        cfg_l.append(base_cfg)

    return cfg_l


def makeConfigListForWorkload(cfg_l, workload_cfg):
    simconfig_group = SimConfigGroup()
    simconfig_group.AddListOfConfig(cfg_l)
    simconfig_group.ReplicateForWorkloads(workload_cfg)
    cfg_l = simconfig_group.GetConfigList()

    return cfg_l
