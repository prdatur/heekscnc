// CorrelationTool.cpp
/*
 * Copyright (c) 2009, Dan Heeks, Perttu Ahola
 * This program is released under the BSD license. See the file COPYING for
 * details.
 */

#include "stdafx.h"
#include "CorrelationTool.h"
#include "CNCConfig.h"
#include "ProgramCanvas.h"
#include "interface/HeeksObj.h"
#include "interface/PropertyInt.h"
#include "interface/PropertyDouble.h"
#include "interface/PropertyChoice.h"
#include "interface/PropertyString.h"
#include "tinyxml/tinyxml.h"
#include "Drilling.h"
#include "interface/Box.h"

#include <sstream>

extern CHeeksCADInterface* heeksCAD;



void CCorrelationToolParams::set_initial_values()
{
	CNCConfig config;

	config.Read(_T("m_min_correlation_factor"), &m_min_correlation_factor, 0.75);
	config.Read(_T("m_max_scale_threshold"), &m_max_scale_threshold, 1.5);
	config.Read(_T("m_number_of_sample_points"), &m_number_of_sample_points, 10);
}

void CCorrelationToolParams::write_values_to_config()
{
	CNCConfig config;

	config.Write(_T("m_min_correlation_factor"), m_min_correlation_factor);
	config.Write(_T("m_max_scale_threshold"), m_max_scale_threshold);
	config.Write(_T("m_number_of_sample_points"), m_number_of_sample_points);
}

static void on_set_min_correlation_factor(double value, HeeksObj* object)
{
	((CCorrelationTool*)object)->m_params.m_min_correlation_factor = value;
	((CCorrelationTool*)object)->m_similar_symbols = ((CCorrelationTool*)object)->SimilarSymbols( ((CCorrelationTool *)object)->m_reference_symbol ); 
}

static void on_set_scale_threshold(double value, HeeksObj* object)
{
	((CCorrelationTool*)object)->m_params.m_max_scale_threshold = value;
	((CCorrelationTool*)object)->m_similar_symbols = ((CCorrelationTool*)object)->SimilarSymbols( ((CCorrelationTool *)object)->m_reference_symbol ); 
}

static void on_set_number_of_sample_points(int value, HeeksObj* object)
{
	((CCorrelationTool*)object)->m_params.m_number_of_sample_points = value;
	((CCorrelationTool*)object)->m_similar_symbols = ((CCorrelationTool*)object)->SimilarSymbols( ((CCorrelationTool *)object)->m_reference_symbol ); 
}


void CCorrelationToolParams::GetProperties(CCorrelationTool* parent, std::list<Property *> *list)
{
	list->push_back(new PropertyDouble(_("min_correlation_factor"), m_min_correlation_factor, parent, on_set_min_correlation_factor));
	list->push_back(new PropertyDouble(_("scale_threshold"), m_max_scale_threshold, parent, on_set_scale_threshold));
	list->push_back(new PropertyInt(_("number_of_sample_points"), m_number_of_sample_points, parent, on_set_number_of_sample_points));
}

void CCorrelationToolParams::WriteXMLAttributes(TiXmlNode *root)
{
	TiXmlElement * element;
	element = new TiXmlElement( "params" );
	root->LinkEndChild( element );  

	element->SetDoubleAttribute("min_correlation_factor", m_min_correlation_factor);
	element->SetDoubleAttribute("max_scale_threshold", m_max_scale_threshold);

	std::ostringstream l_ossValue;
	l_ossValue << m_number_of_sample_points;
	element->SetAttribute("number_of_sample_points", l_ossValue.str().c_str());
}

void CCorrelationToolParams::ReadParametersFromXMLElement(TiXmlElement* pElem)
{
	if (pElem->Attribute("min_correlation_factor")) m_min_correlation_factor = atof(pElem->Attribute("min_correlation_factor"));
	if (pElem->Attribute("max_scale_threshold")) m_max_scale_threshold = atof(pElem->Attribute("max_scale_threshold"));
	if (pElem->Attribute("number_of_sample_points")) m_number_of_sample_points = atoi(pElem->Attribute("number_of_sample_points"));
}

