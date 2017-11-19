/*
    This file is part of Helio Workstation.

    Helio is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Helio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Helio. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

//[Headers]
#include "HighlightedComponent.h"
#include "ComponentFader.h"

class RadioButton;

struct RadioButtonListener
{
    virtual ~RadioButtonListener() {}
    virtual void onRadioButtonClicked(RadioButton *button) = 0;
};
//[/Headers]


class RadioButton  : public HighlightedComponent
{
public:

    RadioButton (const String &text, Colour c, RadioButtonListener *listener);

    ~RadioButton();

    //[UserMethods]
    void deselect();
    void select();

    bool isSelected() const noexcept
    { return this->checkMark != nullptr; }

    Colour getColour() const noexcept
    { return this->colour; }

    int getButtonIndex() const noexcept;
    void setButtonIndex(int val);
    //[/UserMethods]

    void paint (Graphics& g) override;
    void resized() override;
    void mouseDown (const MouseEvent& e) override;


private:

    //[UserVariables]
    Component *createHighlighterComponent() override;

    int index;
    Colour colour;
    ScopedPointer<Component> checkMark;
    RadioButtonListener *owner;
    ComponentFader fader;
    //[/UserVariables]

    ScopedPointer<Label> label;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadioButton)
};
