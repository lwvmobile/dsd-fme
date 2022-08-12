#include <stdio.h>

#ifdef USE_PORTAUDIO
#include <portaudio.h>

#ifdef WIN32
#include <windows.h>
#endif

void printPortAudioDevices()
{
    int     i, numDevices, defaultDisplayed;
    const   PaDeviceInfo *deviceInfo;
    PaStreamParameters inputParameters, outputParameters;
    PaError err;


    Pa_Initialize();

    fprintf (stderr,  "\nPortAudio version number = %d\nPortAudio version text = '%s'\n",
            Pa_GetVersion(), Pa_GetVersionText() );


    numDevices = Pa_GetDeviceCount();
    if( numDevices < 0 )
    {
        fprintf (stderr,  "ERROR: Pa_GetDeviceCount returned 0x%x\n", numDevices );
        err = numDevices;
        goto error;
    }

    fprintf (stderr,  "Number of devices = %d\n", numDevices );
    for( i=0; i<numDevices; i++ )
    {
        deviceInfo = Pa_GetDeviceInfo( i );
        fprintf (stderr,  "--------------------------------------- device #%d\n", i );

    /* Mark global and API specific default devices */
        defaultDisplayed = 0;
        if( i == Pa_GetDefaultInputDevice() )
        {
            fprintf (stderr,  "[ Default Input" );
            defaultDisplayed = 1;
        }
        else if( i == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultInputDevice )
        {
            const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
            fprintf (stderr,  "[ Default %s Input", hostInfo->name );
            defaultDisplayed = 1;
        }

        if( i == Pa_GetDefaultOutputDevice() )
        {
            fprintf (stderr,  (defaultDisplayed ? "," : "[") );
            fprintf (stderr,  " Default Output" );
            defaultDisplayed = 1;
        }
        else if( i == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultOutputDevice )
        {
            const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
            fprintf (stderr,  (defaultDisplayed ? "," : "[") );
            fprintf (stderr,  " Default %s Output", hostInfo->name );
            defaultDisplayed = 1;
        }

        if( defaultDisplayed )
            fprintf (stderr,  " ]\n" );

    /* print device info fields */
#ifdef WIN32
        {   /* Use wide char on windows, so we can show UTF-8 encoded device names */
            wchar_t wideName[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, deviceInfo->name, -1, wideName, MAX_PATH-1);
            wprintf( L"Name                        = %s\n", wideName );
        }
#else
        fprintf (stderr, "Name                        = %s\n", deviceInfo->name );
#endif
        fprintf (stderr, "Host API                    = %s\n",  Pa_GetHostApiInfo( deviceInfo->hostApi )->name );
        fprintf (stderr, "Max inputs = %d", deviceInfo->maxInputChannels  );
        fprintf (stderr, ", Max outputs = %d\n", deviceInfo->maxOutputChannels  );
        fprintf (stderr, "Default sample rate         = %8.2f\n", deviceInfo->defaultSampleRate );
    }

    Pa_Terminate();

    fprintf (stderr,"----------------------------------------------\n");
    return;

error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
}

#else

void printPortAudioDevices()
{
    fprintf (stderr,"PortAudio not supported in this build of dsd\n");
}

#endif
