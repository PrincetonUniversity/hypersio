import re, os, sys, subprocess
# User modules
import dbglib as dbg
import execlib as ex
import config as cfg

def expandCommaSeparatedRanges(s):
    """
        's' format - comma separated ranges or values
            a-b,c,..,d,e-f,a etc.
        Includes the first and the last value in the range
    """
    range_list = s.split(',')
    values = list()
    for r in range_list:
        # range a-b
        if '-' in r:
            # start and end of the range can have
            # any number of digits
            ind = r.index('-')
            s = int(r[0:ind])
            e = int(r[ind+1:])
            values += (range(s,e+1))
        # single value
        else:
            values.append(int(r))
    return values

def expandIDOption(opt):
    """
        opt format - comma separated ranges or values
            a-b,c,..,d,e-f,a etc.
        Includes the first and the last value in the range
    """
    range_list = opt.split(',')
    ret_ids = list()
    for r in range_list:
        # range a-b
        if '-' in r:
            # start and end of the range can have
            # any number of digits
            ind = r.index('-')
            s = int(r[0:ind])
            e = int(r[ind+1:])
            ret_ids += (range(s,e+1))
        # single value
        else:
            ret_ids.append(int(r))
    return ret_ids

def devListFromTNum(tnum):
    first_nic_dev = 2
    # each NIC has different PCIe device number
    dev_l = range(first_nic_dev, first_nic_dev+tnum)
    return dev_l

def sidMatchFromDevList(dev_l):
    rv = '|'.join(hex(d<<3) for d in dev_l)
    return rv

def sidListFromDevList(dev_l):
    rv = [d<<3 for d in dev_l]
    return rv

def isStrInText(s, text):
    m = re.search(s, text)
    return m != None

def validateTrace(trace_path):
    max_line_w = 200

    flog = open(trace_path, 'r')

    cnt = 0
    for l in flog:
        if len(l) > max_line_w:
            dbg.print_error("Encountered a line longer than %d characters in %s" % (max_line_w, trace_path))
        cnt += 1

    dbg.print_info("Number of lines in %s: %d" % (trace_path, cnt))
    flog.close()


def logCmd(flog, cmd):
    flog.write("="*80+"\n")
    flog.write(cmd + "\n")
    flog.write("="*80+"\n")
    flog.flush()

def showFileDiff(fpath1, fpath2, flog_path):
    flog = open(flog_path, 'w')
    f1 = open(fpath1, 'r')
    f2 = open(fpath2, 'r')
    l1 = f1.readline()
    l2 = f2.readline()

    print >> flog, "File 1: %s" % fpath1
    print >> flog, "File 2: %s" % fpath2
    while l1 != "" and l2 != "":
        if l1 != l2:
            print >> flog, "File 1: %s" % l1.strip()
            print >> flog, "File 2: %s" % l2.strip()
            print >> flog
        l1 = f1.readline()
        l2 = f2.readline()
    if l1 == "" and l2 != "":
        print >> flog, "File 1 is longer"
    elif l1 != "" and l2 == "":
        print >> flog, "File 2 is longer"

    f1.close()
    f2.close()
    flog.close()

def getPercentileIndex(l, p):
    total = sum(l)
    tail_sum = 0
    for ind in range(len(l)-1,-1,-1):
        tail_sum += l[ind]
        if tail_sum > (1-p)*total:
            return ind + 1

def percFromValue(frac, total):
    return 100.*frac / total

def getDeviceAllBDF(keyword):
    output, _ = ex.executeCmdBlock("lspci", subprocess.PIPE, "localhost")
    output = output.split("\n")
    device_bdf_l = list()
    for devstr in output:
        if keyword.lower() in devstr.lower():
            # dbg.print_info("BDF info for %s:" % keyword)
            # dbg.print_info(devstr)
            match = re.match(r"([^\s]+:[^\s]+\.[^\s]) .*", devstr)
            if match:
                bdf = match.group(1)
                bdf_split_int_l = [int(v, 16) for v in re.split(":|\.", bdf)]
                device_bdf_l.append(bdf_split_int_l)
    return device_bdf_l

def getDeviceFirstBDF(keyword):
    device_bdf_l = getDeviceAllBDF(keyword)
    if len(device_bdf_l) > 0:
        return device_bdf_l[0]
    else:
        return None

def portForVM(vmtype, vmid):
    offset = 0 if vmid == "base" else int(vmid)
    port = cfg.VMTYPE_FWDPORTBASE_D[vmtype] + offset
    return port


def getQEMUFolder():
    if os.path.exists(cfg.LOCALQEMU_DIR):
        return cfg.LOCALQEMU_DIR
    else:
        return cfg.DEFAULTQEMU_DIR