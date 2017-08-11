#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( location = 0 ) in vec4 in_TexCoord0;

layout( location = 0 ) out vec4 out_Color;

vec2 screenPosToTexcoord ( vec2 pos, vec4 bias_scale ) { 
	return ( pos * bias_scale.zw + bias_scale.xy ); 
}

void main() {
	float renderWidth = 1280.0;
	float renderHeight = 720.0;
	vec4 positionToViewTexture = vec4( 0.5 / renderWidth, 0.5 / renderHeight, 1.0 / renderWidth, 1.0 / renderHeight );
	float interpolatedZOverW = ( 1.0 - ( in_TexCoord0.z / in_TexCoord0.w ) );
	vec3 pos;
	pos.z = 1.0 / interpolatedZOverW;
	pos.xy = pos.z * ( 2.0 * screenPosToTexcoord ( gl_FragCoord.xy, positionToViewTexture ) - 1.0 );
	vec3 normal = normalize( cross( dFdy( pos ), dFdx( pos ) ) );
	vec3 L = normalize( vec3( 1.0, 1.0, 0.0 ) - pos );
	out_Color.xyz = vec3( dot( normal, L ) * 0.75 );
	out_Color.w = 1.0;
}