#ifdef SOUND_OPENAL

#include "main.h"
#include "util.h"
#include "sound.h"
#include "file.h"
#include "SDL.h"
#include <al.h>
#include <alc.h>
//#include <efx.h>
//#include <efx-creative.h>
#include <math.h>

//
// globals
//

// tells forsaken to use 3d functions
BOOL Sound3D = FALSE;

static ALCdevice* Device = NULL;
static ALCcontext* Context = NULL;

//
// buffer description
//

typedef struct sound_t {
	ALuint source;
	ALuint buffer;
};

//
// Generic Functions
//

static void print_info ( void ) 
{
	ALint version_major, version_minor;
	ALenum error;
	ALCdevice *device;

	DebugPrintf("openal: info start\n");

	// Check for EAX 2.0 support
	DebugPrintf("EAX2.0 support = %s\n",
		alIsExtensionPresent("EAX2.0")?"true":"false");

	if(alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_TRUE) 
	{
		DebugPrintf("default playback: %s\n",alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
		DebugPrintf("default capture: %s\n",alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER));
		{	
			// print all playback devices
			const char * s = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
			do { 
				DebugPrintf("playback: %s\n", s); 
				while(*s++ != '\0'); 
			} while(*s != '\0');
			// print all capture devices
			s = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
			do { 
				DebugPrintf("capture: %s\n", s); 
				while(*s++ != '\0'); 
			} while(*s != '\0');
		}
	} 
	else 
		DebugPrintf("No device enumeration available\n");

	DebugPrintf("Default device: %s\n", alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
	DebugPrintf("Default capture device: %s\n", (alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER)));

	device = alcGetContextsDevice(alcGetCurrentContext());

	if ((error = alGetError()) != AL_NO_ERROR)
		DebugPrintf("blah :%s\n", alGetString(error));

	alcGetIntegerv(device, ALC_MAJOR_VERSION, 1, &version_major);
	alcGetIntegerv(device, ALC_MINOR_VERSION, 1, &version_minor);
	
	if ((error = alGetError()) != AL_NO_ERROR)
		DebugPrintf("blah :%s\n", alGetString(error));

	DebugPrintf("ALC version: %d.%d\n", (int)version_major, (int)version_minor);
	DebugPrintf("ALC extensions: %s\n", alcGetString(device, ALC_EXTENSIONS));

	if ((error = alGetError()) != AL_NO_ERROR)
		DebugPrintf("blah :%s\n", alGetString(error));

	DebugPrintf("OpenAL vendor string: %s\n", alGetString(AL_VENDOR));
	DebugPrintf("OpenAL renderer string: %s\n", alGetString(AL_RENDERER));
	DebugPrintf("OpenAL version string: %s\n", alGetString(AL_VERSION));
	DebugPrintf("OpenAL extensions: %s\n", alGetString(AL_EXTENSIONS));

	if ((error = alGetError()) != AL_NO_ERROR)
		DebugPrintf("blah :%s\n", alGetString(error));

	DebugPrintf("openal: info end\n");
}

BOOL sound_init( void )
{
	Device = alcOpenDevice(NULL); // preferred device
	if(!Device)
		return FALSE;

	Context = alcCreateContext(Device,NULL);
	if(!Context)
		return FALSE;

	alcMakeContextCurrent(Context);

	// TODO AL_MIN_GAIN
	sound_minimum_volume = 0;

	{
		ALfloat f;
		ALfloat pos[3];
		alGetListenerf(AL_GAIN,&f);
		DebugPrintf("listener gain: %f\n",f);
		alGetListenerfv(AL_POSITION,pos);
		DebugPrintf("listener position: %f %f %f\n",pos[0],pos[1],pos[2]);
	}

	print_info();

	return TRUE;
}

void sound_destroy( void )
{
	ALCcontext * Context = alcGetCurrentContext();
	ALCdevice * Device = alcGetContextsDevice(Context);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(Context);
	alcCloseDevice(Device);
}

//
// Listener
//

BOOL sound_listener_position( float x, float y, float z )
{
	alListener3f(AL_POSITION, x, y, z);
	return TRUE;
}

BOOL sound_listener_velocity( float x, float y, float z )
{
	alListener3f(AL_VELOCITY, x, y, z);
	return TRUE;
}

// TODO - does the at-vector equate to forward vector?
BOOL sound_listener_orientation( 
	float fx, float fy, float fz, // forward vector
	float ux, float uy, float uz  // up vector
)
{
	float vector[6] = {fx,fy,fz,ux,uy,uz};
	alListenerfv(AL_ORIENTATION, &vector[0]);
	return TRUE;
}

//
// Buffers
//

void sound_play( sound_t * source )
{
	alSourcePlay( source->source );
}

void sound_play_looping( sound_t * source )
{
	alSourcei( source->source, AL_LOOPING, AL_TRUE );
	sound_play( source );
}

void sound_stop( sound_t * source )
{
	alSourceStop( source->source );
}

long sound_size( sound_t * source )
{
	ALint size;
	alGetBufferi( source->buffer, AL_SIZE, &size );
	return (long) size;
}

long sound_rate( sound_t * source )
{
	ALint freq;
	alGetBufferi( source->buffer, AL_FREQUENCY, &freq );
	return (long) freq;
}

void sound_release( sound_t * source )
{
	if(!source)
		return;
	// deleting source implicitly detaches buffer
	alDeleteSources( 1, &source->source );
	// now good to delete the buffer
	alDeleteBuffers( 1, &source->buffer );
	free(source);
	source = NULL;
}

void sound_set_freq( sound_t * source, float freq )
{
	alBufferi(source->buffer,AL_FREQUENCY, (ALint) freq);
}

