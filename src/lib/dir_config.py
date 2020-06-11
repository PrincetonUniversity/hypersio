import os

HYPERSIO_HOME   = os.environ["HYPERSIO_HOME"]

SRC_DIR             = os.path.join(HYPERSIO_HOME, "src")
SIOPERF_DIR         = os.path.join(SRC_DIR, "sioperf")
BUILD_DIR           = os.path.join(HYPERSIO_HOME, "build")
SIOPERF_CONFIG_DIR  = os.path.join(BUILD_DIR, "perf_configs") 
LOG_DIR             = os.path.join(BUILD_DIR, "qemu_logs")
TRACEGEN_LOG_DIR    = os.path.join(BUILD_DIR, "trace_gen_logs")
VMSTORAGE_DIR   = os.path.join(BUILD_DIR, "vm_images")
LOCALQEMU_DIR   = os.path.join(BUILD_DIR, "qemu-3.0.0/build/x86_64-softmmu")
DEFAULTQEMU_DIR = "/usr/bin"
ISO_DIR = BUILD_DIR