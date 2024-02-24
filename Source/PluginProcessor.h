#pragma once

#include <JuceHeader.h>

namespace Params
{
    enum Names
    {
        low_mid_crossover_freq,
        mid_high_crossover_freq,

        threshold_low_band,
        threshold_mid_band,
        threshold_high_band,

        attack_low_band,
        attack_mid_band,
        attack_high_band,

        release_low_band,
        release_mid_band,
        release_high_band,

        ratio_low_band,
        ratio_mid_band,
        ratio_high_band,

        bypassed_low_band,
        bypassed_mid_band,
        bypassed_high_band,
    };

    inline const std::map<Names, juce::String>& GetParams()
    {
        static std::map<Names, juce::String> params =
        {
            {low_mid_crossover_freq, "Low-Mid Crossover Freq"},
            {mid_high_crossover_freq, "Mid-High Crossover Freq"},

            {threshold_low_band, "Threshold Low Band"},
            {threshold_mid_band, "Threshold Mid Band"},
            {threshold_high_band, "Threshold High Band"},
            
            {attack_low_band, "Attack Low Band"},
            {attack_mid_band, "Attack Mid Band"},
            {attack_high_band, "Attack High Band"},

            {release_low_band, "Release Low Band"},
            {release_mid_band, "Release Mid Band"},
            {release_high_band, "Release High Band"},

            {ratio_low_band, "Ratio Low Band"},
            {ratio_mid_band, "Ratio Mid Band"},
            {ratio_high_band, "Ratio High Band"},

            {bypassed_low_band, "Bypassed Low Band"},
            {bypassed_mid_band, "Bypassed Mid Band"},
            {bypassed_high_band, "Bypassed High Band"},
        };

        return params;
    }
}

struct CompressorBand
{
    juce::AudioParameterFloat* attack{ nullptr };
    juce::AudioParameterFloat* release{ nullptr };
    juce::AudioParameterFloat* threshold{ nullptr };
    juce::AudioParameterChoice* ratio{ nullptr };
    juce::AudioParameterBool* bypassed{ nullptr };

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        compressor.prepare(spec);
    }

    void updateCompressorSettings()
    {
        compressor.setAttack(attack->get());
        compressor.setRelease(release->get());
        compressor.setThreshold(threshold->get());
        compressor.setRatio(ratio->getCurrentChoiceName().getFloatValue());
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto context = juce::dsp::ProcessContextReplacing<float>(block);

        context.isBypassed = bypassed->get();

        compressor.process(context);
    }

private:
    juce::dsp::Compressor<float> compressor;
};

//==============================================================================
/**
*/
class MultiBandCompressorAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    MultiBandCompressorAudioProcessor();
    ~MultiBandCompressorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout();
    APVTS apvts{ *this, nullptr, "Parameters", createParameterLayout() };

private:
    CompressorBand compressor;

    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    //     fc0  fc1
    Filter LP1, AP2,
           HP1, LP2,
                HP2;

    juce::AudioParameterFloat* lowMidCrossover{ nullptr };
    juce::AudioParameterFloat* midHighCrossover{ nullptr };

    std::array<juce::AudioBuffer<float>, 3> filterBuffers;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiBandCompressorAudioProcessor)
};