/*
 * Registers and bits for the DSP 56301
 */

#define HSTR_HINT 0x40
#define HSTR_HC5  0x20
#define HSTR_HC4  0x10
#define HSTR_HC3  0x08
#define HSTR_HRRQ 0x04
#define HSTR_HTRQ 0x02
#define HSTR_TRDY 0x01

#define HCTR_HF0  0x08
#define HCTR_HF1  0x10
#define HCTR_HF2  0x20


#pragma pack(1)

typedef struct {

	volatile u32 unused1[4];
	volatile u32 hctr;      // Host control register
	volatile u32 hstr;      // Host status register
	volatile u32 hcvr;      // Host command vector register(base+$018)
	volatile u32 htxr_hrxs; // Host transmit / receive data

} dsp_reg_t;

#pragma pack()

