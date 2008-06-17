#!/usr/bin/python

# Some code based on embedding_in_ex.py by Jeremy O'Donoghue

# communicate.py

import wx

import matplotlib
matplotlib.use('WX')
from matplotlib.backends.backend_wx import Toolbar, FigureCanvasWx,\
     FigureManager

from matplotlib.figure import Figure

class FigureFrame(wx.Frame):
    def __init__(self, parent, id, style=0):
        wx.Frame.__init__(self, parent, id, "Test embed", \
                          style=wx.DEFAULT_FRAME_STYLE & ~wx.CLOSE_BOX)

#        self.SetWindowStyle(wx.SIMPLE_BORDER)

        self.fig = Figure((9,8), 75)
        self.canvas = FigureCanvasWx(self, -1, self.fig)
        self.subp = self.fig.add_subplot(111)

        self.Update(0)
        
    def Update(self, x):
        r = range(10)
        x = [ (rr+x)**2 for rr in r ]
        self.fig.delaxes(self.subp)

        self.subp = self.fig.add_subplot(111)
        self.subp.plot(r, x)
        self.Refresh()

class LeftPanel(wx.Panel):
    def __init__(self, parent, id):
        wx.Panel.__init__(self, parent, id, style=wx.BORDER_SUNKEN)

        self.text = parent.GetParent().rightPanel.text
        self.parent = parent.GetParent()

        button1 = wx.Button(self, -1, '+', (10, 10))
        button2 = wx.Button(self, -1, '-', (10, 60))
        button3 = wx.Button(self, -1, '!!!', (10, 110))

        self.Bind(wx.EVT_BUTTON, self.OnPlus, id=button1.GetId())
        self.Bind(wx.EVT_BUTTON, self.OnMinus, id=button2.GetId())
        self.Bind(wx.EVT_BUTTON, self.OnHigh, id=button3.GetId())

    def OnHigh(self, even):
        f = self.parent.target
        if f != None:
            f.Update(int(self.text.GetLabel()))

    def OnPlus(self, event):
        value = int(self.text.GetLabel())
        value = value + 1
        self.text.SetLabel(str(value))

    def OnMinus(self, event):
        value = int(self.text.GetLabel())
        value = value - 1
        self.text.SetLabel(str(value))


class RightPanel(wx.Panel):
    def __init__(self, parent, id):
        wx.Panel.__init__(self, parent, id, style=wx.BORDER_SUNKEN)
        self.text = wx.StaticText(self, -1, '0', (40, 60))

class Communicate(wx.Frame):
    def __init__(self, parent, id, title):
        wx.Frame.__init__(self, parent, id, title, size=(280, 200))

        self.target = FigureFrame(self, -1 ) #, style = wx.SIMPLE_BORDER)
        
        panel = wx.Panel(self, -1)
        self.rightPanel = RightPanel(panel, -1)

        leftPanel = LeftPanel(panel, -1)

        hbox = wx.BoxSizer()
        hbox.Add(leftPanel, 1, wx.EXPAND | wx.ALL, 5)
        hbox.Add(self.rightPanel, 1, wx.EXPAND | wx.ALL, 5)

        panel.SetSizer(hbox) 
        self.Centre()
        self.Show(True)
        self.target.Show()

    def Destroy(self, force=False):
        print 'Caught!'
        self.fig.Close(force)
        wx.Frame.Destroy(self, force)


#app = wx.App()
#Communicate(None, -1, 'widgets communicate')
#app.MainLoop()
