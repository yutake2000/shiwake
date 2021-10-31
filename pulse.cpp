#include <stdio.h>
#include <pulse/mainloop.h>
#include <pulse/context.h>
#include <pulse/stream.h>
#include <pulse/error.h>
#include <chrono>
#include <sys/time.h>
#include <ctime>

#include "yswavfile.h"

using namespace std;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;

static YSRESULT YsPulseAudioWaitForConnectionEstablished(pa_context *paContext,pa_mainloop *paMainLoop,time_t timeOut)
{
	time_t timeLimit=time(NULL)+timeOut;
	while(timeLimit>=time(NULL))
	{
		pa_mainloop_iterate(paMainLoop,0,NULL);
		if(PA_CONTEXT_READY==pa_context_get_state(paContext))
		{
			return YSOK;
		}
	}
	return YSERR;
}

void playSound(char filename[], bool &musicPlaying, time_t &beginTime, time_t skip) {
	YsWavFile wavFile;
	if(YSOK!=wavFile.LoadWav(filename))
	{
		fprintf(stderr,"Cannot open wave file.\n");
		return;
	}

	// Test Resampling >>
	printf("Before Resampling:\n");
	printf("Bit per sample: %d\n",wavFile.BitPerSample());
	printf("Stereo: %d\n",wavFile.Stereo());
	printf("Playback Rate: %d\n",wavFile.PlayBackRate());
	printf("Signed: %d\n",wavFile.IsSigned());

	wavFile.ConvertTo16Bit();
	wavFile.ConvertToSigned();
	wavFile.ConvertToStereo();
	wavFile.Resample(44100);
	wavFile.ConvertToMono();

	printf("After Resampling:\n");
	printf("Bit per sample: %d\n",wavFile.BitPerSample());
	printf("Stereo: %d\n",wavFile.Stereo());
	printf("Playback Rate: %d\n",wavFile.PlayBackRate());
	printf("Signed: %d\n",wavFile.IsSigned());
	// << Test Resampling



	pa_mainloop *paMainLoop=pa_mainloop_new();  // Pennsylvania Main loop?
	if(NULL==paMainLoop)
	{
		fprintf(stderr,"Cannot create main loop.\n");
		return;
	}

	pa_context *paContext=pa_context_new(pa_mainloop_get_api(paMainLoop),"YsPulseAudioCon");
	if(NULL==paContext)
	{
		fprintf(stderr,"Cannot create context.\n");
		return;
	}

	printf("Mainloop and Context Created.\n");

	// pa_context_set_state_callback(paContext,YsPulseAudioConnectionCallBack,NULL);
	pa_context_connect(paContext,NULL,(pa_context_flags_t)0,NULL);

	// I seem to be able to either wait for call back or poll context state until it is ready.
	YsPulseAudioWaitForConnectionEstablished(paContext,paMainLoop,5);

	printf("I hope it is connected.\n");




	pa_sample_format_t format;
	switch(wavFile.BitPerSample())
	{
	case 8:
		if(YSTRUE==wavFile.IsSigned())
		{
			wavFile.ConvertToUnsigned();
		}
		format=PA_SAMPLE_U8;
		break;
	case 16:
		if(YSTRUE!=wavFile.IsSigned())
		{
			wavFile.ConvertToSigned();
		}
		format=PA_SAMPLE_S16LE;
		break;
	}
	const int rate=wavFile.PlayBackRate();
	const int nChannel=(YSTRUE==wavFile.Stereo() ? 2 : 1);

    const pa_sample_spec ss=
    {
        format,
        (unsigned)rate,
        (unsigned char)nChannel
    };

	pa_stream *paStream=pa_stream_new(paContext,"YsStream",&ss,NULL);

	if(NULL!=paStream)
	{
		printf("Stream created!  Getting there!\n");
	}

	pa_stream_connect_playback(paStream,NULL,NULL,(pa_stream_flags_t)0,NULL,NULL);



	printf("Entering main loop.\n");

	unsigned int playBackPtr= skip * rate * nChannel * wavFile.BitPerSample() / 8;
	YSBOOL checkForUnderflow=YSTRUE;

	const time_t t0=time(NULL);
	time_t prevT=time(NULL)-1;
	beginTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - skip * 1000;
	for(;;)
	{
		if(time(NULL)!=prevT)
		{
			//printf("Ping...\n");
			prevT=time(NULL);
		}

		if(PA_STREAM_READY==pa_stream_get_state(paStream))
		{
			const size_t writableSize=pa_stream_writable_size(paStream);
			const size_t sizeRemain=wavFile.SizeInByte()-playBackPtr;
			const size_t writeSize=(sizeRemain<writableSize ? sizeRemain : writableSize);

			if(0<writeSize)
			{
				//printf("Write %d\n",(int)writeSize);
				pa_stream_write(paStream,wavFile.DataPointer()+playBackPtr,writeSize,NULL,0,PA_SEEK_RELATIVE);
				playBackPtr+=writeSize;
			}
		}

		if(wavFile.SizeInByte()<=playBackPtr &&
		   0<=pa_stream_get_underflow_index(paStream) &&
		   YSTRUE==checkForUnderflow)
		{
			printf("Underflow detected. (Probably the playback is done.)\n");
			checkForUnderflow=YSFALSE;
			break;
		}

		pa_mainloop_iterate(paMainLoop,0,NULL);

		if (!musicPlaying) break;

	}

	musicPlaying = false;

	pa_stream_disconnect(paStream);
	pa_stream_unref(paStream);
	pa_context_disconnect(paContext);
	pa_context_unref(paContext);
	pa_mainloop_free(paMainLoop);

	return;
}