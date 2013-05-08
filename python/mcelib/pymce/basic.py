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
    _numpify = False  # Set to true to return arrays rather than lists.

    def __init__(self, device_index=-1, numpify=False):
        self.context = mcelib.connect(device_index)
        self._numpify = numpify

    def read(self, card, param, count=-1, offset=0, array=True):
        d = mcelib.read(self.context, card, param, offset, count)
        if self._numpify:
            d = numpy.array(d)
        if not array and len(d) == 1:
            return d[0]
        return d

    def write(self, card, param, data, offset=0):
        if numpy.isscalar(data) or \
                (isinstance(data, numpy.ndarray) and data.ndim==0):
            data = [data]
        if len(data) == 0:
            # This messes up the MCE, so forbid it here.
            return False
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
            raise ValueError, "Invalid card list %s" % str(cards)
        frame_size = self.frame_size(cards, n_rows)
        card_code = sum([1<<(c-1) for c in cards])
        # Create array, read.
        data = numpy.empty((count,frame_size),'int32')
        ok = mcelib.read_data(self.context, card_code, count, data)
        if not ok:
            return None
        return data

    def read_data(self, count=1, cards=None, fields=None, extract=False,
                  row_col=False, raw_frames=False, unfilter=False):
        d = MCEBinaryData(mce=self)
        # Get card list, but leave error checking to read_raw...
        cards = self.card_list(cards)
        # Load frame data
        d.data = self.read_raw(count, cards)
        if raw_frames:
            return d.data
        if d.data == None:
            return None
        # To go any further we need mce_data
        if mce_data == None:
            raise RuntimeError, "mce_data module is required to process data "\
                "(pass raw_frames=True to suppress)."
        d.fast_axis = 'dets'
        # head_binary needed for _ReadHeader, called by GetPayloadInfo
        d._ReadHeader(head_binary=d.data[0,:43])
        d._GetPayloadInfo()
        d.headers = d.data[:,:43]
        d.data = d.data[:,43:-1]
        d._GetContentInfo()
        dm_data = mce_data.MCE_data_modes.get('%i'%d.data_mode)
        d.fields = [x for x in dm_data.fields]
        if not extract and not row_col and fields==None:
            return d
        if extract or row_col:
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
            return d.extract(fields, unfilter=unfilter)
        return d

    def lock_query(self):
        return mcelib.lock_op(self.context, 0)
    def lock_down(self):
        return mcelib.lock_op(self.context, 1)
    def lock_up(self):
        return mcelib.lock_op(self.context, 2)
    def lock_reset(self):
        return mcelib.lock_op(self.context, 3)
    
"""
The MCEBinaryData object is based on SmallMCEFile, because that class
knows how to unpack frame data.

Someday, SmallMCEFile sould return one of these, instead of sort of
being one of these.
"""

class MCEBinaryData(mce_data.SmallMCEFile):
    data = None
    headers = None
    fast_axis = None  # axis -1; either 'time' or 'dets'

    def __init__(self, mce=None, head_binary=None):
        mce_data.SmallMCEFile(self, runfile=False, basic_info=False)
        self.mce = mce
        self.runfile = 'fake'
        self.filename = None
        self.header = None
        self.n_ro = 1
        if head_binary != None:
            self._ReadHeader(head_binary=self.head_binary)
        
    def extract(self, field, unfilter=False):
        dm_data = mce_data.MCE_data_modes.get('%i'%self.data_mode)
        if field == 'default':
            field = dm_data.fields[0]
        elif field == 'all':
            field = dm_data.fields
        if not isinstance(field, str):
            return [self.get_field(f) for f in field]
        data = dm_data[field].extract(self.data)
        if field == 'fb_filt' and unfilter == 'DC':
            ftype = self.mce.read('rc1', 'fltr_type')[0]
            fpara = self.mce.read('rc1', 'fltr_coeff')
            filt = mce_data.MCEButterworth.from_params(ftype, fpara)
            return data / filt.gain()
        return data

    # Replace _rfMCEParam, and load stuff right from the MCE
    def _rfMCEParam(self, card, param, array=False, check_sys=True):
        data = self.mce.read(card, param)
        if len(data) == 1 and not array:
            return data[0]
        return data
