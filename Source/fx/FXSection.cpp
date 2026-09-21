#include "FXSection.h"

namespace glitch::fx::ui
{
using namespace glitch::theme;
namespace pid = params::id;

namespace
{
constexpr int tabBarHeight = 28;
constexpr int headerHeight = 26;   // the title row inside a tab, not the drawer header
} // namespace

// -------------------------------------------------- DraggableTabButton -----

DraggableTabButton::DraggableTabButton (const juce::String& name, juce::TabbedButtonBar& bar,
                                        std::function<void (int from, int to)> mover)
    : juce::TabBarButton (name, bar), onMove (std::move (mover))
{
    // Don't steal focus from the on-screen keyboard's note input when a tab is
    // clicked or dragged to reorder the chain.
    setMouseClickGrabsKeyboardFocus (false);
}

void DraggableTabButton::mouseDrag (const juce::MouseEvent& e)
{
    auto& bar = getTabbedButtonBar();
    const int idx = getIndex();
    const auto px = bar.getLocalPoint (this, e.position).x;

    int target = idx;   // the tab whose slot the pointer is over
    for (int i = 0; i < bar.getNumTabs(); ++i)
        if (auto* b = bar.getTabButton (i))
            if (px >= (float) b->getX() && px < (float) b->getRight()) { target = i; break; }

    if (target != idx && target >= 0 && onMove)
        onMove (idx, target);
}

void DraggableTabButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto front = getTabbedButtonBar().getCurrentTabIndex() == getIndex();
    const auto lit = isEngaged && isEngaged (getButtonText());

    auto body = getLocalBounds().toFloat().reduced (1.5f, 3.0f);
    g.setColour (front ? paper.withAlpha (0.92f)
                       : paper.withAlpha (over || down ? 0.7f : 0.45f));
    g.fillRoundedRectangle (body, 6.0f);
    g.setColour (front ? sage.withAlpha (0.5f) : sage.withAlpha (0.22f));
    g.drawRoundedRectangle (body, 6.0f, 1.0f);

    // Grip-dots handle at the left, so the drag is discoverable.
    const float x = body.getX() + 6.0f;
    const float cy = body.getCentreY();
    constexpr float d = 1.5f, gap = 3.0f;
    g.setColour (ink.withAlpha (over || down ? 0.55f : 0.30f));
    for (int col = 0; col < 2; ++col)
        for (int row = -1; row <= 1; ++row)
            g.fillEllipse (x + (float) col * gap, cy + (float) row * gap - d * 0.5f, d, d);

    g.setFont (juce::Font (juce::FontOptions (10.0f, lit ? juce::Font::bold : juce::Font::plain)));
    drawGlitchText (g, *this, getButtonText(), body.withTrimmedLeft (14.0f).withTrimmedRight (4.0f),
                    juce::Justification::centred, lit ? electric : (front ? ink : muted));
}

// -------------------------------------------------------- DraggableTabs ----

DraggableTabs::DraggableTabs()
    : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop)
{
}

juce::Array<int> DraggableTabs::currentOrder() const
{
    juce::Array<int> ids;
    for (const auto& n : getTabNames())
        ids.add (moduleNames.indexOf (n));
    return ids;
}

void DraggableTabs::applyOrder (const juce::Array<int>& ids)
{
    for (int pos = 0; pos < ids.size(); ++pos)
    {
        if (! juce::isPositiveAndBelow (ids[pos], moduleNames.size()))
            continue;
        const auto name = moduleNames[ids[pos]];
        const int cur = getTabNames().indexOf (name);
        if (cur >= 0 && cur != pos)
            moveTab (cur, pos, false);   // TabbedComponent::moveTab: button + content
    }
}

juce::TabBarButton* DraggableTabs::createTabButton (const juce::String& name, int)
{
    auto* button = new DraggableTabButton (name, getTabbedButtonBar(),
        [this] (int from, int to)
        {
            moveTab (from, to, true);   // button + content
            if (onOrderChanged) onOrderChanged();
        });
    button->isEngaged = [this] (const juce::String& tabName)
    {
        return isTabEngaged && isTabEngaged (tabName);
    };
    return button;
}

// ---------------------------------------------------------------- FXTab ----

FXTab::FXTab (juce::AudioProcessorValueTreeState& apvts, params::Section s, FXScope::Kind kind,
              MidiLearnManager* learn)
    : section (s), title (params::sectionTitles()[(int) s]), grid (apvts, s, learn)
{
    display = std::make_unique<FXScope> (apvts, kind, s);
    addAndMakeVisible (*display);
    addAndMakeVisible (grid);
    buildHeader (apvts);
}

