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
layout( binding = 1 ) uniform MAT_UBO {
	vec4 matrices[ 408 ];
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;
layout( location = 3 ) in vec4 in_Tangent;
layout( location = 4 ) in vec4 in_Color;
layout( location = 5 ) in vec4 in_Color2;

layout( location = 0 ) out vec3 out_TexCoord0;
layout( location = 1 ) out vec3 out_TexCoord1;
layout( location = 2 ) out vec4 out_Color;

float dot3( vec4 a, vec4 b ) { return dot( a.xyz, b.xyz ); }

void main() {
	vec4 vNormal = in_Normal * 2.0 - 1.0;
	
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
	
	vec3 vNormalSkinned;
	vNormalSkinned.x = dot3( matX, vNormal );
	vNormalSkinned.y = dot3( matY, vNormal );
	vNormalSkinned.z = dot3( matZ, vNormal );
	vNormalSkinned = normalize( vNormalSkinned );
	
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
	
	vec4 toEye = rpLocalViewOrigin - modelPosition;
	out_TexCoord0 = toEye.xyz;
	out_TexCoord1 = vNormalSkinned.xyz;
	out_Color = rpColor;
}