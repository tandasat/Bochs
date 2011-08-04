/////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////
//
//   Copyright (c) 2011 Stanislav Shwartsman
//          Written by Stanislav Shwartsman [sshwarts at sourceforge net]
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA B 02110-1301 USA
//
/////////////////////////////////////////////////////////////////////////

#include "bochs.h"
#include "cpu/cpu.h"
#include "param_names.h"
#include "p4_willamette.h"

#define LOG_THIS cpu->

#if BX_CPU_LEVEL >= 6 && BX_SUPPORT_X86_64 == 0

p4_willamette_t::p4_willamette_t(BX_CPU_C *cpu): bx_cpuid_t(cpu)
{
#if BX_SUPPORT_SMP
  nthreads = SIM->get_param_num(BXPN_CPU_NTHREADS)->get();
  ncores = SIM->get_param_num(BXPN_CPU_NCORES)->get();
  nprocessors = SIM->get_param_num(BXPN_CPU_NPROCESSORS)->get();
#endif

  if (BX_SUPPORT_X86_64)
    BX_PANIC(("x86-64 should be disabled for P4 (Willamette) configuration"));
}

void p4_willamette_t::get_cpuid_leaf(Bit32u function, Bit32u subfunction, cpuid_function_t *leaf) const
{
  switch(function) {
  case 0x80000000:
    get_ext_cpuid_leaf_0(leaf);
    return;
  case 0x80000001:
    get_ext_cpuid_leaf_1(leaf);
    return;
  case 0x80000002:
  case 0x80000003:
  case 0x80000004:
    get_ext_cpuid_brand_string_leaf(function, leaf);
    return;
  case 0x00000000:
    get_std_cpuid_leaf_0(leaf);
    return;
  case 0x00000001:
    get_std_cpuid_leaf_1(leaf);
    return;
  case 0x00000002:
  default:
    get_std_cpuid_leaf_2(leaf);
    return;
  }
}

Bit32u p4_willamette_t::get_isa_extensions_bitmask(void) const
{
  return BX_CPU_X87 |
         BX_CPU_486 |
         BX_CPU_PENTIUM |
         BX_CPU_P6 |
         BX_CPU_MMX |
         BX_CPU_FXSAVE_FXRSTOR |
         BX_CPU_SYSENTER_SYSEXIT |
         BX_CPU_CLFLUSH |
         BX_CPU_SSE |
         BX_CPU_SSE2;
}

Bit32u p4_willamette_t::get_cpu_extensions_bitmask(void) const
{
  return BX_CPU_DEBUG_EXTENSIONS |
         BX_CPU_VME |
         BX_CPU_PSE |
         BX_CPU_PAE |
         BX_CPU_PGE |
#if BX_PHY_ADDRESS_LONG
         BX_CPU_PSE36 |
#endif
         BX_CPU_PAT_MTRR |
         BX_CPU_XAPIC;
}

// leaf 0x00000000 //
void p4_willamette_t::get_std_cpuid_leaf_0(cpuid_function_t *leaf) const
{
  static const char* vendor_string = "GenuineIntel";

  // EAX: highest std function understood by CPUID
  // EBX: vendor ID string
  // EDX: vendor ID string
  // ECX: vendor ID string
  leaf->eax = 0x2;

  // CPUID vendor string (e.g. GenuineIntel, AuthenticAMD, CentaurHauls, ...)
  memcpy(&(leaf->ebx), vendor_string,     4);
  memcpy(&(leaf->edx), vendor_string + 4, 4);
  memcpy(&(leaf->ecx), vendor_string + 8, 4);
#ifdef BX_BIG_ENDIAN
  leaf->ebx = bx_bswap32(leaf->ebx);
  leaf->ecx = bx_bswap32(leaf->ecx);
  leaf->edx = bx_bswap32(leaf->edx);
#endif
}

