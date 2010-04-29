// ZigZag.cpp
/*
 * Copyright (c) 2009, Dan Heeks
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "stdafx.h"
#include "ZigZag.h"
#include "CNCConfig.h"
#include "ProgramCanvas.h"
#include "interface/PropertyLength.h"
#include "interface/PropertyChoice.h"
#include "tinyxml/tinyxml.h"
#include "Reselect.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include "PythonStuff.h"
#include "../kurve/geometry/geometry.h"

#include <sstream>

int CZigZag::number_for_stl_file = 1;

static void on_set_minx(double value, HeeksObj* object){((CZigZag*)object)->m_params.m_box.m_x[0] = value;}
static void on_set_maxx(double value, HeeksObj* object){((CZigZag*)object)->m_params.m_box.m_x[3] = value;}
static void on_set_miny(double value, HeeksObj* object){((CZigZag*)object)->m_params.m_box.m_x[1] = value;}
static void on_set_maxy(double value, HeeksObj* object){((CZigZag*)object)->m_params.m_box.m_x[4] = value;}
static void on_set_step_over(double value, HeeksObj* object){((CZigZag*)object)->m_params.m_step_over = value;}
static void on_set_direction(int value, HeeksObj* object){((CZigZag*)object)->m_params.m_direction = value;}
static void on_set_material_allowance(double value, HeeksObj* object){((CZigZag*)object)->m_params.m_material_allowance = value;}
static void on_set_style(int value, HeeksObj* object){((CZigZag*)object)->m_params.m_style = value;}

void CZigZagParams::GetProperties(CZigZag* parent, std::list<Property *> *list)
{
	list->push_back(new PropertyLength(_("minimum x"), m_box.m_x[0], parent, on_set_minx));
	list->push_back(new PropertyLength(_("maximum x"), m_box.m_x[3], parent, on_set_maxx));
	list->push_back(new PropertyLength(_("minimum y"), m_box.m_x[1], parent, on_set_miny));
	list->push_back(new PropertyLength(_("maximum y"), m_box.m_x[4], parent, on_set_maxy));
	list->push_back(new PropertyLength(_("step over"), m_step_over, parent, on_set_step_over));
	{
		std::list< wxString > choices;
		choices.push_back(_("X"));
		choices.push_back(_("Y"));
		list->push_back(new PropertyChoice(_("direction"), choices, m_direction, parent, on_set_direction));
	}
	list->push_back(new PropertyLength(_("material allowance"), m_material_allowance, parent, on_set_material_allowance));
	{
		std::list< wxString > choices;
		choices.push_back(_("one way"));
		choices.push_back(_("back and forth"));
		list->push_back(new PropertyChoice(_("style"), choices, m_style, parent, on_set_style));
	}
}

void CZigZagParams::WriteXMLAttributes(TiXmlNode *root)
{
	TiXmlElement * element;
	element = new TiXmlElement( "params" );
	root->LinkEndChild( element );
	element->SetDoubleAttribute("minx", m_box.m_x[0]);
	element->SetDoubleAttribute("maxx", m_box.m_x[3]);
	element->SetDoubleAttribute("miny", m_box.m_x[1]);
	element->SetDoubleAttribute("maxy", m_box.m_x[4]);
	element->SetDoubleAttribute("step_over", m_step_over);
	element->SetAttribute("dir", m_direction);
	element->SetDoubleAttribute("material_allowance", m_material_allowance);
	element->SetAttribute("style", m_style);
}

void CZigZagParams::ReadFromXMLElement(TiXmlElement* pElem)
{
	// get the attributes
	pElem->Attribute("minx", &m_box.m_x[0]);
	pElem->Attribute("maxx", &m_box.m_x[3]);
	pElem->Attribute("miny", &m_box.m_x[1]);
	pElem->Attribute("maxy", &m_box.m_x[4]);
	pElem->Attribute("step_over", &m_step_over);
	pElem->Attribute("dir", &m_direction);
	pElem->Attribute("material_allowance", &m_material_allowance);
	pElem->Attribute("style", &m_style);
}

CZigZag::CZigZag(const std::list<int> &solids, const int cutting_tool_number)
    :CDepthOp(GetTypeString(), NULL, cutting_tool_number, ZigZagType), m_solids(solids)
{
	ReadDefaultValues();

	// set m_box from the extents of the solids
	for(std::list<int>::const_iterator It = solids.begin(); It != solids.end(); It++)
	{
		int solid = *It;
		HeeksObj* object = heeksCAD->GetIDObject(SolidType, solid);
		if(object)
		{
			if(object->GetType() == StlSolidType)
			{
				object->GetBox(m_params.m_box);
			}
			else
			{
				double extents[6];
				if(heeksCAD->BodyGetExtents(object, extents))
				{
					m_params.m_box.Insert(CBox(extents));
				}
			}

			Add(object, NULL);
		}
	}

	SetDepthOpParamsFromBox();

	// add tool radius all around the box
	if(m_params.m_box.m_valid)
	{
		CCuttingTool *pCuttingTool = CCuttingTool::Find(m_cutting_tool_number);
		if(pCuttingTool)
		{
			double extra = pCuttingTool->m_params.m_diameter/2 + 0.01;
			m_params.m_box.m_x[0] -= extra;
			m_params.m_box.m_x[1] -= extra;
			m_params.m_box.m_x[3] += extra;
			m_params.m_box.m_x[4] += extra;
		}
	}
}

CZigZag::CZigZag( const CZigZag & rhs ) : CDepthOp(rhs)
{
	*this = rhs;	// Call the assignment operator.
}

CZigZag & CZigZag::operator= ( const CZigZag & rhs )
{
	if (this != &rhs)
	{
		CDepthOp::operator =(rhs);

		m_solids.clear();
		std::copy( rhs.m_solids.begin(), rhs.m_solids.end(), std::inserter( m_solids, m_solids.begin() ) );

		m_params = rhs.m_params;
		// static int number_for_stl_file;
	}

	return(*this);
}

/**
	The old version of the CZigZag object stored references to graphics as type/id pairs
	that get read into the m_solids list.  The new version stores these graphics references
	as child elements (based on ObjList).  If we read in an old-format file then the m_solids
	list will have data in it for which we don't have children.  This routine converts
	these type/id pairs into the HeeksObj pointers as children.
 */
