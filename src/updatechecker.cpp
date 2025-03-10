/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2024 Vaclav Slavik
 *  Copyright (C) 2007 Andy Matuschak
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#include "updatechecker.h"
#include "appcast.h"
#include "ui.h"
#include "error.h"
#include "settings.h"
#include "download.h"
#include "utils.h"
#include "appcontroller.h"

#include <ctime>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <winsparkle.h>
#include <wininet.h>

using namespace std;

namespace winsparkle
{

/*--------------------------------------------------------------------------*
                              version comparison
 *--------------------------------------------------------------------------*/

// Note: This code is based on Sparkle's SUStandardVersionComparator by
//       Andy Matuschak.

namespace
{

// String characters classification. Valid components of version numbers
// are numbers, period or string fragments ("beta" etc.).
enum CharType
{
    Type_Number,
    Type_Period,
    Type_String
};

CharType ClassifyChar(char c)
{
    if ( c == '.' )
        return Type_Period;
    else if ( c >= '0' && c <= '9' )
        return Type_Number;
    else
        return Type_String;
}

// Split version string into individual components. A component is continuous
// run of characters with the same classification. For example, "1.20rc3" would
// be split into ["1",".","20","rc","3"].
vector<string> SplitVersionString(const string& version)
{
    vector<string> list;

    if ( version.empty() )
        return list; // nothing to do here

    string s;
    const size_t len = version.length();

    s = version[0];
    CharType prevType = ClassifyChar(version[0]);

    for ( size_t i = 1; i < len; i++ )
    {
        const char c = version[i];
        const CharType newType = ClassifyChar(c);

        if ( prevType != newType || prevType == Type_Period )
        {
            // We reached a new segment. Period gets special treatment,
            // because "." always delimiters components in version strings
            // (and so ".." means there's empty component value).
            list.push_back(s);
            s = c;
        }
        else
        {
            // Add character to current segment and continue.
            s += c;
        }

        prevType = newType;
    }

    // Don't forget to add the last part:
    list.push_back(s);

    return list;
}

std::string HttpGetWinINet(const std::wstring& url)
{
    const auto hInternet = InternetOpen(L"OETH", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet)
    {
        return "";
    }

    const auto hConnect = InternetOpenUrl(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect)
    {
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string response;
    char buffer[512] = {};
    DWORD bytesRead = 0;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return response;
}

std::wstring ToWString(const std::string& str)
{
    const auto count = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    if (count == 0)
    {
        return {};
    }

    std::wstring wstr(count, L'\0');
    if (0 == MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], count))
    {
        return {};
    }

    wstr.resize(wstr.size() - 1);
    return wstr;
}

std::string ParseGetVersionResponseJSON(const std::string& json, const std::string& key)
{
    auto start = json.find(key);
    if (start == std::string::npos)
    {
        return "";
    }

    start = json.find(":", start);
    if (start == std::string::npos)
    {
        return "";
    }

    start = json.find("\"", start);
    if (start == std::string::npos)
    {
        return "";
    }

    const auto end = json.find("\"", start + 1);
    if (end == std::string::npos)
    {
        return "";
    }

    return json.substr(start + 1, end - start - 1);
}

std::string GetServerVersion()
{
    const auto host = ApplicationController::GetAvailableHost();
    const auto url = host + "/getVersion";
    
    const auto json_response = HttpGetWinINet(ToWString(url));
    if (json_response.empty())
    {
        return "";
    }

    auto value = ParseGetVersionResponseJSON(json_response, "oethServerVersion");
    if (!value.empty())
    {
        return value;
    }

    value = ParseGetVersionResponseJSON(json_response, "error_msg");
    if (!value.empty())
    {
        auto start = value.find("404");
        if (start != std::string::npos)
        {
            const auto kDefaultServerVersion = "2.1.2";
            return kDefaultServerVersion;
        }
        else
        {
            return "";
        }
    }

    return "";
}

} // anonymous namespace