/**
	This method is called when the CAD operator presses the Python button.  This method generates
	Python source code whose job will be to generate RS-274 GCode.  It's done in two steps so that
	the Python code can be configured to generate GCode suitable for various CNC interpreters.
 */
void CCorrelationTool::AppendTextToProgram()
{
}


/**
	NOTE: The m_title member is a special case.  The HeeksObj code looks for a 'GetShortDesc()' method.  If found, it
	adds a Property called 'Object Title'.  If the value is changed, it tries to call the 'OnEditString()' method.
	That's why the m_title value is not defined here
 */
void CCorrelationTool::GetProperties(std::list<Property *> *list)
{
	m_params.GetProperties(this, list);
	HeeksObj::GetProperties(list);
}



/**
        This is the Graphics Library Commands (from the OpenGL set).  This method calls the OpenGL
        routines to paint the drill action in the graphics window.  The graphics is transient.

        Part of its job is to re-paint the elements that this CDrilling object refers to so that
        we know what CAD objects this CNC operation is referring to.
 */
void CCorrelationTool::glCommands(bool select, bool marked, bool no_color)
{
        if(marked && !no_color)
        {
                for (Symbols_t::const_iterator l_itSymbol = m_similar_symbols.begin(); l_itSymbol != m_similar_symbols.end(); l_itSymbol++)
                {
                        HeeksObj* object = heeksCAD->GetIDObject(l_itSymbol->first, l_itSymbol->second);

                        if(object)object->glCommands(false, true, false);
                } // End for
	} // End if - then
} // End glCommands() method



HeeksObj *CCorrelationTool::MakeACopy(void)const
{
	return new CCorrelationTool(*this);
}

void CCorrelationTool::CopyFrom(const HeeksObj* object)
{
	operator=(*((CCorrelationTool*)object));
}

bool CCorrelationTool::CanAddTo(HeeksObj* owner)
{
	return owner->GetType() == ToolsType;
}

void CCorrelationTool::WriteXML(TiXmlNode *root)
{
	TiXmlElement * element = new TiXmlElement( "CorrelationTool" );
	root->LinkEndChild( element );  
	element->SetAttribute("title", Ttc(m_title.c_str()));

	{
		std::wostringstream l_ossVar;
		l_ossVar << m_reference_symbol.first;
		element->SetAttribute("reference_object_type", Ttc(l_ossVar.str().c_str()));
	}

	{
		std::wostringstream l_ossVar;
		l_ossVar << m_reference_symbol.second;
		element->SetAttribute("reference_object_id", Ttc(l_ossVar.str().c_str()));
	}

	m_params.WriteXMLAttributes(element);
	WriteBaseXML(element);
}

// static member function
HeeksObj* CCorrelationTool::ReadFromXMLElement(TiXmlElement* element)
{

	CCorrelationTool::Symbol_t reference_symbol;

	if (element->Attribute("reference_object_type")) reference_symbol.first = atoi(element->Attribute("reference_object_type"));
	if (element->Attribute("reference_object_id")) reference_symbol.second = atoi(element->Attribute("reference_object_id"));

	wxString title(Ctt(element->Attribute("title")));
	CCorrelationTool* new_object = new CCorrelationTool( title.c_str(), reference_symbol);

	// read point and circle ids
	for(TiXmlElement* pElem = TiXmlHandle(element).FirstChildElement().Element(); pElem; pElem = pElem->NextSiblingElement())
	{
		std::string name(pElem->Value());
		if(name == "params"){
			new_object->m_params.ReadParametersFromXMLElement(pElem);
		}
	}

	new_object->ReadBaseXML(element);

	return new_object;
}


void CCorrelationTool::OnEditString(const wxChar* str){
        m_title.assign(str);
	heeksCAD->WasModified(this);
}


/**
	This routine determines how different the two bounding boxes are in size.  If the difference
	is less than the max_scale_threshold (either bigger or smaller) then it returns 'true'
	indicating that the two objects are of a similar size.

	It also returns the scale factor required to be applied to the sample bounding box to make
	it the same size as the reference object.
 */