void CZigZag::ReloadPointers()
{
	for (std::list<int>::iterator symbol = m_solids.begin(); symbol != m_solids.end(); symbol++)
	{
		HeeksObj *object = heeksCAD->GetIDObject( SolidType, *symbol );
		if (object != NULL)
		{
			Add( object, NULL );
		}
	}

	CDepthOp::ReloadPointers();
}

void CZigZag::SetDepthOpParamsFromBox()
{
	m_depth_op_params.m_start_depth = m_params.m_box.MaxZ();
	m_depth_op_params.m_clearance_height = m_params.m_box.MaxZ() + 5.0;
	m_depth_op_params.m_final_depth = m_params.m_box.MinZ();
	m_depth_op_params.m_rapid_down_to_height = m_params.m_box.MaxZ() + 2.0;
	m_depth_op_params.m_step_down = m_params.m_box.Depth(); // set it to a finishing pass
}

void CZigZag::AppendTextToProgram(const CFixture *pFixture)
{
    ReloadPointers();   // Make sure all the solids in m_solids are included as child objects.

	CCuttingTool *pCuttingTool = CCuttingTool::Find(m_cutting_tool_number);
	if(pCuttingTool == NULL)
	{
		return;
	}

	CDepthOp::AppendTextToProgram(pFixture);

	// write the corner radius
	theApp.m_program_canvas->AppendText(_T("corner_radius = float("));
	double cr = pCuttingTool->m_params.m_corner_radius - pCuttingTool->m_params.m_flat_radius;
	if(cr<0)cr = 0.0;
	theApp.m_program_canvas->AppendText( cr / theApp.m_program->m_units );
	theApp.m_program_canvas->AppendText(_T(")\n"));

	heeksCAD->CreateUndoPoint();

	//write stl file
	std::list<HeeksObj*> solids;
	for (HeeksObj *object = GetFirstChild(); object != NULL; object = GetNextChild())
	{
	    if (object->GetType() != SolidType && object->GetType() != StlSolidType)
	    {
	        continue;
	    }

		if (object != NULL)
		{
			// Need to rotate a COPY of the solid by the fixture settings.
			HeeksObj* copy = object->MakeACopy();
			if (copy != NULL)
			{
				double m[16];	// A different form of the transformation matrix.
				CFixture::extract( pFixture->GetMatrix(), m );

                copy->ModifyByMatrix(m);
                solids.push_back(copy);
            } // End if - then
        } // End if - then
	} // End for


    wxStandardPaths standard_paths;
    wxFileName filepath( standard_paths.GetTempDir().c_str(), wxString::Format(_T("zigzag%d.stl"), number_for_stl_file).c_str() );
	number_for_stl_file++;

	heeksCAD->SaveSTLFile(solids, filepath.GetFullPath(), 0.01);

	// We don't need the duplicate solids any more.  Delete them.
	for (std::list<HeeksObj*>::iterator l_itSolid = solids.begin(); l_itSolid != solids.end(); l_itSolid++)
	{
		heeksCAD->Remove( *l_itSolid );
	} // End for
	heeksCAD->Changed();

#ifdef UNICODE
	std::wostringstream ss;
#else
    std::ostringstream ss;
#endif
    ss.imbue(std::locale("C"));

			// Rotate the coordinates to align with the fixture.
			gp_Pnt min = pFixture->Adjustment( gp_Pnt( m_params.m_box.m_x[0], m_params.m_box.m_x[1], m_params.m_box.m_x[2] ) );
			gp_Pnt max = pFixture->Adjustment( gp_Pnt( m_params.m_box.m_x[3], m_params.m_box.m_x[4], m_params.m_box.m_x[5] ) );

	ss << "ocl_funcs.zigzag(" << PythonString(filepath.GetFullPath()).c_str() << ", tool_diameter, corner_radius, " << m_params.m_step_over / theApp.m_program->m_units << ", " << min.X() / theApp.m_program->m_units << ", " << max.X() / theApp.m_program->m_units << ", " << min.Y() / theApp.m_program->m_units << ", " << max.Y() / theApp.m_program->m_units << ", " << ((m_params.m_direction == 0) ? "'X'" : "'Y'") << ", " << m_params.m_material_allowance / theApp.m_program->m_units << ", " << m_params.m_style << ", clearance, rapid_down_to_height, start_depth, step_down, final_depth, "<<theApp.m_program->m_units<<")\n";

	theApp.m_program_canvas->m_textCtrl->AppendText(ss.str().c_str());
}

