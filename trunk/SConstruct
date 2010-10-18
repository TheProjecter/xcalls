import os

opts = Options('custom.py')
opts.Add(BoolOption('test', 'Run unit tests. Use select_test to select specific test to run.', False))
opts.Add(BoolOption('ubench', 'Build library performance microbenchmarks.', False))
opts.Add(EnumOption('mode', 'Set library type.', 'debug', allowed_values=('debug', 'release')))
opts.Add(EnumOption('linkage', 'Set library linkage.', 'dynamic', allowed_values=('static', 'dynamic')))
opts.Add(EnumOption('tm_system', 'Set transactional memory system.', 'itm', allowed_values=('itm', 'logtm')))
opts.Add(BoolOption('stats', 'Build library with statistics support.', True))
opts.Add(PathOption('prefix','Installation directory', '/usr/lib'))

env = Environment(options = opts)
Help(opts.GenerateHelpText(env))

# Setup environment flags
env['CC'] = '/scratch/local/intel/Compiler/11.0/606/bin/ia32/icc'
env['CCFLAGS'] = '-Qtm_enabled'
env.Append(CCFLAGS = 	' -D_REENTRANT' + \
			 			' -fno-builtin-tolower' )
if env['tm_system'] == 'itm':
	env.Append(CCFLAGS = ' -D_TM_SYSTEM_ITM')
elif env['tm_system'] == 'logtm':
	env.Append(CCFLAGS = ' -D_TM_SYSTEM_LOGTM')

Export('env')

rootDir = os.getcwd()
env['MY_BUILD_DIR'] = os.path.join(rootDir, 'build')

SConscript('src/SConscript', variant_dir = env['MY_BUILD_DIR'])


#Testing Environment
if env['test'] == 1:
	SConscript('test/SConscript', variant_dir = os.path.join(env['MY_BUILD_DIR'], 'test'))

#Benchmark Environment
if env['ubench'] == 1:
	SConscript('benchmarks/ubench/SConscript', variant_dir = os.path.join(env['MY_BUILD_DIR'], 'benchmarks/ubench'))
