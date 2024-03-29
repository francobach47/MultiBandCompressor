#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MultiBandCompressorAudioProcessor::MultiBandCompressorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    using namespace Params;
    const auto& params = GetParams();

    auto floatHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };

    auto choiceHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };

    auto boolHelper = [&apvts = this->apvts, &params](auto& param, const auto& paramName)
    {
        param = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(paramName)));
        jassert(param != nullptr);
    };

    floatHelper(lowBandComp.attack,     Names::attack_low_band);
    floatHelper(lowBandComp.release,    Names::release_low_band);
    floatHelper(lowBandComp.threshold,  Names::threshold_low_band);
    
    floatHelper(midBandComp.attack,     Names::attack_mid_band);
    floatHelper(midBandComp.release,    Names::release_mid_band);
    floatHelper(midBandComp.threshold,  Names::threshold_mid_band);

    floatHelper(highBandComp.attack,    Names::attack_high_band);
    floatHelper(highBandComp.release,   Names::release_high_band);
    floatHelper(highBandComp.threshold, Names::threshold_high_band);

    choiceHelper(lowBandComp.ratio,     Names::ratio_low_band);
    choiceHelper(midBandComp.ratio,     Names::ratio_mid_band);
    choiceHelper(highBandComp.ratio,    Names::ratio_high_band);

    boolHelper(lowBandComp.bypassed,    Names::bypassed_low_band);
    boolHelper(midBandComp.bypassed,    Names::bypassed_mid_band);
    boolHelper(highBandComp.bypassed,   Names::bypassed_high_band);

    boolHelper(lowBandComp.mute,        Names::mute_low_band);
    boolHelper(midBandComp.mute,        Names::mute_mid_band);
    boolHelper(highBandComp.mute,       Names::mute_high_band);

    boolHelper(lowBandComp.solo,        Names::solo_low_band);
    boolHelper(midBandComp.solo,        Names::solo_mid_band);
    boolHelper(highBandComp.solo,       Names::solo_high_band);

    floatHelper(lowMidCrossover,        Names::low_mid_crossover_freq);
    floatHelper(midHighCrossover,       Names::mid_high_crossover_freq);

    floatHelper(inputGainParam,         Names::Gain_In);
    floatHelper(outputGainParam,        Names::Gain_Out);

    LP1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    AP2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);

    LP2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    //invAP1.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    //invAP2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);

}

MultiBandCompressorAudioProcessor::~MultiBandCompressorAudioProcessor()
{
}

//==============================================================================
const juce::String MultiBandCompressorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MultiBandCompressorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MultiBandCompressorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MultiBandCompressorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MultiBandCompressorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MultiBandCompressorAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MultiBandCompressorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MultiBandCompressorAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MultiBandCompressorAudioProcessor::getProgramName (int index)
{
    return {};
}

void MultiBandCompressorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MultiBandCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getNumOutputChannels();
    spec.sampleRate = sampleRate;

    for (auto& comp : compressors)
        comp.prepare(spec);

    LP1.prepare(spec);
    HP1.prepare(spec);

    AP2.prepare(spec);

    LP2.prepare(spec);
    HP2.prepare(spec);

  /*  invAP1.prepare(spec);
    invAP2.prepare(spec);

    invAPBuffer.setSize(spec.numChannels, samplesPerBlock);*/

    inputGain.prepare(spec);
    outputGain.prepare(spec);

    inputGain.setRampDurationSeconds(0.05);
    outputGain.setRampDurationSeconds(0.05);

    for (auto& buffer : filterBuffers)
    {
        buffer.setSize(spec.numChannels, samplesPerBlock);
    }
}

void MultiBandCompressorAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MultiBandCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void MultiBandCompressorAudioProcessor::updateState()
{
    for (auto& compressor : compressors)
        compressor.updateCompressorSettings();

    auto lowMidCutOffFreq = lowMidCrossover->get();
    LP1.setCutoffFrequency(lowMidCutOffFreq);
    HP1.setCutoffFrequency(lowMidCutOffFreq);

    auto midHighCutOffFreq = midHighCrossover->get();
    AP2.setCutoffFrequency(midHighCutOffFreq);
    LP2.setCutoffFrequency(midHighCutOffFreq);
    HP2.setCutoffFrequency(midHighCutOffFreq);

    inputGain.setGainDecibels(inputGainParam->get());
    outputGain.setGainDecibels(outputGainParam->get());
}

void MultiBandCompressorAudioProcessor::splitBands(const juce::AudioBuffer<float>& inputBuffer)
{
    for (auto& fb : filterBuffers)
    {
        fb = inputBuffer;
    }

    auto fb0Block = juce::dsp::AudioBlock<float>(filterBuffers[0]);
    auto fb1Block = juce::dsp::AudioBlock<float>(filterBuffers[1]);
    auto fb2Block = juce::dsp::AudioBlock<float>(filterBuffers[2]);

    auto fb0Ctx = juce::dsp::ProcessContextReplacing<float>(fb0Block);
    auto fb1Ctx = juce::dsp::ProcessContextReplacing<float>(fb1Block);
    auto fb2Ctx = juce::dsp::ProcessContextReplacing<float>(fb2Block);

    LP1.process(fb0Ctx);
    AP2.process(fb0Ctx);

    HP1.process(fb1Ctx);
    filterBuffers[2] = filterBuffers[1];
    LP2.process(fb1Ctx);

    HP2.process(fb2Ctx);
}

void MultiBandCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateState();
    
    applyGain(buffer, inputGain);
    
    splitBands(buffer);

    for (size_t i = 0; i < filterBuffers.size(); i++)
    {
        compressors[i].process(filterBuffers[i]);
    }

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    buffer.clear();

    auto addFilterBand = [nc = numChannels, ns = numSamples](auto& inputBuffer, const auto& source)
    {
        for (auto i = 0; i < nc; ++i)
        {
            inputBuffer.addFrom(i, 0, source, i, 0, ns);
        }
    };

    auto bandsAreSolo = false;
    for (auto& comp : compressors)
    {
        if (comp.solo->get())
        {
            bandsAreSolo = true;
            break;
        }
    }

    if (bandsAreSolo)
    {
        for (size_t i = 0; i < compressors.size(); ++i)
        {
            auto& comp = compressors[i];
            if (comp.solo->get())
            {
                addFilterBand(buffer, filterBuffers[i]);
            }
        }   
    }
    else
    {
        for (size_t i = 0; i < compressors.size(); ++i)
        {
            auto& comp = compressors[i];
            if (!comp.mute->get());
            {
                addFilterBand(buffer, filterBuffers[i]);
            }
        }
    }

    applyGain(buffer, outputGain);
}

//==============================================================================
bool MultiBandCompressorAudioProcessor::hasEditor() const
{
    return false;
}

juce::AudioProcessorEditor* MultiBandCompressorAudioProcessor::createEditor()
{
    return new MultiBandCompressorAudioProcessorEditor (*this);
}

//==============================================================================
void MultiBandCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void MultiBandCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiBandCompressorAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout MultiBandCompressorAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    using namespace juce;
    using namespace Params;
    const auto& params = GetParams();

    // THRESHOLD
    auto thresholdRange = NormalisableRange<float>(-60, 12, 1, 1);
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::threshold_low_band),
        params.at(Names::threshold_low_band),
        thresholdRange,
        0
    ));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::threshold_mid_band),
        params.at(Names::threshold_mid_band),
        thresholdRange,
        0
    ));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::threshold_high_band),
        params.at(Names::threshold_high_band),
        thresholdRange,
        0
    ));

    // ATTACK
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::attack_low_band),
        params.at(Names::attack_low_band),
        NormalisableRange<float>(5, 500, 1, 1),
        50
    ));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::attack_mid_band),
        params.at(Names::attack_mid_band),
        NormalisableRange<float>(5, 500, 1, 1),
        50
    ));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::attack_high_band),
        params.at(Names::attack_high_band),
        NormalisableRange<float>(5, 500, 1, 1),
        50
    ));

    // RELEASE
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::release_low_band),
        params.at(Names::release_low_band),
        NormalisableRange<float>(5, 500, 1, 1),
        250
    ));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::release_mid_band),
        params.at(Names::release_mid_band),
        NormalisableRange<float>(5, 500, 1, 1),
        250
    ));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::release_high_band),
        params.at(Names::release_high_band),
        NormalisableRange<float>(5, 500, 1, 1),
        250
    ));

    // RATIO
    auto choices = std::vector<double>{ 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 10, 15, 20, 50, 100 };
    juce::StringArray ratio_choices;
    for (auto choice : choices)
    {
        ratio_choices.add(juce::String(choice, 1));
    }
    layout.add(std::make_unique<AudioParameterChoice>(
        params.at(Names::ratio_low_band),
        params.at(Names::ratio_low_band),
        ratio_choices,
        0
    ));
    layout.add(std::make_unique<AudioParameterChoice>(
        params.at(Names::ratio_mid_band),
        params.at(Names::ratio_mid_band),
        ratio_choices,
        0
    ));
    layout.add(std::make_unique<AudioParameterChoice>(
        params.at(Names::ratio_high_band),
        params.at(Names::ratio_high_band),
        ratio_choices,
        0
    ));

    // BYPASSED
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::bypassed_low_band),
        params.at(Names::bypassed_low_band),
        false
    ));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::bypassed_mid_band),
        params.at(Names::bypassed_mid_band),
        false
    ));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::bypassed_high_band),
        params.at(Names::bypassed_high_band),
        false
    ));

    // MUTE
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::mute_low_band),
        params.at(Names::mute_low_band),
        false
    ));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::mute_mid_band),
        params.at(Names::mute_mid_band),
        false
    ));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::mute_high_band),
        params.at(Names::mute_high_band),
        false
    ));

    // SOLO
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::solo_low_band),
        params.at(Names::solo_low_band),
        false
    ));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::solo_mid_band),
        params.at(Names::solo_mid_band),
        false
    ));
    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::solo_high_band),
        params.at(Names::solo_high_band),
        false
    ));

    // CUT OFF FREQUENCIES
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::low_mid_crossover_freq),
        params.at(Names::low_mid_crossover_freq),
        NormalisableRange<float>(20, 999, 1, 1),
        400
    ));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::mid_high_crossover_freq),
        params.at(Names::mid_high_crossover_freq),
        NormalisableRange<float>(1000, 20000, 1, 1),
        2000
    ));

    // GAIN
    auto gainRange = NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f);
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Gain_In),
        params.at(Names::Gain_In),
        gainRange,
        0
    ));
    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::Gain_Out),
        params.at(Names::Gain_Out),
        gainRange,
        0
    ));


    return layout;
}