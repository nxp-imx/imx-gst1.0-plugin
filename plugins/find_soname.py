import re
import sys
import os
import subprocess

filename = sys.argv[1]
if not os.path.exists(filename):
  print('file_not_exist')
  sys.exit()

cmd = ['objdump', '-p', filename]
out = subprocess.check_output(cmd)

result = re.search('^\s+SONAME\s+(.+)$',out.decode("utf-8"),re.MULTILINE)

print(result.group(1))
