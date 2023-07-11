from __future__ import print_function
from builtins import range
from builtins import object
import wx
import matplotlib

matplotlib.use('WX')

from matplotlib.backends.backend_wx import Toolbar, FigureCanvasWx, \
     FigureManager
from matplotlib.figure import Figure

class mcePlotInput(object):
    """
    Structure to contain MCE data for the purposes of plotting.
    """
    def __init__(self):
        self.y = []
        self.name = 'MCE'
        self.y_limits = None
        self.x_limits = None

class mceViewport(wx.Frame):
    """
    wx.Frame inheritor that provides visualization of MCE data.  This
    should probably just expose some more general plotting object,
    some day.
    """
    def __init__(self, parent, id, style=0):
        wx.Frame.__init__(self, parent, id, "MCE Viewport", \
                          style=wx.DEFAULT_FRAME_STYLE & ~wx.CLOSE_BOX)

#        self.SetWindowStyle(wx.SIMPLE_BORDER)

        self.fig = Figure((9,8), 60)
        self.canvas = FigureCanvasWx(self, -1, self.fig)
        self.axes = self.fig.add_subplot(111)
        
        self.toolbar = Toolbar(self.canvas)
        self.toolbar.Realize()
        # Create a figure manager to manage things
        self.figmgr = FigureManager(self.canvas, 1, self)
        # Now put all into a sizer
        sizer = wx.BoxSizer(wx.VERTICAL)
        # This way of adding to sizer allows resizing
        sizer.Add(self.canvas, 1, wx.LEFT|wx.TOP|wx.GROW)
        # Best to allow the toolbar to resize!
        sizer.Add(self.toolbar, 0, wx.GROW)
        self.SetSizer(sizer)
        self.Fit()

        #self.Update([0])
        #self.toolbar.update()

    def GetToolBar(self):
        # You will need to override GetToolBar if you are using an
        # unmanaged toolbar in your frame
        return self.toolbar
                                
    def Update(self, mce_data):

        y = mce_data.y
        x = list(range(len(y)))

        ll = self.axes.lines
        if len(ll) > 0: del ll[0]
        
        self.axes.plot(x, y)

        if mce_data.x_limits != None:
            print('Setting x-lims to', mce_data.x_limits)
            self.axes.set_xlim(mce_data.x_limits)
        
        self.axes.set_title(mce_data.name)
#        self.toolbar.update()
        self.Refresh()

    def set_xlim(self, xlim):
        self.axes.set_xlim(xlim)
        self.Refresh()
