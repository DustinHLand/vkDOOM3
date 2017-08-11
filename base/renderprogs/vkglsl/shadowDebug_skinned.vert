#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpLocalLightOrigin;
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
};
layout( binding = 1 ) uniform MAT_UBO {
	vec4 matrices[ 408 ];
};

layout( location = 0 ) in vec4 in_Position;
layout( location = 1 ) in vec4 in_Color;
layout( location = 2 ) in vec4 in_Color2;

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
	
	vec4 modelPosition;
	modelPosition.x = dot( matX, in_Position );
	modelPosition.y = dot( matY, in_Position );
	modelPosition.z = dot( matZ, in_Position );
	modelPosition.w = 1.0;
	vec4 vPos = modelPosition - rpLocalLightOrigin;
	vPos = ( vPos.wwww * rpLocalLightOrigin ) + vPos;
	gl_Position.x = dot( vPos, rpMVPmatrixX );
	gl_Position.y = dot( vPos, rpMVPmatrixY );
	gl_Position.z = dot( vPos, rpMVPmatrixZ );
	gl_Position.w = dot( vPos, rpMVPmatrixW );
}