void CZigZag::glCommands(bool select, bool marked, bool no_color)
{
	CDepthOp::glCommands(select, marked, no_color);
}

void CZigZag::GetProperties(std::list<Property *> *list)
{
	AddSolidsProperties(list, m_solids);
	m_params.GetProperties(this, list);
	CDepthOp::GetProperties(list);
}

HeeksObj *CZigZag::MakeACopy(void)const
{
	return new CZigZag(*this);
}

void CZigZag::CopyFrom(const HeeksObj* object)
{
	operator=(*((CZigZag*)object));
}

bool CZigZag::CanAddTo(HeeksObj* owner)
{
	return owner->GetType() == OperationsType;
}

void CZigZag::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element = new TiXmlElement( "ZigZag" );
	root->LinkEndChild( element );
	m_params.WriteXMLAttributes(element);

	// write solid ids
	for(std::list<int>::iterator It = m_solids.begin(); It != m_solids.end(); It++)
	{
		int solid = *It;
		TiXmlElement * solid_element = new TiXmlElement( "solid" );
		element->LinkEndChild( solid_element );
		solid_element->SetAttribute("id", solid);
	}

	WriteBaseXML(element);
}

// static member function
HeeksObj* CZigZag::ReadFromXMLElement(TiXmlElement* element)
{
	CZigZag* new_object = new CZigZag;

	std::list<TiXmlElement *> elements_to_remove;

	// read solid ids
	for(TiXmlElement* pElem = TiXmlHandle(element).FirstChildElement().Element(); pElem; pElem = pElem->NextSiblingElement())
	{
		std::string name(pElem->Value());
		if(name == "params"){
			new_object->m_params.ReadFromXMLElement(pElem);
			elements_to_remove.push_back(pElem);
		}
		else if(name == "solid"){
			for(TiXmlAttribute* a = pElem->FirstAttribute(); a; a = a->Next())
			{
				std::string name(a->Name());
				if(name == "id"){
					int id = a->IntValue();
					new_object->m_solids.push_back(id);
				}
			}
			elements_to_remove.push_back(pElem);
		}
	}

	for (std::list<TiXmlElement*>::iterator itElem = elements_to_remove.begin(); itElem != elements_to_remove.end(); itElem++)
	{
		element->RemoveChild(*itElem);
	}

	new_object->ReadBaseXML(element);

	return new_object;
}

