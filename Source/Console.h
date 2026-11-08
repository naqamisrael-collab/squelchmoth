#pragma once
// Squelchmoth 1.10 "Console" - the Silver / Fader channel strip language.
//
//  * Console::Look      fixed console palette (chrome, wells, cyan = compressor, pink = limiter, LED level colours)
//  * Console::Map       ONE dB -> position map shared by fader, level meter and scale, so every label sits on its value
//  * Console::Model     per-track strip width / meter type, saved mixer layouts, the COMP and LIMIT slots, live GR
//  * widgets            segmented LED level meter, GR meters, VU dials, width selector, dynamics selector, scales
//
// Gain reduction is only ever shown when the plug-in reports it (Cocoon via the Tracktion patch, Apex, Opto).
// Anything else shows "n/a" - the console never invents a number.

#include "Session.h"
#include "Theme.h"
#include "Edition.h"
#include "Signature.h"
#include "TrackOps.h"

namespace Console
{
    //==========================================================================
    namespace Look
    {
        inline juce::Colour cyan()      { return juce::Colour (0xff38e1ff); }
        inline juce::Colour pink()      { return juce::Colour (0xffff3d9a); }
        inline juce::Colour well()      { return juce::Colour (0xff05080c); }
        inline juce::Colour bodyTop()   { return juce::Colour (0xff141c27); }
        inline juce::Colour bodyBot()   { return juce::Colour (0xff080c12); }
        inline juce::Colour text()      { return juce::Colour (0xffe9f1f8); }
        inline juce::Colour dim()       { return juce::Colour (0xff8796a8); }
        inline juce::Colour faint()     { return juce::Colour (0xff3a4656); }
        inline juce::Colour green()     { return juce::Colour (0xff2bff88); }
        inline juce::Colour yellow()    { return juce::Colour (0xffffe23a); }
        inline juce::Colour orange()    { return juce::Colour (0xffff9a2a); }
        inline juce::Colour red()       { return juce::Colour (0xffff3b3b); }
        inline juce::Colour live()      { return juce::Colour (0xff35f08a); }

        /** LED colour for a level: green < -12, yellow -12..-3, orange -3..0, red > 0 dBFS. */
        inline juce::Colour ledFor (float db)
        {
            if (db > 0.0f)   return red();
            if (db > -3.0f)  return orange();
            if (db > -12.0f) return yellow();
            return green();
        }

        inline juce::Font caps (float h, bool bold = true)    { return Theme::font (h, bold).withExtraKerningFactor (0.08f); }

        /** Brushed-chrome rounded body with an inner shadow and an optional cyan rim glow. */
        inline void drawChromePanel (juce::Graphics& g, juce::Rectangle<float> r, float radius, float glow, juce::Colour rim = cyan())
        {
            if (glow > 0.0f)
                for (int i = 3; i >= 1; --i)
                {
                    g.setColour (rim.withAlpha (glow * 0.10f * (float) (4 - i)));
                    g.drawRoundedRectangle (r.expanded ((float) i * 0.9f), radius + (float) i, 1.6f);
                }
            juce::ColourGradient chrome (juce::Colour (0xffdfe7ef), r.getX(), r.getY(), juce::Colour (0xff5d6876), r.getX(), r.getBottom(), false);
            chrome.addColour (0.08, juce::Colour (0xff9aa6b4));
            chrome.addColour (0.55, juce::Colour (0xff3b4552));
            chrome.addColour (0.94, juce::Colour (0xff8d99a7));
            g.setGradientFill (chrome);
            g.fillRoundedRectangle (r, radius);
            auto inner = r.reduced (2.2f);
            g.setGradientFill (juce::ColourGradient (bodyTop(), inner.getX(), inner.getY(), bodyBot(), inner.getX(), inner.getBottom(), false));
            g.fillRoundedRectangle (inner, radius - 1.5f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawRoundedRectangle (inner, radius - 1.5f, 1.0f);
            if (glow > 0.0f)
            {
                g.setColour (rim.withAlpha (juce::jlimit (0.0f, 1.0f, glow)));
                g.drawRoundedRectangle (r.reduced (0.6f), radius, 1.5f);
            }
        }

        /** Recessed well (meters, fader track, readouts). */
        inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius = 4.0f)
        {
            g.setColour (well());
            g.fillRoundedRectangle (r, radius);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + radius, r.getRight() - radius);
        }

