#pragma once

#include "include/cef_scheme.h"
#include <string>

class NewTabFactory : public CefSchemeHandlerFactory {
public:
    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         const CefString& scheme_name,
                                         CefRefPtr<CefRequest> request) override;
    IMPLEMENT_REFCOUNTING(NewTabFactory);
};