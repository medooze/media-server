#include <X11/Xlib.h>
#include <include/internal/cef_types.h>
#include <include/internal/cef_linux.h>
#include "Browser.h"
#include "Client.h"
#include <signal.h>


int XErrorHandlerImpl(Display *display, XErrorEvent *event) {
	return Error("X error received [code:%d,request:%d,minor:%d]\n",static_cast<int>(event->error_code),static_cast<int>(event->request_code),static_cast<int>(event->minor_code));
}

int XIOErrorHandlerImpl(Display *display) {
	return  Error("X IO error received\n");
}

//CEF library initializers
class BrowserLib
{
public:
	BrowserLib()	{ 
		// Install xlib error handlers so that the application won't be terminated
		// on non-fatal errors.
		XSetErrorHandler(XErrorHandlerImpl);
		XSetIOErrorHandler(XIOErrorHandlerImpl);
	}
};

BrowserLib cfe;

	

Browser::Browser()
{
	//Y no tamos iniciados
	inited = 0;
}


/************************
* ~ Browser
* 	Destructor
*************************/
Browser::~Browser()
{
	//Check we have been correctly ended
	if (inited)
		//End it anyway
		End();
}


/************************
* Init
* 	
*************************/
int Browser::Init()
{

	CefMainArgs main_args;
	CefRefPtr<Browser> app(this);

	Log(">Init CEF Browser\n");

	// CEF applications have multiple sub-processes (render, plugin, GPU, etc)
	// that share the same executable. This function checks the command-line and,
	// if this is a sub-process, executes the appropriate logic.
	Log("-CefExecuteProcess\n");
	int exit_code = CefExecuteProcess(main_args, app.get(), NULL);
	if (exit_code >= 0) 
		// The sub-process has completed so return here.
		return Error("Error executing CEF Borwser [exit:%d]\n",exit_code);

	//Check not already inited
	if (inited)
		//Error
		return Error("-Init: CEF Browser Server is already running.\n");

	//Enable remote debugging
	settings.remote_debugging_port=2012;
	
	///
	// Set to true (1) to enable windowless (off-screen) rendering support. Do not
	// enable this value if the application does not use windowless rendering as
	// it may reduce rendering performance on some systems.
	///
	settings.windowless_rendering_enabled = true;
	
	///
	// Set to true (1) to use a single process for the browser and renderer. This
	// run mode is not officially supported by Chromium and is less stable than
	// the multi-process default. Also configurable using the "single-process"
	// command-line switch.
	///
	settings.single_process = true;
	
	///
	// Set to true (1) to disable the sandbox for sub-processes. See
	// cef_sandbox_win.h for requirements to enable the sandbox on Windows. Also
	// configurable using the "no-sandbox" command-line switch.
	///
	settings.no_sandbox = true;

	//Verbose logs	
	settings.log_severity = LOGSEVERITY_VERBOSE;

	// Initialize CEF for the browser process.
	Debug("-Init CEF Browser\n");
	CefInitialize(main_args, settings, app.get(), NULL);
	//I am inited
	inited = 1;

	//Log
	Log("<Inited CEF Browser\n");

	//Return ok
	return 1;
}

/***************************
 * Run
 * 	Server running thread
 ***************************/
void Browser::Run()
{
	//Log
	Log(">Run CEF Browser [%p]\n",this);
	
	// Run the CEF message loop. This will block until CefQuitMessageLoop() is called.
	CefRunMessageLoop();
	
	Log("<Run CEF Browser\n");

}


/************************
* Stop
* 	Exit message loop only
*************************/
void Browser::Stop()
{
	//Quite message loop 
	CefQuitMessageLoop();
}



/************************
* End
* 	End server and close all connections
*************************/
int Browser::End()
{
	Log(">End CEF Browser\n");

	//Check we have been inited
	if (!inited)
		//Do nothing
		return 0;

	//Stop thread
	inited = 0;
	
	// Shut down CEF.
	CefShutdown();

	Log("<End CEF Browser\n");
}

int Browser::CreateFrame(std::string url,DWORD width, DWORD height,Client::Listener *listener)
{
	// Information about the window that will be created including parenting, size, etc.
	CefWindowInfo info;
	
	//Without parent and white solid background
	info.SetAsWindowless(0,0);
	//Set dimeensions
	info.x = 0;
	info.y = 0;
	info.width = width;
	info.height = height;
	
	 // Client implements browser-level callbacks and RenderHandler
	CefRefPtr<Client> handler(new Client(listener));

	// Specify CEF browser settings here.
	CefBrowserSettings browser_settings;
	
	// Create the first browser window.
	CefBrowserHost::CreateBrowser(info, handler.get(), url, browser_settings, NULL);
}


/*Off-Screen Rendering
 * With off-screen rendering CEF does not create a native browser window. 
 * Instead, CEF provides the host application with invalidated regions and a 
 * pixel buffer and the host application notifies CEF of mouse, keyboard and 
 * focus events. Off-screen rendering does not currently support accelerated 
 * compositing so performance may suffer as compared to a windowed browser.
 * Off-screen browsers will receive the same notifications as windowed browsers 
 * including the life span notifications described in the previous section.
 * To use off-screen rendering:
 *  Implement the CefRenderHandler interface. All methods are required unless otherwise indicated.
 *  Call CefWindowInfo::SetAsOffScreen() and optionally CefWindowInfo::SetTransparentPainting() before passing the CefWindowInfo structure to CefBrowserHost::CreateBrowser().
 *  If no parent window is passed to SetAsOffScreen some functionality like ontext menus may not be available.
 *  The CefRenderHandler::GetViewRect() method will be called to retrieve the desired view rectangle.
 *  The CefRenderHandler::OnPaint() method will be called to provide invalid regions and the updated pixel buffer.
 *  The cefclient application draws the buffer using OpenGL but your application can use whatever technique you prefer.
 *  To resize the browser call CefBrowserHost::WasResized(). This will result in a call to GetViewRect() to retrieve the new size followed by a call to OnPaint().
 *  Call the CefBrowserHost::SendXXX() methods to notify the browser of mouse, keyboard and focus events.
 *  Call CefBrowserHost::CloseBrowser() to destroy browser.
 */