FXTab::FXTab (juce::AudioProcessorValueTreeState& apvts, params::Section s,
              std::unique_ptr<juce::Component> customDisplay, MidiLearnManager* learn)
    : section (s), title (params::sectionTitles()[(int) s]), grid (apvts, s, learn)
{
    // EQ is edited entirely on its curve (CHARACTER is its only knob), and the
    // limiter's transfer curve + GR meter want room to be read at a glance.
    // OTT is the other way round: three bars need very little width, and it
    // has fifteen controls to place. At the limiter's 0.46 the grid only gets
    // seven columns, which wraps those fifteen onto a third row and pushes the
    // last one out of the band entirely.
    displayWidthFraction = s == params::Section::eq  ? 0.80f
                         : s == params::Section::ott ? 0.34f
                                                     : 0.46f;
    display = std::move (customDisplay);
    if (display != nullptr)
        addAndMakeVisible (*display);
    addAndMakeVisible (grid);
    buildHeader (apvts);
}

void FXTab::buildHeader (juce::AudioProcessorValueTreeState& apvts)
{
    if (const auto* on = params::enableID (section))
    {
        power = std::make_unique<PowerButton> (apvts, on,
                                               section == params::Section::tremVib ? "TREM" : "ON");
        addAndMakeVisible (*power);
    }
    if (const auto* second = params::secondEnableID (section))
    {
        secondPower = std::make_unique<PowerButton> (apvts, second, "VIB");
        addAndMakeVisible (*secondPower);
    }

    // A rate control only means something in the mode it belongs to, so dim the
    // one that is currently inert.
    auto pair = [&] (const char* sync, const char* freeID, const char* divID)
    {
        if (auto* freeRun = grid.componentFor (freeID))
            dependencies.push_back (std::make_unique<DependentEnable> (
                apvts, sync, [] (float v) { return v < 0.5f; },
                std::vector<juce::Component*> { freeRun }));
        if (auto* synced = grid.componentFor (divID))
            dependencies.push_back (std::make_unique<DependentEnable> (
                apvts, sync, [] (float v) { return v >= 0.5f; },
                std::vector<juce::Component*> { synced }));
    };

    switch (section)
    {
        case params::Section::delay:
            pair (pid::delaySync, pid::delayTime, pid::delayDivision);
            break;
        case params::Section::mod:
            pair (pid::modSync, pid::modRate, pid::modDivision);
            break;
        case params::Section::tremVib:
            pair (pid::tremSync, pid::tremRate, pid::tremDivision);
            pair (pid::vibSync, pid::vibRate, pid::vibDivision);
            break;
        default:
            break;
    }
}

void FXTab::paint (juce::Graphics& g)
{
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    drawGlitchText (g, *this, title.toUpperCase(),
                    getLocalBounds().removeFromTop (headerHeight).withTrimmedLeft (8),
                    juce::Justification::centredLeft, muted);
}

void FXTab::resized()
{
    auto area = getLocalBounds().reduced (6, 4);

    auto header = area.removeFromTop (headerHeight - 4);
    if (secondPower != nullptr)
        secondPower->setBounds (header.removeFromRight (64).reduced (2, 3));
    if (power != nullptr)
        power->setBounds (header.removeFromRight (64).reduced (2, 3));

    area.removeFromTop (4);

    // The band is wide and short, so the display sits BESIDE the controls
    // rather than above them: stacked, a scope stretched to the full width
    // flattens its curve into a near-horizontal line and leaves the knobs
    // barely tall enough to read.
    if (display != nullptr)
    {
        const auto displayWidth = juce::roundToInt ((float) area.getWidth() * displayWidthFraction);
        display->setBounds (area.removeFromLeft (displayWidth));
        area.removeFromLeft (10);
    }

    // Captions must never clip, so the grid always gets the full height its
    // rows need; it is centred in whatever vertical space the tab has.
    const auto gridWidth = juce::jmin (grid.naturalWidthFor (area.getWidth()), area.getWidth());
    const auto gridHeight = juce::jmin (grid.heightForWidth (gridWidth), area.getHeight());
    grid.setBounds (area.withSizeKeepingCentre (gridWidth, gridHeight));
}

// ------------------------------------------------------------ FXSection ----

