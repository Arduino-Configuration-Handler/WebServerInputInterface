#ifndef __H_WEB_SERVER_INPUT_INTERFACE__
#define __H_WEB_SERVER_INPUT_INTERFACE__
#include <WebServer.h>
#include <WiFi.h>
#include <config-handler-core.h>
#include <internal/string-utils.h>
#include <utility>

#define _WEB_SERVER_DEFAULT_SSID "ESP_Config_AP"

/**
 * @brief Construct the element's name from the configuration category and parameter's name.
 *
 * @param category The title of the configuration.
 * @param parameterName The name of the parameter.
 * @return String that represents the element's name.
 */
String getElementName(const String &category, const String &parameterName);
/**
 * @brief Deconstructs the element's name back to configuration's category and parameter's name.
 *
 * @param elementName The element's name that was constructed using `getElementName`.
 * @return std::pair<String, String> whose first element is the configuration's title and second element is the parameter's name.
 */
std::pair<String, String> getCategoryAndParamName(const String &elementName);
/**
 * @brief Generates a valid HTML code that will display the given string as a title.
 *
 * @param title The title to show.
 * @return Valid HTML string code for displaying `title` as a title.
 */
String createHtmlTitle(const String &title);
/**
 * @brief Generates a valid HTML code for an HTML form's input element based on the parameter's type and special attribute.
 * For example a checkbox for boolean parameter, or numeric text box for ints and floats.
 *
 * The input element will also be populated with the current parameter's value.
 *
 * @param elementName The element name as returned from `getElementName` function for this parameter.
 * @param parameter The parameter's metadata (type, special attribute, name...).
 * @param currentValue The current value of the given parameter.
 * @return Valid HTML code for displaying an input field for the given parameter.
 */
String createHtmlElementFor(const String &elementName, const ParameterInfo &parameter, const String &currentValue);
/**
 * @brief Generates a valid HTML select box element containing all the valid options for the given parameter.
 *
 * The selected value will be the first element that equals `currentValue`, or empty selection if `currentValue` wasn't found.
 *
 * @param elementName The element name as returned from `getElementName` function for this parameter.
 * @param parameter The parameter's metadata (type, special attribute, name...).
 * @param currentValue The current value of the given parameter.
 * @param options A vector of `String`s containing all the valid options for the given parameter.
 * @return Valid HTML code for displaying an selection box for the given parameter, containing all the given options.
 */
String createHtmlComboBoxFor(const String &elementName, const ParameterInfo &parameter, const String &currentValue, const std::vector<String> &options);

class WebServerInputInterface : public InputInterface
{
public:
    WebServerInputInterface(const String &ssid = _WEB_SERVER_DEFAULT_SSID, const String &password = "")
        : ssid(ssid), password(password), server(80) {}

    virtual ~WebServerInputInterface()
    {
        // Make sure to clean the resources if this class intance is deleted while in use.
        if (serverRunning)
        {
            server.close();
            WiFi.softAPdisconnect(true);
        }
    }

protected:
    void init(const ConfigInfo &configInfo, const std::map<String, String> &currentValues) override
    {
        htmlForm = createHtmlTitle(configInfo.title);
        for (const ParameterInfo &param : configInfo.parameters)
        {
            const String elementName = getElementName(configInfo.title, param.name);
            const auto &it = currentValues.find(param.name);
            String value = it == currentValues.end() ? "" : it->second;
            if (param.type == ParameterType::TYPE_OPTIONSET)
                htmlForm += createHtmlComboBoxFor(elementName, param, value, getParametersManger().getParameterOptions(configInfo.title, param.name));
            else
                htmlForm += createHtmlElementFor(elementName, param, value);

            htmlForm += "<button type='button' onclick='resetValue(\"" + elementName + "\")'>Reset</button><br>";
        }
    }

