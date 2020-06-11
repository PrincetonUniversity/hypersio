import sys, re
import execlib as ex
import dbglib as dbg
import common, subprocess

class HostConfig:
    def __init__(self, hname, ifname, nwname):
        self.host_name = hname
        self.if_name = ifname
        self.nw_name = nwname

class ContainerConfig:
    def __init__(self, hname, name, image):
        self.name = name
        self.host_name = hname
        self.host_port = ""
        self.image = image
        self.network = None
        self.command = None
        self.sh_script = "run_%s_%s.sh" % (hname, name)

    def getRunCmd(self):
        cmd = "docker run -i --name %s" % self.name
        if self.network != None:
            cmd += " --network %s" % self.network
        cmd += " %s" % self.image
        if self.command != None:
            cmd += " %s" % self.command
        return cmd


def tryExecute(cmd, hostname, valid_error_str):
    # dbg.print_info("cmd: %s, host: %s, valid error: %s" % \
                   # (cmd, hostname, valid_error_str))
    script_name = "%s_cmd.sh" % hostname
    p = ex.executeCmd(cmd, subprocess.PIPE, hostname, script_name)
    output, error = p.communicate()
    if p.returncode != 0:
        if common.isStrInText(valid_error_str, error):
            dbg.print_warning(error)
        else:
            dbg.print_error(error)
            return 1

    return 0

def leaveSwarm(h_cfg):
    cmd = "docker swarm leave -f"
    valid_error_str = "This node is not part of a swarm"
    rv = tryExecute(cmd, h_cfg.host_name, valid_error_str)
    if rv == 0:
        dbg.print_info("%s: left a previous swarm" % h_cfg.host_name)

    return rv


def initSwarm(h_cfg):
    if leaveSwarm(h_cfg) != 0:
        return None
        
    # Initialize swarm on a host
    cmd = "docker swarm init --advertise-addr %s" % h_cfg.if_name
    p = ex.executeCmd(cmd, subprocess.PIPE, h_cfg.host_name, "docker_server.sh")
    output, error = p.communicate()
    if p.returncode != 0:
        dbg.print_error("Can not initialize swarm on %s" % h_cfg.host_name)
        dbg.print_error(error)
        return None

    # Take only join command from init output
    m = re.search("docker swarm join --token .*", output)
    if m != None:
        join_cmd = m.group(0)
    else:
        join_cmd = None
    
    return join_cmd

def joinSwarm(h_cfg, join_cmd):
    if leaveSwarm(h_cfg) != 0:
        return 1

    p = ex.executeCmd(join_cmd, subprocess.PIPE, h_cfg.host_name, "docker_client.sh")
    output, error = p.communicate()
    if p.returncode != 0:
        dbg.print_error(error)
        return 1

    return 0

def createSwarm(h1_cfg, h2_cfg):
    # Create swarm on server's host
    dbg.print_info("%s: initializing swarm" % h1_cfg.host_name)
    join_cmd = initSwarm(h1_cfg)
    if join_cmd == None:
        return 1

    # Join to a swarm on client's host
    dbg.print_info("%s: joining swarm" % h2_cfg.host_name)
    rv = joinSwarm(h2_cfg, join_cmd)
    return rv

def removeNetwork(h_cfg):
    cmd = "docker network rm %s" % h_cfg.nw_name
    valid_error_str = "No such network"
    rv = tryExecute(cmd, h_cfg.host_name, valid_error_str)
    if rv == 0:
        dbg.print_info("%s: removed network '%s'" % \
                        (h_cfg.host_name, h_cfg.nw_name))

    return rv

def removeNetworkOld(h_cfg):
    no_network_str = "No such network"

    # Remove network
    cmd = "docker network rm %s" % h_cfg.nw_name
    p = ex.executeCmd(cmd, subprocess.PIPE, h_cfg.host_name, "remote_nw.sh")
    output, error = p.communicate()
    if p.returncode != 0:
        if common.isStrInText(no_network_str, error):
            dbg.print_warning("%s: '%s' network does not exist!" % \
                              (h_cfg.host_name, h_cfg.nw_name))
        else:
            dbg.print_error("%s: can not remove network '%s'!" % \
                            (h_cfg.nw_name, h_cfg.host_name))
            dbg.print_error(error)
            return 1
    else:
        dbg.print_info("Removed network '%s'" % h_cfg.nw_name)

    return 0

def createOverlayNetwork(h_cfg):
    if removeNetwork(h_cfg) != 0:
        return None

    dbg.print_info("%s: creating overlay network '%s'" % \
                   (h_cfg.host_name, h_cfg.nw_name))
    cmd = "docker network create --driver=overlay --attachable %s" % h_cfg.nw_name
    nw_id = None
    p = ex.executeCmd(cmd, subprocess.PIPE, h_cfg.host_name, "create_nw.sh")
    output, error = p.communicate()
    if p.returncode != 0:
        dbg.print_error("Can not create network '%s' on host '%s'" % \
                        (h_cfg.nw_name, h_cfg.host_name))
        dbg.print_error(error)
        return None
    else:
        nw_id = output

    return nw_id


def removeContainer(container_cfg):
    cmd = "docker rm %s" % container_cfg.name
    valid_error_str = "No such container"
    rv = tryExecute(cmd, container_cfg.host_name, valid_error_str)
    if rv == 0:
        dbg.print_info("%s: removed container '%s'" % \
                       (container_cfg.host_name, container_cfg.name))

    return rv

def runContainer(container_cfg, flog):
    rv = removeContainer(container_cfg)
    if rv != 0:
        return 1

    cmd = container_cfg.getRunCmd()
    p = ex.executeCmd(cmd, flog, container_cfg.host_name, container_cfg.sh_script)
    dbg.print_info("%s: container '%s' is running" % \
                  (container_cfg.host_name, container_cfg.name))
    return p

def stopContainer(container_cfg, flog):
    cmd = "docker container stop %s" % container_cfg.name
    ex.executeCmdBlock(cmd, flog, container_cfg.host_name, container_cfg.sh_script)