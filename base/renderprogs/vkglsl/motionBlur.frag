#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	mat4 rpMVP;
	vec4 rpProjectionMatrixZ;
	vec4 rpOverbright;
};
layout( binding = 1 ) uniform sampler2D samp0;
layout( binding = 2 ) uniform sampler2D samp1;

layout( location = 0 ) in vec2 in_TexCoord0;

layout( location = 0 ) out vec4 out_Color;

void main() {
	if ( texture( samp0, in_TexCoord0.xy ).w == 0.0 ) {
		discard;
	}
	float windowZ = texture( samp1, in_TexCoord0.xy ).x;
	vec3 ndc = vec3( in_TexCoord0 * 2.0 - 1.0, windowZ * 2.0 - 1.0 );
	float clipW = - rpProjectionMatrixZ.w / ( -rpProjectionMatrixZ.z - ndc.z );
	vec4 clip = vec4( ndc * clipW, clipW );
	vec4 reClip = rpMVP * clip;
	vec2 prevTexCoord;
	prevTexCoord.x = ( reClip. x / reClip. w ) * 0.5 + 0.5;
	prevTexCoord.y = ( reClip. y / reClip. w ) * 0.5 + 0.5;
	vec2 texCoord = prevTexCoord;
	vec2 delta = ( in_TexCoord0 - prevTexCoord );
	vec3 sum = vec3 ( 0.0 );
	float goodSamples = 0;
	float samples = rpOverbright.x;
	for ( float i = 0; i < samples; i = i + 1 ) {
		vec2 pos = in_TexCoord0 + delta * ( ( i / ( samples - 1 ) ) - 0.5 );
		vec4 color = texture( samp0, pos.xy );
		sum += color.xyz * color.w;
		goodSamples += color.w;
	}
	float invScale = 1.0 / goodSamples;
	out_Color = vec4( sum * invScale, 1.0 );
}