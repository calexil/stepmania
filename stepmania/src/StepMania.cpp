#include "stdafx.h"
/*
-----------------------------------------------------------------------------
 File: StepMania.cpp

 Desc: 

 Copyright (c) 2001 Chris Danford.  All rights reserved.
-----------------------------------------------------------------------------
*/

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "resource.h"

#include "RageScreen.h"
#include "RageTextureManager.h"
#include "RageSound.h"
#include "RageMusic.h"
#include "RageInput.h"

#include "GameInfo.h"
#include "ThemeManager.h"
#include "WindowManager.h"

#include "WindowSandbox.h"
#include "WindowLoading.h"
#include "WindowMenuResults.h"
#include "WindowTitleMenu.h"
#include "WindowPlayerOptions.h"

#include <DXUtil.h>

//-----------------------------------------------------------------------------
// Links
//-----------------------------------------------------------------------------
#pragma comment(lib, "d3dx8.lib")
#pragma comment(lib, "d3d8.lib")


//-----------------------------------------------------------------------------
// Application globals
//-----------------------------------------------------------------------------
const CString g_sAppName		= "StepMania";
const CString g_sAppClassName	= "StepMania Class";


HWND		g_hWndMain;				// Main Window Handle
HINSTANCE	g_hInstance;			// The Handle to Window Instance

#include "ScreenDimensions.h"

const DWORD g_dwWindowStyle = WS_VISIBLE|WS_POPUP|WS_CAPTION|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU;


BOOL	g_bIsActive		= FALSE;	// Whether the focus is on our app


//LPRageMovieTexture	g_pMovieTexture = NULL;


//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
// Main game functions
LRESULT CALLBACK WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

void Update();			// Update the game logic
void Render();			// Render a frame
void ShowFrame();		// Display the contents of the back buffer to the screen

// Functions that work with game objects
HRESULT		CreateObjects( HWND hWnd );	// allocate and initialize game objects
HRESULT		InvalidateObjects();		// invalidate game objects before a display mode change
HRESULT		RestoreObjects();			// restore game objects after a display mode change
VOID		DestroyObjects();			// deallocate game objects when we're done with them

BOOL SwitchDisplayMode( BOOL bWindowed, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP );

BOOL WeAreAlone( LPSTR szName );