bool CZigZag::CanAdd(HeeksObj* object)
{
	switch (object->GetType())
	{
	case SolidType:
	case SketchType:
		return(true);

	default:
		return(false);
	}
}

void CZigZag::WriteDefaultValues()
{
	CDepthOp::WriteDefaultValues();

	CNCConfig config(ConfigScope());
	config.Write(wxString(GetTypeString()) + _T("BoxXMin"), m_params.m_box.m_x[0]);
	config.Write(wxString(GetTypeString()) + _T("BoxYMin"), m_params.m_box.m_x[1]);
	config.Write(wxString(GetTypeString()) + _T("BoxXMax"), m_params.m_box.m_x[3]);
	config.Write(wxString(GetTypeString()) + _T("BoxYMax"), m_params.m_box.m_x[4]);
	config.Write(wxString(GetTypeString()) + _T("StepOver"), m_params.m_step_over);
	config.Write(wxString(GetTypeString()) + _T("Direction"), m_params.m_direction);
	config.Write(wxString(GetTypeString()) + _T("Direction"), m_params.m_direction);
	config.Write(wxString(GetTypeString()) + _T("Style"), m_params.m_style);
}

void CZigZag::ReadDefaultValues()
{
	CDepthOp::ReadDefaultValues();

	CNCConfig config(ConfigScope());
	config.Read(wxString(GetTypeString()) + _T("BoxXMin"), &m_params.m_box.m_x[0], -7.0);
	config.Read(wxString(GetTypeString()) + _T("BoxYMin"), &m_params.m_box.m_x[1], -7.0);
	config.Read(wxString(GetTypeString()) + _T("BoxXMax"), &m_params.m_box.m_x[3], 7.0);
	config.Read(wxString(GetTypeString()) + _T("BoxYMax"), &m_params.m_box.m_x[4], 7.0);
	config.Read(wxString(GetTypeString()) + _T("StepOver"), &m_params.m_step_over, 1.0);
	config.Read(wxString(GetTypeString()) + _T("Direction"), &m_params.m_direction, 0);
	config.Read(wxString(GetTypeString()) + _T("MatAllowance"), &m_params.m_material_allowance, 0.0);
	config.Read(wxString(GetTypeString()) + _T("Style"), &m_params.m_style, 0);
}


class ResetBoundary: public Tool {
public:
	// Tool's virtual functions
	const wxChar* GetTitle(){return _("Reset Boundary");}
	void Run()
	{
	    CBox bounding_box;

	    for (HeeksObj *object = m_pThis->GetFirstChild(); object != NULL; object = m_pThis->GetNextChild())
	    {
                object->GetBox(bounding_box);
		}

        // add tool radius all around the box
        if(bounding_box.m_valid)
        {
            CCuttingTool *pCuttingTool = CCuttingTool::Find(m_pThis->m_cutting_tool_number);
            if(pCuttingTool)
            {
                double extra = pCuttingTool->m_params.m_diameter/2 + 0.01;
                bounding_box.m_x[0] -= extra;
                bounding_box.m_x[1] -= extra;
                bounding_box.m_x[3] += extra;
                bounding_box.m_x[4] += extra;
            }
        }

	    m_pThis->m_params.m_box = bounding_box;
		m_pThis->SetDepthOpParamsFromBox();
	}

public:
	void Set( CZigZag *pThis )
	{
	    m_pThis = pThis;
	}

	wxString BitmapPath(){ return _T("import");}
	CZigZag *m_pThis;
};

static ResetBoundary reset_boundary;



static ReselectSolids reselect_solids;

void CZigZag::GetTools(std::list<Tool*>* t_list, const wxPoint* p)
{
	reselect_solids.m_solids = &m_solids;
	reselect_solids.m_object = this;
	t_list->push_back(&reselect_solids);

	reset_boundary.Set(this);
	t_list->push_back(&reset_boundary);

	CDepthOp::GetTools( t_list, p );
}