        /** Rounded outlined pill (selectors, buttons). */
        inline void drawPill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour outline, bool on, bool hover)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (on ? 0xff16303a : 0xff111821), r.getX(), r.getY(),
                                                     juce::Colour (on ? 0xff0a1a22 : 0xff090d13), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (outline.withAlpha (on ? 1.0f : (hover ? 0.85f : 0.6f)));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, on ? 1.5f : 1.2f);
        }

        inline void drawChevron (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
        {
            juce::Path p;
            auto cx = r.getCentreX(), cy = r.getCentreY();
            p.startNewSubPath (cx - 3.5f, cy - 1.5f); p.lineTo (cx, cy + 2.0f); p.lineTo (cx + 3.5f, cy - 1.5f);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    //==========================================================================
    /** The console's dB map. +12..+6 dB -> top 8 %, +6..-24 dB -> 76 % (2.53 % per dB, where mixing happens),
        -24..-70 dB -> bottom 16 %, below -70 dB = -inf. Fader travel, meter fill and every scale label use it. */
    namespace Map
    {
        constexpr float top = 12.0f, knee1 = 6.0f, knee2 = -24.0f, floorDb = -70.0f;
        constexpr float p1 = 0.92f, p2 = 0.16f;

        inline float dbToProp (float db)
        {
            if (db >= knee1) return juce::jmin (1.0f, p1 + (db - knee1) / (top - knee1) * (1.0f - p1));
            if (db >= knee2) return p2 + (db - knee2) / (knee1 - knee2) * (p1 - p2);
            if (db > floorDb) return (db - floorDb) / (knee2 - floorDb) * p2;
            return 0.0f;
        }
        inline float propToDb (float p)
        {
            p = juce::jlimit (0.0f, 1.0f, p);
            if (p >= p1) return knee1 + (p - p1) / (1.0f - p1) * (top - knee1);
            if (p >= p2) return knee2 + (p - p2) / (p1 - p2) * (knee1 - knee2);
            return floorDb + p / p2 * (knee2 - floorDb);
        }
        inline juce::NormalisableRange<double> faderRange()
        {
            return { (double) floorDb, (double) top,
                     [] (double, double, double p) { return (double) propToDb ((float) p); },
                     [] (double, double, double v) { return (double) dbToProp ((float) v); },
                     [] (double, double, double v) { return juce::jlimit ((double) floorDb, (double) top, std::round (v * 10.0) / 10.0); } };
        }
        /** GR meters: 0 dB at the top, -24 dB at the bottom, linear. */
        inline float grToProp (float grDb)   { return juce::jlimit (0.0f, 1.0f, -grDb / 24.0f); }
    }

    //==========================================================================
    enum class Width { compact = 0, standard = 1, expanded = 2 };
    enum class MeterType { peak = 0, rms = 1, lufsM = 2, lufsS = 3 };
    enum class View { dynamics = 0, full = 1, overview = 2 };
    enum class Layout { flexible = 0, allCompact = 1, allStandard = 2, allExpanded = 3 };

    inline juce::StringArray widthNames()      { return { "Compact", "Standard", "Expanded" }; }
    inline juce::StringArray meterTypeNames()  { return { "Peak", "Level (RMS)", "LUFS Momentary", "LUFS Short-term" }; }
    inline juce::StringArray viewNames()       { return { "Dynamics", "Full", "Overview" }; }
    inline juce::StringArray layoutNames()     { return { "Flexible", "All Compact", "All Standard", "All Expanded" }; }

    inline int widthPixels (Width w)           { return w == Width::compact ? 100 : (w == Width::standard ? 168 : 312); }

    //==========================================================================
    namespace Model
    {
        static const juce::Identifier widthId ("sqmStripWidth"), meterId ("sqmMeterType"), roleId ("sqmDynRole"),
                                      masterWidthId ("sqmMasterStripWidth"), masterMeterId ("sqmMasterMeterType"),
                                      layoutsId ("SQMMIXLAYOUTS"), layoutId ("LAYOUT");

        inline View view()                 { return Prefs::getBool ("mx_narrow", false) ? View::overview : (View) juce::jlimit (0, 2, Prefs::getInt ("mx_view", 0)); }
        inline Layout layout()             { return (Layout) juce::jlimit (0, 3, Prefs::getInt ("mx_layout", 0)); }
        inline void setView (View v)       { if (Prefs::getBool ("mx_narrow", false)) Prefs::set ("mx_narrow", false); Prefs::set ("mx_view", (int) v); }
        inline void setLayout (Layout l)   { Prefs::set ("mx_layout", (int) l); }

        /** What the strip stores (Flexible layout uses it). */
        inline Width storedWidth (Session& s, te::Track* row)
        {
            if (row == nullptr) return s.edit != nullptr ? (Width) juce::jlimit (0, 2, (int) s.edit->state.getProperty (masterWidthId, 1)) : Width::standard;
            return (Width) juce::jlimit (0, 2, (int) row->state.getProperty (widthId, 1));
        }
        /** What is drawn: Overview forces Compact, the All-* layouts override every strip. */
        inline Width effectiveWidth (Session& s, te::Track* row)
        {
            if (view() == View::overview) return Width::compact;
            switch (layout())
            {
                case Layout::allCompact:  return Width::compact;
                case Layout::allStandard: return Width::standard;
                case Layout::allExpanded: return Width::expanded;
                default: break;
            }
            return storedWidth (s, row);
        }
        inline void setWidth (Session& s, te::Track* row, Width w)
        {
            if (row == nullptr) { if (s.edit != nullptr) s.edit->state.setProperty (masterWidthId, (int) w, nullptr); }
            else row->state.setProperty (widthId, (int) w, nullptr);
            if (layout() != Layout::flexible) setLayout (Layout::flexible);   // choosing a width on a strip means "per strip"
        }
        inline MeterType meterType (Session& s, te::Track* row)
        {
            if (row == nullptr) return s.edit != nullptr ? (MeterType) juce::jlimit (0, 3, (int) s.edit->state.getProperty (masterMeterId, 3)) : MeterType::lufsS;
            return (MeterType) juce::jlimit (0, 3, (int) row->state.getProperty (meterId, 0));
        }
        inline void setMeterType (Session& s, te::Track* row, MeterType m)
        {
            if (row == nullptr) { if (s.edit != nullptr) s.edit->state.setProperty (masterMeterId, (int) m, nullptr); }
            else row->state.setProperty (meterId, (int) m, nullptr);
        }

        //======================================================================
        // Saved mixer layouts (FL wish: saved mixer views). Stored in the session: name, view, layout and every width.
        inline juce::ValueTree layoutsTree (Session& s, bool create)
        {
            if (s.edit == nullptr) return {};
            auto t = s.edit->state.getChildWithName (layoutsId);
            if (! t.isValid() && create) { t = juce::ValueTree (layoutsId); s.edit->state.addChild (t, -1, nullptr); }
            return t;
        }
        inline juce::StringArray layoutList (Session& s)
        {
            juce::StringArray out;
            auto t = layoutsTree (s, false);
            for (auto c : t) out.add (c.getProperty ("name").toString());
            return out;
        }
        inline void saveLayout (Session& s, const juce::String& name)
        {
            auto t = layoutsTree (s, true);
            if (! t.isValid()) return;
            for (int i = t.getNumChildren(); --i >= 0;) if (t.getChild (i).getProperty ("name").toString() == name) t.removeChild (i, nullptr);
            juce::ValueTree l (layoutId);
            l.setProperty ("name", name, nullptr).setProperty ("view", (int) view(), nullptr).setProperty ("layout", (int) layout(), nullptr);
            juce::StringArray widths;
            for (auto r : s.getRows()) widths.add (r->itemID.toString() + ":" + juce::String ((int) storedWidth (s, r)) + ":" + juce::String ((int) meterType (s, r)));
            l.setProperty ("widths", widths.joinIntoString (","), nullptr);
            l.setProperty ("master", (int) storedWidth (s, nullptr), nullptr);
            t.addChild (l, -1, nullptr);
        }
        inline bool recallLayout (Session& s, const juce::String& name)
        {
            auto t = layoutsTree (s, false);
            for (auto l : t)
                if (l.getProperty ("name").toString() == name)
                {
                    auto items = juce::StringArray::fromTokens (l.getProperty ("widths").toString(), ",", {});
                    for (auto r : s.getRows())
                        for (auto& it : items)
                        {
                            auto parts = juce::StringArray::fromTokens (it, ":", {});
                            if (parts.size() >= 2 && parts[0] == r->itemID.toString())
                            {
                                r->state.setProperty (widthId, juce::jlimit (0, 2, parts[1].getIntValue()), nullptr);
                                if (parts.size() >= 3) r->state.setProperty (meterId, juce::jlimit (0, 3, parts[2].getIntValue()), nullptr);
                            }
                        }
                    if (s.edit != nullptr) s.edit->state.setProperty (masterWidthId, (int) l.getProperty ("master", 1), nullptr);
                    setLayout ((Layout) juce::jlimit (0, 3, (int) l.getProperty ("layout", 0)));
                    setView ((View) juce::jlimit (0, 2, (int) l.getProperty ("view", 0)));
                    return true;
                }
            return false;
        }
        inline bool deleteLayout (Session& s, const juce::String& name)
        {
            auto t = layoutsTree (s, false);
            for (int i = t.getNumChildren(); --i >= 0;) if (t.getChild (i).getProperty ("name").toString() == name) { t.removeChild (i, nullptr); return true; }
            return false;
        }

        //======================================================================
        // COMP / LIMIT slots, with our own plug-ins only.
        struct Choice { const char* type; const char* name; bool limitRole; };
        inline const std::vector<Choice>& compChoices()
        {
            static const std::vector<Choice> c {
                { "compressor", "Cocoon Compressor", false }, { "sqmApexDyn", "Apex Dynamics", false }, { "sqmOpto", "Opto Leveler", false },
                { "sqmApexMB", "Apex Multiband", false }, { "airwindows_pressure4", "Moth Press", false }, { "airwindows_buttercomp2", "Pupa Buss Comp", false },
                { "airwindows_varimu", "Vari-Mu Tube", false } };
            return c;
        }
        inline const std::vector<Choice>& limitChoices()
        {
            static const std::vector<Choice> c {
                { "sqmApexLim", "Apex Limiter", true }, { "compressor", "Cocoon Limit", true }, { "airwindows_adclip7", "Clamp Limiter", true },
                { "sqmApexTriad", "Apex Triad", true } };
            return c;
        }
        inline bool isLimitTypeByNature (const juce::String& type)   { return type == "sqmApexLim" || type == "airwindows_adclip7" || type == "sqmApexTriad"; }

        inline juce::String typeOf (te::Plugin& p)   { return p.getPluginType(); }

        /** The plug-in in this list playing the role (first match among the inserts). */
        inline te::Plugin* findDynamics (te::PluginList& list, te::AudioTrack* track, bool limit)
        {
            for (auto p : Session::getInserts (list, track))
            {
                auto type = typeOf (*p);
                const auto role = p->state.getProperty (roleId).toString();
                if (limit)
                {
                    if (role == "limit" || (role.isEmpty() && isLimitTypeByNature (type))) return p;
                }
                else
                {
                    if (role == "limit") continue;
                    for (auto& c : compChoices()) if (type == c.type) return p;
                }
            }
            return nullptr;
        }

        /** Label for the selector ("Cocoon Limit" for a compressor in the limit role). */
        inline juce::String labelFor (te::Plugin* p, bool limit)
        {
            if (p == nullptr) return "None";
            if (limit && typeOf (*p) == "compressor") return "Cocoon Limit";
            return Signature::displayName (*p);
        }
        /** The short form a Standard strip's selector shows (the full name is in Expanded and the tooltip). */
        inline juce::String shortLabel (const juce::String& full)
        {
            static const std::map<juce::String, juce::String> m {
                { "Cocoon Compressor", "Cocoon" }, { "Apex Dynamics", "Apex Dyn" }, { "Opto Leveler", "Opto" }, { "Apex Multiband", "Apex MB" },
                { "Moth Press", "Press" }, { "Pupa Buss Comp", "Pupa Buss" }, { "Vari-Mu Tube", "Vari-Mu" }, { "Apex Limiter", "Apex Lim" },
                { "Cocoon Limit", "Cocoon" }, { "Clamp Limiter", "Clamp" }, { "Apex Triad", "Triad" } };
            auto it = m.find (full);
            return it != m.end() ? it->second : full;
        }

        /** Live gain reduction in dB (<= 0), or nullopt when the plug-in does not report it. */
        inline std::optional<float> gainReduction (te::Plugin* p)
        {
            if (p == nullptr) return std::nullopt;
            auto neg = [] (float v) { return -std::abs (v); };
            if (! p->isEnabled()) return 0.0f;
            if (auto c = dynamic_cast<te::CompressorPlugin*> (p))   return juce::jmin (0.0f, c->gainReductionDb.load (std::memory_order_relaxed));
            if (auto t = dynamic_cast<SqApexTriadPlugin*> (p))      return neg (t->limiterGrDb.load());
            if (auto mb = dynamic_cast<SqApexMultibandPlugin*> (p))
            {
                float deepest = 0.0f;
                for (int b = 0; b < mb->numBands.get(); ++b) deepest = juce::jmin (deepest, mb->bandGr[(size_t) b].load());
                return deepest;
            }
            if (auto a = dynamic_cast<SqApexBase*> (p))             return juce::jmin (0.0f, a->grDb.load());
            if (auto o = dynamic_cast<SqOptoPlugin*> (p))           return juce::jmin (0.0f, o->grDb.load());
            return std::nullopt;
        }

        inline void setParam (te::Plugin& p, const char* id, float v)
        {
            if (auto ap = p.getAutomatableParameterByID (id)) ap->setParameter (v, juce::sendNotification);
        }

        /** Put `typeName` in the COMP or LIMIT slot ("" = remove). Replaces in place, otherwise inserts: COMP before the
            limiter, LIMIT last before the fader. Returns an error for locked or unknown types. */
        inline juce::String setDynamics (Session& s, te::Track* row, bool limit, const juce::String& choiceName)
        {
            if (s.edit == nullptr) return "No session";
            auto& list = row != nullptr ? row->pluginList : s.edit->getMasterPluginList();
            auto* track = dynamic_cast<te::AudioTrack*> (row);
            auto existing = findDynamics (list, track, limit);

            if (choiceName.isEmpty() || choiceName.equalsIgnoreCase ("none"))
            {
                if (existing != nullptr) existing->deleteFromParent();
                return {};
            }
            const Choice* ch = nullptr;
            for (auto& c : (limit ? limitChoices() : compChoices())) if (choiceName.equalsIgnoreCase (c.name) || choiceName == c.type) ch = &c;
            if (ch == nullptr) return "Unknown " + juce::String (limit ? "limiter" : "compressor") + ": " + choiceName;
            if (! Edition::isTypeAllowed (ch->type)) return juce::String (ch->name) + " is locked: " + Edition::lockReason (ch->type);

            juce::String err;
            // Session::createPluginByName resolves our display names (type IDs only for built-ins), so pass the name
            auto p = s.createPluginByName (juce::String (ch->name) == "Cocoon Limit" ? juce::String ("Cocoon Compressor") : juce::String (ch->name), err);
            if (p == nullptr) return err.isNotEmpty() ? err : "Could not create " + juce::String (ch->name);
            p->state.setProperty (roleId, limit ? "limit" : "comp", nullptr);

            int idx;
            if (existing != nullptr) { idx = list.indexOf (existing); existing->deleteFromParent(); }
            else if (! limit)
            {
                auto lim = findDynamics (list, track, true);
                idx = lim != nullptr ? list.indexOf (lim) : Session::insertEndIndex (list, row);
            }
            else idx = Session::insertEndIndex (list, row);
            list.insertPlugin (p, idx, nullptr);

            if (limit && juce::String (ch->type) == "compressor")
            {   // Cocoon Limit: infinite ratio, fastest attack, short release, ceiling at -1 dBFS
                setParam (*p, "ratio", 0.0f);
                setParam (*p, "attack", 0.3f);
                setParam (*p, "release", 50.0f);
                setParam (*p, "threshold", juce::Decibels::decibelsToGain (-1.0f));
            }
            s.log.add ("You", juce::String (limit ? "LIMIT" : "COMP") + " slot on " + (row != nullptr ? row->getName() : juce::String ("Master")) + ": " + ch->name);
            return {};
        }

        //======================================================================
        // The strip tap (RMS / LUFS meters). Tracks: post-fader, just before the level meter. Master: last in the
        // master chain; the master fader is applied after the chain, so readings add the fader's dB (exact at centre pan).
        inline SqStripTapPlugin* findTap (te::PluginList& list)
        {
            for (auto p : list.getPlugins()) if (auto t = dynamic_cast<SqStripTapPlugin*> (p)) return t;
            return nullptr;
        }
        inline SqStripTapPlugin* ensureTap (Session& s, te::Track* row)
        {
            if (s.edit == nullptr) return nullptr;
            auto& list = row != nullptr ? row->pluginList : s.edit->getMasterPluginList();
            if (auto t = findTap (list))
            {
                if (row == nullptr)   // keep it last in the printed master chain (Control Room inserts come after it)
                    if (auto parent = t->state.getParent(); parent.isValid())
                    {
                        int firstMonitor = -1;
                        for (int i = 0; i < parent.getNumChildren(); ++i) if ((bool) parent.getChild (i).getProperty ("sqmMonitorOnly", false)) { firstMonitor = i; break; }
                        const int cur = parent.indexOf (t->state);
                        const int want = firstMonitor < 0 ? parent.getNumChildren() - 1 : (cur < firstMonitor ? firstMonitor - 1 : firstMonitor);
                        if (cur != want) parent.moveChild (cur, want, nullptr);
                    }
                return t;
            }
            // Added straight to the model without the undo manager: a meter is not an edit the user should undo.
            juce::ValueTree parent, before;
            if (auto at = dynamic_cast<te::AudioTrack*> (row))
            {
                auto m = at->getLevelMeterPlugin();
                if (m == nullptr) return nullptr;
                before = m->state; parent = before.getParent();
            }
            else if (row == nullptr)
            {
                parent = s.edit->state.getChildWithName (te::IDs::MASTERPLUGINS);
                if (! parent.isValid()) { parent = juce::ValueTree (te::IDs::MASTERPLUGINS); s.edit->state.addChild (parent, -1, nullptr); }
            }
            else return nullptr;   // folders and VCAs meter with their peak meter only
            if (! parent.isValid()) return nullptr;
            auto p = s.edit->getPluginCache().createNewPlugin (SqStripTapPlugin::xmlTypeName, {});
            if (p == nullptr) return nullptr;
            parent.addChild (p->state, before.isValid() ? parent.indexOf (before) : -1, nullptr);
            return findTap (list);
        }
        inline bool canTap (te::Track* row)   { return row == nullptr || dynamic_cast<te::AudioTrack*> (row) != nullptr; }
    }

    //==========================================================================
    // Widgets
    //==========================================================================

    /** Segmented stereo LED meter (+6 .. -24 dB in the main zone, Console::Map), peak-hold ticks and a clip LED. */
    class LedMeter : public juce::Component
    {
    public:
        void setLevels (float l, float r, float holdL, float holdR, bool clipped)
        {
            if (std::abs (l - lv[0]) < 0.05f && std::abs (r - lv[1]) < 0.05f && holdL == hold[0] && holdR == hold[1] && clipped == clip) return;
            lv[0] = l; lv[1] = r; hold[0] = holdL; hold[1] = holdR; clip = clipped;
            repaint();
        }
        bool stereo = true;
        std::function<void()> onClick;
        void mouseDown (const juce::MouseEvent&) override   { if (onClick) onClick(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto led = r.removeFromTop (4.0f).reduced (1.0f, 0.0f);
            g.setColour (clip ? Look::red() : Look::faint().withAlpha (0.6f));
            g.fillRoundedRectangle (led, 1.0f);
            r.removeFromTop (2.0f);
            const int n = stereo ? 2 : 1;
            const float gap = 2.0f, w = (r.getWidth() - gap * (float) (n - 1)) / (float) n;
            const float pitch = 3.0f;   // 2 px LED + 1 px gap
            for (int c = 0; c < n; ++c)
            {
                auto bar = juce::Rectangle<float> (r.getX() + (float) c * (w + gap), r.getY(), w, r.getHeight());
                const float litTop = bar.getBottom() - bar.getHeight() * Map::dbToProp (lv[c]);
                for (float y = bar.getBottom() - 2.0f; y >= bar.getY(); y -= pitch)
                {
                    const float db = Map::propToDb ((bar.getBottom() - (y + 1.0f)) / bar.getHeight());
                    const bool lit = y + 1.0f >= litTop;
                    g.setColour (lit ? Look::ledFor (db) : Look::ledFor (db).withAlpha (0.10f));
                    g.fillRect (bar.getX(), y, bar.getWidth(), 2.0f);
                }
                if (hold[c] > -69.0f)
                {
                    auto py = bar.getBottom() - bar.getHeight() * Map::dbToProp (hold[c]);
                    g.setColour (Look::ledFor (hold[c]).brighter (0.4f));
                    g.fillRect (bar.getX(), py - 1.0f, bar.getWidth(), 2.0f);
                }
            }
        }
    private:
        float lv[2] = { -100, -100 }, hold[2] = { -100, -100 };
        bool clip = false;
    };

    /** Gain-reduction bar hanging from 0 dB (top) to -24 dB (bottom). Dimmed with "n/a" when unavailable. */
    class GrMeter : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit GrMeter (juce::Colour c) : colour (c) {}
        void setGr (std::optional<float> gr)
        {
            const bool av = gr.has_value();
            const float v = av ? *gr : 0.0f;
            if (av == available && std::abs (v - value) < 0.05f) return;
            available = av; value = v; peak = juce::jmin (peak, v);
            repaint();
        }
        void resetPeak()   { peak = 0.0f; repaint(); }
        bool segmented = true;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            Look::drawWell (g, r, 3.0f);
            auto inner = r.reduced (2.0f, 2.0f);
            if (! available)
            {
                g.setColour (Look::faint());
                g.setFont (Look::caps (8.0f));
                g.drawText ("n/a", inner.withHeight (14.0f), juce::Justification::centred);
                return;
            }
            const float bottom = inner.getY() + inner.getHeight() * Map::grToProp (value);
            if (segmented)
            {
                for (float y = inner.getY(); y + 2.0f <= inner.getBottom(); y += 3.0f)
                {
                    const bool lit = y + 1.0f <= bottom && value < -0.05f;
                    g.setColour (lit ? colour : colour.withAlpha (0.09f));
                    g.fillRect (inner.getX(), y, inner.getWidth(), 2.0f);
                }
            }
            else
            {
                g.setColour (colour.withAlpha (0.10f));
                g.fillRoundedRectangle (inner, 2.0f);
                if (value < -0.05f)
                {
                    juce::ColourGradient gr (colour.brighter (0.3f), inner.getX(), inner.getY(), colour.darker (0.4f), inner.getX(), bottom, false);
                    g.setGradientFill (gr);
                    g.fillRoundedRectangle (inner.withBottom (bottom), 2.0f);
                }
            }
            if (peak < -0.2f)
            {
                g.setColour (colour.brighter (0.6f));
                g.fillRect (inner.getX(), inner.getY() + inner.getHeight() * Map::grToProp (peak) - 1.0f, inner.getWidth(), 1.5f);
            }
        }
        void mouseDown (const juce::MouseEvent&) override   { resetPeak(); }

        float value = 0.0f, peak = 0.0f;
        bool available = false;
    private:
        juce::Colour colour;
    };

    /** Analog-style dial with a needle (Compact strips: COMP GR and LIMIT GR as VU needles). */
    class VuDial : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit VuDial (juce::Colour c) : accent (c) {}
        void setGr (std::optional<float> gr)
        {
            const float target = gr.has_value() ? *gr : 0.0f;
            if (gr.has_value() == available && std::abs (target - value) < 0.05f) return;
            available = gr.has_value(); value = target; repaint();
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            const float side = juce::jmin (r.getWidth(), r.getHeight());
            auto d = r.withSizeKeepingCentre (side, side);
            juce::ColourGradient bezel (juce::Colour (0xffe6edf4), d.getX(), d.getY(), juce::Colour (0xff4a5562), d.getX(), d.getBottom(), false);
            g.setGradientFill (bezel);
            g.fillEllipse (d);
            auto face = d.reduced (2.0f);
            juce::ColourGradient amberFace (juce::Colour (0xfff2b65a), face.getCentreX(), face.getY(), juce::Colour (0xff6b3d10), face.getCentreX(), face.getBottom(), false);
            g.setGradientFill (amberFace);
            g.fillEllipse (face);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawEllipse (face, 1.0f);
            // scale: 0 at the right end, -24 at the left
            const float a0 = juce::MathConstants<float>::pi * -0.75f, a1 = juce::MathConstants<float>::pi * 0.25f;
            auto c = face.getCentre().translated (0.0f, face.getHeight() * 0.18f);
            const float rad = face.getWidth() * 0.46f;
            g.setColour (juce::Colour (0xff2a1404).withAlpha (0.8f));
            for (int i = 0; i <= 8; ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / 8.0f;
                auto p1 = c.getPointOnCircumference (rad, a), p2 = c.getPointOnCircumference (rad * (i % 4 == 0 ? 0.74f : 0.84f), a);
                g.drawLine ({ p1, p2 }, 1.0f);
            }
            g.setColour (Look::red().withAlpha (0.8f));
            g.drawLine ({ c.getPointOnCircumference (rad, a1 - 0.18f), c.getPointOnCircumference (rad * 0.86f, a1 - 0.18f) }, 1.6f);
            const float prop = available ? 1.0f - Map::grToProp (value) : 1.0f;
            const float a = a0 + (a1 - a0) * prop;
            g.setColour (available ? juce::Colour (0xff140a02) : juce::Colour (0xff140a02).withAlpha (0.35f));
            g.drawLine ({ c, c.getPointOnCircumference (rad * 0.98f, a) }, 1.4f);
            g.setColour (accent);
            g.fillEllipse (c.x - 2.0f, c.y - 2.0f, 4.0f, 4.0f);
        }
    private:
        juce::Colour accent;
        float value = 0.0f;
        bool available = false;
    };

    /** COMPACT / STANDARD / EXPANDED selector: three tiny glyph buttons (one bar, two bars, three bars). */
    class WidthSelector : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void (Width)> onChange;
        Width current = Width::standard;
        void setWidth (Width w)   { if (w != current) { current = w; repaint(); } }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            Look::drawWell (g, r, 6.0f);
            const float cw = r.getWidth() / 3.0f;
            for (int i = 0; i < 3; ++i)
            {
                auto cell = juce::Rectangle<float> (r.getX() + (float) i * cw, r.getY(), cw, r.getHeight()).reduced (2.0f);
                const bool on = (int) current == i, hov = hoverIdx == i;
                if (on)
                {
                    g.setColour (Look::cyan().withAlpha (0.18f));
                    g.fillRoundedRectangle (cell, 4.0f);
                    g.setColour (Look::cyan());
                    g.drawRoundedRectangle (cell, 4.0f, 1.2f);
                }
                const int bars = i + 1;
                const float bw = 2.0f, gap = 2.0f, total = (float) bars * bw + (float) (bars - 1) * gap;
                auto glyph = cell.withSizeKeepingCentre (total + 4.0f, cell.getHeight() * 0.58f);
                g.setColour (on ? Look::cyan() : (hov ? Look::text() : Look::dim()));
                g.drawRoundedRectangle (glyph, 1.5f, 1.0f);
                for (int b = 0; b < bars; ++b)
                    g.fillRect (glyph.getX() + 2.0f + (float) b * (bw + gap), glyph.getY() + 2.0f, bw, glyph.getHeight() - 4.0f);
            }
        }
        void mouseMove (const juce::MouseEvent& e) override   { auto i = juce::jlimit (0, 2, (int) (e.x * 3 / juce::jmax (1, getWidth()))); if (i != hoverIdx) { hoverIdx = i; repaint(); } }
        void mouseExit (const juce::MouseEvent&) override     { hoverIdx = -1; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! getLocalBounds().contains (e.getPosition())) return;
            auto w = (Width) juce::jlimit (0, 2, (int) (e.x * 3 / juce::jmax (1, getWidth())));
            setWidth (w);
            if (onChange) onChange (w);
        }
    private:
        int hoverIdx = -1;
    };

    /** Outlined dropdown pill: caption-coloured outline, label, chevron. Used for COMP (cyan), LIMIT (pink), METER TYPE. */
    class Selector : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Selector (juce::Colour c) : outline (c) {}
        std::function<void()> onClick;
        juce::String text;
        bool locked = false, active = false;
        juce::Colour outline;
        void setText (const juce::String& t, bool isActive)   { if (t != text || isActive != active) { text = t; active = isActive; repaint(); } }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            Look::drawPill (g, r, outline, active, isMouseOver());
            auto chev = r.removeFromRight (16.0f);
            Look::drawChevron (g, chev, active ? outline : Look::dim());
            g.setColour (active ? Look::text() : Look::dim());
            g.setFont (Theme::font (r.getHeight() > 22.0f ? 12.5f : 11.5f, true));
            g.drawFittedText (text, r.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 1.0f);
        }
        void mouseEnter (const juce::MouseEvent&) override   { repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { repaint(); }
        void mouseUp (const juce::MouseEvent& e) override    { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    };

    /** Small square console button (M, S, Ø, Mono, R, I) with a lit state. */
    class KeyButton : public juce::Button
    {
    public:
        KeyButton (const juce::String& label, juce::Colour onColour) : juce::Button (label), lit (onColour) {}
        void paintButton (juce::Graphics& g, bool hover, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const bool on = getToggleState();
            g.setGradientFill (juce::ColourGradient (on ? lit.withAlpha (0.9f) : juce::Colour (down ? 0xff0c1118 : 0xff1a222c), r.getX(), r.getY(),
                                                     on ? lit.darker (0.5f) : juce::Colour (0xff0b1016), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (on ? lit.brighter (0.3f) : juce::Colour (hover ? 0xff8d9aab : 0xff56626f));
            g.drawRoundedRectangle (r, 5.0f, 1.1f);
            g.setColour (on ? juce::Colours::black.withAlpha (0.85f) : Look::text());
            if (getButtonText() == "mono")
            {   // one ring with a centre bar: a single (summed) channel
                auto c = r.withSizeKeepingCentre (r.getHeight() * 0.5f, r.getHeight() * 0.5f);
                g.drawEllipse (c, 1.4f);
                g.fillRect (c.getCentreX() - 0.7f, c.getY() + 2.0f, 1.4f, c.getHeight() - 4.0f);
                return;
            }
            g.setFont (Theme::font (juce::jmin (13.0f, r.getHeight() * 0.62f), true));
            g.drawText (getButtonText(), r, juce::Justification::centred);
        }
    private:
        juce::Colour lit;
    };

    /** Vertical tick scale (level: +6 0 -6 -12 -18 -24, dB foot; GR: 0..-24 every 3 dB when it fits). */
    class Scale : public juce::Component
    {
    public:
        enum Kind { level, gr };
        Kind kind = level;
        juce::Justification just = juce::Justification::centredRight;
        juce::Rectangle<int> meterArea;   // in parent coordinates: the span the scale annotates
        void paint (juce::Graphics& g) override
        {
            if (meterArea.isEmpty()) return;
            const float top = (float) (meterArea.getY() - getY()), h = (float) meterArea.getHeight();
            g.setFont (Theme::font (9.5f));
            auto drawTick = [&] (float y, const juce::String& t, bool major)
            {
                g.setColour (major ? Look::dim() : Look::faint());
                const bool right = just == juce::Justification::centredRight;
                g.fillRect (right ? (float) getWidth() - 3.0f : 0.0f, y, 3.0f, 1.0f);
                if (t.isNotEmpty())
                {
                    g.setColour (t == "0" ? Look::text() : Look::dim());
                    g.drawText (t, juce::Rectangle<float> (right ? 0.0f : 4.0f, y - 6.0f, (float) getWidth() - 5.0f, 12.0f), just, false);
                }
            };
            if (kind == level)
            {
                for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -40.0f })
                    drawTick (top + h * (1.0f - Map::dbToProp (db)), db > 0 ? "+" + juce::String ((int) db) : juce::String ((int) db), true);
                for (float db = 3.0f; db >= -21.0f; db -= 3.0f)
                    if (std::fmod (std::abs (db), 6.0f) > 0.5f) drawTick (top + h * (1.0f - Map::dbToProp (db)), {}, false);
            }
            else
            {
                const int step = h >= 9 * 13.0f ? 3 : (h >= 5 * 13.0f ? 6 : 12);
                for (int db = 0; db >= -24; db -= 3)
                    drawTick (top + h * Map::grToProp ((float) db), db % step == 0 ? juce::String (db) : juce::String(), db % step == 0);
            }
        }
    };
}
