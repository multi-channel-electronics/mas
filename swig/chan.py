#!/usr/bin/python

# Some code based on embedding_in_ex.py by Jeremy O'Donoghue

from __future__ import print_function
from builtins import str
import wx
import mce
from mceViewport import *
from mce_acquisitions import *

class myText(wx.TextCtrl):
    def __init__(self, parent, id, start, pos=None):
        wx.TextCtrl.__init__(self, parent, id, str(start), pos)

class labelledText(wx.BoxSizer):
    def __init__(self, parent, id, label_text, ctrl):
        wx.BoxSizer.__init__(self, wx.HORIZONTAL)
        self.ctrl = ctrl
        self.label = wx.StaticText(parent, -1, label_text)
        self.Add(self.label, 1, flag=wx.ALIGN_LEFT |wx.ALIGN_CENTER_VERTICAL)
        self.Add(self.ctrl, 1, flag=wx.ALIGN_RIGHT |wx.ALIGN_CENTER_VERTICAL)

    def GetValue(self):
        return self.ctrl.GetValue()

    def SetValue(self, val):
        return self.ctrl.SetVaule(val)

class ChannelView(wx.Frame):
    def __init__(self, parent, id, title, mce):
        wx.Frame.__init__(self, parent, id, title, size=(480, 200))

        self.viewport = mceViewport(self, -1 ) #, style = wx.SIMPLE_BORDER)
        self.mce = mce

        panel = wx.Panel(self, -1)
        panelSizer = wx.BoxSizer(wx.VERTICAL)

        self.n_frames = labelledText(panel, -1, 'count:',
                                     myText(panel, -1, 100))
        self.column = labelledText(panel, -1, 'col:',
                                   wx.SpinCtrl(panel, -1, '0', min=0, max=31))
        self.row = labelledText(panel, -1, 'row:',
                                   wx.SpinCtrl(panel, -1, '0', min=0, max=32))
        self.trim = labelledText(panel, -1, 'trim_to:',
                                 myText(panel, -1, '-1'))
        self.go_button = wx.Button(panel, -1, 'Acquire')

        self.all_columns = wx.CheckBox(panel, -1, 'All columns')
        self.all_rows = wx.CheckBox(panel, -1, 'All rows')

        self.Bind(wx.EVT_BUTTON, self.Go, id=self.go_button.GetId())

        panelSizer.Add(self.n_frames, 1, flag=wx.EXPAND)
        panelSizer.Add(self.column, 1, flag=wx.EXPAND)
        panelSizer.Add(self.row, 1, flag=wx.EXPAND)
        panelSizer.Add(self.trim, 1, flag=wx.EXPAND)
        panelSizer.Add(self.go_button, 1, flag=wx.EXPAND)
        panelSizer.Add(self.all_columns, 1, flag=wx.EXPAND)
        panelSizer.Add(self.all_rows, 1, flag=wx.EXPAND)

        panel.SetSizer(panelSizer) 
        self.Centre()
        self.Show(True)
        self.viewport.Show()

    def Go(self, even):

        n_frames = int(self.n_frames.GetValue())
        row = int(self.row.GetValue())
        col = int(self.column.GetValue())

        ca = mceChannelAcq(self.mce)
        p = ca.acq(col, row, n_frames)
        
        trim = int(self.trim.GetValue())
        print("Trim is %i" % trim)
        if trim > 0:
            p.x_limits = (0, trim)
        else:
            p.x_limits = None
      
        if self.viewport != None:
            self.viewport.Update(p)

    def Destroy(self, force=False):
        print('Caught!')
        self.fig.Close(force)
        wx.Frame.Destroy(self, force)


app = wx.App()
m = mce.mce()
ChannelView(None, -1, 'ChannelView', m)
app.MainLoop()