// leaf 0x00000001 //
void p4_willamette_t::get_std_cpuid_leaf_1(cpuid_function_t *leaf) const
{
  // EAX:       CPU Version Information
  //   [3:0]   Stepping ID
  //   [7:4]   Model: starts at 1
  //   [11:8]  Family: 4=486, 5=Pentium, 6=PPro, ...
  //   [13:12] Type: 0=OEM, 1=overdrive, 2=dual cpu, 3=reserved
  //   [19:16] Extended Model
  //   [27:20] Extended Family
  leaf->eax = 0x00000F12;

  // EBX:
  //   [7:0]   Brand ID
  //   [15:8]  CLFLUSH cache line size (value*8 = cache line size in bytes)
  //   [23:16] Number of logical processors in one physical processor
  //   [31:24] Local Apic ID

#if BX_SUPPORT_SMP
  unsigned n_logical_processors = ncores*nthreads;
#else
  unsigned n_logical_processors = 1;
#endif
  leaf->ebx = ((CACHE_LINE_SIZE / 8) << 8) |
              (n_logical_processors << 16);
#if BX_SUPPORT_APIC
  leaf->ebx |= ((cpu->get_apic_id() & 0xff) << 24);
#endif

  // ECX: Extended Feature Flags
  leaf->ecx = 0;

  // EDX: Standard Feature Flags
  // * [0:0]   FPU on chip
  // * [1:1]   VME: Virtual-8086 Mode enhancements
  // * [2:2]   DE: Debug Extensions (I/O breakpoints)
  // * [3:3]   PSE: Page Size Extensions
  // * [4:4]   TSC: Time Stamp Counter
  // * [5:5]   MSR: RDMSR and WRMSR support
  // * [6:6]   PAE: Physical Address Extensions
  // * [7:7]   MCE: Machine Check Exception
  // * [8:8]   CXS: CMPXCHG8B instruction
  // * [9:9]   APIC: APIC on Chip
  //   [10:10] Reserved
  // * [11:11] SYSENTER/SYSEXIT support
  // * [12:12] MTRR: Memory Type Range Reg
  // * [13:13] PGE/PTE Global Bit
  // * [14:14] MCA: Machine Check Architecture
  // * [15:15] CMOV: Cond Mov/Cmp Instructions
  // * [16:16] PAT: Page Attribute Table
  // * [17:17] PSE-36: Physical Address Extensions
  //   [18:18] PSN: Processor Serial Number
  // * [19:19] CLFLUSH: CLFLUSH Instruction support
  //   [20:20] Reserved
  // * [21:21] DS: Debug Store
  // * [22:22] ACPI: Thermal Monitor and Software Controlled Clock Facilities
  // * [23:23] MMX Technology
  // * [24:24] FXSR: FXSAVE/FXRSTOR (also indicates CR4.OSFXSR is available)
  // * [25:25] SSE: SSE Extensions
  // * [26:26] SSE2: SSE2 Extensions
  // * [27:27] Self Snoop
  // * [28:28] Hyper Threading Technology
  // * [29:29] TM: Thermal Monitor
  //   [30:30] Reserved
  //   [31:31] PBE: Pending Break Enable
  leaf->edx = BX_CPUID_STD_X87 |
              BX_CPUID_STD_VME |
              BX_CPUID_STD_DEBUG_EXTENSIONS |
              BX_CPUID_STD_PSE |
              BX_CPUID_STD_TSC |
              BX_CPUID_STD_MSR |
              BX_CPUID_STD_PAE |
              BX_CPUID_STD_MCE |
              BX_CPUID_STD_CMPXCHG8B |
              BX_CPUID_STD_SYSENTER_SYSEXIT |
              BX_CPUID_STD_MTRR |
              BX_CPUID_STD_GLOBAL_PAGES |
              BX_CPUID_STD_MCA |
              BX_CPUID_STD_CMOV |
              BX_CPUID_STD_PAT |
#if BX_PHY_ADDRESS_LONG
              BX_CPUID_STD_PSE36 |
#endif
              BX_CPUID_STD_CLFLUSH |
              BX_CPUID_STD_DEBUG_STORE |
              BX_CPUID_STD_ACPI |
              BX_CPUID_STD_MMX |
              BX_CPUID_STD_FXSAVE_FXRSTOR |
              BX_CPUID_STD_SSE |
              BX_CPUID_STD_SSE2 |
              BX_CPUID_STD_SELF_SNOOP |
              BX_CPUID_STD_HT;
#if BX_SUPPORT_APIC
  // if MSR_APICBASE APIC Global Enable bit has been cleared,
  // the CPUID feature flag for the APIC is set to 0.
  if (cpu->msr.apicbase & 0x800)
    leaf->edx |= BX_CPUID_STD_APIC; // APIC on chip
#endif
}

// leaf 0x00000002 //
void p4_willamette_t::get_std_cpuid_leaf_2(cpuid_function_t *leaf) const
{
  // CPUID function 0x00000002 - Cache and TLB Descriptors
  leaf->eax = 0x665B5001;
  leaf->ebx = 0x00000000;
  leaf->ecx = 0x00000000;
  leaf->edx = 0x007A7040;
}

// leaf 0x80000000 //
void p4_willamette_t::get_ext_cpuid_leaf_0(cpuid_function_t *leaf) const
{
  // EAX: highest extended function understood by CPUID
  // EBX: reserved
  // EDX: reserved
  // ECX: reserved
  leaf->eax = 0x80000004;
  leaf->ebx = 0;
  leaf->edx = 0; // Reserved for Intel
  leaf->ecx = 0;
}