bool CCorrelationTool::SimilarScale( 
	const CBox &reference_box, 
	const CBox &sample_box, 
	const double max_scale_threshold,
	double *pRequiredScaling ) const
{
	// Calculate the diagonal distance for each bounding box.  This is as good an estimate
	// as any.

	double sample_diagonal = sqrt( 	(sample_box.Width() * sample_box.Width()) +
					(sample_box.Height() * sample_box.Height()));

	double reference_diagonal = sqrt( 	(reference_box.Width() * reference_box.Width()) +
						(reference_box.Height() * reference_box.Height()));

	if (sample_diagonal > reference_diagonal)
	{
		// It's bigger. How much do we need to scale it down?
		double required_scaling = reference_diagonal / sample_diagonal;

		if (pRequiredScaling) *pRequiredScaling = required_scaling;
		return( (1.0 / required_scaling) < max_scale_threshold);
	} // End if - then
	else
	{
		// It's smaller. How much do we need to scale it up?
		double required_scaling = reference_diagonal / sample_diagonal;

		if (pRequiredScaling) *pRequiredScaling = required_scaling;
		return( required_scaling < max_scale_threshold);
	} // End if - else
} // End SimilarScale() method


/**
	Get a set of correlation points that represent a single object.
 */
CCorrelationTool::CorrelationData_t CCorrelationTool::CorrelationData(      const CCorrelationTool::Symbol_t & sample_symbol,
                                                const CCorrelationTool::Symbol_t & reference_symbol,
                                                const int number_of_sample_points,
                                                const double max_scale_threshold ) const
{
	CorrelationData_t results;
	double required_scaling = 1.0;	// How much do we need to scale the sample object to make it the same
					// size as the reference object?  We need this value so that we can
					// scale the distance values as well.

	HeeksObj *sample_object = heeksCAD->GetIDObject( sample_symbol.first, sample_symbol.second );
	if (! sample_object)
	{
		// Couldn't find reference.  Return an empty set.
		return(results);
	} // End if - then

	HeeksObj *reference_object = heeksCAD->GetIDObject( reference_symbol.first, reference_symbol.second );
	if (! reference_object)
	{
		// Couldn't find reference.  Return an empty set.
		return(results);
	} // End if - then

	// Get each symbol's bounding box.
	CBox reference_box, sample_box;
	reference_object->GetBox(reference_box);
	sample_object->GetBox(sample_box);

	// First see if they're close enough in size to be a possible match.
	if (! SimilarScale( reference_box, sample_box, max_scale_threshold, &required_scaling ))
	{
		// They're too different in size.
		return(results);	// return an empty data set.
	} // End if - then

	// They're close enough in scale to warrant further investigation.  Now roll up your sleeves and
	// describe this thar critter.

	for (int sample=0; sample<number_of_sample_points; sample++)
	{
		// We want to draw a line from the centre of the bounding box out at this
		// sample's angle for a length that is long enough to overlap the bounding box's edge.
		double angle = (double(sample) / 360.0);
	
		double radius = ((max_scale_threshold * reference_box.Width()) + (max_scale_threshold * reference_box.Height()));
		double alpha = 3.1415926 * 2 / number_of_sample_points;
                double theta = alpha * sample;
		double sample_centroid[3];
		sample_box.Centre( sample_centroid );

		// Construct a line from the centre of the sample object to somewhere away from it at this sample's angle.  Make
		// sure we're comparing both sets on the same Z plane.  We will worry about rotation matrices when I get
		// smarter.

		sample_centroid[2] = 0.0;
                double verification_line_end[3] = { (cos( theta ) * radius) + sample_centroid[0], (sin( theta ) * radius) + sample_centroid[1], 0.0 };

		// Now find all intersection points between this verification line and the sample object.
		HeeksObj* verification_line = heeksCAD->NewLine(sample_centroid, verification_line_end);
		if (verification_line)
		{
			verification_line->glCommands(false, true, false);

			std::list<double> intersections;
                        if (sample_object->Intersects( verification_line, &intersections ))
                        {
				CorrelationSample_t correlation_sample;
                                while (((intersections.size() % 3) == 0) && (intersections.size() > 0))
                                {
                                        Point3d intersection;

                                        intersection.x = *(intersections.begin());
                                        intersections.erase(intersections.begin());

                                        intersection.y = *(intersections.begin());
                                        intersections.erase(intersections.begin());

                                        intersection.z = *(intersections.begin());
                                        intersections.erase(intersections.begin());
				
					correlation_sample.m_intersection_points.insert( intersection );
				
					// pythagorus	
					double distance = sqrt( ((intersection.x - sample_centroid[0]) * (intersection.x - sample_centroid[0])) +
								((intersection.y - sample_centroid[1]) * (intersection.y - sample_centroid[1])) );

					// Now scale the distance up or down so that it's proportional to the
					// reference object's bounding box.  We've decided the scales are
					// close enough to be acceptable.  The distance valus are really
					// designed to compare shapes rather than sizes.  Make the two similar.
					distance *= required_scaling;

					correlation_sample.m_intersection_distances.insert( distance );
                                } // End while

				results.insert( std::make_pair( angle, correlation_sample ) );
                        } // End if - then
			else
			{
				// It didn't intersect it.  We need to remember this.
				CorrelationSample_t empty;
				results.insert( std::make_pair( angle, empty ) );
			} // End if - else

			heeksCAD->DeleteUndoably(verification_line);
			verification_line = NULL;
		} // End if - then
	} // End for

	return(results);

} // End CorrelationPoints() method