int UpdateChecker::CompareVersions(const string& verA, const string& verB)
{
    const vector<string> partsA = SplitVersionString(verA);
    const vector<string> partsB = SplitVersionString(verB);

    // Compare common length of both version strings.
    const size_t n = min(partsA.size(), partsB.size());
    for ( size_t i = 0; i < n; i++ )
    {
        const string& a = partsA[i];
        const string& b = partsB[i];

        const CharType typeA = ClassifyChar(a[0]);
        const CharType typeB = ClassifyChar(b[0]);

        if ( typeA == typeB )
        {
            if ( typeA == Type_String )
            {
                int result = a.compare(b);
                if ( result != 0 )
                    return result;
            }
            else if ( typeA == Type_Number )
            {
                const int intA = atoi(a.c_str());
                const int intB = atoi(b.c_str());
                if ( intA > intB )
                    return 1;
                else if ( intA < intB )
                    return -1;
            }
        }
        else // components of different types
        {
            if ( typeA != Type_String && typeB == Type_String )
            {
                // 1.2.0 > 1.2rc1
                return 1;
            }
            else if ( typeA == Type_String && typeB != Type_String )
            {
                // 1.2rc1 < 1.2.0
                return -1;
            }
            else
            {
                // One is a number and the other is a period. The period
                // is invalid.
                return (typeA == Type_Number) ? 1 : -1;
            }
        }
    }

    // The versions are equal up to the point where they both still have
    // parts. Lets check to see if one is larger than the other.
    if ( partsA.size() == partsB.size() )
        return 0; // the two strings are identical

    // Lets get the next part of the larger version string
    // Note that 'n' already holds the index of the part we want.

    int shorterResult, longerResult;
    CharType missingPartType; // ('missing' as in "missing in shorter version")

    if ( partsA.size() > partsB.size() )
    {
        missingPartType = ClassifyChar(partsA[n][0]);
        shorterResult = -1;
        longerResult = 1;
    }
    else
    {
        missingPartType = ClassifyChar(partsB[n][0]);
        shorterResult = 1;
        longerResult = -1;
    }

    if ( missingPartType == Type_String )
    {
        // 1.5 > 1.5b3
        return shorterResult;
    }
    else
    {
        // 1.5.1 > 1.5
        return longerResult;
    }
}


/*--------------------------------------------------------------------------*
                             UpdateChecker::Run()
 *--------------------------------------------------------------------------*/

UpdateChecker::UpdateChecker(): Thread("WinSparkle updates check")
{
}

