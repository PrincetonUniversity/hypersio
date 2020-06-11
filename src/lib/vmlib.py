import re, sys, os, time, socket, shutil
import subprocess as subp
import execlib as ex 
import dbglib as dbg
import config as cfg
import common as common

FNULL = open(os.devnull, 'w')

class VirtualMachine(object):
    def __init__(self, name, host_machine, drive_name):
        self.name = name
        self.host_machine = host_machine
        self.ssh_name = None
        self.max_boot_time = 300
        self.remote_script_name = self.name + ".sh"
        self.drive_path = os.path.join(cfg.DRIVE_DIR[host_machine], drive_name)
        self.memory = "8G"
        self.use_kvm = True
        self.use_graphic = False
        self.vm_proc = None
        self.log_file_name = "vmlog_%s.log" % self.name
        self.flog_path = os.path.join(cfg.LOG_DIR, self.log_file_name)
        self.flog = None

    def WaitForBoot(self):
        dbg.print_info("Waiting for '%s' to boot" % self.name)
        retry_interval = 2
        retry_num = self.max_boot_time / retry_interval
        rv = ex.executeCmdBlockWithRetry("hostname", retry_num, retry_interval, self.flog, self.ssh_name, self.remote_script_name)
        if rv != 0:
            dbg.print_error("Waiting for boot failed for '%s'" % self.name)
        return rv

    def GetLaunchCmd(self):
        opt_kvm = "--enable-kvm -cpu host" if self.use_kvm else ""
        opt_graphic = "--nographic" if not self.use_graphic else ""

        qemu_exec = os.path.join(cfg.QEMU_DIR[self.host_machine], cfg.QEMU_SYSTEM)
        cmd = "sudo %s -m %s -drive file=%s,format=raw %s %s" % \
              (qemu_exec, self.memory, self.drive_path, opt_kvm, opt_graphic)

        return cmd

    def Launch(self):
        if self.flog == None:
            self.flog = open(self.flog_path, 'w')
        dbg.print_info("Launching '%s'" % self.name)
        cmd = self.GetLaunchCmd()
        common.logCmd(self.flog, cmd)
        proc = ex.executeCmd(cmd, self.flog, self.host_machine, self.remote_script_name)
        self.vm_proc = proc
        # reset terminal for correct input
        # cmd = "reset"
        # ex.executeLocalBlock(cmd, sys.stdout)

    def ExecuteBlock(self, cmd):
        p = self.ExecuteNonBlock(cmd, subp.PIPE)
        output, error = p.communicate()
        rv = p.returncode
        return (rv, output, error)

    def ExecuteBlockCheck(self, cmd):
        (rv, output, error) = self.ExecuteBlock(cmd)
        if rv != 0:
            dbg.print_error("'%s' failed on '%s'" % (cmd, self.name))
            dbg.print_error("%s" % error)
        print >> self.flog, cmd
        print >> self.flog, "OUTPUT: %s" % output
        print >> self.flog, "ERROR: %s" % error
        return (rv, output, error)

    def ExecuteNonBlock(self, cmd, flog):
        p = ex.executeRemotely(cmd, flog, "root", self.ssh_name, "", self.remote_script_name)
        return p

    def Poweroff(self):
        self.ExecuteBlock("poweroff")
        dbg.print_info("Waiting until %s is powered off" % self.name)
        if self.vm_proc != None:
            self.vm_proc.wait()

    def GetName(self):
        return self.name

class ClientVM(VirtualMachine):
    def __init__(self, client_id):
        if client_id == "base":
            name = "client_base"
            drive_name = cfg.CLIENT_BASEIMG
            self.fwd_port = cfg.CLIENT_FWDPORT_BASE
        else:
            name = "client_%s" % client_id
            drive_name = "%s_%s.img" % (cfg.CLIENT_VMPREFIX, client_id)
            self.fwd_port = cfg.CLIENT_FWDPORT_BASE + int(client_id)

        super(ClientVM, self).__init__(name, "localhost", drive_name)
        self.client_id = client_id
        self.mac = "52:54:00:12:34:99"
        self.ssh_name = "localhost -p %d" % self.fwd_port

    def GetLaunchCmd(self):
        cmd = super(ClientVM, self).GetLaunchCmd()
        # Tap interface
        cmd += " -device e1000,netdev=nettap,mac=%s -netdev tap,id=nettap" % self.mac
        # Management Interface
        cmd += " -net user,hostfwd=tcp::%d-:22 -net nic" % self.fwd_port
        return cmd