    void startImpl() override
    {
        WiFi.softAP(ssid, password);
        server.on("/", HTTP_GET, [this]()
                  { showWebPage(); });
        server.on("/save", HTTP_POST, [this]()
                  { handleSaveRequest(); });
        server.on("/resetValue", [this]()
                  { handleResetRequest(); });
        server.on("/refresh", [this]()
                  { handleRefreshRequest(); });
        server.begin();
        Serial.print("HTTP server started on IP: ");
        Serial.println(WiFi.softAPIP().toString());
        serverRunning = true;
    }

    void update()
    {
        server.handleClient();
    }
    void cleanup()
    {
        server.close();
        WiFi.softAPdisconnect(true);
        serverRunning = false;
    }

private:
    WebServer server;
    String htmlForm;
    bool serverRunning;
    const String ssid, password;

    void showWebPage()
    {
        // JS function that accepts the element's name and sends a request to the server to get its original value. Once the value is retrieved, the form is updated.
        const String resetFunction = F("function resetValue(elementName){const element=document.getElementById(elementName),elementTag=element.tagName.toLowerCase();fetch('/resetValue?param='+elementName).then(response=>response.json()).then(data=>{if(elementTag==='input'){element.type==='checkbox'?(element.checked=(data.value==='true')):(element.value=data.value);}else if(elementTag==='select'){let selectedIndex=-1;for(let i=0;i<element.length;i++)if(element[i].value===data.value){selectedIndex=i;break;}element.selectedIndex=selectedIndex;}});}");
        // JS function that accepts the element's name and sends a request to the server to force-refresh the options for this value. Once the values are retrieved, the form is updated.
        const String refreshFunction = F("function refreshOptions(elementName){fetch('/refresh?param='+elementName).then(response=>response.json()).then(data=>{const select=document.getElementById(elementName);select.innerHTML='';data.options.forEach(option=>{const opt=document.createElement('option');opt.value=option;opt.text=option;if(option===data.current)opt.selected=true;select.add(opt);});});}");

        // Build the HTML string for the server, and insert the form contents we generated in the `init` function.
        String html = "<!DOCTYPE html><html><body><h1>ESP32 Configuration</h1><form action='/save' method='POST'>";
        html += htmlForm;
        html += "<input type='submit' value='Save'></form><script type=\"text/javascript\">" + resetFunction + "\n" + refreshFunction + "</script></body></html>";
        server.send(200, "text/html", html);
    }

    void handleSaveRequest()
    {
        int argsCount = server.args();
        // Get all the arguments from the server.
        for (int i = 0; i < argsCount; i++)
        {
            String argName = server.argName(i);
            String argValue = server.arg(i);
            const auto [category, parameterName] = getCategoryAndParamName(argName);

            this->getParametersManger().setParameterValue(category, parameterName, argValue);
        }

        const ChainedValidationResults &result = validateInput();
        const auto &[code, contents] = result.match<std::pair<int, String>>([this]()
                                                                            { return std::make_pair(200, "Configurations were saved successfully."); },
                                                                            [this](const std::vector<String> &errors)
                                                                            {
                                                                                const String response = "Validation errors:\n" + vectorToString(errors, "\n");
                                                                                return std::make_pair(400, response);
                                                                            });

        server.send(code, "text/plain", contents);
    }

    void handleResetRequest()
    {
        String elementName = server.arg("param");
        const auto [category, parameterName] = getCategoryAndParamName(elementName);

        const String &originalValue = this->getParametersManger().getOriginalValue(category, parameterName);
        server.send(200, "application/json", "{\"value\":\"" + originalValue + "\"}");
    }

    void handleRefreshRequest()
    {
        String elementName = server.arg("param");
        const auto [category, parameterName] = getCategoryAndParamName(elementName);
        const String &originalValue = this->getParametersManger().getOriginalValue(category, parameterName);
        const std::vector<String> options = this->getParametersManger().getParameterOptions(category, parameterName, true);

        String json = "{\"current\":\"" + originalValue + "\",\"options\":[";

        for (size_t i = 0; i < options.size(); i++)
        {
            json += "\"" + options[i] + "\"";
            if (i < options.size() - 1)
            {
                json += ",";
            }
        }

        json += "]}";
        server.send(200, "application/json", json);
    }
};

#endif // __H_WEB_SERVER_INPUT_INTERFACE__
