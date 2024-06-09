/** \file
 *   looper.
 *   Record and loop/overdub
 */

// DONE: remove rec trigger
// DONE: set default rec mode to boomerang mode
// DONE: remove snap
// TODO: figure out how to put the leds on top of the buttons (maybe need the custom gui)
// DONE: link leds to switches
// TODO: add stack mode
// TODO: add ONCE mode
// TODO: Flash record LED when loop cycles around
// TODO: add 1/2 speed mode
// TODO: figure out how to flip Play switch when record stops


// NOTES:
// - "armed" means "the button is pressed or in Enable state"


/*  Effect Description
 *
 */
string name="Boomerang Phrase Sampler";
string description="An attempt to emulate the Boomerang+ Looper pedal";

/* Parameters Description.
 */
enum InputParamsIndexes
{
    kOutputLevelParam=0,
    kThruMuteParam,
    kRecordParam,
    kPlayParam,
    kOnceParam,
    kReverseParam,
    kStackParam,
    kHalfSpeedParam

};

enum RecordMode
{
    kRecLoopOver=0, ///< standard looper: records over existing material and keep original loop length
    kRecExtend,     ///< records over existing material and extends original loop length when reaching end of loop
    kRecAppend,     ///< append to existing loop (without recording original loop content after loop duration has been reached) <-- stack mode?
    kRecOverWrite,  ///< overwrite loop content with new material, and extend original loop until recording ends
    kRecPunch,      ///< overwrite loop content with new material, but keeps original loop length
    kRecClear,       ///< clear original loop when recording starts  <-- pretty sure this is the boomerang mode when press record (5)
};


array<string> inputParametersNames={"Output Level", "Thru Mute", "Record", "Play (Stop)", "Once", "Direction", "Stack (Speed)"};
array<double> inputParameters(inputParametersNames.length);
array<double> inputParametersDefault={
    5, // OutputLevel
    0, // Thru Mute
    0, // Record
    0, // Play/Stop
    0, // Once
    0, // Reverse
    0, // Stack
    0  // 1/2 Speed
};
array<double> inputParametersMax={
    10,  // Output Level, if not entered, defaults to percentage
    1,   // Thru Mute
    1,   // Record
    1,   // Play/Stop
    1,   // Once
    1,   // Reverse
    1,   // Stack
    1    // 1/2 Speed
};

// these are the number of available steps/modes for each parameter - 1-based
array<int> inputParametersSteps={
    -1, // OutputLevel // -1 means continuous
    2,  // Thru Mute
    2,  // Record
    2,  // Play
    2,  // Once
    2,  // Reverse
    2,  // Stack
    2   // 1/2 Speed
};

// these are the labels under each input control
array<string> inputParametersEnums={
    "",            // Output Level
    ";THRU MUTE",  // Thru Mute
    ";Recording",  // Record
    ";Playing",    // Play
    ";Once",       // Once
    "Fwd;Rev",     // Reverse
    ";STACK"       // Stack
    ";1/2 Speed"   // 1/2 Speed
};

enum OutputParamsIndexes
{
    kThruMuteLed=0,
    kRecordLed,  // 1
    kPlayLed,    // 2
    kOnceLed,    // 3
    kReverseLed, // 4
    kStackLed,   // 5
    kSpeedLed    // 6
};

array<string> outputParametersNames={"Thru Mute","Record","Play","Once","Reverse","Stack", "1/2 Speed"};
array<double> outputParameters(outputParametersNames.length);
array<double> outputParametersMin={0,0,0,0,0,0,0};
array<double> outputParametersMax={1,1,1,1,1,1,1};
array<string> outputParametersEnums={";",";",";",";",";",";",";"};



/* Internal Variables.
 * These appear to be global variables that are used to store the state of the plugin
 */
array<array<double>> buffers(audioInputsCount);

const int MAX_LOOP_DURATION_SECONDS=60; // 60 seconds max recording

int allocatedLength=int(sampleRate * MAX_LOOP_DURATION_SECONDS); // 60 seconds max recording

bool recording=false;         // currently recording
bool recordingArmed=false;    // record button is pressed

double OutputLevel=0;         // output level

double playbackGain=0;        // playback gain
double playbackGainInc=0;     // playback gain increment