class BareVM(VirtualMachine):
    def __init__(self, vm_id, syscfg):
        if vm_id == "base":
            name = "barevm_base"
            drive_name = cfg.BAREVM_BASEIMG
            self.fwd_port = cfg.BAREVM_FWDPORT_BASE
            vf_df = "%02x.0" % syscfg.nic_cfg.bus
            self.vm_ifip = "172.16.0.%d" % cfg.BAREVM_BASEIP
        else:
            name = "barevm_%s" % vm_id
            drive_name = "%s_%s.img" % (cfg.BAREVM_VMPREFIX, vm_id)
            self.fwd_port = cfg.BAREVM_FWDPORT_BASE + int(vm_id)
            id_for_pf = vm_id % cfg.X540_VF_PER_PF
            pf_group = int(vm_id / cfg.X540_VF_PER_PF)
            (vf_bus, vf_dev, vf_fun) = syscfg.nic_cfg.getVFbyID(vm_id)
            vf_df = "%02x.%x" % (vf_dev, vf_fun)
            self.vm_ifip = "172.16.0.%d" % (cfg.BAREVM_BASEIP + int(vm_id))
        self.bdf = "0000:%02x:%s" % (syscfg.nic_cfg.bus, vf_df)

        super(BareVM, self).__init__(name, "localhost", drive_name)
        self.vm_id = vm_id
        self.ssh_name = "localhost -p %s" % self.fwd_port
        self.memory = "400M"
        self.syscfg = syscfg


    def CopyBaseImg(self):
        base_img_path = os.path.join(cfg.DRIVE_DIR[self.host_machine], cfg.BAREVM_BASEIMG)
        (src_path, dst_path) = (base_img_path, self.drive_path)
        dbg.print_info("Copying %s to %s" % (src_path, dst_path))
        shutil.copy(src_path, dst_path)

    def GetLaunchCmd(self):
        cmd = super(BareVM, self).GetLaunchCmd()
        if self.syscfg.useHugePages():
            huge_page_opt = "-mem-path /dev/hugepages"
        else:
            huge_page_opt = ""
        cmd += " -net user,hostfwd=tcp::%d-:22 -net nic" % self.fwd_port
        cmd += " -device vfio-pci,host=%s " % self.bdf
        cmd += huge_page_opt
        return cmd

    def Launch(self):
        # Unbinding PCI device
        dbg.print_info("Unbinding VF @%s" % self.bdf)
        cmd = "sudo modprobe vfio-pci"
        rv = ex.executeCmdBlockReturnCode(cmd, self.flog, "localhost", self.remote_script_name)
        if rv != 0:
            dbg.print_info("'%s' failed on machine '%s'" % (cmd, self.name))
            return rv

        sys_path = "/sys/bus/pci/devices/%s/driver/unbind" % self.bdf
        sys_path = sys_path.replace(':', '\\:')
        cmd = "sudo sh -c \"echo '%s' > %s\"" % (self.bdf, sys_path)
        ex.executeCmdCheckReturnCode(cmd, self.flog, "localhost", self.remote_script_name)

        sys_path = "/sys/bus/pci/drivers/vfio-pci/new_id"
        sys_path = sys_path.replace(':', '\\:')
        vendor_device = self.syscfg.getVFVendorDevice()
        cmd = "sudo sh -c \"echo '%x %x' > %s\"" % (vendor_device[0], vendor_device[1], sys_path)
        ex.executeCmdCheckReturnCode(cmd, self.flog, "localhost", self.remote_script_name)

        super(BareVM, self).Launch()

    def Configure(self):
        # 1 - Change host name
        # 2 - Configure a passthrough interface with a static IP\n
        # Note: new netplan network manager on Ubuntu 18.04
        cmd = """echo "%s" > /etc/hostname
echo "127.0.0.1     localhost" > /etc/hosts
echo "127.0.1.1     %s" >> /etc/hosts
echo "network:"                 > /etc/netplan/01-netcfg.yaml
echo "  version: 2"             >> /etc/netplan/01-netcfg.yaml
echo "  renderer: networkd"     >> /etc/netplan/01-netcfg.yaml
echo "  ethernets:"             >> /etc/netplan/01-netcfg.yaml
echo "    ens3:"                >> /etc/netplan/01-netcfg.yaml
echo "      dhcp4: yes"         >> /etc/netplan/01-netcfg.yaml
echo "    ens4:"                >> /etc/netplan/01-netcfg.yaml
echo "      addresses: [%s/24]" >> /etc/netplan/01-netcfg.yaml
netplan apply""" % (self.name, self.name, self.vm_ifip)
        p = ex.executeRemotely(cmd, sys.stdout, "root", self.ssh_name, "", self.remote_script_name)
        p.wait()
        rv = p.returncode
        return rv

    def Prepare(self):
        self.CopyBaseImg()
        self.Launch()
        self.WaitForBoot()
        self.Configure()
        self.Poweroff()


