[TOC]

# General

This document outlines some CPU- and threading-specific information for running
all executables (`nr-softmodem`, `lte-softmodem`, `nr-uesoftmodem`,
`lte-uesoftmodem`, `nr-cuup`, called softmodems for short in the following).

## 4G-specific notes

It is still recommend to run eNB/lteUE using `sudo`. The below comments on
capabilities apply in general as well; however, 4G executable might not warn
about missing capabilities or just fail.

## 5G-specific notes

_None_.

# CPU

OAI used to try to set the minimum CPU-DMA latency to 2 us by writing to
`/dev/cpu_dma_latency`. However, it is unclear if this has a significant
effect, given that on systems I investigated, that value already 4. Further, in
containerized workloads, it might not be possible to set this at all. We assume
the user of OAI sets this value before starting OAI. See the [Linux kernel
documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpuidle.html)
for more information.

***TODO reviewer: ok? put back***

OAI used to try to set the minimum frequency of cores running OAI to the
maximum frequency. We assume that the user sets CPU frequency policies
accordingly; in fact, tutorials generally suggest to either set a performance
CPU governor, or disable any sleep states at all. Hence, setting these
low-level parameters seems useless, and we assume the user of OAI sets this
value before starting OAI. See the [Linux kernel
documentation](https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html)
for more information.

***TODO reviewer: ok? put back***

# Memory

OAI used to try to lock all its virtual address space into RAM, preventing that
memory from being paged to the swap area. Similarly to CPU issues, the process
might not have the rights to request this, especially in containerized
workloads. Currently, we assume the user of OAI sets this value before starting
OAI.

***TODO reviewer: ok? put back***

# Capabilities

Historically, all softmodems are executed as `root`, typically using `sudo`.
This remains a possibility, but we do not recommend it due to security
considerations. Rather, consider giving specific capabilities as outlined
below.  Read `capabilities(7)` for more information on each of the below
capabilities.

Refer to any of the docker-compose files under `ci-scripts/` to see how to give
capabilities in docker. If you run from source, you can use `capsh` to run a
process with specific capabilities.

## General capabilities

- `SYS_NICE`: required by all softmodems to assign a higher priority to
  threads to increase the likelihood that the Linux scheduler schedules a
  thread for CPU time. Also, in some configuration, CPU pinning is employed to
  ensure consistent performance, also known as setting a CPU affinity

  This is the only capability that is necessary by default when running any
  softmodem, for setting real-time requirements on processing threads. To allow
  to run without these requirements, the softmodems check if `SYS_NICE` is
  available, and skip any thread priority and affinity settings if the
  capability is not available.  This allows to run any softmodem without user
  rights in RFsim; you can see this by either observing a corresponding warning
  at the beginning of the execution, or the fact that no affinity/default
  priority is set for new threads.

- `NET_ADMIN`: Required at the UE to open the interface `oaitun_ue1` or eNB/gNB
  in noS1 mode. 5G executables will throw an error if they cannot create or
  modify an interface, but will continue running. It is therefore possible to
  run without this capability, but you cannot inject any traffic into the
  system.

## Capabilities with DPDK

You need these capabilities when running with 7.2 fronthaul, which uses the
xran library with a dependency on DPDK.

- `IPC_LOCK`
- `SYS_RESOURCE`
- `NET_RAW`

## Capabilities with UHD

You don't need any specific capabilities.

## Capabilites with AW2S

You don't need any specific capabilities.