double recordGain=0;          // record gain
double recordGainInc=0;       // record gain increment

int currentPlayingIndex=0;    // current playback index
int currentRecordingIndex=0;  // current recording index

int loopDuration=0;           // loop duration

bool eraseValueMem=false;     // erase value memory  ???

const int fadeTime=int(.001 * sampleRate); // 1ms fade time
const double xfadeInc=1/double(fadeTime);  // fade increment

RecordMode recordingMode=kRecClear; // Hardcode to boomerang mode. Clear loop when record is pressed.

///// These are checked in the processBlock function - called for each block of samples
bool Reverse=false;       // reverse mode
bool ReverseArmed=false;  // reverse (direction) button is pressed

bool playing=false;       // currently playing state
bool playArmed=false;     // play button is pressed

bool onceMode=false;      // once mode
bool onceArmed=false;     // once button is pressed

bool stackMode=false;     // stack mode
bool stackArmed=false;    // stack button is pressed

bool halfSpeedMode=false; // half speed mode
bool halfSpeedArmed=false;// half speed button is pressed

bool bufferFilled=false;

/* Initialization
 *
 */
void initialize()
{
    // create buffer
    for(uint channel=0;channel<audioInputsCount;channel++)
    {
        buffers[channel].resize(allocatedLength);
    };
    loopDuration=0;
}

int getTailSize()
{
    // infinite tail (sample player)
    return -1;
}

void startRecording()
{
    // Don't reset loop if in once mode
    if(stackMode)
    {
        // stack mode
        // add the incoming audio to the buffer
        // reduce existing audio by 2.5Db
        ;
    }
    else
    {
        loopDuration=0;
        currentPlayingIndex=0;
    }
    
    // actually start recording
    recording=true;
    currentRecordingIndex=currentPlayingIndex;

    // pre fade to avoid clicks
    recordGain=0;
    recordGainInc=xfadeInc;

    // reduce existing audio by 2.5Db
    
}

void stopRecording()
{
    recording=false;
    const int recordedCount=currentRecordingIndex;
    // post fade to avoid clicks
    recordGainInc=-xfadeInc;
    
    // // recorded beyond previous loop -> update duration, start playback at 0
    // if(recordedCount>loopDuration)
    // {
    //     loopDuration=recordedCount;
    //     currentPlayingIndex=0;
    // }

    // normal mode:
    // set loop length to recorded length
    // start playback at 0
    if(!stackMode) {
        loopDuration=recordedCount;
        currentPlayingIndex=0;
    }
    // stack mode
    // add the incoming audio to the buffer
    // reduce existing audio by 2.5Db
    else {
        
        ;
    }


    // if(recordingMode==kRecPunch)
    // {
    //     // increase playback gain to start fade in
    //     playbackGainInc=xfadeInc;
    // }
}

void startPlayback()
{
    // start playback
    playing=true;
    currentPlayingIndex=0;
    playbackGain=0;
    playbackGainInc=xfadeInc;
}

void stopPlayback()
{
    playing=false;
    playbackGainInc=-xfadeInc;
}

bool isPlaying()
{
    // true if playing or if playback gain is not 0
    return (playing || (playbackGainInc !=0 ));
}

