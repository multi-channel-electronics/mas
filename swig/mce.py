# mce.py
# vim: ts=4 sw=4 et

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

class ChannelSet:
    """
    Defines a set of channels and offers information to Mce class on
    how to acquire them.
    """
    def __init__(self, rc='', rcs_cards=0xf):

        self.use_readout_index = False
        self.rcs_cards = rcs_cards

        self.has_list = False
        self.channel_list = [[],[]]
        self.rows = range(41)

        if rc != '':
            self.columns = self.columns_from_cards(self.cards_from_list([ rc ]))
        else:
            self.columns = range(32)

    def cards_span(self):
        """
        Returns card_bits for acq necessary to acquire the ChannelSet
        channels.
        """
        max_card = max(self.columns)/8
        min_card = min(self.columns)/8
        if max_card != min_card:
            return self.cards_from_list(['rcs'])
        return self.cards_from_list([ 'rc%i' % (max_card+1) ])

    def rows_span(self):
        if self.use_readout_index:
            return [min(self.rows), max(self.rows)]
        return [0, max(self.rows)]

    def columns_span(self):
        cards = self.cards_span()
        c_list = self.columns_from_cards(cards)
        return [min(c_list), max(c_list)]

    def acq_size(self):
        c_span = self.columns_span()
        r_span = self.rows_span()
        return (c_span[1] - c_span[0] + 1)*(r_span[1] - r_span[0] + 1)

    def data_size(self):
        return len(self.columns) * len(self.rows)
                
    def cards_from_list(self, card_list):
        """
        Given a list of card names, returns card_bits of the minimal
        acq.
        """
        cards = 0
        for cc in card_list:
            c = cc.lower()
            if c == "rcs": cards |= self.rcs_cards
            elif c == "rc1": cards |= 0x1
            elif c == "rc2": cards |= 0x2
            elif c == "rc3": cards |= 0x4
            elif c == "rc4": cards |= 0x8
            else:
                raise ParamError, 'Unknown card ' + cc
        return cards

    def columns_from_cards(self, cards):
        """
        Given card_bits, returns the list of associated columns.
        """
        c = []
        if cards & 0x1:
            c.extend(range(0,8))
        if cards & 0x2:
            c.extend(range(8,16))
        if cards & 0x4:
            c.extend(range(16,24))
        if cards & 0x8:
            c.extend(range(24,32))
        return c

    def extract_channels(self, data):
        c_span = self.columns_span()
        r_span = self.rows_span()
        c_num = c_span[1] - c_span[0] + 1
        g = [ [data[c_num*(r-r_span[0]) + (c - c_span[0])] \
              for c in self.columns ] for r in self.rows ]
        return g

    def count(self):
        if self.has_list:
            return len(self.channel_list)
        else:
            return len(self.columns)*len(self.rows)

    def item(self, index):
        if self.has_list:
            return self.channel_list[index]
        else:
            return [ self.columns[index / len(self.rows)],
                     self.rows[index % len(self.rows)] ]

    def frame_indices(self):

        data_offset = 43
        rs = self.rows_span()
        cs = self.columns_span()
        
        if self.has_list:
            return [ data_offset +
                     (self.channel_list[i].row - rs[0])*(cs[1] - cs[0] + 1) +
                     self.channel_list[i].column - cs[0]
                     for i in range(len(self.channel_list)) ]
        else:
            return [ data_offset +
                     (r - rs[0])*(cs[1] - cs[0] + 1) + c - cs[0]
                     for r in self.rows for c in self.columns ]
    
