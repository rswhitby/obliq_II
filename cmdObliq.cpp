// cmdObliq.cpp : command file
//

#include "stdafx.h"
#include "ObliqPlugIn.h"
#include "ObliqueConduit.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// BEGIN Obliq command
//

#pragma region Obliq command

// Do NOT put the definition of class CCommandObliq in a header
// file. There is only ONE instance of a CCommandObliq class
// and that instance is the static theObliqCommand that appears
// immediately below the class definition.

class CCommandObliq : public CRhinoCommand
{
public:
  // The one and only instance of CCommandObliq is created below.
  // No copy constructor or operator= is required.
  // Values of member variables persist for the duration of the application.

  // CCommandObliq::CCommandObliq()
  // is called exactly once when static theObliqCommand is created.
  CCommandObliq() = default;

  // CCommandObliq::~CCommandObliq()
  // is called exactly once when static theObliqCommand is destroyed.
  // The destructor should not make any calls to the Rhino SDK. 
  // If your command has persistent settings, then override 
  // CRhinoCommand::SaveProfile and CRhinoCommand::LoadProfile.
  ~CCommandObliq() = default;

  // Returns a unique UUID for this command.
  // If you try to use an id that is already being used, then
  // your command will not work. Use GUIDGEN.EXE to make unique UUID.
  UUID CommandUUID() override
  {
    // {538A3CCE-B912-474B-95DF-69BDE41AE976}
    static const GUID ObliqCommand_UUID = 
    {0x538a3cce,0xb912,0x474b,{0x95,0xdf,0x69,0xbd,0xe4,0x1a,0xe9,0x76}};
    return ObliqCommand_UUID;
  }

  // Returns the English command name.
  // If you want to provide a localized command name, then override 
  // CRhinoCommand::LocalCommandName.
  const wchar_t* EnglishCommandName() override { return L"Obliq"; }

  // Rhino calls RunCommand to run the command.
  CRhinoCommand::result RunCommand(const CRhinoCommandContext& context) override;
};

// The one and only CCommandObliq object
// Do NOT create any other instance of a CCommandObliq class.
static class CCommandObliq theObliqCommand;

CRhinoCommand::result CCommandObliq::RunCommand(const CRhinoCommandContext& context)
{
  // CCommandObliq::RunCommand() is called when the user
  // runs the "Obliq".

  // TODO: Add command code here.
  CRhinoDoc* doc = context.Document();
  if (nullptr == doc)
      return CRhinoCommand::failure;

  // Record existing viewport IDs before creating a new one
  ON_SimpleArray<ON_UUID> viewport_ids;
  ON_SimpleArray<CRhinoView*> view_list;
  CRhinoView* view = nullptr;
  int i = 0;

  doc->GetViewList(view_list, CRhinoView::ViewTypeFilter::Model);
  for (i = 0; i < view_list.Count(); i++)
  {
      view = view_list[i];
      if (view)
          viewport_ids.Append(view->ActiveViewportID());
  }
  view_list.Empty();

  // Create the new view
  doc->NewView(ON_3dmView(), true);

  // Find the newly created view by diffing against the old ID list
  doc->GetViewList(view_list, CRhinoView::ViewTypeFilter::Model);
  for (i = 0; i < view_list.Count(); i++)
  {
      view = view_list[i];
      if (view)
      {
          int rc = viewport_ids.Search(view->ActiveViewportID());
          if (rc < 0)
              break;
          else
              view = nullptr;
      }
  }

  if (view)
  {
      ON_Viewport onvp = view->ActiveViewport().VP();
      onvp.SetProjection(ON::parallel_view);

      onvp.SetCameraLocation(ON_3dPoint(0.0, 0.0, 100.0));
      onvp.SetCameraDirection(ON_3dVector(0.0, 0.0, -1.0));
      onvp.SetCameraUp(ON_3dVector(0.0, 1.0, 0.0));

      double hw = 30.0, hh = 30.0;
      onvp.SetFrustum(-hw, hw, -hh, hh, 0.1, 1000.0);
      onvp.SetTargetPoint(ON_3dPoint(0.0, 0.0, 0.0));

      view->ActiveViewport().SetVP(onvp, true);

      ON_3dmView v = view->ActiveViewport().View();
      v.m_name = L"Oblique";
      view->ActiveViewport().SetView(v);
      view->FloatRhinoView(true);

      static CObliqueConduit s_conduit;
      s_conduit.SetObliqueParams(90.0, 1.0);
      s_conduit.SetViewportId(view->ActiveViewportID()); // track which viewport
      s_conduit.Bind(view->ActiveViewport().VP());       // Bind takes const ON_Viewport&
      s_conduit.Enable(doc->RuntimeSerialNumber());

      view->Redraw();
  }
  else
  {
      RhinoApp().Print(L"Ptest: Failed to find new viewport.\n");
      return CRhinoCommand::failure;
  }

  return CRhinoCommand::success;
}

#pragma endregion

//
// END Obliq command
//
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
