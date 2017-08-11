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

layout( location = 0 ) in vec4 in_Position;

void main() {
	vec4 vPos = in_Position - rpLocalLightOrigin;
	vPos = ( vPos.wwww * rpLocalLightOrigin ) + vPos;
	gl_Position.x = dot( vPos, rpMVPmatrixX );
	gl_Position.y = dot( vPos, rpMVPmatrixY );
	gl_Position.z = dot( vPos, rpMVPmatrixZ );
	gl_Position.w = dot( vPos, rpMVPmatrixW );
}