#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform sampler2D samp0;

layout( location = 0 ) in vec4 in_TexCoord0;
layout( location = 1 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void main() {
	vec4 texSample = textureProj( samp0, in_TexCoord0.xyw );
	out_Color = texSample * in_Color;
}