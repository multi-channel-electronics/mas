import mcelib
import const

import numpy

try:
    import mce_data
except:
    mce_data = None

class BasicMCE:
    """
    Minimal connection to the MCE library.

    No higher level functionality.  Extend me.
    """
    def __init__(self, device_index=-1):
        self.context = mcelib.connect(device_index)

    def read(self, card, param, count=-1, offset=0, array=True):
        d = mcelib.read(self.context, card, param, offset, count)
        if not array and len(d) == 1:
            return d[0]
        return d

    def write(self, card, param, data, offset=0):
        if not hasattr(data, '__getitem__'):
            data = [data]
        return mcelib.write(self.context, card, param, offset, data)

    def frame_size(self, cards=None, n_rows=None):
        cards = self.card_list(None)
        card_count = len(cards)
        cards = sum([1<<(c-1) for c in cards])
        n_cols = const.COLS_PER_RC*card_count
        if n_rows == None:
            n_rows = self.read('cc', 'num_rows_reported')[0]
        n_extra = 44
        return n_cols*n_rows + n_extra

    def card_list(self, cards):
        """
        Turns cards argument into a list of RC numbers.  Cards can be
        a list of integers (e.g. [1,2] for rc1 and rc2), an rc name
        ("rc2" or "rcs"...) or None, in which case the current MCE
        setting of cc rcs_to_report_data is used.
        """
        cards_in = cards
        if cards == None:
            rc = self.read('cc', 'rcs_to_report_data', array=False)
            cards = []
            for i, bits in enumerate(const.RCS_BITS):
                if rc & (1 << bits):
                    cards.append(i+1)
        else:
            # It could be a string...
            if isinstance(cards, str):
                _card = const.RC_CODES.get(cards.lower(), None)
                if _card == None:
                    print "Could not parse card string '%s'"%_card
                    return None
                if _card == -1:
                    cards = [i+1 for i in range(len(const.RCS_BITS))]
                else:
                    cards = [_card+1]
            if not hasattr(cards, '__getitem__'):
                # Maybe this is the problem
                cards = [cards]
        # Validate
        ok = (len(cards) > 0) and \
            (sum([(c<=0) or (c>const.MAX_RC) for c in cards]) == 0) and \
            (sum([cards[i]-cards[i-1]<=0 for i in range(1,len(cards))]) == 0)
        if not ok:
            print "Could not decode cards argument", cards_in
            print "... got as far as ", cards
            return None
        return cards

    def read_raw(self, count, cards=None, n_rows=None):
        # Decode cards list -- ends up as a list of card numbers [1,2]
        cards = self.card_list(cards)
        if cards == None:
            raise RuntimeError, "bad cards specification."
        frame_size = self.frame_size(cards, n_rows)
        card_code = sum([1<<(c-1) for c in cards])
        # Create array, read.
        data = numpy.zeros((count,frame_size),'int32')
        mcelib.read_data(self.context, card_code, count, data)
        return data

    def read_data(self, count, cards=None, fields=None, extract=False,
                  row_col=False, raw_frames=False):
        cards = self.card_list(cards)
        d = MCEBinaryData()
        d.data = self.read_raw(count, cards)
        if raw_frames:
            return d.data
        # To go any further we need mce_data
        if mce_data == None:
            raise RuntimeError, "mce_data module is required to process data "\
                "(pass raw_frames=True to suppress)."
        d.fast_axis = 'dets'
        d._GetPayloadInfo(self, cards, d.data.shape[-1])
        d.headers = d.data[:,:43]
        d.data = d.data[:,43:-1]
        if extract or row_col:
            dm_data = mce_data.MCE_data_modes.get('%i'%d.data_mode)
            if dm_data.raw:
                # 50 MHz data; automatically contiguous
                d.data = d._ExtractRaw(d.data, dm_data.raw_info['n_cols'])
            else:
                # Rectangle mode data
                d.data = d._ExtractRect(d.data, None)
                if row_col:
                    d.data.shape = (d.n_rows, d.n_cols*d.n_rc, -1)
            d.fast_axis = 'time'
        if fields != None:
            return d.extract(fields)
        return d

        
    
"""
The MCEBinaryData object is based on SmallMCEFile, because that class
knows how to unpack frame data.
"""

class MCEBinaryData(mce_data.SmallMCEFile):
    data = None
    headers = None
    fast_axis = None  # axis -1; either 'time' or 'dets'

    def __init__(self):
        mce_data.SmallMCEFile(self, runfile=False, basic_info=False)
        
    def _GetPayloadInfo(self, mce, cards, frame_size):
        self.n_rc = len(cards)
        rc = 'rc%i' % cards[0]
        mult1 = mce.read('cc', 'num_rows_reported')[0]
        mult2 = mce.read('cc', 'num_cols_reported')[0]
        self.n_rows = mce.read(rc, 'num_rows_reported')[0]
        self.n_cols = mce.read(rc, 'num_cols_reported')[0]
        self.rc_step = mult2
        self.size_ro = mult1*mult2
        self.frame_bytes = frame_size * 4
        self.data_mode = mce.read(rc, 'data_mode')[0]

    def extract(self, field):
        dm_data = mce_data.MCE_data_modes.get('%i'%self.data_mode)
        if not isinstance(field, str):
            return [self.get_field(f) for f in field]
        return dm_data[field].extract(self.data)

class DataMCE(BasicMCE):
    def read_data(self, count, cards=None, fields=None, extract=False,
                  row_col=False, raw_frames=False):
        cards = self.card_list(cards)
        d = MCEBinaryData()
        d.data = self.read_raw(count, cards)
        if raw_frames:
            return d.data
        # To go any further we need mce_data
        if mce_data == None:
            raise RuntimeError, "mce_data module is required to process data "\
                "(pass raw_frames=True to suppress)."
        d.fast_axis = 'dets'
        d._GetPayloadInfo(self, cards, d.data.shape[-1])
        d.headers = d.data[:,:43]
        d.data = d.data[:,43:-1]
        if extract or row_col:
            dm_data = mce_data.MCE_data_modes.get('%i'%d.data_mode)
            if dm_data.raw:
                # 50 MHz data; automatically contiguous
                d.data = d._ExtractRaw(d.data, dm_data.raw_info['n_cols'])
            else:
                # Rectangle mode data
                d.data = d._ExtractRect(d.data, None)
                if row_col:
                    d.data.shape = (d.n_rows, d.n_cols*d.n_rc, -1)
            d.fast_axis = 'time'
        if fields != None:
            return d.extract(fields)
        return d

