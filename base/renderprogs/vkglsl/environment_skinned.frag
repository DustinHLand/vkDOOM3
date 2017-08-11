#version 450
#pragma shader_stage( fragment )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 2 ) uniform samplerCube samp0;

layout( location = 0 ) in vec3 in_TexCoord0;
layout( location = 1 ) in vec3 in_TexCoord1;
layout( location = 2 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_Color;

void main() {
	vec3 globalNormal = normalize( in_TexCoord1 );
	vec3 globalEye = normalize( in_TexCoord0 );
	vec3 reflectionVector = vec3( dot( globalEye, globalNormal ) );
	reflectionVector *= globalNormal;
	reflectionVector = ( reflectionVector * 2.0 ) - globalEye;
	vec4 envMap = texture( samp0, reflectionVector.xyz );
	out_Color = vec4( envMap.xyz , 1.0 ) * in_Color;
}