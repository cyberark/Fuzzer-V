# Fuzzer-V

A [kafl](https://github.com/RUB-SysSec/kAFL) based fuzzer for the kernel components of Hyper-V.

# Goal
The fuzzer is designed to find vulnerabilities in the Hyper-V host components (ultimately finding a VM escape).

# Design

## Setup
Nested virtualization: physical ubuntu machine running (L0) running a QEMU Windows "host" VM (L1), which runs a Hyper-V Windows "guest" VM (L2).

![image info](/pictures/HyperVFuzz.drawio.png)

## Components
### L0
1. kafl
- generating inputs.
- transerring inputs to L1.
- collecting L1 kernel code coverage. 

### L1
1. Agent (C) <TODO: add name>
- receiving inputs from L0.
- tranferring inputs to L2.
2. HyperVPatcher (C driver)
- patching L1 kernel to clear background noise.

### L2
1. PSAgent (powershell)
- receiving inputs from L1.
- sending IOCTLs to HyperVAgent.
2. HyperVAgent (C driver)
- receiving IOCTLs from PSAgent.
- triggering code in L1 kernel (e.g. by VMBUS, port io).



# Variations
The fuzzer can be adapted to target the Hyper-V components of the guest machine, possibly leading to privilege escalation on Hyper-V VMs.

# License
Copyright (c) 2023 CyberArk Software Ltd. All rights reserved  
This repository is licensed under  Apache-2.0 License - see [`LICENSE`](LICENSE) for more details.


# References:
For more comments, suggestions, or questions, you can contact Or Ben Porath ([@OrBenPorath](https://twitter.com/OrBenPorath)) and CyberArk Labs.
