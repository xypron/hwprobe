hwprobe
=======

Linux provides a hwprobe syscall to determine supported RISC-V extensions.

This project provides tests for RVA23U64 compatibility written in Go, C,
and Python using this interface.

The C version additionally reports the Zicboz, Zicbom, and Zicbop cache
block sizes in verbose mode and checks the hart rules of the RISC-V Server
Platform Requirements that are observable via hwprobe. All harts are assumed
to be identical. ``make test`` runs the unit tests.

kvmprobe
========

Kvmprobe creates a vCPU and reports its features.
