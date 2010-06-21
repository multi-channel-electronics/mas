import mce

from numpy import *
import time

class xmce(mce.mce):
    def __init__(self, load_status=True):
        mce.mce.__init__(self)
        if load_status:
            self._load_status()

    def _load_status(self):
        # Row / col / TES count
        self.n_rows = self.read('rca', 'num_rows')[0]
        self.n_cols = len(self.read('sa', 'bias'))
        self.n_tes = len(self.read('tes', 'bias'))
        # Alternate hardware configurations
        self.hardware_bac = self._test_command('bac', 'enbl_mux')
        self.hardware_bc_f = self._test_command('bc2', 'enbl_mux')
        # Readout cards
        self.rcs_cards = self._rcs_cards()
        self.rcs_index = self._rcs_cards(index=True)
        # Per-detector configuration
        self.flux_quanta = self.read_rcs_grid('flx_quanta')
        self.adc_offsets = self.read_rcs_grid('adc_offset')
        self.gain_i = self.read_rcs_grid('gaini')

    def _test_command(self, card, param):
        try:
            self.lookup(card, param)
        except mce.ParamError:
            return False
        return True

    def _zeros(self, n):
        return [0 for i in range(n)]

    def _rcs_cards(self, index=False):
        rc = self.read('cc', 'rcs_to_report_data', array=False)
        if index:
            return [ i for i,b in enumerate([5,4,3,2]) if (rc ^ (1 << b)) ]
        else:
            return [ bool(rc ^ (1 << b)) for b in [5,4,3,2] ]

    def _rcs_columns(self):
        cards = self._rcs_cards()
        cols = []
        for i, card_on in enumerate(cards):
            if card_on:
                cols += [x for x in range(i*8, i*8+8)]
        return cols
    
    # Facilitate cross-RC reads and writes for, e.g.
    #  adc_offset, gaini, flx_quanta.

    def read_rcs_grid(self, param, n_rows=None):
        if n_rows == None:
            n_rows = self.n_rows
        data = []
        for rc in self.rcs_index:
            data += [ self.read('rc%i' % (rc+1), '%s%i' % (param,c),
                                count=n_rows) for c in range(8)]
        return array(data).transpose()

    def write_rcs_grid(self, param, data, n_rows=None):
        data = array(data).astype('int')
        if n_rows == None:
            n_rows = self.n_rows
        for rc in self.rcs_index:
            for c in range(8):
                self.write('rc%i' % (rc+1), '%s%i' % (param,c),
                           list(data[:n_rows,c+rc*8]))
        
    # Routines for shutting down various squid stages.

    def sa_off(self):
        self.write('sa', 'bias', self._zeros(self.n_cols))
    
    def sq2_off(self):
        self.write('sq2', 'bias', self._zeros(self.n_cols))

    def sq1_off(self, mux=False):
        self.write('ac', 'on_bias', self._zeros(self.n_rows))
        self.write('ac', 'off_bias', self._zeros(self.n_rows))
        self.write('ac', 'enbl_mux', [1])
        time.sleep(0.01)
        if not mux:
            self.write('ac', 'enbl_mux', [0])

    def tes_off(self):
        self.write('tes', 'bias', self_zeros(self.n_tes))

    def servo_off(self):
        # Things are quieter with flux jumping disabled.
        self.write('rca', 'en_fb_jump', [0])
        # Turn off servo
        self.write('rca', 'servo_mode', self._zeros(8))
        # May as well go to error readout
        self.write('rca', 'data_mode', [0])
        
    def freeze_servo(self, row=0, n_frames=10, degain=False):
        """
        Leaves the MCE in open loop mode (servo_off), but with
        fb_const set at the current lock point for the S1s on some row
        of each column.  The AC row_order and RC adc_offsets are set 
        """
        # Measure a few frames
        self.write('rca', 'data_mode', [1])
        data = array(self.read_frames(n_frames, data_only=True)).mean(axis=0)
        # Restrict to row of interest
        data = data.reshape((self.n_rows, self.n_cols))[row]
        # Mod out the flux quantum
        q = self.flux_quanta[row]
        if degain:
            data += q/2
        data = (data % q + q) % q
        # Prepare adc_offset
        new_adc = zeros(self.adc_offsets.shape)
        new_adc[:,:] = self.adc_offsets[row,:]
        # Write to fb_const, disable servo, set row_order
        self.write('sq1', 'fb_const', list(data.astype('int')))
        self.servo_off()
        self.write('ac', 'row_order', [row for i in range(self.n_rows)])
        self.write_rcs_grid('adc_offset', new_adc)


    # Rectangle

    def one_row(self):
        # The most trivial acceleration.
        self.write('rca', 'num_rows_reported', [1])
        self.write('cc', 'data_rate', [self.n_rows])
        

if __name__ == '__main__':
    from optparse import OptionParser
    o = OptionParser()
    o.add_option('--no-status', action='store_true')
    opts, args = o.parse_args()
    m = xmce(load_status=not opts.no_status)
    print 'm is your object.'
    if opts.no_status:
        print 'run m._load_status() before using.'

