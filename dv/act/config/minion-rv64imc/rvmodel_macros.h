#ifndef CORE_ET_ACT_RVMODEL_MACROS_H
#define CORE_ET_ACT_RVMODEL_MACROS_H

#define RVMODEL_DATA_SECTION

#define RVMODEL_BOOT

#define RVMODEL_HALT_PASS   \
  .global core_et_act_pass ;\
core_et_act_pass:          ;\
  j core_et_act_pass       ;

#define RVMODEL_HALT_FAIL   \
  .global core_et_act_fail ;\
core_et_act_fail:          ;\
  j core_et_act_fail       ;

#define RVMODEL_IO_INIT(_R1, _R2, _R3)
#define RVMODEL_IO_WRITE_STR(_R1, _R2, _R3, _STR_PTR)

#define RVMODEL_INTERRUPT_LATENCY 10
#define RVMODEL_TIMER_INT_SOON_DELAY 100
#define RVMODEL_MTIME_ADDRESS
#define RVMODEL_MTIMECMP_ADDRESS

#define RVMODEL_SET_MEXT_INT(_R1, _R2)
#define RVMODEL_CLR_MEXT_INT(_R1, _R2)
#define RVMODEL_SET_MSW_INT(_R1, _R2)
#define RVMODEL_CLR_MSW_INT(_R1, _R2)
#define RVMODEL_SET_SEXT_INT(_R1, _R2)
#define RVMODEL_CLR_SEXT_INT(_R1, _R2)
#define RVMODEL_SET_SSW_INT(_R1, _R2)
#define RVMODEL_CLR_SSW_INT(_R1, _R2)

#endif