void sound_volume( sound_t * source, long volume )
{
	// volume must be scaled from 0 <-> +1 and converted from dsound format
	alSourcef(source->source, AL_GAIN, (ALfloat) pow(10.0, volume/2000.0));
}

void sound_pan( sound_t * source, long _pan )
{
	// where pan is -1 (left) to +1 (right)
	// must be scaled from -1 <-> +1
	// probably need to scale down by 10,000, since dsound goes from -10000 to +10000
	// so:
	float pan = (float) _pan / 10000.0f;
	alSource3f(source->source, AL_POSITION, pan, (float) sqrt(1 - pan*pan), 0.0f);
}

BOOL sound_is_playing( sound_t * source )
{
	ALint state;
	alGetSourcei( source->source, AL_SOURCE_STATE, &state	);
	return (state == AL_PLAYING);
}

void sound_get_seek( sound_t * source, long * bytes )
{
	ALint i;
	alGetSourcei(source->source, AL_BYTE_OFFSET, &i);
	*bytes = (long) i;
}

void sound_set_seek( sound_t * source, long bytes )
{
	alSourcei(source->source, AL_BYTE_OFFSET, bytes);
}

// TODO - we'll want to set velocity some day for moving players ?
void sound_position( sound_t * source, float x, float y, float z, float min, float max )
{
	alSource3f(source->source, AL_POSITION, x, y, z);
	alSourcef(source->source, AL_MAX_DISTANCE, max);
	alSourcef(source->source, AL_REFERENCE_DISTANCE, min); // is this right?
}

sound_t * sound_load(char *path)
{
	ALenum error;
	ALenum format;
	SDL_AudioSpec wav_spec;
	Uint8 *wav_buffer;
	sound_t * source = malloc(sizeof(sound_t));
	char * file_path = convert_path(path);
 
	// clear error code
	alGetError();

	// Generate Buffers
	alGenBuffers(1, &source->buffer);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DebugPrintf("alGenBuffers: %s\n", alGetString(error));
		return NULL;
	}

	if( SDL_LoadWAV(file_path, &wav_spec, &wav_buffer, &wav_spec.size) == NULL )
	{
		DebugPrintf("Could not open test.wav: %s\n", SDL_GetError());
		return NULL;
	}

	if(wav_spec.format == AUDIO_U8 || wav_spec.format == AUDIO_S8) // 8 bit
	{
		// openal only supports usigned 8bit
		if(wav_spec.format == AUDIO_S8)
		{
			int i;
			for(i = 0; i < (int) wav_spec.size; i++)
				wav_buffer[i] ^= 0x80; // converts S8 to U8
		}
		if(wav_spec.channels == 1)
			format = AL_FORMAT_MONO8;
		else
			format = AL_FORMAT_STEREO8;
	}
	else // 16 bit
	{
		// openal only supports signed 16bit
		if(wav_spec.format == AUDIO_U16)
		{
			int i;
			for(i = 0;i < (int)wav_spec.size/2;i++)
				((Uint16*)wav_buffer)[i] ^= 0x8000; // converts U16 to S16
		}
		if(wav_spec.channels == 1)
			format = AL_FORMAT_MONO16;
		else
			format = AL_FORMAT_STEREO16;
	}

	// Copy data into AL Buffer 0
	alBufferData(source->buffer,format,wav_buffer,wav_spec.size,wav_spec.freq);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DebugPrintf("alBufferData: %s\n", alGetString(error));
		alDeleteBuffers(1, &source->buffer);
		return NULL;
	}

	// data has been coppied
	SDL_FreeWAV(wav_buffer);

	// Generate Sources
	alGenSources(1,&source->source);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DebugPrintf("alGenSources: %s\n", alGetString(error));
		return NULL;
	}

	// Attach buffer 0 to source
	alSourcei(source->source, AL_BUFFER, source->buffer);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DebugPrintf("alSourcei AL_BUFFER: %s\n", alGetString(error));
		return NULL;
	}

	if(!Sound3D)
	{
		alSourcei(source->source,AL_SOURCE_RELATIVE,AL_TRUE);
	}

	{
		ALint i;
		ALfloat f, pos[3];
		sound_listener_position(0,0,0);
		alGetSourcef(source->source,AL_GAIN,&f);
		DebugPrintf("buffer gain: %f\n",f);
		alGetSource3f(source->source,AL_POSITION,&pos[0],&pos[1],&pos[2]);
		DebugPrintf("buffer position: %f %f %f\n",pos[0],pos[1],pos[2]);
		alGetSourcei(source->source,AL_SOURCE_RELATIVE,&i);
		DebugPrintf("buffer relative? %s\n",(i==AL_TRUE)?"true":"false");
	}

	return source;
}

BOOL sound_duplicate( sound_t * source, sound_t ** destination )
{
	ALenum error;

	// allocate space for new source
	*destination = malloc(sizeof(sound_t));

	// use the same buffer as the source
	(*destination)->buffer = source->buffer;

	// clear errors
	alGetError();

	// generate a new source id
	alGenSources(1,&(*destination)->source);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DebugPrintf("alGenSources: %s\n", alGetString(error));
		free(*destination);
		*destination = NULL;
		return FALSE;
	}

	// attach the buffer to the source
	alSourcei((*destination)->source, AL_BUFFER, (*destination)->buffer);
	if ((error = alGetError()) != AL_NO_ERROR)
	{
		DebugPrintf("alSourcei AL_BUFFER: %s\n", alGetString(error));
		free(*destination);
		*destination = NULL;
		return FALSE;
	}

	if(!Sound3D)
	{
		alSourcei((*destination)->source,AL_SOURCE_RELATIVE,AL_TRUE);
	}

	return TRUE;
}

#endif // SOUND_OPENAL
