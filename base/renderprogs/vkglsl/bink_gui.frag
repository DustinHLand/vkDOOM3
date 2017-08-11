#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 1 ) uniform sampler2D samp0;
layout( binding = 2 ) uniform sampler2D samp1;
layout( binding = 3 ) uniform sampler2D samp2;

layout( location = 0 ) in vec2 in_TexCoord0;
layout( location = 1 ) in vec4 in_TexCoord1;
layout( location = 2 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void main() {
	vec3 crc = vec3 ( 1.595794678, -0.813476563, 0 );
	vec3 crb = vec3 ( 0, -0.391448975 , 2.017822266 );
	vec3 adj = vec3 ( -0.87065506, 0.529705048, -1.081668854 );
	vec3 YScalar = vec3 ( 1.164123535, 1.164123535, 1.164123535 );
	float Y  = texture( samp0, in_TexCoord0.xy ).x;
	float Cr = texture( samp1, in_TexCoord0.xy ).x;
	float Cb = texture( samp2, in_TexCoord0.xy ).x;
	vec3 p = ( YScalar * Y );
	p += ( crc * Cr ) + ( crb * Cb ) + adj;
	vec4 binkImage;
	binkImage.xyz = p;
	binkImage.w = 1.0;
	vec4 color = ( binkImage * in_Color ) + in_TexCoord1;
	out_Color.xyz = color.xyz * color.w;
	out_Color.w = color.w ;
}