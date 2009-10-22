// HeeksCNC.cpp
/*
 * Copyright (c) 2009, Dan Heeks
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "stdafx.h"
#include <errno.h>

#ifndef WIN32
	#include <dirent.h>
#endif

#include <wx/stdpaths.h>
#include <wx/dynlib.h>
#include <wx/aui/aui.h>
#include "interface/PropertyString.h"
#include "interface/Observer.h"
#include "interface/ToolImage.h"
#include "PythonStuff.h"
#include "Program.h"
#include "ProgramCanvas.h"
#include "OutputCanvas.h"
#include "CNCConfig.h"
#include "NCCode.h"
#include "Profile.h"
#include "Pocket.h"
#include "ZigZag.h"
#include "Adaptive.h"
#include "Drilling.h"
#include "Locating.h"
#include "CuttingTool.h"
#include "CounterBore.h"
#include "TurnRough.h"
#include "Fixture.h"
#include "SpeedReference.h"
#include "CuttingRate.h"
#include "Operations.h"
#include "Fixtures.h"
#include "Tools.h"
#include "CuttingTool.h"
#include "interface/strconv.h"
#include "CNCPoint.h"
#include "BOM.h"
#include "Probing.h"

#include <sstream>

CHeeksCADInterface* heeksCAD = NULL;

CHeeksCNCApp theApp;

CHeeksCNCApp::CHeeksCNCApp(){
	m_draw_cutter_radius = true;
	m_program = NULL;
	m_run_program_on_new_line = false;
	m_machiningBar = NULL;
}

CHeeksCNCApp::~CHeeksCNCApp(){
}

void CHeeksCNCApp::OnInitDLL()
{
}

void CHeeksCNCApp::OnDestroyDLL()
{
	// save any settings
	//config.Write("SolidSimWorkingDir", m_working_dir_for_solid_sim);

#if !defined WXUSINGDLL
	wxUninitialize();
#endif

	heeksCAD = NULL;
}

void OnMachiningBar( wxCommandEvent& event )
{
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();
	wxAuiPaneInfo& pane_info = aui_manager->GetPane(theApp.m_machiningBar);
	if(pane_info.IsOk()){
		pane_info.Show(event.IsChecked());
		aui_manager->Update();
	}
}

void OnUpdateMachiningBar( wxUpdateUIEvent& event )
{
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();
	event.Check(aui_manager->GetPane(theApp.m_machiningBar).IsShown());
}

void OnProgramCanvas( wxCommandEvent& event )
{
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();
	wxAuiPaneInfo& pane_info = aui_manager->GetPane(theApp.m_program_canvas);
	if(pane_info.IsOk()){
		pane_info.Show(event.IsChecked());
		aui_manager->Update();
	}
}

void OnUpdateProgramCanvas( wxUpdateUIEvent& event )
{
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();
	event.Check(aui_manager->GetPane(theApp.m_program_canvas).IsShown());
}

static void OnOutputCanvas( wxCommandEvent& event )
{
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();
	wxAuiPaneInfo& pane_info = aui_manager->GetPane(theApp.m_output_canvas);
	if(pane_info.IsOk()){
		pane_info.Show(event.IsChecked());
		aui_manager->Update();
	}
}

static void OnUpdateOutputCanvas( wxUpdateUIEvent& event )
{
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();
	event.Check(aui_manager->GetPane(theApp.m_output_canvas).IsShown());
}

static bool GetSketches(std::list<int>& sketches, std::list<int> &cutting_tools )
{
	// check for at least one sketch selected

	const std::list<HeeksObj*>& list = heeksCAD->GetMarkedList();
	for(std::list<HeeksObj*>::const_iterator It = list.begin(); It != list.end(); It++)
	{
		HeeksObj* object = *It;
		if(object->GetType() == SketchType)
		{
			sketches.push_back(object->m_id);
		} // End if - then

		if ((object != NULL) && (object->GetType() == CuttingToolType))
		{
			cutting_tools.push_back( object->m_id );
		} // End if - then
	}

	if(sketches.size() == 0)
	{
		wxMessageBox(_("You must select some sketches, first!"));
		return false;
	}

	return true;
}

static void NewProfileOpMenuCallback(wxCommandEvent &event)
{
	std::list<int> drill_bits;
	std::list<int> cutting_tools;
	std::list<int> sketches;
	int milling_cutting_tool_number = -1;
	if(GetSketches(sketches, cutting_tools))
	{
		// Look through the cutting tools that have been selected.  If any of them
		// are drill or centre bits then create Drilling cycle objects as well.
		// If we find a milling bit then pass that through to the CProfile object.
		std::list<int>::const_iterator l_itTool;
		for (l_itTool = cutting_tools.begin(); l_itTool != cutting_tools.end(); l_itTool++)
		{
			HeeksObj *ob = heeksCAD->GetIDObject( CuttingToolType, *l_itTool );
			if (ob != NULL)
			{
				switch (((CCuttingTool *)ob)->m_params.m_type)
				{
					case CCuttingToolParams::eDrill:
					case CCuttingToolParams::eCentreDrill:
						// Keep a list for later.  We need to create the Profile object
						// before we create Drilling objects that refer to it.
						drill_bits.push_back( *l_itTool );
						break;

					case CCuttingToolParams::eChamfer:
					case CCuttingToolParams::eEndmill:
					case CCuttingToolParams::eSlotCutter:
					case CCuttingToolParams::eBallEndMill:
						// We only want one.  Just keep overwriting this variable.
						milling_cutting_tool_number = ((CCuttingTool *)ob)->m_tool_number;
						break;

					default:
						break;
				} // End switch
			} // End if - then
		} // End for

		CProfile *new_object = new CProfile(sketches, milling_cutting_tool_number);
		theApp.m_program->Operations()->Add(new_object,NULL);
		heeksCAD->ClearMarkedList();
		heeksCAD->Mark(new_object);

		CDrilling::Symbols_t profiles;
		profiles.push_back( CDrilling::Symbol_t( new_object->GetType(), new_object->m_id ) );

		for (l_itTool = drill_bits.begin(); l_itTool != drill_bits.end(); l_itTool++)
		{
			HeeksObj *ob = heeksCAD->GetIDObject( CuttingToolType, *l_itTool );
			if (ob != NULL)
			{
				CDrilling *new_object = new CDrilling( profiles, ((CCuttingTool *)ob)->m_tool_number, -1 );
				theApp.m_program->Operations()->Add(new_object, NULL);
				heeksCAD->ClearMarkedList();
				heeksCAD->Mark(new_object);
			} // End if - then
		} // End for
	}
}

static void NewPocketOpMenuCallback(wxCommandEvent &event)
{
	std::list<int> cutting_tools;
	std::list<int> sketches;
	if(GetSketches(sketches, cutting_tools))
	{
		CPocket *new_object = new CPocket(sketches, (cutting_tools.size()>0)?(*cutting_tools.begin()):-1 );
		theApp.m_program->Operations()->Add(new_object, NULL);
		heeksCAD->ClearMarkedList();
		heeksCAD->Mark(new_object);
	}
}

static void NewZigZagOpMenuCallback(wxCommandEvent &event)
{
	// check for at least one solid selected
	std::list<int> solids;

	const std::list<HeeksObj*>& list = heeksCAD->GetMarkedList();
	for(std::list<HeeksObj*>::const_iterator It = list.begin(); It != list.end(); It++)
	{
		HeeksObj* object = *It;
		if(object->GetType() == SolidType || object->GetType() == StlSolidType)solids.push_back(object->m_id);
	}

	// if no selected solids, 
	if(solids.size() == 0)
	{
		// use all the solids in the drawing
		for(HeeksObj* object = heeksCAD->GetFirstObject();object; object = heeksCAD->GetNextObject())
		{
			if(object->GetType() == SolidType || object->GetType() == StlSolidType)solids.push_back(object->m_id);
		}
	}

	if(solids.size() == 0)
	{
		wxMessageBox(_("There are no solids!"));
		return;
	}

	CZigZag *new_object = new CZigZag(solids);
	theApp.m_program->Operations()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}

static void NewAdaptiveOpMenuCallback(wxCommandEvent &event)
{
	std::list<int> solids;
	std::list<int> sketches;
	int cutting_tool_number = 0;
	int reference_object_type = -1;
	unsigned int reference_object_id = -1;

	const std::list<HeeksObj*>& list = heeksCAD->GetMarkedList();
	for(std::list<HeeksObj*>::const_iterator It = list.begin(); It != list.end(); It++)
	{
		HeeksObj* object = *It;
		if(object->GetType() == SolidType || object->GetType() == StlSolidType)solids.push_back(object->m_id);
		if(object->GetType() == SketchType) sketches.push_back(object->m_id);
		if(object->GetType() == CuttingToolType) cutting_tool_number = ((CCuttingTool *)object)->m_tool_number;
		if((object->GetType() == PointType) ||
		   (object->GetType() == DrillingType))
		{
			reference_object_type = object->GetType();
			reference_object_id = object->m_id;
		} // End if - then
	}

	// if no selected solids, 
	if(solids.size() == 0)
	{
		// use all the solids in the drawing
		for(HeeksObj* object = heeksCAD->GetFirstObject();object; object = heeksCAD->GetNextObject())
		{
			if(object->GetType() == SolidType || object->GetType() == StlSolidType)solids.push_back(object->m_id);
		}
	}

	if(solids.size() == 0)
	{
		wxMessageBox(_("There are no solids!"));
		return;
	}
#if 0
	if(sketches.size() == 0)
	{
		wxMessageBox(_("You must select some sketches first!"));
		return;
	}
#endif
	CAdaptive *new_object = new CAdaptive(	solids, 
						sketches, 
						cutting_tool_number, 
						reference_object_type, 
						reference_object_id);
	theApp.m_program->Operations()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}


static void NewDrillingOpMenuCallback(wxCommandEvent &event)
{
	std::vector<CNCPoint> intersections;
	CDrilling::Symbols_t symbols;
	CDrilling::Symbols_t cuttingTools;
	int cutting_tool_number = 0;

	const std::list<HeeksObj*>& list = heeksCAD->GetMarkedList();
	for(std::list<HeeksObj*>::const_iterator It = list.begin(); It != list.end(); It++)
	{
		HeeksObj* object = *It;
		if (object->GetType() == CuttingToolType)
		{
			cuttingTools.push_back( CDrilling::Symbol_t( object->GetType(), object->m_id ) );
			cutting_tool_number = ((CCuttingTool *)object)->m_tool_number;
		} // End if - then
		else
		{
			symbols.push_back( CDrilling::Symbol_t( object->GetType(), object->m_id ) );
		} // End if - else
	} // End for

	double depth = -1;
	CDrilling::Symbols_t cuttingToolsThatMatchCircles;
	CDrilling drill( symbols, -1, -1 );

	intersections = drill.FindAllLocations( symbols );

	if ((cuttingTools.size() == 0) && (cuttingToolsThatMatchCircles.size() > 0))
	{
		// The operator didn't point to a cutting tool object and one of the circles that they
		// did point to happenned to match the diameter of an existing cutting tool.  Use that
		// one as our default.  The operator can always overwrite it later on.

		std::copy( cuttingToolsThatMatchCircles.begin(), cuttingToolsThatMatchCircles.end(),
				std::inserter( cuttingTools, cuttingTools.begin() ));
	} // End if - then

	if (intersections.size() == 0)
	{
		wxMessageBox(_("You must select some points, circles or other intersecting elements first!"));
		return;
	}

	if(cuttingTools.size() == 0)
	{
		wxMessageBox(_("You haven't selected a cutting tool for this hole.  By default no 'tool definition' or 'select tool' code will be generated for this drilling cycle."));
	}

	if(cuttingTools.size() > 1)
	{
		wxMessageBox(_("You may only select a single cutting tool for each drilling operation.!"));
		return;
	}

	CDrilling *new_object = new CDrilling( symbols, cutting_tool_number, depth );
	theApp.m_program->Operations()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}

static void NewLocatingOpMenuCallback(wxCommandEvent &event)
{
	std::vector<CNCPoint> intersections;
	CDrilling::Symbols_t symbols;

	const std::list<HeeksObj*>& list = heeksCAD->GetMarkedList();
	for(std::list<HeeksObj*>::const_iterator It = list.begin(); It != list.end(); It++)
	{
		HeeksObj* object = *It;
		if (object != NULL)
		{
			symbols.push_back( CDrilling::Symbol_t( object->GetType(), object->m_id ) );
		} // End if - then
	} // End for

	CLocating *new_object = new CLocating( symbols );
	theApp.m_program->Operations()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}

static void NewProbe_Centre_MenuCallback(wxCommandEvent &event)
{
	CCuttingTool::ToolNumber_t cutting_tool_number = 0;

	const std::list<HeeksObj*>& list = heeksCAD->GetMarkedList();
	for(std::list<HeeksObj*>::const_iterator It = list.begin(); It != list.end(); It++)
	{
		HeeksObj* object = *It;
		if ((object != NULL) && (object->GetType() == CuttingToolType))
		{
			cutting_tool_number = ((CCuttingTool *)object)->m_tool_number;
		} // End if - then
	} // End for

	CProbe_Centre *new_object = new CProbe_Centre( cutting_tool_number );
	theApp.m_program->Operations()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}


static void NewProbe_Edge_MenuCallback(wxCommandEvent &event)
{
	CCuttingTool::ToolNumber_t cutting_tool_number = 0;

	const std::list<HeeksObj*>& list = heeksCAD->GetMarkedList();
	for(std::list<HeeksObj*>::const_iterator It = list.begin(); It != list.end(); It++)
	{
		HeeksObj* object = *It;
		if ((object != NULL) && (object->GetType() == CuttingToolType))
		{
			cutting_tool_number = ((CCuttingTool *)object)->m_tool_number;
		} // End if - then
	} // End for

	CProbe_Edge *new_object = new CProbe_Edge( cutting_tool_number );
	theApp.m_program->Operations()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}


static void NewFixtureMenuCallback(wxCommandEvent &event)
{
	if (CFixture::GetNextFixture() > 0)
	{
		CFixture::eCoordinateSystemNumber_t coordinate_system_number = CFixture::eCoordinateSystemNumber_t(CFixture::GetNextFixture());

		CFixture *new_object = new CFixture( NULL, coordinate_system_number );
		theApp.m_program->Fixtures()->Add(new_object, NULL);
		heeksCAD->ClearMarkedList();
		heeksCAD->Mark(new_object);
	} // End if - then
	else
	{
		wxMessageBox(_T("There are no more coordinate systems available"));
	} // End if - else
}


static void DesignRulesAdjustment(const bool apply_changes)
{
	std::list<wxString> changes;

	HeeksObj* operations = theApp.m_program->Operations();

	for(HeeksObj* obj = operations->GetFirstChild(); obj; obj = operations->GetNextChild())
	{
		if (COp::IsAnOperation( obj->GetType() ))
		{
			if (obj != NULL)
			{
				std::list<wxString> change = ((COp *)obj)->DesignRulesAdjustment(apply_changes);
				std::copy( change.begin(), change.end(), std::inserter( changes, changes.end() ));
			} // End if - then
		} // End if - then
	} // End for

	std::wostringstream l_ossChanges;
	for (std::list<wxString>::const_iterator l_itChange = changes.begin(); l_itChange != changes.end(); l_itChange++)
	{
		l_ossChanges << l_itChange->c_str();
	} // End for

	if (l_ossChanges.str().size() > 0)
	{
		wxMessageBox( l_ossChanges.str().c_str() );
	} // End if - then

} // End DesignRulesAdjustmentMenuCallback() routine


static void DesignRulesAdjustmentMenuCallback(wxCommandEvent &event)
{
	DesignRulesAdjustment(true);
} // End DesignRulesAdjustmentMenuCallback() routine

static void DesignRulesCheckMenuCallback(wxCommandEvent &event)
{
	DesignRulesAdjustment(false);
} // End DesignRulesCheckMenuCallback() routine

static void NewCounterBoreOpMenuCallback(wxCommandEvent &event)
{
	std::vector<CNCPoint> intersections;
	CCounterBore::Symbols_t symbols;
	CCounterBore::Symbols_t cuttingTools;
	int cutting_tool_number = 0;

	const std::list<HeeksObj*>& list = heeksCAD->GetMarkedList();
	for(std::list<HeeksObj*>::const_iterator It = list.begin(); It != list.end(); It++)
	{
		HeeksObj* object = *It;
		if (object->GetType() == CuttingToolType)
		{
			cuttingTools.push_back( CCounterBore::Symbol_t( object->GetType(), object->m_id ) );
			cutting_tool_number = ((CCuttingTool *)object)->m_tool_number;
		} // End if - then
		else
		{
			symbols.push_back( CCounterBore::Symbol_t( object->GetType(), object->m_id ) );
		} // End if - else
	} // End for

	CCounterBore::Symbols_t cuttingToolsThatMatchCircles;
	CCounterBore counterbore( symbols, -1 );
	intersections = counterbore.FindAllLocations( symbols, NULL );

	if ((cuttingTools.size() == 0) && (cuttingToolsThatMatchCircles.size() > 0))
	{
		// The operator didn't point to a cutting tool object and one of the circles that they
		// did point to happenned to match the diameter of an existing cutting tool.  Use that
		// one as our default.  The operator can always overwrite it later on.

		std::copy( cuttingToolsThatMatchCircles.begin(), cuttingToolsThatMatchCircles.end(),
				std::inserter( cuttingTools, cuttingTools.begin() ));
	} // End if - then

	if (intersections.size() == 0)
	{
		wxMessageBox(_("You must select some points, circles or other intersecting elements first!"));
		return;
	}

	if(cuttingTools.size() == 0)
	{
		wxMessageBox(_("You haven't selected a cutting tool for this hole.  By default no 'tool definition' or 'select tool' code will be generated for this drilling cycle."));
	}

	if(cuttingTools.size() > 1)
	{
		wxMessageBox(_("You may only select a single cutting tool for each drilling operation.!"));
		return;
	}

	CCounterBore *new_object = new CCounterBore( symbols, cutting_tool_number );
	theApp.m_program->Operations()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}

static void NewSpeedReferenceMenuCallback(wxCommandEvent &event)
{
	CSpeedReference *new_object = new CSpeedReference(_T("Fill in material name"), int(CCuttingToolParams::eCarbide), 0.0, 0.0);
	theApp.m_program->SpeedReferences()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}

static void NewCuttingRateMenuCallback(wxCommandEvent &event)
{
	CCuttingRate *new_object = new CCuttingRate(0.0, 0.0);
	theApp.m_program->SpeedReferences()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}

static void NewRoughTurnOpMenuCallback(wxCommandEvent &event)
{
	std::list<int> cutting_tools;
	std::list<int> sketches;
	if(GetSketches(sketches, cutting_tools))
	{
		CTurnRough *new_object = new CTurnRough(sketches, (cutting_tools.size()>0)?(*cutting_tools.begin()):-1 );
		theApp.m_program->Operations()->Add(new_object, NULL);
		heeksCAD->ClearMarkedList();
		heeksCAD->Mark(new_object);
	}
}

static void AddNewCuttingTool(CCuttingToolParams::eCuttingToolType type)
{
	// Add a new cutting tool.
	CCuttingTool *new_object = new CCuttingTool(NULL, type, heeksCAD->GetNextID(CuttingToolType));
	theApp.m_program->Tools()->Add(new_object, NULL);
	heeksCAD->ClearMarkedList();
	heeksCAD->Mark(new_object);
}

static void NewDrillMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eDrill);
}

static void NewCentreDrillMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eCentreDrill);
}

static void NewEndmillMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eEndmill);
}

static void NewSlotCutterMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eSlotCutter);
}

static void NewBallEndMillMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eBallEndMill);
}

static void NewChamferMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eChamfer);
}

static void NewTurningToolMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eTurningTool);
}

static void NewTouchProbeMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eTouchProbe);
}

static void NewToolLengthSwitchMenuCallback(wxCommandEvent &event)
{
	AddNewCuttingTool(CCuttingToolParams::eToolLengthSwitch);
}

static void MakeScriptMenuCallback(wxCommandEvent &event)
{
	// create the Python program
	theApp.m_program->RewritePythonProgram();
}

static void RunPythonScript()
{
	{
		// clear the output file
		wxFile f(theApp.m_program->GetOutputFileName().c_str(), wxFile::write);
		if(f.IsOpened())f.Write(_T("\n"));
	}
	{
		// clear the backplot file
		wxString backplot_path = theApp.m_program->GetOutputFileName() + _T(".nc.xml");
		wxFile f(backplot_path.c_str(), wxFile::write);
		if(f.IsOpened())f.Write(_T("\n"));
	}
	HeeksPyPostProcess(theApp.m_program, theApp.m_program->GetOutputFileName(), true );
}

static void RunScriptMenuCallback(wxCommandEvent &event)
{
	RunPythonScript();
}

static void PostProcessMenuCallback(wxCommandEvent &event)
{
	// write the python program
	theApp.m_program->RewritePythonProgram();

	// run it
	RunPythonScript();
}

static void CancelMenuCallback(wxCommandEvent &event)
{
	HeeksPyCancel();
}

static void OpenNcFileMenuCallback(wxCommandEvent& event)
{
	wxString ext_str(_T("*.*")); // to do, use the machine's NC extension
	wxString wildcard_string = wxString(_("NC files")) + _T(" |") + ext_str;
    wxFileDialog dialog(theApp.m_output_canvas, _("Open NC file"), wxEmptyString, wxEmptyString, wildcard_string);
    dialog.CentreOnParent();

    if (dialog.ShowModal() == wxID_OK)
    {
		HeeksPyBackplot(theApp.m_program, theApp.m_program, dialog.GetPath().c_str());
	}
}

static void OpenBOMFileMenuCallback(wxCommandEvent& event)
{
	wxString ext_str(_T("*.*")); // to do, use the machine's NC extension
	wxString wildcard_string = wxString(_("BOM files")) + _T(" |") + ext_str;
    wxFileDialog dialog(theApp.m_output_canvas, _("Open BOM file"), wxEmptyString, wxEmptyString, wildcard_string);
    dialog.CentreOnParent();

    if (dialog.ShowModal() == wxID_OK)
    {
		theApp.m_program->Owner()->Add(new CBOM(dialog.GetPath()),NULL);
	}
}


static void SaveNcFileMenuCallback(wxCommandEvent& event)
{
	wxString ext_str(_T("*.tap")); // to do, use the machine's NC extension
	wxString wildcard_string = wxString(_("NC files")) + _T(" |") + ext_str;
	wxFileDialog fd(theApp.m_output_canvas, _("Save NC file"), wxEmptyString, wxEmptyString, wildcard_string, wxSAVE|wxOVERWRITE_PROMPT);
	fd.SetFilterIndex(1);
	if (fd.ShowModal() == wxID_OK)
	{
		wxString nc_file_str = fd.GetPath().c_str();
		{
			wxFile ofs(nc_file_str.c_str(), wxFile::write);
			if(!ofs.IsOpened())
			{
				wxMessageBox(wxString(_("Couldn't open file")) + _T(" - ") + nc_file_str);
				return;
			}

			ofs.Write(theApp.m_output_canvas->m_textCtrl->GetValue());
		}
		HeeksPyBackplot(theApp.m_program, theApp.m_program, nc_file_str);
	}
}

// a class to re-use existing "OnButton" functions in a Tool class
class CCallbackTool: public Tool{
public:
	wxString m_title;
	wxString m_bitmap_path;
	void(*m_callback)(wxCommandEvent&);

	CCallbackTool(const wxString& title, const wxString& bitmap_path, void(*callback)(wxCommandEvent&)): m_title(title), m_bitmap_path(bitmap_path), m_callback(callback){}

	// Tool's virtual functions
	const wxChar* GetTitle(){return m_title;}
	void Run(){
		wxCommandEvent dummy_evt;
		(*m_callback)(dummy_evt);
	}
	wxString BitmapPath(){ return m_bitmap_path;}
};

static CCallbackTool new_drill_tool(_("New Drill..."), _T("drill"), NewDrillMenuCallback);
static CCallbackTool new_centre_drill_tool(_("New Centre Drill..."), _T("centredrill"), NewCentreDrillMenuCallback);
static CCallbackTool new_endmill_tool(_("New End Mill..."), _T("endmill"), NewEndmillMenuCallback);
static CCallbackTool new_slotdrill_tool(_("New Slot Drill..."), _T("slotdrill"), NewSlotCutterMenuCallback);
static CCallbackTool new_ball_end_mill_tool(_("New Ball End Mill..."), _T("ballmill"), NewBallEndMillMenuCallback);
static CCallbackTool new_chamfer_mill_tool(_("New Chamfer Mill..."), _T("chamfmill"), NewChamferMenuCallback);
static CCallbackTool new_turn_tool(_("New Turning Tool..."), _T("turntool"), NewTurningToolMenuCallback);
static CCallbackTool new_touch_probe(_("New Touch Probe..."), _T("probe"), NewTouchProbeMenuCallback);
static CCallbackTool new_tool_length_switch(_("New Tool Length Switch..."), _T("probe"), NewToolLengthSwitchMenuCallback);

void CHeeksCNCApp::GetNewCuttingToolTools(std::list<Tool*>* t_list)
{
	t_list->push_back(&new_drill_tool);
	t_list->push_back(&new_centre_drill_tool);
	t_list->push_back(&new_endmill_tool);
	t_list->push_back(&new_slotdrill_tool);
	t_list->push_back(&new_ball_end_mill_tool);
	t_list->push_back(&new_chamfer_mill_tool);
	t_list->push_back(&new_turn_tool);
	t_list->push_back(&new_touch_probe);
	t_list->push_back(&new_tool_length_switch);
}

static CCallbackTool new_profile_operation(_("New Profile Operation..."), _T("opprofile"), NewProfileOpMenuCallback);
static CCallbackTool new_pocket_operation(_("New Pocket Operation..."), _T("pocket"), NewPocketOpMenuCallback);
static CCallbackTool new_zigzag_operation(_("New ZigZag Operation..."), _T("zigzag"), NewZigZagOpMenuCallback);
static CCallbackTool new_adaptive_rough_operation(_("New Adaptive Roughing Operation..."), _T("adapt"), NewAdaptiveOpMenuCallback);
static CCallbackTool new_drilling_operation(_("New Drilling Operation..."), _T("drilling"), NewDrillingOpMenuCallback);
static CCallbackTool new_counterbore_operation(_("New CounterBore Operation..."), _T("counterbore"), NewCounterBoreOpMenuCallback);
static CCallbackTool new_rough_turn_operation(_("New Rough Turning Operation..."), _T("turnrough"), NewRoughTurnOpMenuCallback);

void CHeeksCNCApp::GetNewOperationTools(std::list<Tool*>* t_list)
{
	t_list->push_back(&new_profile_operation);
	t_list->push_back(&new_pocket_operation);
	t_list->push_back(&new_zigzag_operation);
	t_list->push_back(&new_adaptive_rough_operation);
	t_list->push_back(&new_drilling_operation);
	t_list->push_back(&new_counterbore_operation);
	t_list->push_back(&new_rough_turn_operation);
}

static void AddToolBars()
{
	wxFrame* frame = heeksCAD->GetMainFrame();
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();
	if(theApp.m_machiningBar)delete theApp.m_machiningBar;
	theApp.m_machiningBar = new wxToolBar(frame, -1, wxDefaultPosition, wxDefaultSize, wxTB_NODIVIDER | wxTB_FLAT);
	theApp.m_machiningBar->SetToolBitmapSize(wxSize(ToolImage::GetBitmapSize(), ToolImage::GetBitmapSize()));
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Profile"), ToolImage(_T("opprofile")), _("New Profile Operation..."), NewProfileOpMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Pocket"), ToolImage(_T("pocket")), _("New Pocket Operation..."), NewPocketOpMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("ZigZag"), ToolImage(_T("zigzag")), _("New ZigZag Operation..."), NewZigZagOpMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Adaptive"), ToolImage(_T("adapt")), _("New Special Adaptive Roughing Operation..."), NewAdaptiveOpMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Drill"), ToolImage(_T("drilling")), _("New Drill Cycle Operation..."), NewDrillingOpMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("CounterBore"), ToolImage(_T("counterbore")), _("New CounterBore Cycle Operation..."), NewCounterBoreOpMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Cutting Tool"), ToolImage(_T("drill")), _("New Cutting Tool Definition..."), NewDrillMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Fixture"), ToolImage(_T("fixture")), _("New Fixture..."), NewFixtureMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Design Rules Check"), ToolImage(_("design_rules_check")), _("Design Rules Check..."), DesignRulesCheckMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Design Rules Adjustment"), ToolImage(_("design_rules_adjustment")), _("Design Rules Adjustment..."), DesignRulesAdjustmentMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Speed Reference"), ToolImage(_T("speed_reference")), _("Add Speed Reference..."), NewSpeedReferenceMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Cutting Rate"), ToolImage(_T("cutting_rate")), _("Add Cutting Rate Reference..."), NewCuttingRateMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Locating"), ToolImage(_T("locating")), _("New Locating Operation..."), NewLocatingOpMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Probing"), ToolImage(_T("probe")), _("New Probe Centre Operation..."), NewProbe_Centre_MenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Probing"), ToolImage(_T("probe")), _("New Probe Edge Operation..."), NewProbe_Edge_MenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("PostProcess"), ToolImage(_T("postprocess")), _("Post-Process"), PostProcessMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("OpenNC"), ToolImage(_T("opennc")), _("Open NC File"), OpenNcFileMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("SaveNC"), ToolImage(_T("savenc")), _("Save NC File"), SaveNcFileMenuCallback);
	heeksCAD->AddToolBarButton((wxToolBar*)(theApp.m_machiningBar), _("Cancel"), ToolImage(_T("cancel")), _("Cancel Python Script"), CancelMenuCallback);
	theApp.m_machiningBar->Realize();
	aui_manager->AddPane(theApp.m_machiningBar, wxAuiPaneInfo().Name(_T("MachiningBar")).Caption(_T("Machining tools")).ToolbarPane().Top());
	heeksCAD->RegisterToolBar(theApp.m_machiningBar);
}

void CHeeksCNCApp::OnStartUp(CHeeksCADInterface* h, const wxString& dll_path)
{
	m_dll_path = dll_path;
	heeksCAD = h;
#if !defined WXUSINGDLL
	wxInitialize();
#endif

	CNCConfig config;

	// About box, stuff
	heeksCAD->AddToAboutBox(wxString(_T("\n\n")) + _("HeeksCNC is the free machining add-on to HeeksCAD")
		+ _T("\n") + _("          http://code.google.com/p/heekscnc/")
		+ _T("\n") + _("Written by Dan Heeks, Hirutso Enni, Perttu Ahola, David Nicholls")
		+ _T("\n") + _("With help from archivist, crotchet1, DanielFalck, fenn, Sliptonic")
		+ _T("\n\n") + _("geometry code, donated by Geoff Hawkesford, Camtek GmbH http://www.peps.de/")
		+ _T("\n") + _("pocketing code from http://code.google.com/p/libarea/ , derived from the kbool library written by Klaas Holwerda http://boolean.klaasholwerda.nl/bool.html")
		+ _T("\n") + _("Zig zag code from pycam http://sourceforge.net/svn/?group_id=237831")
		+ _T("\n") + _("Adaptive Roughing code from http://code.google.com/p/libactp/, using code written in 2004")
		+ _T("\n") + _("For a more modern version of adaptive roughing, see Julian Todd and Martin Dunschen of http://www.freesteel.co.uk/")
		+ _T("\n\n") + _("This HeeksCNC software installation is restricted by the GPL license http://www.gnu.org/licenses/gpl-3.0.txt")
		+ _T("\n") + _("  which means it is free and open source, and must stay that way")
		);

	// add menus and toolbars
	wxFrame* frame = heeksCAD->GetMainFrame();
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();

	// tool bars
	heeksCAD->RegisterAddToolBars(AddToolBars);
	AddToolBars();

	// Operations menu
	wxMenu *menuOperations = new wxMenu;
	std::list<Tool*> optools;
	GetNewOperationTools(&optools);
	for(std::list<Tool*>::iterator It = optools.begin(); It != optools.end(); It++)
	{
		Tool* t = *It;
		heeksCAD->AddMenuItem(menuOperations, t->GetTitle(), ToolImage(t->BitmapPath()), ((CCallbackTool*)t)->m_callback);
	}
	heeksCAD->AddMenuItem(menuOperations, _("Design Rules Check..."), ToolImage(_T("design_rules_check")), DesignRulesCheckMenuCallback);
	heeksCAD->AddMenuItem(menuOperations, _("Design Rules Adjustment..."), ToolImage(_T("design_rules_adjustment")), DesignRulesAdjustmentMenuCallback);
	heeksCAD->AddMenuItem(menuOperations, _("New Speed Reference..."), ToolImage(_T("speed_reference")), NewSpeedReferenceMenuCallback);
	heeksCAD->AddMenuItem(menuOperations, _("New Cutting Rate Reference..."), ToolImage(_T("cutting_rate")), NewCuttingRateMenuCallback);
	heeksCAD->AddMenuItem(menuOperations, _("New Locating Operation..."), ToolImage(_T("locating")), NewLocatingOpMenuCallback);
	heeksCAD->AddMenuItem(menuOperations, _("New Probe Centre Operation..."), ToolImage(_T("probe")), NewProbe_Centre_MenuCallback);
	heeksCAD->AddMenuItem(menuOperations, _("New Probe Edge Operation..."), ToolImage(_T("probe")), NewProbe_Edge_MenuCallback);

	// Tools menu
	wxMenu *menuTools = new wxMenu;
	heeksCAD->AddMenuItem(menuTools, _("New Drill..."), ToolImage(_T("drill")), NewDrillMenuCallback);
	heeksCAD->AddMenuItem(menuTools, _("New Centre Drill..."), ToolImage(_T("centredrill")), NewCentreDrillMenuCallback);
	heeksCAD->AddMenuItem(menuTools, _("New End Mill..."), ToolImage(_T("endmill")), NewEndmillMenuCallback);
	heeksCAD->AddMenuItem(menuTools, _("New Slot Drill..."), ToolImage(_T("slotdrill")), NewSlotCutterMenuCallback);
	heeksCAD->AddMenuItem(menuTools, _("New Ball End Mill..."), ToolImage(_T("ballmill")), NewBallEndMillMenuCallback);
	heeksCAD->AddMenuItem(menuTools, _("New Chamfer Mill..."), ToolImage(_T("chamfmill")), NewChamferMenuCallback);
	heeksCAD->AddMenuItem(menuTools, _("New Turning Tool..."), ToolImage(_T("turntool")), NewTurningToolMenuCallback);
	heeksCAD->AddMenuItem(menuTools, _("New Touch Probe..."), ToolImage(_T("probe")), NewTouchProbeMenuCallback);
	heeksCAD->AddMenuItem(menuTools, _("New Tool Length Switch..."), ToolImage(_T("probe")), NewToolLengthSwitchMenuCallback);

	// Fixtures menu
	wxMenu *menuFixtures = new wxMenu;
	heeksCAD->AddMenuItem(menuFixtures, _("New Fixture..."), ToolImage(_T("fixture")), NewFixtureMenuCallback);

	// Machining menu
	wxMenu *menuMachining = new wxMenu;
	heeksCAD->AddMenuItem(menuMachining, _("Operations"), ToolImage(_T("ops")), NULL, NULL, menuOperations);
	heeksCAD->AddMenuItem(menuMachining, _("Tools"), ToolImage(_T("tools")), NULL, NULL, menuTools);
	heeksCAD->AddMenuItem(menuMachining, _("Fixtures"), ToolImage(_T("fixtures")), NULL, NULL, menuFixtures);
	heeksCAD->AddMenuItem(menuMachining, _("Make Python Script"), ToolImage(_T("python")), MakeScriptMenuCallback);
	heeksCAD->AddMenuItem(menuMachining, _("Run Python Script"), ToolImage(_T("postprocess")), RunScriptMenuCallback);
	heeksCAD->AddMenuItem(menuMachining, _("Post-Process"), ToolImage(_T("postprocess")), PostProcessMenuCallback);
	heeksCAD->AddMenuItem(menuMachining, _("Open NC File"), ToolImage(_T("opennc")), OpenNcFileMenuCallback);
	heeksCAD->AddMenuItem(menuMachining, _("Save NC File"), ToolImage(_T("savenc")), SaveNcFileMenuCallback);
	heeksCAD->AddMenuItem(menuMachining, _("Open BOM File"), ToolImage(_T("opennc")), OpenBOMFileMenuCallback);
	frame->GetMenuBar()->Append(menuMachining,  _("Machining"));

	// add the program canvas
    m_program_canvas = new CProgramCanvas(frame);
	aui_manager->AddPane(m_program_canvas, wxAuiPaneInfo().Name(_T("Program")).Caption(_T("Program")).Bottom().BestSize(wxSize(600, 200)));

	// add the output canvas
    m_output_canvas = new COutputCanvas(frame);
	aui_manager->AddPane(m_output_canvas, wxAuiPaneInfo().Name(_T("Output")).Caption(_T("Output")).Bottom().BestSize(wxSize(600, 200)));

	bool program_visible;
	bool output_visible;

	config.Read(_T("ProgramVisible"), &program_visible);
	config.Read(_T("OutputVisible"), &output_visible);

	// Read NC Code colors
	CNCCode::ReadColorsFromConfig();

	// read auto speed set
	CSpeedOp::ReadFromConfig();

	aui_manager->GetPane(m_program_canvas).Show(program_visible);
	aui_manager->GetPane(m_output_canvas).Show(output_visible);

	// add tick boxes for them all on the view menu
	wxMenu* view_menu = heeksCAD->GetWindowMenu();
	heeksCAD->AddMenuItem(view_menu, _T("Program"), wxBitmap(), OnProgramCanvas, OnUpdateProgramCanvas, NULL, true);
	heeksCAD->AddMenuItem(view_menu, _T("Output"), wxBitmap(), OnOutputCanvas, OnUpdateOutputCanvas, NULL, true);
	heeksCAD->AddMenuItem(view_menu, _T("Machining"), wxBitmap(), OnMachiningBar, OnUpdateMachiningBar, NULL, true);
	heeksCAD->RegisterHideableWindow(m_program_canvas);
	heeksCAD->RegisterHideableWindow(m_output_canvas);
	heeksCAD->RegisterHideableWindow(m_machiningBar);

	// add object reading functions
	heeksCAD->RegisterReadXMLfunction("Program", CProgram::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("nccode", CNCCode::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Operations", COperations::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Tools", CTools::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Profile", CProfile::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Pocket", CPocket::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("ZigZag", CZigZag::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Adaptive", CAdaptive::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Drilling", CDrilling::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Locating", CLocating::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("ProbeCentre", CProbe_Centre::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("ProbeEdge", CProbe_Edge::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("CounterBore", CCounterBore::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("CuttingTool", CCuttingTool::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Fixture", CFixture::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("TurnRough", CTurnRough::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("SpeedReferences", CSpeedReferences::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("SpeedReference", CSpeedReference::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("CuttingRate", CCuttingRate::ReadFromXMLElement);
	heeksCAD->RegisterReadXMLfunction("Fixtures", CFixtures::ReadFromXMLElement);

	heeksCAD->SetDefaultLayout(wxString(_T("layout2|name=ToolBar;caption=General Tools;state=2108156;dir=1;layer=10;row=0;pos=0;prop=100000;bestw=279;besth=31;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=GeomBar;caption=Geometry Tools;state=2108156;dir=1;layer=10;row=3;pos=0;prop=100000;bestw=285;besth=31;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=522;floaty=290;floatw=312;floath=71|name=SolidBar;caption=Solid Tools;state=2108156;dir=1;layer=10;row=3;pos=296;prop=100000;bestw=254;besth=31;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=495;floaty=356;floatw=281;floath=71|name=ViewingBar;caption=Viewing Tools;state=2108156;dir=1;layer=10;row=3;pos=561;prop=100000;bestw=248;besth=31;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=498;floaty=441;floatw=275;floath=71|name=TransformBar;caption=Transformation Tools;state=2108159;dir=4;layer=10;row=0;pos=578;prop=100000;bestw=217;besth=31;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=937;floaty=515;floatw=244;floath=71|name=Graphics;caption=Graphics;state=768;dir=5;layer=0;row=0;pos=0;prop=100000;bestw=800;besth=600;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=Objects;caption=Objects;state=2099196;dir=4;layer=1;row=0;pos=0;prop=100000;bestw=300;besth=400;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=204;floaty=327;floatw=318;floath=440|name=Options;caption=Options;state=2099196;dir=4;layer=1;row=0;pos=1;prop=100000;bestw=300;besth=200;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=Input;caption=Input;state=2099196;dir=4;layer=1;row=0;pos=2;prop=100000;bestw=300;besth=200;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=Properties;caption=Properties;state=2099196;dir=4;layer=1;row=0;pos=3;prop=100000;bestw=300;besth=200;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=MachiningBar;caption=Machining tools;state=2108156;dir=1;layer=10;row=0;pos=290;prop=100000;bestw=558;besth=31;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=Program;caption=Program;state=2099196;dir=3;layer=0;row=0;pos=0;prop=100000;bestw=600;besth=200;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|name=Output;caption=Output;state=2099196;dir=3;layer=0;row=0;pos=1;prop=100000;bestw=600;besth=200;minw=-1;minh=-1;maxw=-1;maxh=-1;floatx=-1;floaty=-1;floatw=-1;floath=-1|dock_size(5,0,0)=504|dock_size(4,1,0)=234|dock_size(3,0,0)=110|dock_size(1,10,0)=33|dock_size(1,10,3)=33|")));
}

std::list<wxString> CHeeksCNCApp::GetFileNames( const char *p_szRoot ) const
#ifdef WIN32
{
	std::list<wxString>	results;

	WIN32_FIND_DATA file_data;
	HANDLE hFind;

	std::string pattern = std::string(p_szRoot) + "\\*";
	hFind = FindFirstFile(Ctt(pattern.c_str()), &file_data);

	// Now recurse down until we find document files within 'current' directories.
	if (hFind != INVALID_HANDLE_VALUE) 
	{
		do
		{
			if ((file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) continue;
						
			results.push_back( file_data.cFileName );
		} while (FindNextFile( hFind, &file_data));

		FindClose(hFind);
	} // End if - then

	return(results);
} // End of GetFileNames() method.
#else
{
	// We're in UNIX land now.

	std::list<wxString>	results;

	DIR *pdir = opendir(".");	// Look in the current directory for files
				// whose names begin with "default."
	if (pdir != NULL) 
	{
		struct dirent *pent = NULL;
		while ((pent=readdir(pdir)))
		{
			results.push_back(Ctt(pent->d_name));
		} // End while
		closedir(pdir);
	} // End if - then

	return(results);
} // End of GetFileNames() method
#endif




void CHeeksCNCApp::OnNewOrOpen(bool open)
{
	// check for existance of a program

	bool program_found = false;
	for(HeeksObj* object = heeksCAD->GetFirstObject(); object; object = heeksCAD->GetNextObject())
	{
		if(object->GetType() == ProgramType)
		{
			program_found = true;
			break;
		}
	}

	if(!program_found)
	{
		// add the program
		m_program = new CProgram;
		m_program->AddMissingChildren();
		heeksCAD->GetMainObject()->Add(m_program, NULL);
		theApp.m_program_canvas->Clear();
		theApp.m_output_canvas->Clear();
		heeksCAD->Changed();
	}


	if (! open)
	{
		// Must be a new file.
		// Read in any default speed reference or tool table data.
		std::list<wxString> all_file_names = GetFileNames( "." );
		std::list<wxString> seed_file_names;

		for (std::list<wxString>::iterator l_itFileName = all_file_names.begin();
				l_itFileName != all_file_names.end(); l_itFileName++)
		{
#ifdef UNICODE
			std::wstring l_ssName;
			std::wstring l_ssPrefix = _T("default");
#else
			std::string l_ssName;
			std::string l_ssPrefix = _T("default");
#endif

			l_ssName = l_itFileName->c_str();

			if (l_ssName.substr(0,l_ssPrefix.size()) == l_ssPrefix)
			{
				seed_file_names.push_back(l_ssName.c_str());
			} // End if - then
		} // End for

		seed_file_names.sort();	// Sort them so that the user can assign an order alphabetically if they wish.
		for (std::list<wxString>::const_iterator l_itFile = seed_file_names.begin(); l_itFile != seed_file_names.end(); l_itFile++)
		{

			wxString lowercase_file_name( *l_itFile );
			lowercase_file_name.MakeLower();

			if (lowercase_file_name.Find(_T("speed")) != -1) 
			{
				printf("Importing data from %s\n",  Ttc(l_itFile->c_str()));
				heeksCAD->OpenXMLFile( l_itFile->c_str(), theApp.m_program->SpeedReferences() );
				heeksCAD->Changed();
			} // End if - then
			else if (lowercase_file_name.Find(_T("feed")) != -1) 
			{
				printf("Importing data from %s\n",  Ttc(l_itFile->c_str()));
				heeksCAD->OpenXMLFile( l_itFile->c_str(), theApp.m_program->SpeedReferences() );
				heeksCAD->Changed();
			}
			else if (lowercase_file_name.Find(_T("tool")) != -1) 
			{
				printf("Importing data from %s\n",  Ttc(l_itFile->c_str()));
				heeksCAD->OpenXMLFile( l_itFile->c_str(), theApp.m_program->Tools() );
				heeksCAD->Changed();
			}
			else if (lowercase_file_name.Find(_T("fixture")) != -1) 
			{
				printf("Importing data from %s\n",  Ttc(l_itFile->c_str()));
				heeksCAD->OpenXMLFile( l_itFile->c_str(), theApp.m_program->Fixtures() );
				heeksCAD->Changed();
			}
			else
			{
				printf("possible default filename does not contain either 'speed', 'feed' or 'tool' in its name.\n");
			} // End if - else
		} // End for
	} // End if - then
}

void CHeeksCNCApp::GetOptions(std::list<Property *> *list){
	CNCCode::GetOptions(list);
	CSpeedOp::GetOptions(list);
}

void CHeeksCNCApp::OnFrameDelete()
{
	wxAuiManager* aui_manager = heeksCAD->GetAuiManager();
	CNCConfig config;
	config.Write(_T("ProgramVisible"), aui_manager->GetPane(m_program_canvas).IsShown());
	config.Write(_T("OutputVisible"), aui_manager->GetPane(m_output_canvas).IsShown());
	config.Write(_T("MachiningBarVisible"), aui_manager->GetPane(m_machiningBar).IsShown());

	CNCCode::WriteColorsToConfig();
	CSpeedOp::WriteToConfig();
}

wxString CHeeksCNCApp::GetDllFolder()
{
	return m_dll_path;
}

wxString CHeeksCNCApp::GetResFolder()
{
#if defined(WIN32) || defined(RUNINPLACE) //compile with 'RUNINPLACE=yes make' then skip 'sudo make install'
	return m_dll_path;
#else
	return (m_dll_path + _T("/../../share/heekscnc"));
#endif
}





class MyApp : public wxApp
{
 
 public:
 
   virtual bool OnInit(void);
 
 };
 
 bool MyApp::OnInit(void)
 
 {
 
   return true;
 
 }


 DECLARE_APP(MyApp)
 
 IMPLEMENT_APP(MyApp)
 