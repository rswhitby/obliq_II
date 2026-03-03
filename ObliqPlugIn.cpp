// ObliqPlugIn.cpp : defines the initialization routines for the plug-in.
//

#include "stdafx.h"
#include "rhinoSdkPlugInDeclare.h"
#include "ObliqPlugIn.h"
#include "Resource.h"

// The plug-in object must be constructed before any plug-in classes derived
// from CRhinoCommand. The #pragma init_seg(lib) ensures that this happens.
#pragma warning(push)
#pragma warning(disable : 4073)
#pragma init_seg(lib)
#pragma warning(pop)

// Rhino plug-in declaration
RHINO_PLUG_IN_DECLARE

// Rhino plug-in name
// Provide a short, friendly name for this plug-in.
RHINO_PLUG_IN_NAME(L"Obliq");

// Rhino plug-in id
// Provide a unique uuid for this plug-in.
RHINO_PLUG_IN_ID(L"F5D257EE-2BB1-4B22-BD25-9938E5F0719F");

// Rhino plug-in version
// Provide a version number string for this plug-in.
RHINO_PLUG_IN_VERSION(__DATE__ "  " __TIME__)

// Rhino plug-in description
// Provide a description of this plug-in.
RHINO_PLUG_IN_DESCRIPTION(L"Obliq plug-in for Rhinoceros®. Provides utilities for working with oblique (military, cabinet, etc...) projections in a non-destructive way.");

// Rhino plug-in icon resource id
// Provide an icon resource this plug-in.
// Icon resource should contain 16, 24, 32, 48, and 256-pixel image sizes.
RHINO_PLUG_IN_ICON_RESOURCE_ID(IDI_ICON);

// Rhino plug-in developer declarations
// TODO: fill in the following developer declarations with
// your company information. Note, all of these declarations
// must be present or your plug-in will not load.
//
// When completed, delete the following #error directive.
RHINO_PLUG_IN_DEVELOPER_ORGANIZATION(L"Critical Software Lab");
RHINO_PLUG_IN_DEVELOPER_ADDRESS(L"University of Kentucky\r\nLexington KY 40506");
RHINO_PLUG_IN_DEVELOPER_COUNTRY(L"USA");
RHINO_PLUG_IN_DEVELOPER_PHONE(L"000.000.0000");
RHINO_PLUG_IN_DEVELOPER_FAX(L"000.000.0000");
RHINO_PLUG_IN_DEVELOPER_EMAIL(L"galo.canizares@uky.edu");
RHINO_PLUG_IN_DEVELOPER_WEBSITE(L"https://github.com/criticalsoftware-lab");
RHINO_PLUG_IN_UPDATE_URL(L"https://github.com/criticalsoftware-lab");

// The one and only CObliqPlugIn object
static class CObliqPlugIn thePlugIn;

/////////////////////////////////////////////////////////////////////////////
// CObliqPlugIn definition

CObliqPlugIn& ObliqPlugIn()
{
	// Return a reference to the one and only CObliqPlugIn object
	return thePlugIn;
}

CObliqPlugIn::CObliqPlugIn()
{
	// Description:
	//   CObliqPlugIn constructor. The constructor is called when the
	//   plug-in is loaded and "thePlugIn" is constructed. Once the plug-in
	//   is loaded, CObliqPlugIn::OnLoadPlugIn() is called. The
	//   constructor should be simple and solid. Do anything that might fail in
	//   CObliqPlugIn::OnLoadPlugIn().

	// TODO: Add construction code here
	m_plugin_version = RhinoPlugInVersion();
}

/////////////////////////////////////////////////////////////////////////////
// Required overrides

const wchar_t* CObliqPlugIn::PlugInName() const
{
	// Description:
	//   Plug-in name display string.  This name is displayed by Rhino when
	//   loading the plug-in, in the plug-in help menu, and in the Rhino
	//   interface for managing plug-ins.

	// TODO: Return a short, friendly name for the plug-in.
	return RhinoPlugInName();
}

const wchar_t* CObliqPlugIn::PlugInVersion() const
{
	// Description:
	//   Plug-in version display string. This name is displayed by Rhino
	//   when loading the plug-in and in the Rhino interface for managing
	//   plug-ins.

	// TODO: Return the version number of the plug-in.
	return m_plugin_version;
}

GUID CObliqPlugIn::PlugInID() const
{
	// Description:
	//   Plug-in unique identifier. The identifier is used by Rhino to
	//   manage the plug-ins.

	// TODO: Return a unique identifier for the plug-in.
	// {F5D257EE-2BB1-4B22-BD25-9938E5F0719F}
	return ON_UuidFromString(RhinoPlugInId());
}

/////////////////////////////////////////////////////////////////////////////
// Additional overrides

BOOL CObliqPlugIn::OnLoadPlugIn()
{
	// Description:
	//   Called after the plug-in is loaded and the constructor has been
	//   run. This is a good place to perform any significant initialization,
	//   license checking, and so on.  This function must return TRUE for
	//   the plug-in to continue to load.

	// Remarks:
	//    Plug-ins are not loaded until after Rhino is started and a default document
	//    is created.  Because the default document already exists
	//    CRhinoEventWatcher::On????Document() functions are not called for the default
	//    document.  If you need to do any document initialization/synchronization then
	//    override this function and do it here.  It is not necessary to call
	//    CPlugIn::OnLoadPlugIn() from your derived class.
	// TODO: Add plug-in initialization code here.

	return TRUE;
}

void CObliqPlugIn::OnUnloadPlugIn()
{
	// Description:
	//    Called one time when plug-in is about to be unloaded. By this time,
	//    Rhino's mainframe window has been destroyed, and some of the SDK
	//    managers have been deleted. There is also no active document or active
	//    view at this time. Thus, you should only be manipulating your own objects.
	//    or tools here.

	// TODO: Add plug-in cleanup code here.
}