// Convert the list of symbols into a list of reference coordinates.
std::set<CCorrelationTool::Point3d> CCorrelationTool::ReferencePoints( const CCorrelationTool::Symbols_t & sample_symbols ) const
{
	std::set<CCorrelationTool::Point3d> results;

	for (Symbols_t::const_iterator symbol = sample_symbols.begin(); symbol != sample_symbols.end(); symbol++)
	{
		HeeksObj *ob = heeksCAD->GetIDObject( symbol->first, symbol->second );
		CBox box;
		ob->GetBox(box);
		double centroid[3];
		box.Centre(centroid);

		results.insert( Point3d( centroid[0], centroid[1], centroid[2] ) );
	} // End for

	// Run through these symbols and return the centroid of the bounding box.
	return(results);

} // End ReferencePoints() method



/**
	Find a 'score' (represented as a percentage) that represents how similar to two sets
	of correlation data are.
 */
double CCorrelationTool::Score( const CCorrelationTool::CorrelationData_t & sample, const CCorrelationTool::CorrelationData_t & reference ) const
{
	double distance_score = 0.0;
	double intersections_score = 0.0;

	if (sample.size() == 0)
	{
		return(0.0);
	} // End if - then

	if (reference.size() == 0)
	{
		return(0.0);
	} // End if - then

	if (sample.size() != reference.size())
	{
		return(0.0);	// This should not occur.
	} // End if - then

	CorrelationData_t::const_iterator l_itSample;
	CorrelationData_t::const_iterator l_itReference;

	for (l_itSample = sample.begin(), l_itReference = reference.begin(); (l_itSample != sample.end()) && (l_itReference != reference.end()); l_itSample++, l_itReference++)
	{
		if ((l_itSample->second.m_intersection_points.size() == 0) && (l_itReference->second.m_intersection_points.size() > 0))
		{
			// The reference has something at this angle but the sample doesn't
			intersections_score += 0.0;	// Nothing in common
		} // End if - then

		if ((l_itSample->second.m_intersection_points.size() > 0) && (l_itReference->second.m_intersection_points.size() == 0))
		{
			// The sample has something at this angle but the reference doesn't
			intersections_score += 0.0;	// Nothing in common
		} // End if - then

		if ((l_itSample->second.m_intersection_points.size() == 0) && (l_itReference->second.m_intersection_points.size() == 0))
		{
			// The same for both.
			intersections_score += 1.0;
		} // End if - then

		if (l_itSample->second.m_intersection_points.size() == l_itReference->second.m_intersection_points.size())
		{
			// The same for both.
			intersections_score += 1.0;
		} // End if - then

		distance_score += 1.0;	// Assume a perfect match for now.
		/*
		// Make sure the number of intersections is similar between the two.
		if (l_itReference->m_intersection_points.size() > l_itSample->m_intersection_points.size())
		{
			int num = reference_correlation_data.m_intersection_points.size() - sample_correlation_data.m_intersection_points.size();
			if ((1 - (num / l_itReference->m_intersection_points.size())) < m_params.m_min_correlation_factor) continue;
		} // End if - then
		else
		{
			int num = sample_correlation_data.m_intersection_points.size() - reference_correlation_data.m_intersection_points.size();
			if ((1 - (num / sample_correlation_data.m_intersection_points.size())) < m_params.m_min_correlation_factor) continue;
		} // End if - else

		// Well the number of intersection points is close enough.  Look through the distances and see if they're also
		// within scope.  Specifically, figure out what percentage each angle's distance differs by.  Create an average for this
		// difference set.  If the average difference in size is less than the min_correlation_factor then it's still good enough.
		if (sample_correlation_data.m_intersection_distances.size() == 0) continue;

		std::set<double>::const_iterator l_itSampleDistance;
		std::set<double>::const_iterator l_itReferenceDistance;
		*/
	} // End for

	// Return the average score for all tests
	return((distance_score / reference.size()) + (intersections_score / reference.size())) / 2;

} // End Score() method


