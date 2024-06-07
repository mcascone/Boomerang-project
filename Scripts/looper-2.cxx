/** \file
 *   looper.
 *   Record and loop/overdub
 */

// DONE: remove rec trigger
// DONE: set default rec mode to boomerang mode
// DONE: remove snap
// TODO: figure out how to put the leds on top of the buttons (maybe need the custom gui)
// TODO: link leds to switches

/*  Effect Description
 *
 */
string name="Looper-2";
string description="An attempt to emulate the Boomerang+ Looper pedal";

/* Parameters Description.
 */
enum InputParamsIndexes
{
    kThruMtureParam=0,
    kRecordParam,
    kPlayParam,
    kResetParam,
    kRecMode,
    kDirection,
    kOutputLevelParam
};

enum RecordMode
{
    kRecLoopOver=0, ///< standard looper: records over existing material and keep original loop length
    kRecExtend,     ///< records over existing material and extends original loop length when reaching end of loop
    kRecAppend,     ///< append to existing loop (without recording original loop content after loop duration has been reached) <-- stack mode?
    kRecOverWrite,  ///< overwrite loop content with new material, and extend original loop until recording ends
    kRecPunch,      ///< overwrite loop content with new material, but keeps original loop length
    kRecClear       ///< clear original loop when recording starts  <-- pretty sure this is the boomerang mode when press record
};



array<string> inputParametersNames={"Thru Mute", "Record","Play (Stop)","Rec Mode","Direction","Output Level"};
array<double> inputParametersDefault={
    0, // Thru Mute
    0, // Record
    0, // Play
    5, // Rec Mode
    0, // Direction
    5  // OutputLevel
};
array<double> inputParametersMax={
    1, // Thru Mute
    1, // Record
    1, // Play
    4, // Rec Mode
    1, // Direction
    10  // if not entered, defaults to percentage
};

// these are the number of available steps/modes for each parameter - 1-based
array<int>    inputParametersSteps={
    2,  // Thru Mute
    2,  // Record
    2,  // Play
    6,  // Rec Mode
    2,  // Direction
    -1  // OutputLevel // -1 means continuous
};

// these are the labels under each input control
array<string> inputParametersEnums={
    ";THRU MUTE",                 // Thru Mute
    ";Recording",        // Record
    ";Playing",          // Play
    "Loop;Extend;Append;Overwrite;Punch;Clear", // Rec Mode
    "Fwd;Rev",               // Direction
};
array<double> inputParameters(inputParametersNames.length);

array<string> outputParametersNames={"Thru Mute","Record","Play","Once","Reverse","Stack (Speed)", "1/2 Speed"};
array<double> outputParameters(outputParametersNames.length);
array<double> outputParametersMin={0,0,0,0,0,0,0};
array<double> outputParametersMax={1,1,1,1,1,1,1};
array<string> outputParametersEnums={";",";",";",";",";",";",";"};



/* Internal Variables.
 *
 */
array<array<double>> buffers(audioInputsCount);

int allocatedLength=int(sampleRate*60); // 60 seconds max recording
bool recording=false;
bool recordingArmed=false;
bool autoTrigger=false;
double OutputLevel=0;
double playbackGain=0;
double recordGain=0;
double playbackGainInc=0;
double recordGainInc=0;
int currentPlayingIndex=0;
int currentRecordingIndex=0;
int loopDuration=0;
bool eraseValueMem=false;
const int fadeTime=int(.001*sampleRate); // 1ms fade time
const double xfadeInc=1/double(fadeTime);
const double triggerThreshold=.005;
RecordMode recordingMode=kRecLoopOver;
bool triggered=false;
bool Direction=false;
bool DirectionArmed=false;
bool playArmed=false;
bool playing=false;

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

//sync utils
double quarterNotesToSamples(double position,double bpm)
{
    return position*60.0*sampleRate/bpm;
}

double samplesToQuarterNotes(double samples,double bpm)
{
    return samples*bpm/(60.0*sampleRate);
}

void startRecording()
{
    // clear recording if required
    if(recordingMode==kRecClear)
    {
        loopDuration=0;
        currentPlayingIndex=0;
    }
    else if(recordingMode==kRecOverWrite)
    {
        // truncate loop duration to current position (+ crossfade time)
        int newLoopDuration=currentPlayingIndex+int(fadeTime+1);
        if(newLoopDuration<loopDuration)
        {
            loopDuration=newLoopDuration;
        }
    }
    else if(recordingMode==kRecPunch)
    {
        // decrease playback gain to start fade out
        playbackGainInc=-xfadeInc;
    }
    
    // actually start recording
    recording=true;
    currentRecordingIndex=currentPlayingIndex;
    // pre fade to avoid clicks
    recordGain=0;
    recordGainInc=xfadeInc;
}

