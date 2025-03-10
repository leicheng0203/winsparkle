/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2013 Vaclav Slavik
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

#include "appcontroller.h"


namespace winsparkle
{

CriticalSection ApplicationController::ms_csVars;

win_sparkle_get_available_host_callback_t  ApplicationController::ms_cbGetAvailableHost = NULL;
win_sparkle_error_callback_t               ApplicationController::ms_cbError = NULL;
win_sparkle_can_shutdown_callback_t        ApplicationController::ms_cbIsReadyToShutdown = NULL;
win_sparkle_shutdown_request_callback_t    ApplicationController::ms_cbRequestShutdown = NULL;
win_sparkle_did_find_update_callback_t     ApplicationController::ms_cbDidFindUpdate = NULL;
win_sparkle_appcast_xml_unavailable_callback_t   ApplicationController::ms_cbAppcastXmlUnavailable = NULL;
win_sparkle_download_progress_callback_t   ApplicationController::ms_cbDownloadProgress = NULL;
win_sparkle_download_complete_callback_t   ApplicationController::ms_cbDownloadComplete = NULL;
win_sparkle_download_failed_callback_t     ApplicationController::ms_cbDownloadFailed = NULL;
win_sparkle_did_not_find_update_callback_t ApplicationController::ms_cbDidNotFindUpdate = NULL;
win_sparkle_update_cancelled_callback_t    ApplicationController::ms_cbUpdateCancelled = NULL;
win_sparkle_update_skipped_callback_t      ApplicationController::ms_cbUpdateSkipped = NULL;
win_sparkle_update_postponed_callback_t    ApplicationController::ms_cbUpdatePostponed = NULL;
win_sparkle_update_dismissed_callback_t    ApplicationController::ms_cbUpdateDismissed = NULL;
win_sparkle_user_run_installer_callback_t  ApplicationController::ms_cbUserRunInstaller = NULL;

bool ApplicationController::IsReadyToShutdown()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_cbIsReadyToShutdown )
            return (*ms_cbIsReadyToShutdown)() == 0 ? false : true;
    }

    // default implementations:

    return true;
}

void ApplicationController::RequestShutdown()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_cbRequestShutdown )
        {
            (*ms_cbRequestShutdown)();
            return;
        }
    }

    // default implementations:

    // nothing yet
}

void ApplicationController::NotifyUpdateError(int error_code, const char* error_message)
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_cbError )
        {
            (*ms_cbError)(error_code, error_message);
            return;
        }
    }
}

void ApplicationController::NotifyUpdateFound(const Appcast& info)
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbDidFindUpdate)
        {
            (*ms_cbDidFindUpdate)(info.Version.c_str(), info.CriticalUpdate);
            return;
        }
    }
}

void ApplicationController::NotifyAppcastXmlUnavailable()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbAppcastXmlUnavailable)
        {
            (*ms_cbAppcastXmlUnavailable)();
            return;
        }
    }
}

void ApplicationController::NotifyDownloadProgress(size_t downloaded, size_t total)
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbDownloadProgress)
        {
            (*ms_cbDownloadProgress)(downloaded, total);
            return;
        }
    }
}

void ApplicationController::NotifyDownloadComplete()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbDownloadComplete)
        {
            (*ms_cbDownloadComplete)();
            return;
        }
    }
}

void ApplicationController::NotifyDownloadFailed()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbDownloadFailed)
        {
            (*ms_cbDownloadFailed)();
            return;
        }
    }
}

void ApplicationController::NotifyUpdateNotFound()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_cbDidNotFindUpdate )
        {
            (*ms_cbDidNotFindUpdate)();
            return;
        }
    }
}

void ApplicationController::NotifyUpdateCancelled()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if ( ms_cbUpdateCancelled )
        {
            (*ms_cbUpdateCancelled)();
            return;
        }
    }
}

void ApplicationController::NotifyUpdateSkipped()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbUpdateSkipped)
        {
            (*ms_cbUpdateSkipped)();
            return;
        }
    }
}

void ApplicationController::NotifyUpdatePostponed()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbUpdatePostponed)
        {
            (*ms_cbUpdatePostponed)();
            return;
        }
    }
}

void ApplicationController::NotifyUpdateDismissed()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbUpdateDismissed)
        {
            (*ms_cbUpdateDismissed)();
            return;
        }
    }
}


int ApplicationController::UserRunInstallerCallback(const wchar_t* filePath, const char* installer_arguments)
{
    if (!ms_cbUserRunInstaller)
        return false;

    return ms_cbUserRunInstaller(filePath, installer_arguments);
}

std::string ApplicationController::GetAvailableHost()
{
    {
        CriticalSectionLocker lock(ms_csVars);
        if (ms_cbGetAvailableHost)
        {
            return (*ms_cbGetAvailableHost)();
        }
    }
}

} // namespace winsparkle