FXSection::FXSection (juce::AudioProcessorValueTreeState& state,
                      EqEditor::ScopeReader scopeReader,
                      std::function<double()> sampleRateFn,
                      std::function<float()> limiterGainReduction,
                      std::function<float()> limiterOutputPeak,
                      std::function<float (int)> ottBandGain,
                      MidiLearnManager* learn)
    : apvts (state)
{
    tabs.setTabBarDepth (tabBarHeight);
    tabs.setOutline (0);
    tabs.setIndent (0);
    tabs.setColour (juce::TabbedComponent::backgroundColourId, juce::Colours::transparentBlack);
    tabs.setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);

    const auto tabBg = juce::Colours::transparentBlack;
    using S = params::Section;
    using K = FXScope::Kind;

    // Tab order here is the module id order (FXChain::Module), which
    // applyOrder/currentOrder then permute.
    tabs.addTab (params::sectionTabNames()[(int) S::dist], tabBg,
                 new FXTab (apvts, S::dist, K::distortion, learn), true);
    tabs.addTab (params::sectionTabNames()[(int) S::chorus], tabBg,
                 new FXTab (apvts, S::chorus, K::chorus, learn), true);
    tabs.addTab (params::sectionTabNames()[(int) S::delay], tabBg,
                 new FXTab (apvts, S::delay, K::delay, learn), true);
    tabs.addTab (params::sectionTabNames()[(int) S::reverb], tabBg,
                 new FXTab (apvts, S::reverb, K::reverb, learn), true);
    tabs.addTab (params::sectionTabNames()[(int) S::eq], tabBg,
                 new FXTab (apvts, S::eq,
                            std::make_unique<EqEditor> (apvts, std::move (scopeReader),
                                                        std::move (sampleRateFn)), learn), true);
    tabs.addTab (params::sectionTabNames()[(int) S::mod], tabBg,
                 new FXTab (apvts, S::mod, K::chorus, learn), true);
    tabs.addTab (params::sectionTabNames()[(int) S::tremVib], tabBg,
                 new FXTab (apvts, S::tremVib, K::chorus, learn), true);
    tabs.addTab (params::sectionTabNames()[(int) S::limiter], tabBg,
                 new FXTab (apvts, S::limiter,
                            std::make_unique<LimiterDisplay> (apvts,
                                                              std::move (limiterGainReduction),
                                                              std::move (limiterOutputPeak)), learn), true);

    tabs.addTab (params::sectionTabNames()[(int) S::ott], tabBg,
                 new FXTab (apvts, S::ott,
                            std::make_unique<OttDisplay> (apvts, std::move (ottBandGain)),
                            learn), true);

    tabs.setModuleNames (params::sectionTabNames());
    tabs.onOrderChanged = [this]
    {
        if (onOrderChanged) onOrderChanged (tabs.currentOrder());
    };
    tabs.isTabEngaged = [this] (const juce::String& tabName)
    {
        const auto index = params::sectionTabNames().indexOf (tabName);
        if (index < 0) return false;
        const auto section = (params::Section) index;

        auto on = [this] (const char* id)
        {
            auto* v = id != nullptr ? apvts.getRawParameterValue (id) : nullptr;
            return v != nullptr && v->load() >= 0.5f;
        };
        return on (params::enableID (section)) || on (params::secondEnableID (section));
    };

    for (int i = 0; i < params::numSections; ++i)
        for (const auto* id : { params::enableID ((params::Section) i),
                                params::secondEnableID ((params::Section) i) })
            if (id != nullptr)
            {
                watchedEnables.add (id);
                apvts.addParameterListener (id, this);
            }

    header.onToggle = [this] { setCollapsed (! header.isCollapsed()); };
    addAndMakeVisible (header);
    addAndMakeVisible (tabs);
}

void FXSection::setCollapsed (bool shouldBeCollapsed)
{
    if (header.isCollapsed() == shouldBeCollapsed) return;
    header.setCollapsed (shouldBeCollapsed);
    tabs.setVisible (! shouldBeCollapsed);
    resized();
    if (onCollapsedChanged) onCollapsedChanged();
}

FXSection::~FXSection()
{
    for (const auto& id : watchedEnables)
        apvts.removeParameterListener (id, this);
    cancelPendingUpdate();
}

void FXSection::handleAsyncUpdate() { tabs.getTabbedButtonBar().repaint(); }

void FXSection::paint (juce::Graphics& g)
{
    g.setColour (paper);
    g.fillRect (getLocalBounds());

    // A hairline above the band, matching the rule under the faceplate header.
    g.setColour (sage.withAlpha (0.35f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void FXSection::resized()
{
    auto area = getLocalBounds();
    header.setBounds (area.removeFromTop (glitch::ui::DrawerHeader::height));
    if (! header.isCollapsed())
        tabs.setBounds (area.reduced (24, 0).withTrimmedBottom (10));
}

} // namespace glitch::fx::ui
