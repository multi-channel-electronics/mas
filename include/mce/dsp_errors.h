#ifndef _DSP_ERRORS_H_
#define _DSP_ERRORS_H_

#define DSP_ERR_BASE          0x2000

#define DSP_ERR_FAILURE         (DSP_ERR_BASE + 0x0000)
#define DSP_ERR_HANDLE          (DSP_ERR_BASE + 0x0001)
#define DSP_ERR_DEVICE          (DSP_ERR_BASE + 0x0002)
#define DSP_ERR_FORMAT          (DSP_ERR_BASE + 0x0003)
#define DSP_ERR_REPLY           (DSP_ERR_BASE + 0x0004)
#define DSP_ERR_BOUNDS          (DSP_ERR_BASE + 0x0005)
#define DSP_ERR_CHKSUM          (DSP_ERR_BASE + 0x0006)

#define DSP_ERR_KERNEL          (DSP_ERR_BASE + 0x0013)
#define DSP_ERR_IO              (DSP_ERR_BASE + 0x0014)
#define DSP_ERR_STATE           (DSP_ERR_BASE + 0x0015)
#define DSP_ERR_MEMTYPE         (DSP_ERR_BASE + 0x0016)
#define DSP_ERR_TIMEOUT         (DSP_ERR_BASE + 0x0017)
#define DSP_ERR_INTERRUPTED     (DSP_ERR_BASE + 0x0018)
#define DSP_ERR_WOULDBLOCK      (DSP_ERR_BASE + 0x0019)
#define DSP_ERR_UNKNOWN         (DSP_ERR_BASE + 0x001a)
#define DSP_ERR_VER_MISMATCH    (DSP_ERR_BASE + 0x001b)

#endif
