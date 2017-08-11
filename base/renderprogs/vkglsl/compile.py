import os
from glob import glob
from subprocess import call

for glsl_ext, sprv_ext in [ ('vert', 'vspv'), ('frag', 'fspv') ]:
	shaders = glob( '*.{0}'.format( glsl_ext ) )
	for sh in shaders:
		name = sh.split( '.' )[0]
		cmd = [ 'glslc', sh, '-o', '..\\spirv\\{0}.{1}'.format( name, sprv_ext ) ]
		print( cmd )
		call( cmd )