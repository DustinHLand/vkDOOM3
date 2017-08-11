#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpLocalViewOrigin;
	vec4 rpColor;
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;

layout( location = 0 ) out vec3 out_TexCoord0;
layout( location = 1 ) out vec3 out_TexCoord1;
layout( location = 2 ) out vec4 out_Color;

void main() {
	vec4 vNormal = in_Normal * 2.0 - 1.0;
	
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	vec4 toEye = rpLocalViewOrigin - position;
	out_TexCoord0 = toEye.xyz;
	out_TexCoord1 = vNormal.xyz;
	out_Color = rpColor;
}