class mce:
    """
    Object representing an MCE.
    """

    def __init__(self, fibre_card=None):
        self.context = mcelib_create()
        if (fibre_card == None):
          self.__fibre_card__ = mcelib_default_fibre_card()
        else:
          self.__fibre_card__ = fibre_card
        self.open()

    def open(self, cmd_file=None, data_file=None, config_file=None,
             fibre_card=None):
        if (fibre_card != None):
            self.__fibre_card__ = fibre_card
        if (cmd_file == None):
            cmd_file = mcelib_cmd_device(self.__fibre_card__)
        if (data_file == None):
            data_file = mcelib_data_device(self.__fibre_card__)
        if (config_file == None):
            config_file = mcelib_default_hardwarefile(self.__fibre_card__)

        self.__cmd_file__ = cmd_file
        self.__data_file__ = data_file
        self.__config_file__ = config_file

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

    def read(self, card, para, count=-1, offset=0, array=True):
        #Note this has the usual mce_library bizarro conventions for
        #offset and count when it comes to broadcast cards.

        p = self.lookup(card,para)
        if count < 0: count = p.param.count - offset
        count_out = mcecmd_read_size(p, count)

        d = i32array(count_out)
        err = mcecmd_read_range(self.context, p, offset, \
                                u32_from_int_p(d.cast()), count)
        if (err != 0):
            raise MCEError, mcelib_error_string(err)

        if array == False: return d[0]

        dd = [ d[i] for i in range(count_out) ]
        return dd

    def write(self, card, para, data, offset=0):
        p = self.lookup(card, para)
            
        count = len(data)
        if count > p.param.count - offset:
            raise ParamError, "Count is too big for parameter."

        d = i32array(count)
        for i in range(count): d[i] = data[i]

        err = mcecmd_write_range(self.context, p, offset, u32_from_int_p(d.cast()), count)
        if err != 0:
            raise MCEError, mcelib_error_string(err)

    def reset(self, dsp_reset=False, mce_reset=True):
        if mce_reset:
            error = mcecmd_hardware_reset(self.context)
            if error != 0:
                raise MCEError, "Hardware reset: " + mcelib_error_string(err)

        if dsp_reset:
            error = mcecmd_interface_reset(self.context)
            if error != 0:
                raise MCEError, "PCI card reset: " + mcelib_error_string(err)            
    def card_count(self, cards):
        return (cards & 0x1 != 0) + (cards & 0x2 != 0) + \
               (cards & 0x4 != 0) + (cards & 0x8 != 0)
    
    def read_frames(self, count, channel_set=False, data_only=False):
        # Channel_sets aren't supported yet; they need to be smarter.
        if channel_set != False:
            print 'Sorry, channel_set=False only!'
            return None
        else:
            try:
                rc = self.read('cc', 'rcs_to_report_data', array=False)
                cards = 0
                for i, bits in enumerate([5,4,3,2]):
                    if rc & (1 << bits):
                        cards |= (1 << i)
            except:
                print 'Could not query rcs_to_report_data register.'
                cards = 0xf
            channel_set = ChannelSet(rcs_cards=cards)
            n_cols = 8*self.card_count(cards)
            num_rows_rep = self.read('cc', 'num_rows_reported', array=False)
            channel_set.rows = range(num_rows_rep)
            channel_set.columns = range(n_cols)

        indices = channel_set.frame_indices()
        n_extra = 44
        frame_size = n_cols*num_rows_rep + n_extra
        d = u32array(frame_size*count)
        read_frames(self.context, d.cast(), -1, count);

        # Extract headers, and data.
        hh = [ [d[frame_size*f + i] for i in range(43)] for f in range(count) ]

        # This will break if user asks for rows beyond num_rows_rep...
        ii = i32array(frame_size*count)
        u32_to_i32(ii.cast(), d.cast(), frame_size*count)
        
        dd = [ [ii[frame_size*f + i] for i in indices] for f in range(count) ]

        if data_only: return dd
        return dd, hh

    def read_frame(self, channel_set = False, data_only=False):
        if data_only:
            d = self.read_frames(1, channel_set=channel_set, data_only=True)
            return d[0]
        
        d, h = self.read_frames(1, channel_set=channel_set, data_only=False)
        return d[0], h[0]

    def read_channel(self, count=1, channel_set=False):

        if channel_set == False:
            channel_set = ChannelSet()
            channel_set.rows = [0]
            channel_set.columns = [0]

        index = channel_set.frame_indices()

        if (len(index) != 1):
            print 'ChannelSet must have exactly one channel!'
            return -1

        # Calculate index of target data
        cc = u32array(1)
        cc[0] = index[0]

        cards = channel_set.cards_span()
        d = u32array(count)

        read_channels(self.context, d.cast(), cards, count, cc.cast(), 1);

        ii = i32array(count)
        u32_to_i32(ii.cast(), d.cast(), count)

        return ii


#class MCEData:

#    def __init__(self):


#    def load_flatfile(self, filename):


#    def 
