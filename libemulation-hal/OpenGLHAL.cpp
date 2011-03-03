
/**
 * libemulation-hal
 * OpenGL HAL
 * (C) 2010-2011 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Implements an OpenGL HAL.
 */

#include <math.h>

#include "OpenGLHAL.h"

#include "OpenGLHALSignalProcessing.h"

#define NTSC_YIQ_I_SHIFT	(0.6 / (14.31818 / 4))

OpenGLHAL::OpenGLHAL(string resourcePath)
{
	this->resourcePath = resourcePath;
	
	setCapture = NULL;
	setKeyboardFlags = NULL;
	
	nextFrame = NULL;
	
	isConfigurationValid = false;
	
	glViewportSize = OEMakeSize(0, 0);
	for (int i = 0; i < OPENGLHAL_TEXTURE_END; i++)
		glTextures[i] = 0;
	glTextureSize = OEMakeSize(0, 0);
	glFrameSize = OEMakeSize(0, 0);
	for (int i = 0; i < OPENGLHAL_PROGRAM_END; i++)
		glPrograms[i] = 0;
	glProcessProgram = 0;
	
	capture = OPENGLHAL_CAPTURE_NONE;
	
	for (int i = 0; i < CANVAS_KEYBOARD_KEY_NUM; i++)
		keyDown[i] = false;
	keyDownCount = 0;
	ctrlAltWasPressed = false;
	mouseEntered = false;
	for (int i = 0; i < CANVAS_MOUSE_BUTTON_NUM; i++)
		mouseButtonDown[i] = false;
	for (int i = 0; i < CANVAS_JOYSTICK_NUM; i++)
		for (int j = 0; j < CANVAS_JOYSTICK_BUTTON_NUM; j++)
			joystickButtonDown[i][j] = false;
}

// Video

void OpenGLHAL::open(CanvasSetCapture setCapture,
					 CanvasSetKeyboardFlags setKeyboardFlags,
					 void *userData)
{
	this->setCapture = setCapture;
	this->setKeyboardFlags = setKeyboardFlags;
	this->userData = userData;
	
	initOpenGL();
	
	pthread_mutex_init(&drawMutex, NULL);
}

void OpenGLHAL::close()
{
	pthread_mutex_destroy(&drawMutex);
	
	freeOpenGL();
	
	delete nextFrame;
}

void OpenGLHAL::enableGPU()
{
	loadPrograms();
	
	isConfigurationValid = false;
}

void OpenGLHAL::disableGPU()
{
	deletePrograms();
	
	isConfigurationValid = false;
}

OESize OpenGLHAL::getCanvasSize()
{
	return nextConfiguration.canvasSize;
}

bool OpenGLHAL::update(float width, float height, float offset, bool draw)
{
	bool newConfiguration = false;
	OEImage *frame = NULL;
	
	pthread_mutex_lock(&drawMutex);
	if (!isConfigurationValid)
	{
		configuration = nextConfiguration;
		isConfigurationValid = true;
		newConfiguration = true;
	}
	if (nextFrame)
	{
		frame = nextFrame;
		nextFrame = NULL;
	}
	pthread_mutex_unlock(&drawMutex);
	
	if (newConfiguration)
		updateConfiguration();
	
	if ((width != glViewportSize.width) ||
		(height != glViewportSize.height))
	{
		glViewportSize.width = width;
		glViewportSize.height = height;
		
		updateViewport();
		draw = true;
	}
	
	if (frame)
	{
		uploadFrame(frame);
		delete frame;
	}
	
	if (frame || newConfiguration)
	{
		processFrame();
		draw = true;
	}
	
	if (draw)
		drawFrame();
	
	return draw;
}

// OpenGL

bool OpenGLHAL::initOpenGL()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glEnable(GL_TEXTURE_2D);
	
	glGenTextures(OPENGLHAL_TEXTURE_END, glTextures);
	glTextureSize = OEMakeSize(0, 0);
	
	loadShadowMasks();
	
	loadPrograms();
	
	return true;
}

void OpenGLHAL::freeOpenGL()
{
	glDeleteTextures(OPENGLHAL_TEXTURE_END, glTextures);
	
	deletePrograms();
}

void OpenGLHAL::loadShadowMasks()
{
	loadShadowMask("Shadow Mask Triad.png",
				   glTextures[OPENGLHAL_TEXTURE_SHADOWMASK_TRIAD]);
	loadShadowMask("Shadow Mask Inline.png",
				   glTextures[OPENGLHAL_TEXTURE_SHADOWMASK_INLINE]);
	loadShadowMask("Shadow Mask Aperture.png",
				   glTextures[OPENGLHAL_TEXTURE_SHADOWMASK_APERTURE]);
}

void OpenGLHAL::loadShadowMask(string path, GLuint glTexture)
{
#ifdef GL_VERSION_2_0
	OEImage shadowMask;
	shadowMask.readFile(resourcePath + "/images/Generic/" + path);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, glTexture);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB8,
					  shadowMask.getSize().width, shadowMask.getSize().height,
					  ((shadowMask.getFormat() == OEIMAGE_FORMAT_RGB) ?
					   GL_RGB : GL_RGBA),
					  GL_UNSIGNED_BYTE, shadowMask.getPixels());
	glActiveTexture(GL_TEXTURE0);
#endif // GL_VERSION_2_0
}

