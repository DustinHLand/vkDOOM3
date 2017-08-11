#version 150
#define PC

float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }
vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }
vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }
vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }
vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }


uniform vec4 _va_[10];

float dot4 (vec4 a , vec4 b ) {return dot ( a , b ) ; }
float dot4 (vec2 a , vec4 b ) {return dot ( vec4 ( a , 0 , 1 ) , b ) ; }
vec4 swizzleColor (vec4 c ) {return c ; }
uniform matrices_ubo {vec4 matrices [ 408 ] ; } ;

in vec4 in_Position;
in vec2 in_TexCoord;
in vec4 in_Normal;
in vec4 in_Tangent;
in vec4 in_Color;
in vec4 in_Color2;

out vec4 gl_Position;
out vec4 vofi_TexCoord0;
out vec4 vofi_TexCoord1;
out vec4 vofi_TexCoord2;
out vec4 gl_FrontColor;

void main() {vec4 modelPosition = in_Position ; if ( _va_[7 /* rpEnableSkinning */] . x > 0.0 ) {
		float w0 = in_Color2 . x ;
		float w1 = in_Color2 . y ;
		float w2 = in_Color2 . z ;
		float w3 = in_Color2 . w ;
		vec4 matX , matY , matZ ;
		float joint = in_Color . x * 255.1 * 3 ;
		matX = matrices [ int ( joint + 0 ) ] * w0 ;
		matY = matrices [ int ( joint + 1 ) ] * w0 ;
		matZ = matrices [ int ( joint + 2 ) ] * w0 ;
		joint = in_Color . y * 255.1 * 3 ;
		matX += matrices [ int ( joint + 0 ) ] * w1 ;
		matY += matrices [ int ( joint + 1 ) ] * w1 ;
		matZ += matrices [ int ( joint + 2 ) ] * w1 ;
		joint = in_Color . z * 255.1 * 3 ;
		matX += matrices [ int ( joint + 0 ) ] * w2 ;
		matY += matrices [ int ( joint + 1 ) ] * w2 ;
		matZ += matrices [ int ( joint + 2 ) ] * w2 ;
		joint = in_Color . w * 255.1 * 3 ;
		matX += matrices [ int ( joint + 0 ) ] * w3 ;
		matY += matrices [ int ( joint + 1 ) ] * w3 ;
		matZ += matrices [ int ( joint + 2 ) ] * w3 ;
		modelPosition. x = dot4 ( matX , in_Position ) ;
		modelPosition. y = dot4 ( matY , in_Position ) ;
		modelPosition. z = dot4 ( matZ , in_Position ) ;
		modelPosition. w = 1.0 ;
	}
	gl_Position . x = dot4 ( modelPosition , _va_[0 /* rpMVPmatrixX */] ) ;
	gl_Position . y = dot4 ( modelPosition , _va_[1 /* rpMVPmatrixY */] ) ;
	gl_Position . z = dot4 ( modelPosition , _va_[2 /* rpMVPmatrixZ */] ) ;
	gl_Position . w = dot4 ( modelPosition , _va_[3 /* rpMVPmatrixW */] ) ;
	vofi_TexCoord0 = vec4 ( in_TexCoord , 0 , 0 ) ;
	vec4 textureScroll = _va_[8 /* rpUser0 */] ;
	vofi_TexCoord1 = vec4 ( in_TexCoord , 0 , 0 ) + textureScroll ;
	vec4 vec = vec4 ( 0 , 1 , 0 , 1 ) ;
	vec. z = dot4 ( modelPosition , _va_[6 /* rpModelViewMatrixZ */] ) ;
	float magicProjectionAdjust = 0.43 ;
	float x = dot4 ( vec , _va_[4 /* rpProjectionMatrixY */] ) * magicProjectionAdjust ;
	float w = dot4 ( vec , _va_[5 /* rpProjectionMatrixW */] ) ;
	w = max ( w , 1.0 ) ;
	x /= w ;
	x = min ( x , 0.02 ) ;
	vec4 deformMagnitude = _va_[9 /* rpUser1 */] ;
	vofi_TexCoord2 = x * deformMagnitude ;
	gl_FrontColor = swizzleColor ( in_Color ) ;
}