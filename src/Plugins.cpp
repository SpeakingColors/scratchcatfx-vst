// Plugin entry point.
#include "fx/ScratchFx.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ScratchFxProcessor(); }
