/* 
 * File:   Browser.h
 * Author: Sergio
 *
 * Created on 2 de julio de 2015, 13:42
 */

#ifndef CEFBROWSER_H
#define	CEFBROWSER_H

#include "include/cef_app.h"
#include "include/cef_client.h"
#include "config.h"
#include "log.h"
#include "Client.h"

/*
 * Application Structure
 *  Every CEF3 application has the same general structure.
 *  Provide an entry-point function that initializes CEF and runs either sub-process executable logic or the CEF message loop.
 *  Provide an implementation of CefApp to handle process-specific callbacks.
 *  Provide an implementation of CefClient to handle browser-instance-specific callbacks.
 *  Call CefBrowserHost::CreateBrowser() to create a browser instance and manage the browser life span using CefLifeSpanHandler.
 */

class Browser :
	public CefApp,
        public CefBrowserProcessHandler,
        public CefRenderProcessHandler
{
public:
        static Browser& getInstance()
        {
            static Browser instance;
            return instance;
        }
	
private:
        // Dont forget to declare these two. You want to make sure they
        // are unaccessable otherwise you may accidently get copies of
        // your singelton appearing.
	Browser();
        Browser(Browser const&);                  // Don't Implement
        void operator=(Browser const&);           // Don't implement

public:
	int Init();
	void Run();
	int CreateFrame(std::string url,DWORD width, DWORD height,Client::Listener *listener);
	int End();
	virtual ~Browser();	

	//Override Methods
	
	// CefApp methods. Important to return |this| for the handler callbacks.
	virtual void OnBeforeCommandLineProcessing(const CefString& process_type,CefRefPtr<CefCommandLine> command_line) {
		Debug("OnBeforeCommandLineProcessing...\n");
	}
	virtual void OnRegisterCustomSchemes(CefRefPtr<CefSchemeRegistrar> registrar)	{
		Debug("OnRegisterCustomSchemes...\n");
	}
	/*virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler()		{ 
		Debug("GetBrowserProcessHandler...\n");
		return this; 
	}*/
	/*
	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler()		{ 
		Debug("GetRenderProcessHandler...\n");
		return this; 
	}*/

	// CefBrowserProcessHandler methods.
	virtual void OnContextInitialized() {
		Debug("The browser process UI thread has been initialized...\n");
	}
	virtual void OnRenderProcessThreadCreated(CefRefPtr<CefListValue> extra_info) {
		Debug("Send startup information to a new render process...\n");
	}

	// CefRenderProcessHandler methods.
	virtual void OnRenderThreadCreated(CefRefPtr<CefListValue> extra_info) {
		Debug("The render process main thread has been initialized...\n");
	}
	virtual void OnWebKitInitialized(CefRefPtr<Browser> app) {
		Debug("WebKit has been initialized, register V8 extensions...\n");
	}
	virtual void OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
		Debug("Browser created in this render process...browser:%p host:%p\n",browser.get(),browser.get()->GetHost().get());
	}
	virtual void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {
		Debug("Browser destroyed in this render process...\n");
	}
	virtual bool OnBeforeNavigation(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, NavigationType navigation_type, bool is_redirect) {
		Debug("Allow or block different types of navigation...\n");
		return true;
	}
	virtual void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
		Debug("JavaScript context created, add V8 bindings here...\n");
	}
	virtual void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,CefRefPtr<CefV8Context> context) {
		Debug("JavaScript context released, release V8 references here...\n");
	}
	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) {
		Debug("Handle IPC messages from the browser process...\n");
	}

	IMPLEMENT_REFCOUNTING(Browser);

private:
	bool	inited;
	pthread_t serverThread;
	 // Specify CEF global settings here.
	CefSettings settings;
};



#endif	/* CEFBROWSER_H */