/**
	- obtain the bounding box for both the reference and the sample objects.
	- scale the reference object up/down (with a maximum of m_params.m_max_scale_threshold) until
	  both objects are of a similar scale.  If this scaling cannot make their bounding boxes similar
	  enough within the configured maximum threshold then the sample object is discarded as 'not a match'.
	- If the scaling process can produce a similar bounding box, the application draws a logical line
	  from the centroid of each object (reference and sample) at a range of angles (eg: every 'n' degrees
	  around the full 360 circle).  It then intersects this line with the object to find both how many
	  intersections there are and what distance is found from the centroid to the intersection point(s).
	  The difference in these distances for both the reference and sample objects will be combined to
	  produce a correlation factor (score).  If this factor is within the m_params.m_min_correlation_factor
	  then the sample object's centroid is added to this object's reference points.
 */
CCorrelationTool::Symbols_t CCorrelationTool::SimilarSymbols( const CCorrelationTool::Symbol_t & reference_symbol ) const
{
	CCorrelationTool::Symbols_t result_set;
	std::set<CCorrelationTool::Point3d> reference_points;

	HeeksObj *pReference = heeksCAD->GetIDObject( reference_symbol.first, reference_symbol.second );
	if (! pReference)
	{
		// Can't find the reference object.  Return an empty set.
		return(result_set);
	} // End if - then

	CorrelationData_t reference_correlation_data = CorrelationData( reference_symbol, reference_symbol, m_params.m_number_of_sample_points, m_params.m_max_scale_threshold );

	// Scan through all objects looking for something that's like this one.
	for (HeeksObj *ob = heeksCAD->GetFirstObject(); ob != NULL; ob = heeksCAD->GetNextObject())
	{
		if ((ob->GetType() == m_reference_symbol.first) && (ob->m_id == m_reference_symbol.second))
		{
			continue;	// It's the reference object.  They're identicle.
		} // End if - then

		CorrelationData_t sample_correlation_data = CorrelationData(	Symbol_t( ob->GetType(), ob->m_id ),
										reference_symbol,
                                               					m_params.m_number_of_sample_points,
                                               					m_params.m_max_scale_threshold );

		// Now compare the correlation data for both the reference and sample objects to see if we
		// think they're similar.

		if (Score( sample_correlation_data, reference_correlation_data ) > m_params.m_min_correlation_factor)
		{
			result_set.push_back( Symbol_t( ob->GetType(), ob->m_id ) );
		} // End if - then
	} // End for

	return(result_set);

} // End SimilarSimbols() method





/**
 * 	This method looks through the symbols in the list.  If they're PointType objects
 * 	then the object's location is added to the result set.  If it's a circle object
 * 	that doesn't intersect any other element (selected) then add its centre to
 * 	the result set.  Finally, find the intersections of all of these elements and
 * 	add the intersection points to the result set.  We use std::set<Point3d> so that
 * 	we end up with no duplicate points.
 */
std::set<CCorrelationTool::Point3d> CCorrelationTool::FindAllLocations() const
{
	Symbols_t similar_symbols = SimilarSymbols( m_reference_symbol );
	return(ReferencePoints( similar_symbols ));

} // End FindAllLocations() method