void OpenGLHAL::loadPrograms()
{
	deletePrograms();
	
	loadProgram(OPENGLHAL_PROGRAM_NTSC, "\
		uniform sampler2D texture;\
		uniform vec2 texture_size;\
		uniform vec2 comp_phase;\
		uniform float comp_black;\
		uniform vec3 c0, c1, c2, c3, c4, c5, c6, c7, c8;\
		uniform mat3 decoder;\
		\
		float PI = 3.14159265358979323846264;\
		\
		vec3 pixel(vec2 q)\
		{\
			vec3 p = texture2D(texture, q).rgb - comp_black;\
			float phase = 2.0 * PI * dot(comp_phase * texture_size, q);\
			return p * vec3(1.0, sin(phase), cos(phase));\
		}\
		\
		vec3 pixels(vec2 q, float i)\
		{\
			return pixel(vec2(q.x + i, q.y)) + pixel(vec2(q.x - i, q.y));\
		}\
		\
		void main(void)\
		{\
			vec2 q = gl_TexCoord[0].xy;\
			vec3 p = pixel(q) * c0;\
			p += pixels(q, 1.0 / texture_size.x) * c1;\
			p += pixels(q, 2.0 / texture_size.x) * c2;\
			p += pixels(q, 3.0 / texture_size.x) * c3;\
			p += pixels(q, 4.0 / texture_size.x) * c4;\
			p += pixels(q, 5.0 / texture_size.x) * c5;\
			p += pixels(q, 6.0 / texture_size.x) * c6;\
			p += pixels(q, 7.0 / texture_size.x) * c7;\
			p += pixels(q, 8.0 / texture_size.x) * c8;\
			gl_FragColor = vec4(decoder * p, 1.0);\
		}");
	
	loadProgram(OPENGLHAL_PROGRAM_PAL, "\
		uniform sampler2D texture;\
		uniform vec2 texture_size;\
		uniform vec2 comp_phase;\
		uniform float comp_black;\
		uniform vec3 c0, c1, c2, c3, c4, c5, c6, c7, c8;\
		uniform mat3 decoder;\
		\
		float PI = 3.14159265358979323846264;\
		\
		vec3 pixel(vec2 q)\
		{\
			vec3 p = texture2D(texture, q).rgb - comp_black;\
			float phase = 2.0 * PI * dot(comp_phase * texture_size, q);\
			float pal = -sqrt(2.0) * sin(0.5 * PI * texture_size.y * q.y);\
			return p * vec3(1.0, sin(phase), cos(phase) * pal);\
		}\
		\
		vec3 pixels(vec2 q, float i)\
		{\
			return pixel(vec2(q.x + i, q.y)) + pixel(vec2(q.x - i, q.y));\
		}\
		\
		void main(void)\
		{\
			vec2 q = gl_TexCoord[0].xy;\
			vec3 p = pixel(q) * c0;\
			p += pixels(q, 1.0 / texture_size.x) * c1;\
			p += pixels(q, 2.0 / texture_size.x) * c2;\
			p += pixels(q, 3.0 / texture_size.x) * c3;\
			p += pixels(q, 4.0 / texture_size.x) * c4;\
			p += pixels(q, 5.0 / texture_size.x) * c5;\
			p += pixels(q, 6.0 / texture_size.x) * c6;\
			p += pixels(q, 7.0 / texture_size.x) * c7;\
			p += pixels(q, 8.0 / texture_size.x) * c8;\
			gl_FragColor = vec4(decoder * p, 1.0);\
		}");
				
	loadProgram(OPENGLHAL_PROGRAM_RGB, "\
		uniform sampler2D texture;\
		uniform vec2 texture_size;\
		uniform vec3 c0, c1, c2, c3, c4, c5, c6, c7, c8;\
		uniform mat3 decoder;\
		\
		vec3 pixel(vec2 q)\
		{\
			return texture2D(texture, q).rgb;\
		}\
		\
		vec3 pixels(vec2 q, float i)\
		{\
			return pixel(vec2(q.x + i, q.y)) + pixel(vec2(q.x - i, q.y));\
		}\
		\
		void main(void)\
		{\
			vec2 q = gl_TexCoord[0].xy;\
			vec3 p = pixel(q) * c0;\
			p += pixels(q, 1.0 / texture_size.x) * c1;\
			p += pixels(q, 2.0 / texture_size.x) * c2;\
			p += pixels(q, 3.0 / texture_size.x) * c3;\
			p += pixels(q, 4.0 / texture_size.x) * c4;\
			p += pixels(q, 5.0 / texture_size.x) * c5;\
			p += pixels(q, 6.0 / texture_size.x) * c6;\
			p += pixels(q, 7.0 / texture_size.x) * c7;\
			p += pixels(q, 8.0 / texture_size.x) * c8;\
			gl_FragColor = vec4(decoder * p, 1.0);\
		}");
	
	loadProgram(OPENGLHAL_PROGRAM_SCREEN, "\
		uniform sampler2D texture;\
		uniform vec2 texture_size;\
		uniform float barrel;\
		uniform vec2 barrel_center;\
		uniform float scanline_alpha;\
		uniform float center_lighting;\
		uniform sampler2D shadowmask;\
		uniform vec2 shadowmask_scale;\
		uniform vec2 shadowmask_translate;\
		uniform float shadowmask_alpha;\
		uniform float brightness;\
		\
		float PI = 3.14159265358979323846264;\
		\
		void main(void)\
		{\
			vec2 q = gl_TexCoord[0].xy;\
			\
			vec2 qc = q - barrel_center;\
			q += barrel * qc * dot(qc, qc);\
			\
			vec3 p = texture2D(texture, q).rgb;\
			float s = sin(PI * texture_size.y * q.y);\
			p *= mix(1.0, s * s, scanline_alpha);\
			vec2 c = qc * center_lighting;\
			p *= exp(-dot(c, c));\
			vec3 m = texture2D(shadowmask, q * shadowmask_scale + shadowmask_translate).rgb;\
			p *= mix(vec3(1.0, 1.0, 1.0), m, shadowmask_alpha);\
			p += brightness;\
			gl_FragColor = vec4(p, 1.0);\
		}");
}

