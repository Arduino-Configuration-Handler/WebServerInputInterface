#include "WebServerInputInterface.h"
#include <sstream>

#define ELEMENT_NAME_SEPARATOR '_'

String getElementName(const String &category, const String &parameterName)
{
    return category + ELEMENT_NAME_SEPARATOR + parameterName;
}

std::pair<String, String> getCategoryAndParamName(const String &elementName)
{
    int separatorIndex = elementName.lastIndexOf(ELEMENT_NAME_SEPARATOR);
    String category = elementName.substring(0, separatorIndex);
    String parameterName = elementName.substring(separatorIndex + 1);
    return std::make_pair(category, parameterName);
}

String numericHtmlElement(const ParameterInfo &parameter, const String &elementName, const String &currentValue)
{
    String value = currentValue;
    if (currentValue.isEmpty())
        value = "0";
    if (parameter.specialAttribute != ParameterAttribute::ATTR_PASSWORD)
        return parameter.name + ": <input type='number' id='" + elementName + "' name='" + elementName + "' value='" + value + "'>";
    return parameter.name + ": <input type='password' id='" + elementName + "' name='" + elementName + "' value='" + value + "' pattern='[0-9]+' inputmode='decimal'>";
}
String toggleHtmlElement(const ParameterInfo &parameter, const String &elementName, const String &currentValue)
{
    // Checkbox inputs require two elements since they aren't sent with the form if the checkbox is not checked. The hidden input element makes sure that
    // a "false" value will be sent by default.
    String hiddenElement = "<input type='hidden' name='" + elementName + "' value='false'>";
    // Assume the value wasn't modified somewhere, and just check the first character.
    String checked = currentValue[0] == 't' ? "checked" : "";
    String element = parameter.name + ": <input type='checkbox' id='" + elementName + "' name='" + elementName + "' value='true' " + checked + ">";
    return hiddenElement + element;
}
String textualHtmlElement(const ParameterInfo &parameter, const String &elementName, const String &currentValue)
{
    String type = parameter.specialAttribute == ParameterAttribute::ATTR_PASSWORD ? "password" : "text";
    return parameter.name + ": <input type='" + type + "' id='" + elementName + "' name='" + elementName + "' value='" + currentValue + "'>";
}

String createHtmlTitle(const String &title)
{
    return "<h2>" + title + "</h2>";
}

String createHtmlElementFor(const String &elementName, const ParameterInfo &parameter, const String &currentValue)
{
    switch (parameter.type)
    {
    case ParameterType::TYPE_INT:
    case ParameterType::TYPE_FLOAT:
        return numericHtmlElement(parameter, elementName, currentValue);
    case ParameterType::TYPE_BOOL:
        return toggleHtmlElement(parameter, elementName, currentValue);
    default:
        return textualHtmlElement(parameter, elementName, currentValue);
    }
}

String createHtmlComboBoxFor(const String &elementName, const ParameterInfo &parameter, const String &currentValue, const std::vector<String> &options)
{
    String html = "Select " + parameter.name + ": <select id='" + elementName + "' name='" + elementName + "'>";
    bool found = false;
    for (const auto &option : options)
    {
        html += +"<option value='" + option + "'";
        if (!found && option.equals(currentValue))
        {
            html += +" selected";
            found = true;
        }

        html += +">" + option + "</option>";
    }

    html += "</select><button type='button' onclick='refreshOptions(\"" + elementName + "\")'>Refresh</button>";
    return html;
}
