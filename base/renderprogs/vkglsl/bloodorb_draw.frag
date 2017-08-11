#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform sampler2D samp0;
layout( binding = 2 ) uniform sampler2D samp1;
layout( binding = 3 ) uniform sampler2D samp2;

layout( location = 0 ) in vec2 in_TexCoord0;

layout( location = 0 ) out vec4 out_Color;

void main() {
	vec4 accumSample = texture( samp0, in_TexCoord0.xy );
	vec4 currentRenderSample = texture( samp1, in_TexCoord0.xy );
	vec4 maskSample = texture( samp2, in_TexCoord0.xy );
	out_Color = mix( accumSample, currentRenderSample, maskSample.a );
}