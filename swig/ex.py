#!/usr/bin/python

# Some code based on embedding_in_ex.py by Jeremy O'Donoghue

# communicate.py

from __future__ import print_function
from builtins import str
import wx
from mceViewport import *
from mce_acquisitions import *

class LeftPanel(wx.Panel):
    def __init__(self, parent, id, mce):
        wx.Panel.__init__(self, parent, id, style=wx.BORDER_SUNKEN)

        self.parent = parent.GetParent()

        self.mce = mce

        button1 = wx.Button(self, -1, '+', (10, 10))
        button2 = wx.Button(self, -1, '-', (10, 60))
        button3 = wx.Button(self, -1, '!!!', (10, 110))

        self.Bind(wx.EVT_BUTTON, self.OnPlus, id=button1.GetId())
        self.Bind(wx.EVT_BUTTON, self.OnMinus, id=button2.GetId())
        self.Bind(wx.EVT_BUTTON, self.OnHigh, id=button3.GetId())

    def OnHigh(self, even):
        f = self.parent.target

        n_frames = int(self.parent.n_frames.GetValue())
        row = int(self.parent.row.GetValue())
        col = int(self.parent.column.GetValue())

        #ca = mceChannelAcq(self.mce)
        #p = ca.acq(col, row, n_frames)
        
        #ca = mceColumnAcq(self.mce)
        #p = ca.acq(col, n_frames)
        
        ca = mceColumnCorrAcq(self.mce)
        p = ca.acq(col, row, n_frames)

        trim = int(self.parent.trim.GetValue())
        if trim > 0:
            p.x_limits = (0, trim)
        
        if f != None:
            f.Update(p)

    def OnPlus(self, event):
        value = int(self.text.GetValue())
        value = value + 1
        self.text.SetValue(str(value))

    def OnMinus(self, event):
        value = int(self.text.GetValue())
        value = value - 1
        self.text.SetValue(str(value))


class RightPanel(wx.Panel):
    def __init__(self, parent, id):
        wx.Panel.__init__(self, parent, id, style=wx.BORDER_SUNKEN)
        self.text = wx.StaticText(self, -1, '0', (40, 60))

class myText(wx.TextCtrl):
    def __init__(self, parent, id, start, pos=None):
        wx.TextCtrl.__init__(self, parent, id, str(start), pos)

class labelledText(wx.BoxSizer):
    def __init__(self, parent, id, label_text, ctrl):
        wx.BoxSizer.__init__(self, wx.HORIZONTAL)
        self.ctrl = ctrl
        self.label = wx.StaticText(parent, -1, label_text)
        self.Add(self.label, flag=wx.ALIGN_LEFT | wx.EXPAND)
        self.Add(self.ctrl, flag=wx.ALIGN_RIGHT | wx.EXPAND)

    def GetValue(self):
        return self.ctrl.GetValue()

    def SetValue(self, val):
        return self.ctrl.SetVaule(val)

class Communicate(wx.Frame):
    def __init__(self, parent, id, title, mce):
        wx.Frame.__init__(self, parent, id, title, size=(280, 200))

        self.target = mceViewport(self, -1 ) #, style = wx.SIMPLE_BORDER)
        
        panel = wx.Panel(self, -1)
        #self.rightPanel = RightPanel(panel, -1)
        rightPanel = wx.Panel(panel, -1)
        #self.n_frames = myText(rightPanel, -1, 100, (10,10))
        #self.column = myText(rightPanel, -1, 2, (10,60))
        #self.row = myText(rightPanel, -1, 3, (10,110))
        rightSizer = wx.BoxSizer(wx.VERTICAL)
#        self.n_frames = myText(panel, -1, 100, (10,10))
#        self.column = wx.SpinCtrl(panel, -1, '0', (55, 90), (60, -1), min=0, max=31)
#        self.row = wx.SpinCtrl(panel, -1, '0', (55, 90), (60, -1), min=0, max=32)
        
        self.n_frames = labelledText(panel, -1, 'count:',
                                     myText(panel, -1, 100))
                                     
        self.column = labelledText(panel, -1, 'col:',
                                   wx.SpinCtrl(panel, -1, '0', min=0, max=31))
        self.row = labelledText(panel, -1, 'row:',
                                   wx.SpinCtrl(panel, -1, '0', min=0, max=32))

        self.trim = labelledText(panel, -1, 'trim_to:',
                                 myText(panel, -1, '-1'))

        rightSizer.Add(self.n_frames, 1)
        rightSizer.Add(self.column, 1)
        rightSizer.Add(self.row, 1)
        rightSizer.Add(self.trim, 1)
        
        leftPanel = LeftPanel(panel, -1, mce)

        hbox = wx.BoxSizer()
        hbox.Add(leftPanel, 1, wx.EXPAND | wx.ALL, 5)
        hbox.Add(rightSizer, 1, wx.EXPAND | wx.ALL, 5)
        

        panel.SetSizer(hbox) 
        self.Centre()
        self.Show(True)
        self.target.Show()

    def Destroy(self, force=False):
        print('Caught!')
        self.fig.Close(force)
        wx.Frame.Destroy(self, force)


#app = wx.App()
#Communicate(None, -1, 'widgets communicate')
#app.MainLoop()