void stopRecording()
{
    recording=false;
    const int recordedCount=currentRecordingIndex;
    // post fade to avoid clicks
    recordGainInc=-xfadeInc;
    
    // recorded beyond previous loop -> update duration, start playback at 0
    if(recordedCount>loopDuration)
    {
        loopDuration=recordedCount;
        currentPlayingIndex=0;
    }
    
    if(recordingMode==kRecPunch)
    {
        // increase playback gain to start fade in
        playbackGainInc=xfadeInc;
    }
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
    // while recording and overwriting content without blending, do not play
    return (playing || (playbackGainInc!=0)) && !(recording && ((recordingMode==kRecOverWrite || recordingMode==kRecAppend) && currentRecordingIndex>loopDuration));
}

void startDirection()
{
    if(Direction==false && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Direction=true;
}

void stopDirection()
{
    if(Direction==true && loopDuration>0)
    {
        currentPlayingIndex=(loopDuration-1-currentPlayingIndex);
    }
    Direction=false;
}

void processBlock(BlockData& data)
{
    // // indexes used by auto record trigger or snapped play/stop
    int startRecordingSample=-1; // sample number in buffer when recording should be started
    int stopRecordingSample=-1; // sample number in buffer when recording should be stopped
    int startPlayingSample=-1; // sample number in buffer when playback should be started	
    int stopPlayingSample=-1; // sample number in buffer when playback should be stopped
    int startDirectionSample=-1; // sample number in buffer when Direction should be started
    int stopDirectionSample=-1; // sample number in buffer when Direction should be stopped

    // // Auto Trigger mode: check if there is any sound before starting new recording
    // if(!recording && loopDuration==0 && recordingArmed==true && autoTrigger==true && !triggered)
    // {
    //     for(uint channel=0;channel<audioInputsCount && startRecordingSample==-1;channel++)
    //     {
    //         array<double>@ samplesBuffer=@data.samples[channel];
    //         for(uint i=0;i<data.samplesToProcess && startRecordingSample==-1;i++)
    //         {
    //             if(abs(samplesBuffer[i])>triggerThreshold)
    //             {
    //                 startRecordingSample=i;
    //                 triggered=true;
    //             }
    //         }
    //     }
    //     // not found? next buffer
    //     if(startRecordingSample==-1)
    //         startRecordingSample=data.samplesToProcess;
    // }

    // if in snap mode and the host is actually playing: check our position
    // if(snap!=kSnapNone && @data.transport!=null && data.transport.isPlaying)
    // {
    //     // start position is either buffer start, or startRecordingSample if we are detecting automatically
    //     double currentPos=data.transport.positionInQuarterNotes;
    //     if(startRecordingSample>=0)
    //         currentPos+=samplesToQuarterNotes(startRecordingSample,data.transport.bpm);
        
    //     double expectedPos=0;
        
    //     // snap to next quarter note
    //     if(snap==kSnapQuarter)
    //     {
    //         expectedPos=floor(currentPos);
    //         if(expectedPos!=currentPos)
    //             expectedPos++;
    //     }
    //     // expected position is next down beat (except if buffer is exactly on beat
    //     else if(snap==kSnapMeasure)
    //     {
    //         expectedPos=data.transport.currentMeasureDownBeat;
    //         if(expectedPos!=0)
    //             expectedPos+=(data.transport.timeSigTop)/(data.transport.timeSigBottom)*4.0;
    //         while(expectedPos<currentPos)
    //             expectedPos+=(data.transport.timeSigTop)/(data.transport.timeSigBottom)*4.0;
    //     }
    //     double expectedSamplePosition=quarterNotesToSamples(expectedPos,data.transport.bpm);
    //     int nextSample=int(floor(expectedSamplePosition+.5))-data.transport.positionInSamples;
        
    //     // manage recording
    //     if(!recording && (loopDuration==0 || recordingMode==kRecClear) && recordingArmed==true)
    //     {
    //         startRecordingSample=nextSample;
    //     }
    //     else if (recording && recordingArmed==false)
    //     {
    //         stopRecordingSample=nextSample;
    //     }
        
    //     // manage playing
    //     if(!playing && playArmed==true)
    //     {
    //         startPlayingSample=nextSample;
    //     }
    //     else if (playing && playArmed==false)
    //     {
    //         stopPlayingSample=nextSample;
    //     }
        
    //     // manage reversing
    //     if(!Direction && DirectionArmed==true)
    //     {
    //         startDirectionSample=nextSample;
    //     }
    //     else if (playing && DirectionArmed==false)
    //     {
    //         stopDirectionSample=nextSample;
    //     }
    // }
    
    // smooth OutputLevel update: use begin and end values
    // since the actual gain is exponential, we can use the ratio between begin and end values
    // as an incremental multiplier for the actual gain
    double OutputLevelInc=(data.endParamValues[kOutputLevelParam]-data.beginParamValues[kOutputLevelParam])/data.samplesToProcess;
    
    // actual audio processing happens here
    for(uint i=0;i<data.samplesToProcess;i++)
    {
        // manage recording state
        bool doStartRecording=(int(i)==startRecordingSample);
        bool doStopRecording=(int(i)==stopRecordingSample);
        
        if(doStartRecording)
            startRecording();
        else if(doStopRecording)
            stopRecording();
        
        // manage playback state
        bool startPlaying=(int(i)==startPlayingSample);
        bool stopPlaying=(int(i)==stopPlayingSample);
        
        if(startPlaying)
        {
            startPlayback();
        }
        else if(stopPlaying)
        {
            stopPlayback();
        }

        // manage Direction state
        bool startReversal=(int(i)==startDirectionSample);
        bool stopReversal=(int(i)==stopDirectionSample);
        
        if(startReversal)
            startDirection();
        else if(stopReversal)
            stopDirection();
        
        const bool currentlyPlaying=isPlaying();
        
        // process audio for each channel--------------------------------------------------
        for(uint channel=0;channel<audioInputsCount;channel++)
        {
            array<double>@ channelBuffer=@buffers[channel];
            array<double>@ samplesBuffer=@data.samples[channel];
            double input=samplesBuffer[i];
    
            double playback=0;
            if(currentlyPlaying)
            {
                // read loop content
                int index=currentPlayingIndex;
                if(Direction && loopDuration>0)
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

void updateInputParametersForBlock(const TransportInfo@ info)
{
    
    // Direction--------------------------------------------------------------------------
    DirectionArmed=inputParameters[kDirection]>.5;
    if(DirectionArmed) {
        startDirection();
    }
    else {
        stopDirection();
    }

    // playing--------------------------------------------------------------------------
    bool wasPlaying=playing;
    bool playWasArmed=playArmed;
    playArmed=inputParameters[kPlayParam]>.5;
    
    // start playing right now
    if(!wasPlaying && playArmed) { startPlayback(); }

    // stop playing right now
    if(wasPlaying && playWasArmed && !playArmed) { stopPlayback(); }

    // recording------------------------------------------------------------------------
    bool wasRecording=recording;
    bool wasArmed=recordingArmed;
    recordingArmed=inputParameters[kRecordParam]>.5;
    recordingMode=RecordMode(inputParameters[kRecMode]+.5);
    
    // reset triggered state
    if(!wasArmed && recordingArmed) { 
        triggered=false; 
    }
    
    // start recording, if not in auto trigger mode, or if already recorded (except for clear mode that should sync)
    if(recordingArmed && (!autoTrigger || loopDuration!=0) || (loopDuration!=0 && recordingMode!=kRecClear)) {
        recording=true;
    }

    // stop recording
    if(wasRecording && !recordingArmed) {
        stopRecording();
    }
    
    // start recording at current playback index
    if(!wasRecording && recording)
    {
        startRecording();
    }
    
    // erase content and restart recording to 0
    bool eraseVal=inputParameters[kResetParam]>.5;
    if(eraseVal!=eraseValueMem)
    {
        eraseValueMem=eraseVal;
        loopDuration=0;
        currentRecordingIndex=0;
        currentPlayingIndex=0;
        if(!recordingArmed || autoTrigger) {
            recording=false;
        }
    }
    
    // OutputLevel
    OutputLevel=inputParameters[kOutputLevelParam];
}

void computeOutputData()
{
    // playback status
    if(isPlaying() && loopDuration!=0)
        outputParameters[0]=1;
    else
        outputParameters[0]=0;

    // recording status
    if(recording)
        outputParameters[1]=1;
    else
        outputParameters[1]=0;
    
    // current loop playback
    if(loopDuration>0)
    {
        if(Direction)
            outputParameters[2]=double(loopDuration-1-currentPlayingIndex)/double(loopDuration);            
        else
            outputParameters[2]=double(currentPlayingIndex)/double(loopDuration);
    }
    else
        outputParameters[2]=0;
    
    if(loopDuration!=0 && recording)
        outputParameters[3]=double(currentRecordingIndex)/double(loopDuration);
    else if(recording)
        outputParameters[3]=1;
    else	
        outputParameters[3]=0;
    
    if(outputParameters[3]>1)
        outputParameters[3]=1;
    
    // relative length of current loop
    outputParameters[4]=0;
    if(loopDuration!=0)
    {
        outputParameters[4]=1;
        if(currentRecordingIndex>loopDuration)
            outputParameters[4]=double(loopDuration)/double(currentRecordingIndex);
    }
}
