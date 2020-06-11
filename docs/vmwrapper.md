Tool **`vmwrapper`** is a Python script located in a *src/tools* folder.

# Description
The tool enables to easily perform operation required for creating/configuring/managing VMs. `vmwrapper` requires at least two options -
`--vmtype` and `--action`. The first identifies the type of a VM and its parameters, while the second one determines what should
be done with a VM. Default VMs' parameters are specified in *src/lib/config.py* Python file and **__must__** match parameters specified in
*src/scripts/vmconfig* bash file.

# Options
Parameters and their description can be checked by running `vmwrapper -h`. In addition to the required `--vmtype` and `--action` parameters,
optional parameters allow to run a VM with GUI, log in on a VM under a specific user name or perform an action for a VM with particular ID
(applicable for L2/client VMs, where ID either *base* or a non-negative integer).

# Examples
To create an image for a L2 Base VM:
```
vmwrapper --vmtype l2 --action createbaseimage
```
To login onto a Client VM 3 as the`root` user:
```
vmwrapper --vmtype client --action login --username root
```
To launch an L1 VM with GUI for configuration:
```
vmwrapper --vmtype l1 --action launch --graphic
```
