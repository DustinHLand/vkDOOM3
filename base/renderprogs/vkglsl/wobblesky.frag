#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform samplerCube samp0;

layout( location = 0 ) in vec3 in_TexCoord0;
layout( location = 1 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void main() {
	out_Color = texture( samp0, in_TexCoord0.xyz ) * in_Color;
}