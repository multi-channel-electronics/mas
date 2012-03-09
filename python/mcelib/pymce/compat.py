from pymce import BasicMCE

"""
old_mce is intended to provide an interface compatible with the
original mce.py:mce interface, for transitional purposes.
"""

class old_mce(BasicMCE):
    def read_frames(self, count, channel_set=False, data_only=False):
        d = self.read_data(count, extract=False, row_col=False)
        if data_only:
            return d.data
        return d.data, d.headers
    def read_frame(self, channel_set = False, data_only=False):
        if data_only:
            d = self.read_frames(1, channel_set=channel_set, data_only=True)
            return d[0]
        d, h = self.read_frames(1, channel_set=channel_set, data_only=False)
        return d[0], h[0]
        