//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Application entry point
//-----------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow )
{
	if( !WeAreAlone("StepMania") )
	{
		RageError( "StepMania is already running!" );
	}

	// Make sure the current directory is the root program directory
	if( !DoesFileExist("Songs") )
	{
		// change dir to path of the execuctable
		TCHAR szFullAppPath[MAX_PATH];
		GetModuleFileName(NULL, szFullAppPath, MAX_PATH);
		
		// strip off executable name
		LPSTR pLastBackslash = strrchr(szFullAppPath, '\\');
		*pLastBackslash = '\0';	// terminate the string

		SetCurrentDirectory(szFullAppPath);
	}


    CoInitialize (NULL);    // Initialize COM

    // Register the window class
	WNDCLASS wndClass = { 
		0,
		WndProc,	// callback handler
		0,			// cbClsExtra; 
		0,			// cbWndExtra; 
		hInstance,
		LoadIcon( hInstance, MAKEINTRESOURCE(IDI_ICON) ), 
		LoadCursor( hInstance, IDC_ARROW),
		(HBRUSH)GetStockObject( BLACK_BRUSH ),
		NULL,				// lpszMenuName; 
		g_sAppClassName	// lpszClassName; 
	}; 
 	RegisterClass( &wndClass );


	// Set the window's initial width
    RECT rcWnd;
    SetRect( &rcWnd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );
    AdjustWindowRect( &rcWnd, g_dwWindowStyle, FALSE );


	// Create our main window
	g_hWndMain = CreateWindow(
				  g_sAppClassName,// pointer to registered class name
				  g_sAppName,		// pointer to window name
				  g_dwWindowStyle,	// window style
				  CW_USEDEFAULT,	// horizontal position of window
				  CW_USEDEFAULT,	// vertical position of window
				  RECTWIDTH(rcWnd),	// window width
				  RECTHEIGHT(rcWnd),// window height
				  NULL,				// handle to parent or owner window
				  NULL,				// handle to menu, or child-window identifier
				  hInstance,		// handle to application instance
				  NULL				// pointer to window-creation data
				);
 	if( NULL == g_hWndMain )
		exit(1);


	// Load keyboard accelerators
	HACCEL hAccel = LoadAccelerators( NULL, MAKEINTRESOURCE(IDR_MAIN_ACCEL) );


	// run the game
	CreateObjects( g_hWndMain );	// Create the game objects

	// Now we're ready to recieve and process Windows messages.
	MSG msg;
	ZeroMemory( &msg, sizeof(msg) );


	while( WM_QUIT != msg.message  )
	{
		// Look for messages, if none are found then 
		// update the state and display it
		if( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
		{
	        GetMessage(&msg, NULL, 0, 0 );

			// Translate and dispatch the message
			if( 0 == TranslateAccelerator( g_hWndMain, hAccel, &msg ) )
			{
				TranslateMessage( &msg ); 
				DispatchMessage( &msg );
			}
		}
		else	// No messages are waiting.  Render a frame during idle time.
		{
			Update();
			Render();
			//if( !g_bFullscreen )
			::Sleep(4 );	// give some time for the movie decoding thread
		}
	}	// end  while( WM_QUIT != msg.message  )


	// clean up after a normal exit 
	DestroyObjects();			// deallocate our game objects and leave fullscreen
	DestroyWindow( g_hWndMain );
	UnregisterClass( g_sAppClassName, hInstance );
	CoUninitialize();			// Uninitialize COM
	return 0L;
}



//-----------------------------------------------------------------------------
// Name: WndProc()
// Desc: Callback for all Windows messages
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch( msg )
	{
		case WM_ACTIVATEAPP:
            // Check to see if we are losing our window...
			g_bIsActive = (BOOL)wParam;
			break;

		case WM_SIZE:
            // Check to see if we are losing our window...
			if( SIZE_MAXHIDE==wParam || SIZE_MINIMIZED==wParam )
                g_bIsActive = FALSE;
            else
                g_bIsActive = TRUE;
            break;

		case WM_GETMINMAXINFO:
			{
				// Don't allow the window to be resized smaller than the screen resolution.
				// This should snap to multiples of the screen size two!

				RECT rcWnd;
				SetRect( &rcWnd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );
				DWORD dwWindowStyle = GetWindowLong( g_hWndMain, GWL_STYLE );
				AdjustWindowRect( &rcWnd, dwWindowStyle, FALSE );

				((MINMAXINFO*)lParam)->ptMinTrackSize.x = RECTWIDTH(rcWnd);
				((MINMAXINFO*)lParam)->ptMinTrackSize.y = RECTHEIGHT(rcWnd);
			}
			break;

		case WM_SETCURSOR:
			// Turn off Windows cursor in fullscreen mode
			if( !SCREEN->IsWindowed() )
            {
                SetCursor( NULL );
                return TRUE; // prevent Windows from setting the cursor
            }
            break;

		case WM_SYSCOMMAND:
			// Prevent moving/sizing and power loss
			switch( wParam )
			{
				case SC_MOVE:
				case SC_SIZE:
				case SC_KEYMENU:
				case SC_MONITORPOWER:
					return 1;
				case SC_MAXIMIZE:
					//SendMessage( g_hWndMain, WM_COMMAND, IDM_TOGGLEFULLSCREEN, 0 );
					//return 1;
					break;
			}
			break;


		case WM_COMMAND:
            switch( LOWORD(wParam) )
            {
				case IDM_TOGGLEFULLSCREEN:
					SwitchDisplayMode( !SCREEN->IsWindowed(), SCREEN_WIDTH, SCREEN_HEIGHT, 16 );
					return 0;
                case IDM_EXIT:
                    // Recieved key/menu command to exit app
                    SendMessage( hWnd, WM_CLOSE, 0, 0 );
                    return 0;
            }
            break;

        case WM_NCHITTEST:
            // Prevent the user from selecting the menu in fullscreen mode
            if( !SCREEN->IsWindowed() )
                return HTCLIENT;
            break;

		case WM_PAINT:
			// redisplay the contents of the back buffer if the window needs to be redrawn
			ShowFrame();
			break;

		case WM_DESTROY:
			PostQuitMessage( 0 );
			break;
	}

	return DefWindowProc( hWnd, msg, wParam, lParam );
}