void OpenGLHAL::deletePrograms()
{
	deleteProgram(OPENGLHAL_PROGRAM_NTSC);
	deleteProgram(OPENGLHAL_PROGRAM_PAL);
	deleteProgram(OPENGLHAL_PROGRAM_RGB);
	deleteProgram(OPENGLHAL_PROGRAM_SCREEN);
}

void OpenGLHAL::loadProgram(GLuint program, const char *source)
{
	GLuint glProgram = 0;
	
#ifdef GL_VERSION_2_0
	const GLchar **sourcePointer = (const GLchar **) &source;
	GLint sourceLength = strlen(source);
	
	GLuint glFragmentShader;
	glFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(glFragmentShader, 1, sourcePointer, &sourceLength);
	glCompileShader(glFragmentShader);
	
	GLint bufSize;
	
	bufSize = 0;
	glGetShaderiv(glFragmentShader, GL_INFO_LOG_LENGTH, &bufSize);
	
	if (bufSize > 0)
	{
		GLsizei infoLogLength = 0;
		
		vector<char> infoLog;
		infoLog.resize(bufSize);
		
		glGetShaderInfoLog(glFragmentShader, bufSize,
						   &infoLogLength, &infoLog.front());
		
		string errorString = "could not compile OpenGL fragment shader\n";
		errorString += &infoLog.front();
		
		logMessage(errorString);
	}
	
	glProgram = glCreateProgram();
	glAttachShader(glProgram, glFragmentShader);
	glLinkProgram(glProgram);
	
	glDeleteShader(glFragmentShader);
	
	bufSize = 0;
	glGetProgramiv(glProgram, GL_INFO_LOG_LENGTH, &bufSize);
	
	if (bufSize > 0)
	{
		GLsizei infoLogLength = 0;
		
		vector<char> infoLog;
		infoLog.resize(bufSize);
		
		glGetProgramInfoLog(glProgram, bufSize,
							&infoLogLength, &infoLog.front());
		
		string errorString = "could not link OpenGL program\n";
		errorString += &infoLog.front();
		
		logMessage(errorString);
	}
#endif // GL_VERSION_2_0
	
	glPrograms[program] = glProgram;
}

void OpenGLHAL::deleteProgram(GLuint program)
{
#ifdef GL_VERSION_2_0
	if (glPrograms[program])
		glDeleteProgram(glPrograms[program]);
	glPrograms[program] = 0;
#endif // GL_VERSION_2_0
}

