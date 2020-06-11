import subprocess as subp
import shlex, sys, stat, os, socket, time, re
import dbglib as dbg
import config as cfg

def executeLocal(cmd, flog):
    cmd_split = shlex.split(cmd)
    p = subp.Popen(cmd_split, stdout=flog, stderr=flog)
    return p

def executeLocalBlock(cmd, flog):
    p = executeLocal(cmd, flog)
    p.wait()
    return p.returncode

def executeLocalBlockStdOut(cmd):
    return executeLocalBlock(cmd, sys.stdout)

def executeRemotely(cmd, flog, user, hostname, opt, script_name="./remote.bash"):
    script_path = os.path.join(cfg.BUILD_DIR, script_name)

    f = open(script_path, 'w')
    print >> f, "#!/bin/bash"
    print >> f, "ssh -oStrictHostKeyChecking=no %s@%s %s << 'ENDSSH'" % (user, hostname, opt)
    print >> f, cmd
    print >> f, "ENDSSH"
    f.close()

    st = os.stat(script_path)
    os.chmod(script_path, st.st_mode | stat.S_IEXEC)
    p = subp.Popen(script_path, stdout=flog, stderr=flog)
    return p

def executeCmdUser(cmd, flog, hostname, script_name, username):
    if (hostname == "localhost") or (hostname == socket.gethostname()):
        p = executeLocal(cmd, flog)
    else:
        p = executeRemotely(cmd, flog, username, hostname, "", script_name)
    
    return p

def executeCmd(cmd, flog, hostname, script_name):
    if (hostname == "localhost") or (hostname == socket.gethostname()):
        p = executeLocal(cmd, flog)
    else:
        p = executeRemotely(cmd, flog, cfg.VM_USERNAME, hostname, "", script_name)
    
    return p

def executeCmdBlock(cmd, flog, hostname, script_name="./remote.bash"):
    p = executeCmd(cmd, flog, hostname, script_name)
    return p.communicate()

def executeCmdBlockReturnCode(cmd, flog, hostname, script_name):
    p = executeCmd(cmd, flog, hostname, script_name)
    p.wait()
    return p.returncode

def executeCmdCheckReturnCode(cmd, flog, hostname, script_name):
    rv = executeCmdBlockReturnCode(cmd, flog, hostname, script_name)
    if rv != 0:
        dbg.print_error("'%s' failed on machine '%s'" % (cmd, hostname))
    return rv
    

def executeAndCheckOutput(cmd, flog, hostname, script_name, check_str):
    output, _ = executeCmdBlock(cmd, flog, hostname, script_name)
    m = re.search(check_str, output)
    return m != None

def executeCmdBlockWithRetry(cmd, max_retries, interval, flog, hostname, script_name):
    rv = executeCmdBlockReturnCode(cmd, flog, hostname, script_name)
    retry_cnt = 1
    while (rv != 0) and (retry_cnt <= max_retries):
        time.sleep(interval)
        dbg.print_info("Retry # %d" % retry_cnt)
        rv = executeCmdBlockReturnCode(cmd, flog, hostname, script_name)
        retry_cnt += 1
    return rv

def executeLocalGetOutput(cmd):
    p = executeLocal(cmd, subp.PIPE)
    (out, err) = p.communicate()
    return out

def waitProcGroupAndCheckExitCode(proc_l):
    failed_l = list()
    for p in proc_l:
        p.wait()
        if p.returncode != 0:
            failed_l.append(p)
    return failed_l

