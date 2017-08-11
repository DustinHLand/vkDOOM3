#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpVertexColorModulate;
	vec4 rpVertexColorAdd;
	vec4 rpColor;
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpTextureMatrixS;
	vec4 rpTextureMatrixT;
	vec4 rpTexGen0S;
	vec4 rpTexGen0T;
	vec4 rpTexGen0Q;
	vec4 rpTexGenEnabled;
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec4 in_Color;

layout( location = 0 ) out vec4 out_TexCoord0;
layout( location = 1 ) out vec4 out_Color;

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	gl_Position.x = dot( position, rpMVPmatrixX );
	gl_Position.y = dot( position, rpMVPmatrixY );
	gl_Position.z = dot( position, rpMVPmatrixZ );
	gl_Position.w = dot( position, rpMVPmatrixW );
	
	vec4 tc0;
	tc0.x = dot( position, rpTexGen0S );
	tc0.y = dot( position, rpTexGen0T );
	tc0.z = 0.0;
	tc0.w = dot( position, rpTexGen0Q );
	out_TexCoord0.x = dot( tc0, rpTextureMatrixS );
	out_TexCoord0.y = dot( tc0, rpTextureMatrixT );
	out_TexCoord0.zw = tc0.zw;
	vec4 vertexColor = ( in_Color * rpVertexColorModulate ) + rpVertexColorAdd;
	out_Color = vertexColor * rpColor;
}