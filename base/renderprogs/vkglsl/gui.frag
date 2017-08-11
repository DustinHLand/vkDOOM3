#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform sampler2D samp0;

layout( location = 0 ) in vec2 in_TexCoord0;
layout( location = 1 ) in vec4 in_TexCoord1;
layout( location = 2 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void main() {
	vec4 color = ( texture( samp0, in_TexCoord0.xy ) * in_Color ) + in_TexCoord1;
	out_Color.xyz = color.xyz * color.w;
	out_Color.w = color.w ;
}