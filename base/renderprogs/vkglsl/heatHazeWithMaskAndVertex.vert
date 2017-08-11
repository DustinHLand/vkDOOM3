#version 450
#pragma shader_stage( vertex )

#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UBO {
	vec4 rpMVPmatrixX;
	vec4 rpMVPmatrixY;
	vec4 rpMVPmatrixZ;
	vec4 rpMVPmatrixW;
	vec4 rpProjectionMatrixY;
	vec4 rpProjectionMatrixW;
	vec4 rpModelViewMatrixZ;
	vec4 rpEnableSkinning;
	vec4 rpTextureScroll;
	vec4 rpDeformMagnitude;
};
layout( binding = 1 ) uniform MAT_UBO {
	vec4 matrices[ 408 ];
};

layout( location = 0 ) in vec3 in_Position;
layout( location = 1 ) in vec2 in_TexCoord;
layout( location = 2 ) in vec4 in_Normal;
layout( location = 3 ) in vec4 in_Tangent;
layout( location = 2 ) in vec4 in_Color;
layout( location = 3 ) in vec4 in_Color2;

layout( location = 0 ) out vec4 out_TexCoord0;
layout( location = 1 ) out vec4 out_TexCoord1;
layout( location = 2 ) out vec4 out_TexCoord2;
layout( location = 3 ) out vec4 out_Color;

void main() {
	vec4 position = vec4( in_Position, 1.0 );
	vec4 modelPosition = position; 
	if ( rpEnableSkinning.x > 0.0 ) {
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
		modelPosition.x = dot( matX, position );
		modelPosition.y = dot( matY, position );
		modelPosition.z = dot( matZ, position );
		modelPosition.w = 1.0;
	}
	
	gl_Position.x = dot( modelPosition, rpMVPmatrixX );
	gl_Position.y = dot( modelPosition, rpMVPmatrixY );
	gl_Position.z = dot( modelPosition, rpMVPmatrixZ );
	gl_Position.w = dot( modelPosition, rpMVPmatrixW );
	
	out_TexCoord0 = vec4( in_TexCoord, 0, 0 );
	out_TexCoord1 = vec4( in_TexCoord, 0, 0 ) + rpTextureScroll;
	
	vec4 vec = vec4( 0, 1, 0, 1 );
	vec.z = dot( modelPosition, rpModelViewMatrixZ );
	float magicProjectionAdjust = 0.43;
	float x = dot( vec, rpProjectionMatrixY ) * magicProjectionAdjust;
	float w = dot( vec, rpProjectionMatrixW );
	w = max( w, 1.0 );
	x /= w;
	x = min( x, 0.02 );
	out_TexCoord2 = x * rpDeformMagnitude;
	
	out_Color = in_Color;
}