void startReverse()
{
    if(Reverse==false && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Reverse=true;
}

void stopReverse()
{
    if(Reverse==true && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Reverse=false;
}

// This is called for each block of samples
void processBlock(BlockData& data) {

    // // indexes used by auto record trigger or snapped play/stop
    int startRecordingSample=-1; // sample number in buffer when recording should be started
    int stopRecordingSample=-1; // sample number in buffer when recording should be stopped
    int startPlayingSample=-1; // sample number in buffer when playback should be started	
    int stopPlayingSample=-1;  // sample number in buffer when playback should be stopped
    int startReverseSample=-1; // sample number in buffer when Reverse should be started
    int stopReverseSample=-1; // sample number in buffer when Reverse should be stopped

    // if not recording, recording is pressed -> start recording
    if(!recording && recordingArmed == true)
    {
        // for each channel, while the there is no startRecordingSample, find the first sample that is not 0
        for(uint channel=0; channel<audioInputsCount && startRecordingSample == -1; channel++)
        {
            // create sample buffer
            array<double>@ samplesBuffer=@data.samples[channel];

            // for each sample in the buffer, if the sample is not 0, set the startRecordingSample to the index of the sample
            for(uint i=0;i<data.samplesToProcess && startRecordingSample==-1;i++)
            {
                startRecordingSample=i;
            }
        }
        // not found? next buffer
        if(startRecordingSample==-1)
            startRecordingSample=data.samplesToProcess;
    }

    // smooth OutputLevel update: use begin and end values
    // since the actual gain is exponential, we can use the ratio between begin and end values
    // as an incremental multiplier for the actual gain
    double OutputLevelInc=(data.endParamValues[kOutputLevelParam] - data.beginParamValues[kOutputLevelParam])/data.samplesToProcess;
    
    // actual audio processing happens here
    // i don't think we need all this stop/start stuff since we're not using any snap/sync
    // for each sample in the buffer
    for(uint i=0; i<data.samplesToProcess; i++)
    {
        // manage recording state
        // start recording if startRecordingSample is the current sample
        bool doStartRecording=(int(i)==startRecordingSample);

        // stop recording if stopRecordingSample is the current sample
        bool doStopRecording=(int(i)==stopRecordingSample);
        
        if(doStartRecording) {
            startRecording();
        }
        else if(doStopRecording) {
            stopRecording();
        }
        
        // manage playback state
        // start playing if startPlayingSample is the current sample
        bool startPlaying=(int(i)==startPlayingSample);

        // stop playing if stopPlayingSample is the current sample
        bool stopPlaying=(int(i)==stopPlayingSample);
        
        if(startPlaying)
        {
            startPlayback();
        }
        else if(stopPlaying)
        {
            stopPlayback();
        }

        // manage Reverse state
        bool startReversal=(int(i)==startReverseSample);
        bool stopReversal=(int(i)==stopReverseSample);
        
        if(startReversal) {
            startReverse();
        }
        else if(stopReversal) {
            stopReverse();
        }
        
        const bool currentlyPlaying=isPlaying();
        
        // process audio for each channel--------------------------------------------------
        for(uint channel=0; channel < audioInputsCount; channel++)
        {
            array<double>@ channelBuffer=@buffers[channel];
            array<double>@ samplesBuffer=@data.samples[channel];
            double input=samplesBuffer[i];
    
            double playback=0;
            if(currentlyPlaying)
            {
                // read loop content
                int index=currentPlayingIndex;
                if(Reverse && loopDuration>0)
                    index=loopDuration-1-index;
                playback=channelBuffer[index]*playbackGain;
            }
            
            // update buffer when recording
            if(recording || (recordGainInc!=0))
            {
                channelBuffer[currentRecordingIndex]=playback+recordGain*input;
            }
            
            // copy to output with OutputLevel
            samplesBuffer[i]=input+OutputLevel*playback;
        }
        // end process audio for each channel--------------------------------------------------
        
        
        // update playback index while playing
        if(currentlyPlaying)
        {
            // update index
            currentPlayingIndex++;
            if(currentPlayingIndex>=loopDuration)
                currentPlayingIndex=0;
            
            // playback xfade
            if(loopDuration>0)
            {
                if(!(recording && recordingMode==kRecPunch)) // when recording in punch mode, playback gain is controlled by record status
                {
                    if(currentPlayingIndex==(loopDuration-fadeTime))
                        playbackGainInc=-xfadeInc;
                    else if(currentPlayingIndex<fadeTime)
                        playbackGainInc=xfadeInc;
                }
            }
            else
                playbackGain=0;
        }
        
        // update recording index, if recording
        if(recording || (recordGainInc!=0))
        {
            currentRecordingIndex++;
            // looping over existing => check boundaries
            if((recordingMode==kRecLoopOver||recordingMode==kRecPunch) && loopDuration>0 && currentRecordingIndex>=loopDuration)
            {
                currentRecordingIndex=0;
            }
            if(currentRecordingIndex>=allocatedLength) // stop recording if reached the end of the buffer
            {
                stopRecording();
                recordGainInc=0; // avoid post buffer recording
                // TODO: turn on all LEDs if this happens
                // wait for Record or Play to be pressed
                bufferFilled=true;
            }
        }
        
        // update playback gain
        if(playbackGainInc!=0)
        {
            playbackGain+=playbackGainInc;
            if(playbackGain>1)
            {
                playbackGain=1;
                playbackGainInc=0;
            }
            else if (playbackGain<0)
            {
                playbackGain=0;
                playbackGainInc=0;
            }
        }
        
        // update record gain
        if(recordGainInc!=0)
        {
            recordGain+=recordGainInc;
            if(recordGain>1)
            {
                recordGain=1;
                recordGainInc=0;
            }
            else if (recordGain<0)
            {
                recordGain=0;
                recordGainInc=0;
            }
        }
        OutputLevel+=OutputLevelInc;
    }
}


// This is called when an input param is changed
void updateInputParametersForBlock(const TransportInfo@ info)
{
    
    // Reverse--------------------------------------------------------------------------
    ReverseArmed=inputParameters[kReverseParamParam]>.5;
    if(ReverseArmed) {
        startReverse();
    }
    else {
        stopReverse();
    }

    // playing--------------------------------------------------------------------------
    bool wasPlaying=playing;                    // if we were playing when we entered this block
    bool playWasArmed=playArmed;                // if we were play armed when we entered this block
    playArmed=inputParameters[kPlayParam]>.5;   // if we are play armed now
    
    // start playing right now
    if(!wasPlaying && playArmed) { 
        startPlayback(); 
    }

    // stop playing right now
    if(wasPlaying && playWasArmed && !playArmed) { 
        stopPlayback(); 
    }

    // ONCE
    // Pressing ONCE while recording will halt recording and initiate an immediate playback of the signal just recorded, but the loop will playback only once. 
    // Pressing ONCE during playback tells the Boomerang Phrase Sampler to finish playing the loop and then stop. 
    // If the Boomerang Phrase Sampler is idle, pressing ONCE will playback your recorded loop one time. 
    // After pressing this button, the ONCE LED will be turned on letting you know this is the last time through your loop.
    // There is an interesting twist in the way the ONCE button works. Pressing it while the ONCE LED is on will always immediately restart playback. Repeated presses produces a stutter effect sort of like record scratching.
    bool wasInOnceMode=onceMode;             // if we were in once mode when we entered this block
    bool wasOnceArmed=onceArmed;             // if we were once armed when we entered this block
    onceArmed = inputParameters[kOnceParam]>.5;   // if we are once armed now
    if(onceArmed) {
        onceMode=true;
        if(playing) {
            // keep playing but stop after this loop
            // implemented in processBlock
        }
        else if(recording) {
            stopRecording();
            startPlayback(); // will only playback once due to onceArmed
        }
        else {
            startPlayback(); // will only playback once due to onceArmed
        }
    }
    else {
        onceMode=false;
    }
    
    // recording------------------------------------------------------------------------
    bool wasRecording=recording;                      // if we were recording when we entered this block
    bool wasArmed=recordingArmed;                     // if we were record armed when we entered this block
    recordingArmed=inputParameters[kRecordParam]>.5;  // if we are armed now
    
    // continue recording if already recording
    if(recordingArmed && loopDuration !=0) {
        recording=true;
    }

    // stop recording
    if(wasRecording) {
        stopRecording();
        startPlayback();
    }
    
    // start recording at current playback index
    if(!wasRecording && recording)
    {
        startRecording();
    }
    
    // erase content and restart recording to 0
    // TODO: This should happen when record is pressed while playing
    if(playing && recordingArmed) {
        loopDuration=0;
        currentRecordingIndex=0;
        currentPlayingIndex=0;
        recording=true;
        startRecording();
    }
    
    // OutputLevel
    OutputLevel=inputParameters[kOutputLevelParam];
}

void computeOutputData()
{
    // playback status
    if(isPlaying() && loopDuration != 0) {
        outputParameters[kPlayLed] = 1;
    }
    else
        outputParameters[kPlayLed]=0;

    // recording status
    if(recording)
        outputParameters[kRecordLed]=1;
    else
        outputParameters[kRecordLed]=0;

    // Reverse status
    if(Reverse)
        outputParameters[kReverseLed]=1;
    else
        outputParameters[kReverseLed]=0;

    // Once status
    if(onceMode)
        outputParameters[kOnceLed]=1;
    else
        outputParameters[kOnceLed]=0;
}
