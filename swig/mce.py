# mce.py

# This module exposes mce_context_t as an mce object.

from mce_library import *

class MCEError(Exception):
    """
    Exception raised by MCE or subsystem communication errors.
    """
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class ParamError(Exception):
    """
    Exception raised by argument or addressing errors.
    """
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class mce:
    """
    Object representing an MCE.
    """

    def __init__(self):
        self.context = mcelib_create()
        self.open()

    def open(self,
             cmd_file=default_cmdfile,
             data_file=default_datafile,
             config_file=default_hardwarefile):
        mcecmd_open(self.context, cmd_file)
        mcedata_open(self.context, data_file)
        mceconfig_open(self.context, config_file, "hardware")

    def lookup(self, card, para):
        """
        Lookup a card/param address in the hardware config file.

        Throws ParamError if there's a problem.
        """
        p = mce_param_t()
        err = mcecmd_load_param(self.context, p, card, para)
        if err != 0:
            raise ParamError, mcelib_error_string(err)
        return p

    def read(self, card, para, count=-1, offset=0):

        p = self.lookup(card,para)
        if count < 0: count = p.param.count - offset

        d = u32array(count)
        err = mcecmd_read_range(self.context, p, offset, d.cast(), count)
        if (err != 0):
            raise MCEError, mcelib_error_string(err)

        dd = [ d[i] for i in range(count) ]
        return dd

    def write(self, card, para, data, offset=0):
        p = self.lookup(card, para)
            
        count = len(data)
        if count > p.param.count - offset:
            raise ParamError, "Count is too big for parameter."

        d = u32array(count)
        for i in range(count): d[i] = data[i]

        err = mcecmd_write_range(self.context, p, offset, d.cast(), count)
        if err != 0:
            raise MCEError, mcelib_error_string(err)

    def read_frame(self, card_list = ["RCS"], data_only=False ):
        cards = 0
        for cc in card_list:
            c = cc.lower()
            if c == "rcs": cards |= 0xf
            elif c == "rc1": cards |= 0x1
            elif c == "rc2": cards |= 0x2
            elif c == "rc3": cards |= 0x4
            elif c == "rc4": cards |= 0x8
            else:
                raise ParamError, 'Unknown card ' + cc

        card_count = (cards & 0x1 != 0) + (cards & 0x2 != 0) + \
            (cards & 0x4 != 0) + (cards & 0x8 != 0)
        
        max_size = 44 + 41*8*card_count
        d = u32array(max_size)

        read_frame(self.context, d.cast(), cards);

        header = [ d[i] for i in range(43) ]
        num_rows_reported = header[3]

        dd = [ d[i+43] for i in range(num_rows_reported * 8*card_count) ]

        if data_only: return dd
        return dd, header

