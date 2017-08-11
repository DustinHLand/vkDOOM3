#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 2 ) uniform samplerCube samp0;
layout( binding = 3 ) uniform sampler2D samp1;

layout( location = 0 ) in vec2 in_TexCoord0;
layout( location = 1 ) in vec3 in_TexCoord1;
layout( location = 2 ) in vec3 in_TexCoord2;
layout( location = 3 ) in vec3 in_TexCoord3;
layout( location = 4 ) in vec3 in_TexCoord4;
layout( location = 5 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void main() {
	vec4 bump = texture( samp1, in_TexCoord0.xy ) * 2.0 - 1.0;
	vec3 localNormal = vec3( bump.wy, 0.0 );
	localNormal.z = sqrt( 1.0 - dot( localNormal, localNormal ) );
	vec3 globalNormal;
	globalNormal.x = dot( localNormal, in_TexCoord2 );
	globalNormal.y = dot( localNormal, in_TexCoord3 );
	globalNormal.z = dot( localNormal, in_TexCoord4 );
	vec3 globalEye = normalize( in_TexCoord1 );
	vec3 reflectionVector = globalNormal * dot( globalEye, globalNormal );
	reflectionVector = ( reflectionVector * 2.0 ) - globalEye;
	vec4 envMap = texture( samp0, reflectionVector.xyz );
	out_Color = vec4( envMap.xyz, 1.0 ) * in_Color;
}