void OpenGLHAL::updateConfiguration()
{
#ifdef GL_VERSION_2_0
	// Y'UV filters
	Vector w = Vector::chebyshevWindow(17, 50);
	w = w.normalize();
	
	Vector wy;
	wy = w * Vector::lanczosWindow(17, configuration.lumaBandwidth);
	wy = wy.normalize();
	
	Vector wu, wv;
	switch (configuration.decoder)
	{
		case CANVAS_DECODER_RGB:
		case CANVAS_DECODER_MONOCHROME:
			wu = wv = wy;
			break;
			
		case CANVAS_DECODER_NTSC_YIQ:
			wu = w * Vector::lanczosWindow(17, (configuration.
												compositeChromaBandwidth));
			wu = wu.normalize() * 2;
			wv = w * Vector::lanczosWindow(17, (configuration.
												compositeChromaBandwidth +
												NTSC_YIQ_I_SHIFT));
			wv = wv.normalize() * 2;
			break;
			
		default:
			wu = w * Vector::lanczosWindow(17, (configuration.
												compositeChromaBandwidth));
			wu = wv = wu.normalize() * 2;
			break;
	}
	
	// Decoder matrix
	Matrix3 m(1, 0, 0,
			  0, 1, 0,
			  0, 0, 1);
	// Contrast
	m *= configuration.contrast;
	// Decoder matrices from "Digital Video and HDTV Algorithms and Interfaces"
	switch (configuration.decoder)
	{
		case CANVAS_DECODER_RGB:
		case CANVAS_DECODER_MONOCHROME:
			// Y'PbPr decoder matrix
			m *= Matrix3(1, 1, 1,
						 0, -0.344, 1.772,
						 1.402, -0.714, 0);
			break;
			
		case CANVAS_DECODER_NTSC_YUV:
		case CANVAS_DECODER_NTSC_YIQ:
			// Y'IQ decoder matrix
			m *= Matrix3(1, 1, 1,
						 0.955986, -0.272013, -1.106740,
						 0.620825, -0.647204, 1.704230);
			// Invert IQ
			m *= Matrix3(1, 0, 0,
						 0, 0, 1,
						 0, 1, 0);
			break;
			
		case CANVAS_DECODER_NTSC_CXA2025AS:
			// CXA2025AS decoder matrix
			m *= Matrix3(1, 1, 1,
						 1.630, -0.378, -1.089,
						 0.317, -0.466, 1.677);
			// Invert IQ
			m *= Matrix3(1, 0, 0,
						 0, 0, 1,
						 0, 1, 0);
			break;
			
		case CANVAS_DECODER_PAL:
			// Y'UV decoder matrix
			m *= Matrix3(1, 1, 1,
						 0, -0.394642, 2.032062,
						 1.139883, -0.580622, 0);
			break;
	}
	// Hue
	float hue = 2 * M_PI * configuration.hue;
	m *= Matrix3(1, 0, 0,
				 0, cosf(hue), -sinf(hue),
				 0, sinf(hue), cosf(hue));
	// Saturation
	m *= Matrix3(1, 0, 0,
				 0, configuration.saturation, 0,
				 0, 0, configuration.saturation);
	// Encoder matrices
	switch (configuration.decoder)
	{
		case CANVAS_DECODER_RGB:
			// Y'PbPr encoding matrix
			m *= Matrix3(0.299, -0.169, 0.5,
						 0.587, -0.331, -0.419,
						 0.114, 0.5, -0.081);
			break;
			
		case CANVAS_DECODER_MONOCHROME:
			// Set Y'PbPr maximum hue
			m *= Matrix3(1, 0, -0.5,
						 0, 0, 0,
						 0, 0, 0);
			break;
	}
	// Dynamic range gain
	switch (configuration.decoder)
	{
		case CANVAS_DECODER_NTSC_YIQ:
		case CANVAS_DECODER_NTSC_CXA2025AS:
		case CANVAS_DECODER_NTSC_YUV:
		case CANVAS_DECODER_PAL:
			float levelRange = (configuration.compositeWhiteLevel -
						configuration.compositeBlackLevel);
			if (fabs(levelRange) < 0.01)
				levelRange = 0.01;
			m *= 1 / levelRange;
			break;
	}
	
	switch (configuration.decoder)
	{
		case CANVAS_DECODER_NTSC_YIQ:
		case CANVAS_DECODER_NTSC_YUV:
		case CANVAS_DECODER_NTSC_CXA2025AS:
			glProcessProgram = glPrograms[OPENGLHAL_PROGRAM_NTSC];
			break;
		case CANVAS_DECODER_PAL:
			glProcessProgram = glPrograms[OPENGLHAL_PROGRAM_PAL];
			break;
		default:
			glProcessProgram = glPrograms[OPENGLHAL_PROGRAM_RGB];
			break;
	}
	
	if (glProcessProgram)
	{
		glUseProgram(glProcessProgram);
		if (glProcessProgram != glPrograms[OPENGLHAL_PROGRAM_RGB])
		{
			glUniform2f(glGetUniformLocation(glProcessProgram, "comp_phase"),
						configuration.compositeCarrierFrequency,
						configuration.compositeLinePhase);
			glUniform1f(glGetUniformLocation(glProcessProgram, "comp_black"),
						configuration.compositeBlackLevel);
		}
		glUniform3f(glGetUniformLocation(glProcessProgram, "c0"),
					wy.getValue(8), wu.getValue(8), wv.getValue(8));
		glUniform3f(glGetUniformLocation(glProcessProgram, "c1"),
					wy.getValue(7), wu.getValue(7), wv.getValue(7));
		glUniform3f(glGetUniformLocation(glProcessProgram, "c2"),
					wy.getValue(6), wu.getValue(6), wv.getValue(6));
		glUniform3f(glGetUniformLocation(glProcessProgram, "c3"),
					wy.getValue(5), wu.getValue(5), wv.getValue(5));
		glUniform3f(glGetUniformLocation(glProcessProgram, "c4"),
					wy.getValue(4), wu.getValue(4), wv.getValue(4));
		glUniform3f(glGetUniformLocation(glProcessProgram, "c5"),
					wy.getValue(3), wu.getValue(3), wv.getValue(3));
		glUniform3f(glGetUniformLocation(glProcessProgram, "c6"),
					wy.getValue(2), wu.getValue(2), wv.getValue(2));
		glUniform3f(glGetUniformLocation(glProcessProgram, "c7"),
					wy.getValue(1), wu.getValue(1), wv.getValue(1));
		glUniform3f(glGetUniformLocation(glProcessProgram, "c8"),
					wy.getValue(0), wu.getValue(0), wv.getValue(0));
		glUniformMatrix3fv(glGetUniformLocation(glProcessProgram, "decoder"),
						   9, false, m.getValues());
	}
	
	if (glPrograms[OPENGLHAL_PROGRAM_SCREEN])
	{
		GLuint glProgram = glPrograms[OPENGLHAL_PROGRAM_SCREEN];
		glActiveTexture(GL_TEXTURE1);
		GLuint glTexture;
		switch (configuration.shadowMask)
		{
			case CANVAS_SHADOWMASK_TRIAD:
				glTexture = glTextures[OPENGLHAL_TEXTURE_SHADOWMASK_TRIAD];
				break;
			case CANVAS_SHADOWMASK_INLINE:
				glTexture = glTextures[OPENGLHAL_TEXTURE_SHADOWMASK_INLINE];
				break;
			case CANVAS_SHADOWMASK_APERTURE:
				glTexture = glTextures[OPENGLHAL_TEXTURE_SHADOWMASK_APERTURE];
				break;
			default:
				glTexture = 0;
				break;
		}
		glBindTexture(GL_TEXTURE_2D, glTexture);
		glActiveTexture(GL_TEXTURE0);
		glUseProgram(glProgram);
		glUniform1i(glGetUniformLocation(glProgram, "shadowmask"), 1);
		glUniform1f(glGetUniformLocation(glProgram, "barrel"),
					configuration.barrel);
		float centerLighting = configuration.centerLighting;
		if (fabs(centerLighting) < 0.001)
			centerLighting = 0.001;
		glUniform1f(glGetUniformLocation(glProgram, "center_lighting"),
					1.0 / centerLighting - 1.0);
		glUniform1f(glGetUniformLocation(glProgram, "brightness"),
					configuration.brightness);
	}
	
	glUseProgram(0);
#endif // GL_VERSION_2_0
}

