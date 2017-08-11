#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpTexGen0S;
	vec4 rpTexGen0T;
	vec4 rpTexGen1S;
	vec4 rpTexGen1T;
};
layout( binding = 1 ) uniform MAT_UBO {
	vec4 matrices[ 408 ];
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;
layout( location = 3 ) in vec4 in_Tangent;
layout( location = 4 ) in vec4 in_Color;
layout( location = 5 ) in vec4 in_Color2;

layout( location = 0 ) out vec2 out_TexCoord0;
layout( location = 1 ) out vec2 out_TexCoord1;

void main() {
	float w0 = in_Color2.x;
	float w1 = in_Color2.y;
	float w2 = in_Color2.z;
	float w3 = in_Color2.w;
	
	vec4 matX, matY, matZ;
	float joint = in_Color.x * 255.1 * 3;
	matX = matrices[ int( joint + 0 ) ] * w0;
	matY = matrices[ int( joint + 1 ) ] * w0;
	matZ = matrices[ int( joint + 2 ) ] * w0;
	joint = in_Color.y * 255.1 * 3;
	matX += matrices[ int( joint + 0 ) ] * w1;
	matY += matrices[ int( joint + 1 ) ] * w1;
	matZ += matrices[ int( joint + 2 ) ] * w1;
	joint = in_Color.z * 255.1 * 3;
	matX += matrices[ int( joint + 0 ) ] * w2;
	matY += matrices[ int( joint + 1 ) ] * w2;
	matZ += matrices[ int( joint + 2 ) ] * w2;
	joint = in_Color.w * 255.1 * 3;
	matX += matrices[ int( joint + 0 ) ] * w3;
	matY += matrices[ int( joint + 1 ) ] * w3;
	matZ += matrices[ int( joint + 2 ) ] * w3;
	
	vec4 position = vec4( in_Position, 1.0 );
	vec4 modelPosition;
	modelPosition.x = dot( matX, position );
	modelPosition.y = dot( matY, position );
	modelPosition.z = dot( matZ, position );
	modelPosition.w = 1.0;
	gl_Position.x = dot( modelPosition, rpMVPmatrixX );
	gl_Position.y = dot( modelPosition, rpMVPmatrixY );
	gl_Position.z = dot( modelPosition, rpMVPmatrixZ );
	gl_Position.w = dot( modelPosition, rpMVPmatrixW );
	
	out_TexCoord0.x = dot( modelPosition, rpTexGen0S );
	out_TexCoord0.y = dot( modelPosition, rpTexGen0T );
	out_TexCoord1.x = dot( modelPosition, rpTexGen1S );
	out_TexCoord1.y = dot( modelPosition, rpTexGen1T );
}