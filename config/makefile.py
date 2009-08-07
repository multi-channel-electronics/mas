#!/usr/bin/python

import os
from commands import getoutput as getout
from optparse import OptionParser

master_templates = [('v4', 'templates/master-4.template'),
                    ('v5', 'templates/master-5.template'),
                    ]

cpp = 'cpp'
cpp_opts = '-P -nostdinc -I ./'

output_files = {}
for v, vopt in [('1', ''), ('2', '-DSUBRACK_SMALL')]: #sub-rack style
    for t, topt in [('', ''), ('_16tes', '-DTES_16')]: #16 TES?
        for b, bopt in [('', ''), ('_bac', '-DBIASING_AC')]: #biasing AC?
            output_files['mce_v%s%s%s.cfg' % (v,t,b)] = '%s %s %s' % (vopt, topt, bopt)

if __name__ == '__main__':
    o = OptionParser()
    o.add_option('-M','--dependencies',action='store_true')
    o.add_option('-T','--targets',action='store_true')
    opts, args = o.parse_args()
    if len(args) != 0:
        print 'What, arguments?'
        sys.exit(1)
    
    if opts.targets:
        print ' '.join(['%s/%s' % (o, t) for o,_ in master_templates for t in output_files.keys()])
    elif opts.dependencies:
        deps = []
        for output, template in master_templates:
            for k, v in zip(output_files.keys(), output_files.values()):
                cmd = '%s -M %s %s %s' % (cpp, cpp_opts, v, template)
                deps.extend(getout(cmd).split(':')[-1].split())
        print ' '.join(list(set(deps) -set('\\')))
    else:
        for output, template in master_templates:
            for k, v in zip(output_files.keys(), output_files.values()):
                cmd = '%s %s %s %s %s/%s' % (cpp, cpp_opts, v, template, output, k)
                co = getout(cmd)
                print cmd
                print co,
