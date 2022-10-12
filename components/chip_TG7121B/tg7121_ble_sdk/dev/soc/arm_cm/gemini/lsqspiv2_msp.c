#include "lsqspiv2_msp.h"
#include "reg_sysc_cpu_type.h"
#include "compile_flag.h"
#include "gemini.h"
XIP_BANNED void lsqspiv2_msp_init()
{
    SYSC_CPU->PD_CPU_CLKG = SYSC_CPU_CLKG_SET_QSPI_MASK;
    __NOP(); __NOP();
    SYSC_CPU->PD_CPU_CLKG = SYSC_CPU_CLKG_CLR_QSPI_MASK;
    __NOP(); __NOP();
    SYSC_CPU->PD_CPU_SRST = SYSC_CPU_SRST_CLR_QSPI_N_MASK;
    __NOP(); __NOP();
    SYSC_CPU->PD_CPU_SRST = SYSC_CPU_SRST_SET_QSPI_N_MASK;
    __NOP(); __NOP();
    SYSC_CPU->PD_CPU_CLKG = SYSC_CPU_CLKG_SET_QSPI_MASK;
}