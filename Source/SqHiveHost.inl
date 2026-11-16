// Hive Rack's view of the plug-in catalogue and the edition gate (declared in SqColor111d.h). Included at the end of
// Signature.h so it can see Signature::entries() and Edition.
#include "Edition.h"

namespace HiveHost
{
    inline juce::StringArray slotChoices()
    {
        juce::StringArray a { "Empty" };
        for (auto& e : Signature::entries())
        {
            const juce::String cat (e.category), ty (e.type);
            if (cat == "Instruments" || cat.startsWith ("Instrument") || ty == "sqmHiveRack" || ty.startsWith ("sqmRack") || ty == "sqmMothKey" || ty == "midiModifier") continue;
            a.addIfNotAlreadyThere (e.name);
        }
        return a;
    }
    inline bool resolve (const juce::String& name, juce::String& outName, juce::String& outType)
    {
        auto* e = Signature::findByName (name);
        if (e == nullptr) return false;
        outName = e->name; outType = e->type; return true;
    }
    inline juce::String applyPreset (te::Plugin& p, const juce::String& type, const juce::String& presetName)
    {
        const Signature::Entry* e = nullptr;
        for (auto& x : Signature::entries()) if (type == x.type) { e = &x; break; }
        if (e == nullptr || e->presets.empty()) return {};
        if (presetName.isEmpty()) { Signature::applyPreset (p, e->presets.front()); return e->presets.front().name; }
        for (auto& pr : e->presets) if (juce::String (pr.name).equalsIgnoreCase (presetName)) { Signature::applyPreset (p, pr); return pr.name; }
        for (auto& pr : e->presets) if (juce::String (pr.name).containsIgnoreCase (presetName)) { Signature::applyPreset (p, pr); return pr.name; }
        return {};
    }
    inline bool isTypeAllowed (const juce::String& type) { return Edition::isTypeAllowed (type); }
    inline juce::String lockedBy (const juce::String& type) { if (auto pr = Edition::productForType (type)) return pr->name; return {}; }
}