//-----------------------------------------------------------------------------
// Name: CreateObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CreateObjects( HWND hWnd )
{

	srand( (unsigned)time(NULL) );	// seed number generator

	RageLogStart();

	SCREEN	= new RageScreen( hWnd, true, SCREEN_WIDTH, SCREEN_HEIGHT, 16 );
	TM		= new RageTextureManager( SCREEN );
	THEME	= new ThemeManager;
	WM		= new WindowManager;

	// throw something up on the screen while the game resources are loading
	WM->SetNewWindow( new WindowLoading );
	Render();
	ShowFrame();

	// this stuff takes a long time...
	SOUND	= new RageSound( hWnd );
	MUSIC	= new RageSoundStream;
	INPUT	= new RageInput( hWnd );
	GAMEINFO= new GameInfo;

	BringWindowToTop( hWnd );
	SetForegroundWindow( hWnd );

	GameOptions &go = GAMEINFO->m_GameOptions;
	SwitchDisplayMode( 
		go.m_bWindowed, 
		go.m_iResolution,
		go.m_iResolution==640 ? 480 : 240,
		go.m_iDisplayColor
	);


	//WM->SetNewWindow( new WindowSandbox );
	//WM->SetNewWindow( new WindowMenuResults );
	//WM->SetNewWindow( new WindowPlayerOptions );
	WM->SetNewWindow( new WindowTitleMenu );


    DXUtil_Timer( TIMER_START );    // Start the accurate timer


	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: DestroyObjects()
// Desc:
//-----------------------------------------------------------------------------
void DestroyObjects()
{
    DXUtil_Timer( TIMER_STOP );

	SAFE_DELETE( WM );
	SAFE_DELETE( GAMEINFO );

	SAFE_DELETE( INPUT );
	SAFE_DELETE( MUSIC );
	SAFE_DELETE( SOUND );
	SAFE_DELETE( TM );
	SAFE_DELETE( SCREEN );
}


//-----------------------------------------------------------------------------
// Name: RestoreObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT RestoreObjects()
{
	/////////////////////
	// Restore the window
	/////////////////////
	
    // Set window size
    RECT rcWnd;
	if( SCREEN->IsWindowed() )
	{
		SetRect( &rcWnd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );
  		AdjustWindowRect( &rcWnd, g_dwWindowStyle, FALSE );
	}
	else	// if fullscreen
	{
		SetRect( &rcWnd, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) );
	}

	// Bring the window to the foreground
    SetWindowPos( g_hWndMain, 
				  HWND_NOTOPMOST, 
				  0, 
				  0, 
				  RECTWIDTH(rcWnd), 
				  RECTHEIGHT(rcWnd),
                  0 );


	///////////////////////////
	// Restore all game objects
	///////////////////////////

	SCREEN->Restore();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: InvalidateObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT InvalidateObjects()
{
	SCREEN->Invalidate();
	
	return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Update()
// Desc:
//-----------------------------------------------------------------------------
void Update()
{
	float fDeltaTime = DXUtil_Timer( TIMER_GETELAPSEDTIME );
	
	// This was a hack to fix timing issues with the old WindowSelectSong
	//
	if( fDeltaTime > 0.050f )	// we dropped > 5 frames
		fDeltaTime = 0.050f;
	
	MUSIC->Update( fDeltaTime );

	WM->Update( fDeltaTime );


	static DeviceInputArray diArray;
	diArray.RemoveAll();
	INPUT->GetDeviceInputs( diArray );

	DeviceInput DeviceI;
	PadInput PadI;
	PlayerInput PlayerI;

	for( int i=0; i<diArray.GetSize(); i++ )
	{
		DeviceI = diArray[i];

		GAMEINFO->DeviceToPad( DeviceI, PadI );
		GAMEINFO->PadToPlayer( PadI, PlayerI );

		WM->Input( DeviceI, PadI, PlayerI );
	}

}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc:
//-----------------------------------------------------------------------------
void Render()
{
	HRESULT hr = SCREEN->BeginFrame();
	switch( hr )
	{
		case D3DERR_DEVICELOST:
			// The user probably alt-tabbed out of fullscreen.
			// Do not render a frame until we re-acquire the device
			break;
		case D3DERR_DEVICENOTRESET:
			InvalidateObjects();

            // Resize the device
            if( SUCCEEDED( hr = SCREEN->Reset() ) )
            {
                // Initialize the app's device-dependent objects
                RestoreObjects();
				return;
            }
			else
			{
				RageErrorHr( "Failed to SCREEN->Reset()", hr );
			}

			break;
		case S_OK:
			{
				// set texture and alpha properties
				LPDIRECT3DDEVICE8 pd3dDevice = SCREEN->GetDevice();

				// calculate view and projection transforms

				D3DXMATRIX matProj;
				D3DXMatrixOrthoOffCenterLH( &matProj, 0, 640, 480, 0, -100, 100 );
				pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

				D3DXMATRIX matView;
				D3DXMatrixIdentity( &matView );
				pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

				D3DXMATRIX matWorld;
				D3DXMatrixIdentity( &matWorld );
				SCREEN->ResetMatrixStack( matWorld );


				// draw the game
				WM->Draw();


				SCREEN->EndFrame();
			}
			break;
	}

	ShowFrame();
}


//-----------------------------------------------------------------------------
// Name: ShowFrame()
// Desc:
//-----------------------------------------------------------------------------
void ShowFrame()
{
	// display the contents of the back buffer to the front
	if( SCREEN )
		SCREEN->ShowFrame();
}


//-----------------------------------------------------------------------------
// Name: SwitchDisplayMode()
// Desc:
//-----------------------------------------------------------------------------
BOOL SwitchDisplayMode( BOOL bWindowed, DWORD dwWidth, DWORD dwHeight, DWORD dwBPP )
{
	InvalidateObjects();

	BOOL bResult = SCREEN->SwitchDisplayMode( bWindowed, dwWidth, dwHeight, dwBPP );
	
	RestoreObjects();

	if( GAMEINFO )
	{
	    GAMEINFO->m_GameOptions.m_bWindowed = bWindowed;
		GAMEINFO->SaveConfigToDisk();
	}

	return bResult;
}


//-----------------------------------------------------------------------------
// Name: WeAreAlone()
// Desc:	check for DirectX 8
//-----------------------------------------------------------------------------
BOOL WeAreAlone (LPSTR szName)
{
   HANDLE hMutex = CreateMutex (NULL, TRUE, szName);
   if (GetLastError() == ERROR_ALREADY_EXISTS)
   {
      CloseHandle(hMutex);
      return FALSE;
   }
   return TRUE;
}