void UpdateChecker::PerformUpdateCheck(bool show_dialog)
{
    try
    {
        const std::string url = Settings::GetAppcastURL();
        if ( url.empty() )
            throw std::runtime_error("The update source configuration is missing. Please contact support.");
        CheckForInsecureURL(url, "appcast feed");

        StringDownloadSink appcast_xml;
        DownloadFile(url, &appcast_xml, this, Settings::GetHttpHeadersString(), Download_BypassProxies);

        auto all = Appcast::Load(appcast_xml.data);
        
        // Filter to match the minimum server version
        const auto current_server_version = GetServerVersion();
        all.erase(std::remove_if(all.begin(), all.end(), [current_server_version](const Appcast& appcast)
            {
                return CompareVersions(current_server_version, appcast.MinServerVersion) < 0;
            }),
            all.end()
        );

        if (all.empty())
        {
            // No applicable updates in the feed.
            UI::NotifyNoUpdates(ShouldAutomaticallyInstall(), show_dialog);
            return;
        }

        // Sort by version number and pick the latest:
		std::stable_sort
        (
            all.begin(), all.end(),
            [](const Appcast& a, const Appcast& b) { return CompareVersions(a.Version, b.Version) < 0; }
        );

        const auto currentVersion = WideToAnsi(Settings::GetAppBuildVersion());
        const auto pos = std::find_if(all.begin(), all.end(), [&currentVersion](const Appcast& appcast)
            {
                return appcast.CriticalUpdate && CompareVersions(currentVersion, appcast.Version) < 0;
            });

        const auto appcast = pos != all.end() ? *pos : all.back();

        if (!appcast.ReleaseNotesURL.empty())
            CheckForInsecureURL(appcast.ReleaseNotesURL, "release notes");
        if (!appcast.GetDownloadURL().empty())
            CheckForInsecureURL(appcast.GetDownloadURL(), "update file");

        Settings::WriteConfigValue("LastCheckTime", time(NULL));

        // Check if our version is out of date.
        if ( !appcast.IsValid() || CompareVersions(currentVersion, appcast.Version) >= 0 )
        {
            // The same or newer version is already installed.
            UI::NotifyNoUpdates(ShouldAutomaticallyInstall(), show_dialog);
            return;
        }

        // Check if the user opted to ignore this particular version.
        if ( ShouldSkipUpdate(appcast) && !show_dialog)
        {
            return;
        }

        UI::NotifyUpdateAvailable(appcast, ShouldAutomaticallyInstall());
    }
    catch (const DownloadException& ex)
    {
        if (show_dialog)
        {
            UI::NotifyUpdateError(Err_AppcastXmlUnavailable, ex.what());
        }

        throw;
    }
    catch (const std::exception& ex)
    {
        if (show_dialog)
        {
            UI::NotifyUpdateError(Err_Generic, ex.what());
        }

        throw;
    }
    catch ( ... )
    {
        UI::NotifyUpdateError(Err_Generic, "Unknown exception");
        throw;
    }
}

bool UpdateChecker::ShouldSkipUpdate(const Appcast& appcast) const
{
    std::string toSkip;
    if (appcast.CriticalUpdate)
    {
        return false;
    }
    else if ( Settings::ReadConfigValue("SkipThisVersion", toSkip) )
    {
        return toSkip == appcast.Version;
    }
    else
    {
        return false;
    }
}


void PeriodicUpdateChecker::Run()
{
    // no initialization to do, so signal readiness immediately
    SignalReady();

    while (true)
    {
        // time to wait for next iteration: either a reasonable default or
        // time to next scheduled update check if checks are enabled
        unsigned sleepTimeInSeconds = 60 * 60; // 1 hour

        bool checkUpdates;
        Settings::ReadConfigValue("CheckForUpdates", checkUpdates, false);

        if (checkUpdates)
        {
            const time_t currentTime = time(NULL);
            time_t lastCheck = 0;
            Settings::ReadConfigValue("LastCheckTime", lastCheck);

            // Only check for updates in reasonable intervals:
            const int interval = win_sparkle_get_update_check_interval();
            time_t nextCheck = lastCheck + interval;
            if (currentTime >= nextCheck)
            {
                PerformUpdateCheck(false);
                sleepTimeInSeconds = interval;
            }
            else
            {
                sleepTimeInSeconds = unsigned(nextCheck - currentTime);
            }
        }

        m_terminateEvent.WaitUntilSignaled(sleepTimeInSeconds * 1000);
    }
}


void OneShotUpdateChecker::Run()
{
    // no initialization to do, so signal readiness immediately
    SignalReady();

    PerformUpdateCheck(false);
}


/*--------------------------------------------------------------------------*
                            ManualUpdateChecker
 *--------------------------------------------------------------------------*/

void ManualUpdateChecker::Run()
{
    // no initialization to do, so signal readiness immediately
    SignalReady();

    PerformUpdateCheck(true);
}

bool ManualUpdateChecker::ShouldSkipUpdate(const Appcast&) const
{
    // If I chose "Skip version" it should not prompt me for automatic updates,
    // but if I explicitly open the dialog using
    // win_sparkle_check_update_with_ui() it should still show that version.
    // This is the semantics in Sparkle for Mac.
    return false;
}

} // namespace winsparkle
