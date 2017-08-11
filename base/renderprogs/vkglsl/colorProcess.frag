#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform sampler2D samp0;

layout( location = 0 ) in vec3 in_TexCoord0;
layout( location = 1 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void main() {
	vec4 src = texture( samp0, in_TexCoord0.xy );
	vec4 target = in_Color * dot( vec3( 0.333, 0.333, 0.333 ), src.xyz );
	out_Color = mix( src, target, in_TexCoord0.z );
}