class L2VM(VirtualMachine):
    def __init__(self, vm_id):
        if vm_id == "base":
            name = "l2_base"
            drive_name = cfg.L2_BASEIMG
            self.fwd_port = cfg.L2_FWDPORT_BASE
            devnum = "%02x" % cfg.FIRST_NIC_PCIEDEV
        else:
            name = "%s_%s" % (cfg.L2_VMPREFIX, vm_id)
            drive_name = name + ".img"
            self.fwd_port = cfg.L2_FWDPORT_BASE + int(vm_id)
            devnum = "%02x" % (cfg.FIRST_NIC_PCIEDEV + int(vm_id))
        self.bdf = "0000:00:%s.0" % devnum

        super(L2VM, self).__init__(name, "ubul1", drive_name)
        self.vm_id = vm_id
        self.ssh_name = "localhost -p %s" % self.fwd_port
        self.vm_ifip = "172.16.%s.20" % self.vm_id

    def GetLaunchCmd(self):
        cmd = super(L2VM, self).GetLaunchCmd()
        cmd += " -net user,hostfwd=tcp::%d-:22 -net nic" % self.fwd_port
        cmd += " -device vfio-pci,host=%s,id=netVF" % self.bdf
        return cmd

    def Launch(self, flog):
        if socket.gethostname() != cfg.L1_VM_NAME:
            dbg.print_error("L2 VM can be launched only from inside of '%s' machine")
            return False
        else:
            # Unbinding PCI device
            cmd = "modprobe vfio-pci"
            rv = ex.executeCmdBlockReturnCode(cmd, flog, "localhost", self.remote_script_name)
            if rv != 0:
                dbg.print_info("'%s' failed on machine '%s'" % (cmd, self.name))
                return rv

            cmd = "echo '\"%s\"' > /sys/bus/pci/devices/%s/driver/unbind" % (self.bdf, self.bdf)
            rv = ex.executeCmdBlockReturnCode(cmd, flog, "localhost", self.remote_script_name)
            if rv != 0:
                dbg.print_error("'%s' failed on machine '%s'" % (cmd, self.name))
                return rv

            cmd = "echo '\"8086 100e\"' > /sys/bus/pci/drivers/vfio-pci/new_id"
            rv = ex.executeCmdBlockReturnCode(cmd, flog, "localhost", self.remote_script_name)
            if rv != 0:
                dbg.print_error("'%s' failed on machine '%s'" % (cmd, self.name))
                return rv

            super(L2VM, self).Launch(flog)

    def Configure(self):
        # 1 - Change host name
        # 2 - Configure a passthrough interface with a static IP\n
        # Note: new netplan network manager on Ubuntu 18.04
        cmd = """echo "%s" > /etc/hostname
echo "127.0.0.1     localhost" > /etc/hosts
echo "127.0.1.1     %s" >> /etc/hosts
echo "network:"                 > /etc/netplan/01-netcfg.yaml
echo "  version: 2"             >> /etc/netplan/01-netcfg.yaml
echo "  renderer: networkd"     >> /etc/netplan/01-netcfg.yaml
echo "  ethernets:"             >> /etc/netplan/01-netcfg.yaml
echo "    ens3:"                >> /etc/netplan/01-netcfg.yaml
echo "      dhcp4: yes"         >> /etc/netplan/01-netcfg.yaml
echo "    ens4:"                >> /etc/netplan/01-netcfg.yaml
echo "      addresses: [%s/24]" >> /etc/netplan/01-netcfg.yaml
netplan apply""" % (self.name, self.name, self.vm_ifip)
        p = ex.executeRemotely(cmd, sys.stdout, "root", self.ssh_name, "", self.remote_script_name)
        p.wait()
        rv = p.returncode
        return rv