void OpenGLHAL::updateViewport()
{
	glViewport(0, 0, glViewportSize.width, glViewportSize.height);
}

void OpenGLHAL::setTextureSize(GLuint glProgram)
{
#ifdef GL_VERSION_2_0
	if (!glProgram)
		return;
	
	glUseProgram(glProgram);
	glUniform1i(glGetUniformLocation(glProgram, "texture"), 0);
	glUniform2f(glGetUniformLocation(glProgram, "texture_size"),
				glTextureSize.width, glTextureSize.height);
#endif // GL_VERSION_2_0
}

void OpenGLHAL::uploadFrame(OEImage *frame)
{
	GLint format;
	
	if (frame->getFormat() == OEIMAGE_FORMAT_LUMINANCE)
		format = GL_LUMINANCE;
	else if (frame->getFormat() == OEIMAGE_FORMAT_RGB)
		format = GL_RGB;
	else if (frame->getFormat() == OEIMAGE_FORMAT_RGBA)
		format = GL_RGBA;
	
	if ((glFrameSize.width != frame->getSize().width) ||
		(glFrameSize.height != frame->getSize().height))
	{
		glFrameSize = frame->getSize();
		glTextureSize = OEMakeSize(getNextPowerOf2(frame->getSize().width),
								   getNextPowerOf2(frame->getSize().height));
		
		vector<char> dummy;
		dummy.resize(glTextureSize.width * glTextureSize.height);
		
		glBindTexture(GL_TEXTURE_2D, glTextures[OPENGLHAL_TEXTURE_FRAME_RAW]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
					 glTextureSize.width, glTextureSize.height, 0,
					 GL_LUMINANCE, GL_UNSIGNED_BYTE, &dummy.front());
		glBindTexture(GL_TEXTURE_2D, glTextures[OPENGLHAL_TEXTURE_FRAME_PROCESSED]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
					 glTextureSize.width, glTextureSize.height, 0,
					 GL_LUMINANCE, GL_UNSIGNED_BYTE, &dummy.front());
	}
	
	glBindTexture(GL_TEXTURE_2D, glTextures[OPENGLHAL_TEXTURE_FRAME_RAW]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
					frame->getSize().width, frame->getSize().height,
					format, GL_UNSIGNED_BYTE,
					frame->getPixels());
	
#ifdef GL_VERSION_2_0
	setTextureSize(glPrograms[OPENGLHAL_PROGRAM_RGB]);
	setTextureSize(glPrograms[OPENGLHAL_PROGRAM_NTSC]);
	setTextureSize(glPrograms[OPENGLHAL_PROGRAM_PAL]);
	setTextureSize(glPrograms[OPENGLHAL_PROGRAM_SCREEN]);
#endif // GL_VERSION_2_0
}

void OpenGLHAL::processFrame()
{
#ifdef GL_VERSION_2_0
	if (!glProcessProgram)
		return;
	
	glUseProgram(glProcessProgram);
	
	glBindTexture(GL_TEXTURE_2D, glTextures[OPENGLHAL_TEXTURE_FRAME_RAW]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	
	for (int y = 0; y < glFrameSize.height; y += glViewportSize.height)
		for (int x = 0; x < glFrameSize.width; x += glViewportSize.width)
		{
			// Bind raw frame
			glBindTexture(GL_TEXTURE_2D, glTextures[OPENGLHAL_TEXTURE_FRAME_RAW]);
			
			// Calculate frames
			OERect renderFrame = OEMakeRect(-1, -1, 2, 2);
			if ((x + glViewportSize.width) > glFrameSize.width)
				renderFrame.size.width = 2 * (glFrameSize.width - x) / glViewportSize.width;
			if ((y + glViewportSize.height) > glFrameSize.height)
				renderFrame.size.height = 2 * (glFrameSize.height - y) / glViewportSize.height;
			
			OERect textureFrame = OEMakeRect(x / glTextureSize.width,
											 y / glTextureSize.height,
											 glViewportSize.width / glTextureSize.width,
											 glViewportSize.height / glTextureSize.height);
			if ((x + glViewportSize.width) > glFrameSize.width)
				textureFrame.size.width = (glFrameSize.width - x) / glTextureSize.width;
			if ((y + glViewportSize.height) > glFrameSize.height)
				textureFrame.size.height = (glFrameSize.height - y) / glTextureSize.height;
			
			// Render
			glLoadIdentity();
			
			glBegin(GL_QUADS);
			glTexCoord2f(OEMinX(textureFrame), OEMinY(textureFrame));
			glVertex2f(OEMinX(renderFrame), OEMinY(renderFrame));
			glTexCoord2f(OEMaxX(textureFrame), OEMinY(textureFrame));
			glVertex2f(OEMaxX(renderFrame), OEMinY(renderFrame));
			glTexCoord2f(OEMaxX(textureFrame), OEMaxY(textureFrame));
			glVertex2f(OEMaxX(renderFrame), OEMaxY(renderFrame));
			glTexCoord2f(OEMinX(textureFrame), OEMaxY(textureFrame));
			glVertex2f(OEMinX(renderFrame), OEMaxY(renderFrame));
			glEnd();
			
			// Copy framebuffer
			glBindTexture(GL_TEXTURE_2D, glTextures[OPENGLHAL_TEXTURE_FRAME_PROCESSED]);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0,
								x,
								y,
								0, 0,
								0.5 * renderFrame.size.width * glViewportSize.width,
								0.5 * renderFrame.size.height * glViewportSize.height);
		}
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	glUseProgram(0);
#endif // GL_VERSION_2_0
}

