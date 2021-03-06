#!/usr/bin/env python
# 
# filter-noisy-assembler-warnings.py
# Author: Stuart Berg
# <https://stackoverflow.com/a/41515691>

import sys

for line in sys.stdin:
    # If line is a 'noisy' warning, don't print it or the following two lines.
    if ('warning: section' in line and 'is deprecated' in line
    or 'note: change section name to' in line):
        next(sys.stdin)
        next(sys.stdin)
    else:
        sys.stderr.write(line)
        sys.stderr.flush()