class L1VM(VirtualMachine):
    def __init__(self):
        name = cfg.L1_HOST_NAME    
        drive_name = cfg.L1_BASEIMG
        self.fwd_port = cfg.L1_FWDPORT_BASE
        self.cores = 8
        self.memory = "16G"
        self.tap_num = 0
        super(L1VM, self).__init__(name, "localhost", drive_name)
        self.ssh_name = "localhost -p %s" % self.fwd_port
        self.trace_events = True

    def GetLaunchCmd(self):
        cmd = super(L1VM, self).GetLaunchCmd()
        # Specify chipset
        cmd += " -M q35 -device intel-iommu"
        # Host configuration
        cmd += " -smp %d,sockets=1,cores=%d,threads=1" % (self.cores, self.cores)
        # NICs and Tap IFs
        dev_cmd = ""
        tap_cmd = ""
        for i in range(self.tap_num):
            net_name = "net%d" % i
            pcie_addr = "%02x.0" % (cfg.FIRST_NIC_PCIEDEV + i)
            dev_cmd += " -device e1000,netdev=%s,addr=%s" % \
                       (net_name, pcie_addr)
            tap_cmd += " -netdev tap,id=%s" % net_name
        cmd += dev_cmd
        cmd += tap_cmd
        # Trace events
        trace_file_path = os.path.join(cfg.LOG_DIR, cfg.L1_TRACE_FILE)
        cmd += " -trace events=%s,file=%s" % \
               (cfg.L1_TRACE_EVENTS_FILE, trace_file_path)
        # Management IF
        cmd += " -device e1000,netdev=netcfg,addr=%s" % cfg.L1_CFGNIC_PCIE_ADDR
        cmd += " -netdev user,hostfwd=tcp::%s-:22,id=netcfg" % self.fwd_port

        return cmd

