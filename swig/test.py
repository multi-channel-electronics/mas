from mce import *
# from embedding_in_wx import PlotFigure

m = mce()
g = m.read_frame()
#print g

h = ChannelSet(rc = 'rc2')
print 'Setting rows...'
h.rows = [1, 2, 8, 9]
print h.columns
print h.rows
print h.cards_span()
print h.data_size()
print h.acq_size()

g = m.read_frame(h)
print g[1]

print 'Extracting'
r = h.extract_channels(g[0])
print r

from pylab import *

# delta = 0.025
# x = y = arange(-3.0, 3.0, delta)
# X, Y = meshgrid(x, y)
# Z1 = bivariate_normal(X, Y, 1.0, 1.0, 0.0, 0.0)
# Z2 = bivariate_normal(X, Y, 1.5, 0.5, 1, 1)
# Z = Z2-Z1  # difference of Gaussians

#im = imshow(r, interpolation='bilinear', cmap=cm.gray,
#            origin='lower', extent=[-3,3,-3,3])

#im = imshow(r, cmap=cm.gray,
#            origin='lower', extent=[-3,3,-3,3])

#savefig('image_demo')
#show()


# import biggles

# bg = biggles.FramedPlot()
# pts = biggles.Points( r[0], r[1])
# bg.add( pts )
# bg.show()
# print 'Yup, non-block.'

import wx
from ex import *

app = wx.App()

#frame = wx.Frame(None, -1, 'simple.py')
#frame.Show()

#frame2 = PlotFigure()
#frame2.plot_data()
#frame2.Show()

# from matplotlib.backends.backend_wx import Toolbar, FigureCanvasWx,\
#           FigureManager
# from matplotlib.figure import Figure

# frame2 = wx.Frame(None, -1, 'figure')
# fig = Figure((9,8), 75)
# sp = fig.add_subplot(111)
# sp.plot(r[0],r[1])

# frame2.canvas = FigureCanvasWx(frame2, -1, fig)
# frame2.Show()

c = Communicate(None, -1, 'comm')

app.MainLoop()