void OpenGLHAL::drawFrame()
{
	// Clear
	float clearColor = glProcessProgram ? configuration.brightness : 0;
	glClearColor(clearColor, clearColor, clearColor, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	if ((glTextureSize.width == 0) || (glTextureSize.height == 0))
		return;
	
	// Calculate frames
	OERect renderFrame = OEMakeRect(2 * configuration.contentRect.origin.x - 1,
									2 * configuration.contentRect.origin.y - 1,
									2 * configuration.contentRect.size.width,
									2 * configuration.contentRect.size.height);
	
	OERect textureFrame = OEMakeRect(0, 0,
									 glFrameSize.width / glTextureSize.width, 
									 glFrameSize.height / glTextureSize.height);
	
	float viewAspectRatio = glViewportSize.width / glViewportSize.height;
	float canvasAspectRatio = (configuration.canvasSize.width /
							   configuration.canvasSize.height);
	
	// Apply view mode
	float ratio = viewAspectRatio / canvasAspectRatio;
	
	if (configuration.viewMode == CANVAS_VIEWMODE_FIT_WIDTH)
		renderFrame.size.height *= ratio;
	else
	{
		if (ratio > 1)
		{
			renderFrame.origin.x /= ratio;
			renderFrame.size.width /= ratio;
		}
		else
		{
			renderFrame.origin.y *= ratio;
			renderFrame.size.height *= ratio;
		}
	}
	
	// Set common shader variables
#ifdef GL_VERSION_2_0
	if (glPrograms[OPENGLHAL_PROGRAM_SCREEN])
	{
		GLuint glProgram = glPrograms[OPENGLHAL_PROGRAM_SCREEN];
		glUseProgram(glProgram);
		OEPoint barrelCenter;
		barrelCenter.x = ((0.5 - configuration.contentRect.origin.x) /
						  configuration.contentRect.size.width *
						  glFrameSize.width / glTextureSize.width);
		barrelCenter.y = ((0.5 - configuration.contentRect.origin.y) /
						  configuration.contentRect.size.height *
						  glFrameSize.height / glTextureSize.height);
		glUniform2f(glGetUniformLocation(glProgram, "barrel_center"),
					barrelCenter.x, barrelCenter.y);
		
		float scanlineHeight = (glViewportSize.height / glFrameSize.height *
								configuration.contentRect.size.height);
		float alpha = configuration.scanlineAlpha;
		float scanlineAlpha = ((scanlineHeight > 2.5) ? alpha :
							   (scanlineHeight < 2) ? 0 :
							   (scanlineHeight - 2) / (2.5 - 2) * alpha);
		glUniform1f(glGetUniformLocation(glProgram, "scanline_alpha"),
					scanlineAlpha);
		
		float shadowMaskAlpha = configuration.shadowMaskAlpha;
		glUniform1f(glGetUniformLocation(glProgram, "shadowmask_alpha"),
					shadowMaskAlpha);
		
		float elemNumX = (configuration.canvasSize.width / 75.0 * 25.4 /
						  (configuration.shadowMaskDotPitch + 0.001) / 2.0);
		float elemNumY = (configuration.canvasSize.height / 75.0 * 25.4 /
						  (configuration.shadowMaskDotPitch + 0.001) * (240.0 / 274.0));
		glUniform2f(glGetUniformLocation(glProgram, "shadowmask_scale"),
					glTextureSize.width / glFrameSize.width *
					configuration.contentRect.size.width * elemNumX,
					glTextureSize.height / glFrameSize.height *
					configuration.contentRect.size.height * elemNumY);
		glUniform2f(glGetUniformLocation(glProgram, "shadowmask_translate"),
					configuration.contentRect.origin.x * elemNumX,
					configuration.contentRect.origin.y * elemNumY);
	}
#endif // GL_VERSION_2_0
	
	// Use nearest filter when canvas and screen pixel size match
	glBindTexture(GL_TEXTURE_2D, (glProcessProgram ? 
								  glTextures[OPENGLHAL_TEXTURE_FRAME_PROCESSED] : 
								  glTextures[OPENGLHAL_TEXTURE_FRAME_RAW]));
	
	int renderFrameWidth = glViewportSize.width * renderFrame.size.width / 2;
	GLint param = GL_LINEAR;
	if ((renderFrameWidth == (int) glFrameSize.width) &&
		(renderFrame.origin.x == -1) && (renderFrame.origin.y == -1))
		param = GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, param);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	
	// Render
	glLoadIdentity();
	glRotatef(180, 1, 0, 0);
	
	glBegin(GL_QUADS);
	glTexCoord2f(OEMinX(textureFrame), OEMinY(textureFrame));
	glVertex2f(OEMinX(renderFrame), OEMinY(renderFrame));
	glTexCoord2f(OEMaxX(textureFrame), OEMinY(textureFrame));
	glVertex2f(OEMaxX(renderFrame), OEMinY(renderFrame));
	glTexCoord2f(OEMaxX(textureFrame), OEMaxY(textureFrame));
	glVertex2f(OEMaxX(renderFrame), OEMaxY(renderFrame));
	glTexCoord2f(OEMinX(textureFrame), OEMaxY(textureFrame));
	glVertex2f(OEMinX(renderFrame), OEMaxY(renderFrame));
	glEnd();
	
	glBindTexture(GL_TEXTURE_2D, 0);
#ifdef GL_VERSION_2_0
	glUseProgram(0);
#endif // GL_VERSION_2_0
}

