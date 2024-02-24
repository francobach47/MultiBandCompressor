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

    floatHelper(compressor.attack, Names::attack_low_band);
    floatHelper(compressor.release, Names::release_low_band);
    floatHelper(compressor.threshold, Names::threshold_low_band);
    
    choiceHelper(compressor.ratio, Names::ratio_low_band);

    boolHelper(compressor.bypassed, Names::bypassed_low_band);
    
    floatHelper(lowCrossover, Names::low_mid_crossover_freq);

    LP.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP.setType(juce::dsp::LinkwitzRileyFilterType::highpass);

    AP.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
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

    compressor.prepare(spec);

    LP.prepare(spec);
    HP.prepare(spec);

    AP.prepare(spec);
    apBuffer.setSize(spec.numChannels, samplesPerBlock);

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

    //compressor.updateCompressorSettings();
    //compressor.process(buffer);

    for (auto& fb : filterBuffers)
    {
        fb = buffer;
    }

    auto cutoff = lowCrossover->get();
    LP.setCutoffFrequency(cutoff);
    HP.setCutoffFrequency(cutoff);
    AP.setCutoffFrequency(cutoff);

    auto fb0Block = juce::dsp::AudioBlock<float>(filterBuffers[0]);
    auto fb1Block = juce::dsp::AudioBlock<float>(filterBuffers[1]);

    auto fb0Ctx = juce::dsp::ProcessContextReplacing<float>(fb0Block);
    auto fb1Ctx = juce::dsp::ProcessContextReplacing<float>(fb1Block);

    LP.process(fb0Ctx);
    HP.process(fb1Ctx);

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    //if (compressor.bypassed->get())
    //    return;
    apBuffer = buffer;
    auto apBlock = juce::dsp::AudioBlock<float>(apBuffer);
    auto apCtx = juce::dsp::ProcessContextReplacing<float>(apBlock);
    AP.process(apCtx);

    buffer.clear();

    auto addFilterBand = [nc = numChannels, ns = numSamples](auto& inputBuffer, const auto& source)
    {
        for (auto i = 0; i < nc; ++i)
        {
            inputBuffer.addFrom(i, 0, source, i, 0, ns);
        }
    };

    //if (!compressor.bypassed->get())
    //{
    //    addFilterBand(buffer, filterBuffers[0]);
    //    addFilterBand(buffer, filterBuffers[1]);
    //}
    //else
    //{
    //    addFilterBand(buffer, apBuffer);
    //}

    addFilterBand(buffer, filterBuffers[0]);
    addFilterBand(buffer, filterBuffers[1]);

    if (compressor.bypassed->get())
    {
        for (auto ch = 0; ch < numChannels; ch++)
        {
            juce::FloatVectorOperations::multiply(apBuffer.getWritePointer(ch), -1.f, numSamples);
        }
        
        addFilterBand(buffer, apBuffer);
    }

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

    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::threshold_low_band),
        params.at(Names::threshold_low_band),
        NormalisableRange<float>(-60, 12, 1, 1),
        0
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::attack_low_band),
        params.at(Names::attack_low_band),
        NormalisableRange<float>(5, 500, 1, 1),
        50
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::release_low_band),
        params.at(Names::release_low_band),
        NormalisableRange<float>(5, 500, 1, 1),
        250
    ));

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

    layout.add(std::make_unique<AudioParameterBool>(
        params.at(Names::bypassed_low_band),
        params.at(Names::bypassed_low_band),
        false
    ));

    layout.add(std::make_unique<AudioParameterFloat>(
        params.at(Names::low_mid_crossover_freq),
        params.at(Names::low_mid_crossover_freq),
        NormalisableRange<float>(20, 20000, 1, 1),
        500
    ));

    return layout;
}