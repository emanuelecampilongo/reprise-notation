// SPDX-License-Identifier: GPL-3.0-only

#include "internal/applicationactioncontroller.h"

#include <QDeadlineTimer>
#include <QTimer>
#include <QWindow>

#include <memory>

#include <Windows.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Services.Store.h>
#include <appmodel.h>
#include <shobjidl_core.h>

#include "log.h"
#include "translation.h"

using namespace muse;
using namespace mu::appshell;

namespace {
enum class StoreUpdateStatus { Available, UpToDate, Unpackaged, Unavailable };

using namespace winrt::Windows::Services::Store;
using namespace winrt::Windows::Foundation;
using StoreUpdates = Collections::IVectorView<StorePackageUpdate>;

struct StoreApartment {
    StoreApartment() { winrt::init_apartment(winrt::apartment_type::single_threaded); }
    ~StoreApartment() { winrt::uninit_apartment(); }
};

struct StoreCheck {
    explicit StoreCheck(HWND window)
        : store(StoreContext::GetDefault())
    {
        winrt::check_hresult(store.as<IInitializeWithWindow>()->Initialize(window));
        operation = store.GetAppAndOptionalStorePackageUpdatesAsync();
    }

    StoreApartment apartment;
    StoreContext store;
    IAsyncOperation<StoreUpdates> operation { nullptr };
    QDeadlineTimer deadline { 30000 };
};

template<typename Callback>
void checkStoreForUpdates(QObject* owner, HWND window, Callback finished)
{
    UINT32 packageNameLength = 0;
    const LONG identityStatus = GetCurrentPackageFullName(&packageNameLength, nullptr);
    if (identityStatus == APPMODEL_ERROR_NO_PACKAGE) {
        finished(StoreUpdateStatus::Unpackaged);
        return;
    }
    if (identityStatus != ERROR_INSUFFICIENT_BUFFER || !window) {
        finished(StoreUpdateStatus::Unavailable);
        return;
    }

    try {
        // Start on the window's UI thread and wait for completion without blocking it.
        const auto check = std::make_shared<StoreCheck>(window);
        auto* timer = new QTimer(owner);
        QObject::connect(timer, &QTimer::timeout, timer, [check, timer, finished] {
            auto result = StoreUpdateStatus::Unavailable;
            try {
                if (check->operation.Status() == AsyncStatus::Started && !check->deadline.hasExpired()) {
                    return;
                }
                if (check->operation.Status() == AsyncStatus::Completed) {
                    const auto packageName = winrt::Windows::ApplicationModel::Package::Current().Id().Name();
                    result = StoreUpdateStatus::UpToDate;
                    for (const auto& update : check->operation.GetResults()) {
                        if (update.Package().Id().Name() == packageName) {
                            result = StoreUpdateStatus::Available;
                            break;
                        }
                    }
                } else if (check->operation.Status() == AsyncStatus::Started) {
                    check->operation.Cancel();
                }
            } catch (const winrt::hresult_error& error) {
                LOGW() << "Microsoft Store update check failed: " << winrt::to_string(error.message());
            }
            timer->stop();
            timer->deleteLater();
            finished(result);
        });
        timer->start(200);
    } catch (const winrt::hresult_error& error) {
        LOGW() << "Microsoft Store update check failed: " << winrt::to_string(error.message());
        finished(StoreUpdateStatus::Unavailable);
    }
}
}

void ApplicationActionController::checkForRepriseUpdates()
{
    if (m_checkingRepriseUpdates) {
        return;
    }
    m_checkingRepriseUpdates = true;

    const auto window = qWindow();
    const auto hwnd = window ? reinterpret_cast<HWND>(window->winId()) : nullptr;
    checkStoreForUpdates(this, hwnd, [this](StoreUpdateStatus status) {
        if (status != StoreUpdateStatus::Available) {
            m_checkingRepriseUpdates = false;
            return;
        }

        const auto title = trc("appshell/store", "Reprise Notation updates");
        const auto message = trc("appshell/store", "A new version of Reprise Notation is available. You can install it through Microsoft Store.");
        using Button = IInteractive::Button;
        const IInteractive::ButtonDatas buttons = {
            interactive()->buttonData(Button::Ok),
            { Button::Open, trc("appshell/store", "Open Microsoft Store") }
        };

        interactive()->info(title, IInteractive::Text(message), buttons, int(Button::Ok))
        .onResolve(this, [this](const IInteractive::Result& result) {
            m_checkingRepriseUpdates = false;
            if (result.isButton(Button::Open)) {
                interactive()->openUrl(std::string("ms-windows-store://pdp/?ProductId=9P6V1L676P93"));
            }
        })
        .onReject(this, [this](int, const std::string&) {
            m_checkingRepriseUpdates = false;
        });
    });
}