// HID

void OpenGLHAL::updateCapture(OpenGLHALCapture capture)
{
	//	log("updateCapture");
	
	if (this->capture == capture)
		return;
	this->capture = capture;
	
	if (setCapture)
		setCapture(userData, capture);
}

void OpenGLHAL::becomeKeyWindow()
{
}

void OpenGLHAL::resignKeyWindow()
{
	resetKeysAndButtons();
	
	ctrlAltWasPressed = false;
	
	updateCapture(OPENGLHAL_CAPTURE_NONE);
}

void OpenGLHAL::setKey(int usageId, bool value)
{
	if (keyDown[usageId] == value)
		return;
	
//	log("key " + getHexString(usageId) + ": " + getString(value));
//	log("keyDownCount " + getString(keyDownCount));
	
	keyDown[usageId] = value;
	keyDownCount += value ? 1 : -1;
	if ((keyDown[CANVAS_K_LEFTCONTROL] ||
		 keyDown[CANVAS_K_RIGHTCONTROL]) &&
		(keyDown[CANVAS_K_LEFTALT] ||
		 keyDown[CANVAS_K_RIGHTALT]))
		ctrlAltWasPressed = true;
	
	postHIDNotification(CANVAS_KEYBOARD_DID_CHANGE, usageId, value);
	
	if ((capture == OPENGLHAL_CAPTURE_KEYBOARD_AND_DISCONNECT_MOUSE_CURSOR) &&
		!keyDownCount && ctrlAltWasPressed)
	{
		ctrlAltWasPressed = false;
		
		updateCapture(OPENGLHAL_CAPTURE_NONE);
	}
}

void OpenGLHAL::postHIDNotification(int notification, int usageId, float value)
{
	CanvasHIDNotification data = {usageId, value};
	notify(this, notification, &data);
}

void OpenGLHAL::sendUnicodeKeyEvent(int unicode)
{
//	log("unicode " + getHexString(unicode));
	
	postHIDNotification(CANVAS_UNICODEKEYBOARD_DID_CHANGE, unicode, 0);
}

void OpenGLHAL::enterMouse()
{
	mouseEntered = true;
	
	if (configuration.captureMode == CANVAS_CAPTUREMODE_CAPTURE_ON_MOUSE_ENTER)
		updateCapture(OPENGLHAL_CAPTURE_KEYBOARD_AND_HIDE_MOUSE_CURSOR);
	
	postHIDNotification(CANVAS_POINTER_DID_CHANGE,
						CANVAS_P_PROXIMITY,
						1);
}

void OpenGLHAL::exitMouse()
{
	mouseEntered = false;
	
	if (configuration.captureMode == CANVAS_CAPTUREMODE_CAPTURE_ON_MOUSE_ENTER)
		updateCapture(OPENGLHAL_CAPTURE_NONE);
	
	postHIDNotification(CANVAS_POINTER_DID_CHANGE,
						CANVAS_P_PROXIMITY,
						0);
}

void OpenGLHAL::setMousePosition(float x, float y)
{
	if (capture != OPENGLHAL_CAPTURE_KEYBOARD_AND_DISCONNECT_MOUSE_CURSOR)
	{
		postHIDNotification(CANVAS_POINTER_DID_CHANGE,
							CANVAS_P_X,
							x);
		postHIDNotification(CANVAS_POINTER_DID_CHANGE,
							CANVAS_P_Y,
							y);
	}
}

void OpenGLHAL::moveMouse(float rx, float ry)
{
	if (capture == OPENGLHAL_CAPTURE_KEYBOARD_AND_DISCONNECT_MOUSE_CURSOR)
	{
		postHIDNotification(CANVAS_MOUSE_DID_CHANGE,
							CANVAS_M_RELX,
							rx);
		postHIDNotification(CANVAS_MOUSE_DID_CHANGE,
							CANVAS_M_RELY,
							ry);
	}
}

void OpenGLHAL::setMouseButton(int index, bool value)
{
	if (index >= CANVAS_MOUSE_BUTTON_NUM)
		return;
	if (mouseButtonDown[index] == value)
		return;
	mouseButtonDown[index] = value;
	
	if (capture == OPENGLHAL_CAPTURE_KEYBOARD_AND_DISCONNECT_MOUSE_CURSOR)
		postHIDNotification(CANVAS_MOUSE_DID_CHANGE,
							CANVAS_M_BUTTON1 + index,
							value);
	else if ((configuration.captureMode == CANVAS_CAPTUREMODE_CAPTURE_ON_MOUSE_CLICK) &&
			 (capture == OPENGLHAL_CAPTURE_NONE) &&
			 (index == 0))
		updateCapture(OPENGLHAL_CAPTURE_KEYBOARD_AND_DISCONNECT_MOUSE_CURSOR);
	else
		postHIDNotification(CANVAS_POINTER_DID_CHANGE,
							CANVAS_P_BUTTON1 + index,
							value);
}