class VMGroup(object):
    def __init__(self, vm_type):
        self.vm_l = list()
        self.vm_num = len(self.vm_l)
        self.vm_type = vm_type

    def AddVMByID(self, id_l):
        if self.vm_type == "client":
            for i in id_l:
                self.vm_l.append(ClientVM(i))
        elif self.vm_type == "l2":
            for i in id_l:
                self.vm_l.append(L2VM(i))
        elif self.vm_type == "l1":
            self.vm_l.append(L1VM())
        elif self.vm_type == "bare":
            for i in id_l:
                self.vm_l.append(BareVM(i))
        else:
            dbg.print_error("Uknown VM type: %s" % vm_type)

    def Launch(self):
        dbg.print_info("Launching VM group")
        print len(self.vm_l)
        for v in self.vm_l:
            v.Launch()
            v.WaitForBoot()

    def Reboot(self):
        for v in self.vm_l:
            dbg.print_info("Rebooting %s" % v.GetName())
            v.ExecuteBlock("reboot")
            v.WaitForBoot()
        dbg.print_info("All %d VMs were rebooted" % self.vm_num)

    def WaitForBoot(self):
        was_failure = False
        for v in self.vm_l:
            rv = v.WaitForBoot()
            if rv != 0:
                dbg.print_error("Failed waiting for %s to boot!" % v.GetName())
                was_failure = True
        
        rv = 1 if was_failure else 0
        return rv

    def Poweroff(self):
        self.ExecuteAllBlock("poweroff")

    def ExecuteAllBlock(self, cmd):
        pid_vmname_d = dict()
        proc_l = list()
        for v in self.vm_l:
            proc = v.ExecuteNonBlock(cmd, subp.PIPE)
            proc_l.append(proc)
            pid_vmname_d[proc.pid] = v.name

        err_proc_l = ex.waitProcGroupAndCheckExitCode(proc_l)
        if len(err_proc_l) > 0:
            for proc in err_proc_l:
                dbg.print_error("'%s' failed on %s" % (cmd, pid_vmname_d[proc.pid]))
                output, error = proc.communicate()
                dbg.print_error(error)
        else:
            dbg.print_info("'%s' was executed successfully on %d machines!" % (cmd, len(self.vm_l)))

def L2VMGroup(VMGroup):
    def __init__(self, vm_l):
        super(L2VVGroup, self).__init__(vm_l)

    def CheckConfig(self):
        for v in self.vm_l:
            (rv, output, err) = v.ExecuteBlockCheck("hostname", None)
            name = output
            name = name.strip()
            if name != v.GetName():
                dbg.print_warning("%s wasn't configued as expected")
                v.Configure()
                v.ExecuteNonBlock("reboot", sys.stdout)
                v.WaitForBoot()

# def buildVMGroup(vm_type, id_l):
#     vm_gr = None
#     vm_l = list()
#     dbg.print_info("Building VM group")
#     if vm_type == "client":
#         for i in id_l:
#             vm_l.append(ClientVM(i))
#     elif vm_type == "l2":
#         for i in id_l:
#             vm_l.append(L2VM(i))
#     elif vm_type == "l1":
#         dbg.print_info("Type L1")
#         vm_l.append(L1VM())
#     else:
#         dbg.print_error("Uknown VM type: %s" % vm_type)

#     return VMGroup(vm_l)

def getMAC(vm_name, mac_type):
    p = subp.Popen(['virsh', 'dumpxml', vm_name], stdout=subp.PIPE, stderr=subp.PIPE)
    out, err = p.communicate()
    if_type = "network" if mac_type == "nat" else "hostdev";
    sp = re.compile("interface type='%s'.*\n.*mac.*(([0-9a-f][0-9a-f]:){5}..).*" % if_type)
    m = sp.search(out)
    if m != None:
        return  m.group(1)

    return None

def getIP(mac):
    p = subp.Popen(['arp'], stdout=subp.PIPE, stderr=subp.PIPE)
    out, err = p.communicate()
    sp = re.compile("(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}).*%s.*" % mac)
    m = sp.search(out)
    if m != None:
        return m.group(1)

    return None


def getNATIP(vm_name):
    mac = getMAC(vm_name, "nat")
    ip = getIP(mac)
    return ip

def getVFIP(vm_name):
    nat_ip = getNATIP(vm_name)
    vf_mac = getMAC(vm_name, "vf")

    fname = "tmp"
    f = open(fname, 'w')
    ex.executeCmdBlock("ifconfig -a", f, nat_ip)
    f.close()

    vf_ip = None
    f = open(fname, 'r')
    for l in f:
        m = re.search(r'inet addr:(172.[\d|\.]+)', l)
        if m != None:
            vf_ip = m.group(1)

    f.close()

    return vf_ip

def getHostIP(hostname):
    if hostname == "hanoi4.princeton.edu":
        ip = "172.16.0.10"
    elif hostname == "moscow.princeton.edu":
        ip = "172.16.0.11"
    else:
        ip = getVFIP(hostname)
        if ip == None:
            dbg.print_error("Can't determine IP for %s" % hostname)

    return ip