// leaf 0x80000001 //
void p4_willamette_t::get_ext_cpuid_leaf_1(cpuid_function_t *leaf) const
{
  // EAX:       CPU Version Information (reserved for Intel)
  leaf->eax = 0;

  // EBX:       Brand ID (reserved for Intel)
  leaf->ebx = 0;

  // ECX:
  //   [0:0]   LAHF/SAHF instructions support in 64-bit mode
  //   [1:1]   CMP_Legacy: Core multi-processing legacy mode (AMD)
  //   [2:2]   SVM: Secure Virtual Machine (AMD)
  //   [3:3]   Extended APIC Space
  //   [4:4]   AltMovCR8: LOCK MOV CR0 means MOV CR8
  //   [5:5]   LZCNT: LZCNT instruction support
  //   [6:6]   SSE4A: SSE4A Instructions support (deprecated?)
  //   [7:7]   Misaligned SSE support
  //   [8:8]   PREFETCHW: PREFETCHW instruction support
  //   [9:9]   OSVW: OS visible workarounds (AMD)
  //   [11:10] reserved
  //   [12:12] SKINIT support
  //   [13:13] WDT: Watchdog timer support
  //   [31:14] reserved
  leaf->ecx = 0;

  // EDX:
  // Many of the bits in EDX are the same as FN 0x00000001 [*] for AMD
  //    [10:0] Reserved for Intel
  //   [11:11] SYSCALL/SYSRET support
  //   [19:12] Reserved for Intel
  //   [20:20] No-Execute page protection
  //   [25:21] Reserved
  //   [26:26] 1G paging support
  //   [27:27] Support RDTSCP Instruction
  //   [28:28] Reserved
  //   [29:29] Long Mode
  //   [30:30] AMD 3DNow! Extensions
  //   [31:31] AMD 3DNow! Instructions
  leaf->edx = 0;
}

// leaf 0x80000002 //
// leaf 0x80000003 //
// leaf 0x80000004 //
void p4_willamette_t::get_ext_cpuid_brand_string_leaf(Bit32u function, cpuid_function_t *leaf) const
{
  // CPUID function 0x80000002-0x80000004 - Processor Name String Identifier
  static const char* brand_string = "              Intel(R) Pentium(R) 4 CPU 1.80GHz";

  switch(function) {
  case 0x80000002:
    memcpy(&(leaf->eax), brand_string     , 4);
    memcpy(&(leaf->ebx), brand_string +  4, 4);
    memcpy(&(leaf->ecx), brand_string +  8, 4);
    memcpy(&(leaf->edx), brand_string + 12, 4);
    break;
  case 0x80000003:
    memcpy(&(leaf->eax), brand_string + 16, 4);
    memcpy(&(leaf->ebx), brand_string + 20, 4);
    memcpy(&(leaf->ecx), brand_string + 24, 4);
    memcpy(&(leaf->edx), brand_string + 28, 4);
    break;
  case 0x80000004:
    memcpy(&(leaf->eax), brand_string + 32, 4);
    memcpy(&(leaf->ebx), brand_string + 36, 4);
    memcpy(&(leaf->ecx), brand_string + 40, 4);
    memcpy(&(leaf->edx), brand_string + 44, 4);
    break;
  default:
    break;
  }

#ifdef BX_BIG_ENDIAN
  leaf->eax = bx_bswap32(leaf->eax);
  leaf->ebx = bx_bswap32(leaf->ebx);
  leaf->ecx = bx_bswap32(leaf->ecx);
  leaf->edx = bx_bswap32(leaf->edx);
#endif
}

void p4_willamette_t::dump_cpuid(void) const
{
  struct cpuid_function_t leaf;
  unsigned n;

  for (n=0; n<=0x2; n++) {
    get_cpuid_leaf(n, 0x00000000, &leaf);
    BX_INFO(("CPUID[0x%08x]: %08x %08x %08x %08x", n, leaf.eax, leaf.ebx, leaf.ecx, leaf.edx));
  }

  for (n=0x80000000; n<=0x80000004; n++) {
    get_cpuid_leaf(n, 0x00000000, &leaf);
    BX_INFO(("CPUID[0x%08x]: %08x %08x %08x %08x", n, leaf.eax, leaf.ebx, leaf.ecx, leaf.edx));
  }
}

bx_cpuid_t *create_p4_willamette_cpuid(BX_CPU_C *cpu) { return new p4_willamette_t(cpu); }

#endif
