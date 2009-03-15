import os
import os.path
import sys

env = Environment(
    tools = ['generic'],
    toolpath = ['scons'],
    ENV = os.environ,
)

Export('env')

SConscript('SConscript')
SConscript('samples/SConscript')