void OpenGLHAL::sendMouseWheelEvent(int index, float value)
{
	if (capture == OPENGLHAL_CAPTURE_KEYBOARD_AND_DISCONNECT_MOUSE_CURSOR)
		postHIDNotification(CANVAS_MOUSE_DID_CHANGE,
							CANVAS_M_WHEELX + index,
							value);
	else
		postHIDNotification(CANVAS_POINTER_DID_CHANGE,
							CANVAS_P_WHEELX + index,
							value);
}

void OpenGLHAL::setJoystickButton(int deviceIndex, int index, bool value)
{
	if (deviceIndex >= CANVAS_JOYSTICK_NUM)
		return;
	if (index >= CANVAS_JOYSTICK_BUTTON_NUM)
		return;
	if (joystickButtonDown[deviceIndex][index] == value)
		return;
	joystickButtonDown[deviceIndex][index] = value;
	
	postHIDNotification(CANVAS_JOYSTICK1_DID_CHANGE + deviceIndex,
						CANVAS_J_BUTTON1 + index,
						value);
}

void OpenGLHAL::setJoystickPosition(int deviceIndex, int index, float value)
{
	if (deviceIndex >= CANVAS_JOYSTICK_NUM)
		return;
	if (index >= CANVAS_JOYSTICK_AXIS_NUM)
		return;
	
	postHIDNotification(CANVAS_JOYSTICK1_DID_CHANGE + deviceIndex,
						CANVAS_J_AXIS1 + index,
						value);
}

void OpenGLHAL::sendJoystickHatEvent(int deviceIndex, int index, float value)
{
	if (deviceIndex >= CANVAS_JOYSTICK_NUM)
		return;
	if (index >= CANVAS_JOYSTICK_HAT_NUM)
		return;
	
	postHIDNotification(CANVAS_JOYSTICK1_DID_CHANGE + deviceIndex,
						CANVAS_J_AXIS1 + index,
						value);
}

void OpenGLHAL::moveJoystickBall(int deviceIndex, int index, float value)
{
	if (deviceIndex >= CANVAS_JOYSTICK_NUM)
		return;
	if (index >= CANVAS_JOYSTICK_RAXIS_NUM)
		return;
	
	postHIDNotification(CANVAS_JOYSTICK1_DID_CHANGE + deviceIndex,
						CANVAS_J_AXIS1 + index,
						value);
}

void OpenGLHAL::resetKeysAndButtons()
{
	for (int i = 0; i < CANVAS_KEYBOARD_KEY_NUM; i++)
		setKey(i, false);
	
	for (int i = 0; i < CANVAS_MOUSE_BUTTON_NUM; i++)
		setMouseButton(i, false);
	
	for (int i = 0; i < CANVAS_JOYSTICK_NUM; i++)
		for (int j = 0; j < CANVAS_JOYSTICK_BUTTON_NUM; j++)
			setJoystickButton(i, j, false);
}

bool OpenGLHAL::copy(string& value)
{
	return OEComponent::postMessage(this, CANVAS_COPY, &value);
}

bool OpenGLHAL::paste(string value)
{
	return OEComponent::postMessage(this, CANVAS_PASTE, &value);
}

bool OpenGLHAL::setConfiguration(CanvasConfiguration *configuration)
{
	if (!configuration)
		return false;
	
	if (nextConfiguration.captureMode != configuration->captureMode)
	{
		switch (configuration->captureMode)
		{
			case CANVAS_CAPTUREMODE_NO_CAPTURE:
				updateCapture(OPENGLHAL_CAPTURE_NONE);
				break;
				
			case CANVAS_CAPTUREMODE_CAPTURE_ON_MOUSE_CLICK:
				updateCapture(OPENGLHAL_CAPTURE_NONE);
				break;
				
			case CANVAS_CAPTUREMODE_CAPTURE_ON_MOUSE_ENTER:
				updateCapture(mouseEntered ? 
							  OPENGLHAL_CAPTURE_KEYBOARD_AND_HIDE_MOUSE_CURSOR : 
							  OPENGLHAL_CAPTURE_NONE);
				break;
		}
	}
	
	pthread_mutex_lock(&drawMutex);
	nextConfiguration = *configuration;
	isConfigurationValid = false;
	pthread_mutex_unlock(&drawMutex);
	
	return true;
}

bool OpenGLHAL::postFrame(OEImage *frame)
{
	if (!frame)
		return false;
	
	pthread_mutex_lock(&drawMutex);
	if (nextFrame)
		delete nextFrame;
	
	nextFrame = new OEImage(*frame);
	pthread_mutex_unlock(&drawMutex);
	
	return true;
}

bool OpenGLHAL::postMessage(OEComponent *sender, int message, void *data)
{
	switch (message)
	{
		case CANVAS_CONFIGURE:
			return setConfiguration((CanvasConfiguration *)data);
			
		case CANVAS_POST_FRAME:
			return postFrame((OEImage *)data);
	}
	
	return OEComponent::postMessage(sender, message, data);
}