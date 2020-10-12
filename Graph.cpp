/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of Graph
 */

#ifdef _WIN32
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#endif

#include "Graph.h"
#include <cairomm/context.h>

using namespace std;

void DrawString(
	float x,
	float y,
	const Cairo::RefPtr<Cairo::Context>& cr,
	const string& str,
	const Pango::FontDescription& font);
void DrawStringVertical(
	float x,
	float y,
	const Cairo::RefPtr<Cairo::Context>& cr,
	const string& str,
	const Pango::FontDescription& font);
void GetStringWidth(
	const Cairo::RefPtr<Cairo::Context>& cr,
	const string& str,
	int& width,
	int& height,
	const Pango::FontDescription& font);

Series* Graphable::GetSeries(const string& name)
{
	if(m_series.find(name) == m_series.end())
		m_series[name] = new Series;
	return m_series[name];
}

bool Graphable::visible()
{
	return true;
}

Graphable::~Graphable()
{
	for(auto x : m_series)
		delete x.second;
	m_series.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Graph::Graph(size_t update_ms)
: m_font("sans normal 8")
, m_lmargin(70)
, m_rmargin(20)
, m_tmargin(10)
, m_bmargin(20)
{
	m_font.set_weight(Pango::WEIGHT_NORMAL);

	//Set our timer
	sigc::slot<bool> slot = sigc::bind(sigc::mem_fun(*this, &Graph::OnTimer), 1);
	sigc::connection conn = Glib::signal_timeout().connect(slot, update_ms);

	m_minScale = 0;
	m_maxScale = 100;
	m_scaleBump = 10;
	m_units = "%";
	m_unitScale = 1;
	m_timeScale = 10;
	m_timeTick = 10;
	m_drawLegend = true;

	m_lineWidth = 1;

	//Redlines default to off scale
	m_minRedline = -1;
	m_maxRedline = 101;
}

Graph::~Graph()
{
}

bool Graph::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	Glib::RefPtr<Gdk::Window> window = get_bin_window();
	if(window)
	{
		//Get dimensions
		Gtk::Allocation allocation = get_allocation();
		m_width = allocation.get_width();
		m_height = allocation.get_height();

		//Grab time
		m_now = GetTime();

		cr->save();

			//Calculate dimensions
			m_bottom = m_height - m_bmargin;
			m_top = m_tmargin;
			m_left = m_lmargin;
			m_right = m_width - m_rmargin;
			m_bodywidth = m_right - m_left;
			m_bodyheight = m_bottom - m_top;
			m_pheight = m_bodyheight / (m_maxScale - m_minScale);

			//Calculate size of legend
			int legendvspace = 5;
			int lineheight = 0;
			int legendw = 0;
			int legendh = 0;
			for(size_t i=0; i<m_series.size(); i++)
			{
				int w, h;
				GetStringWidth(cr, m_series[i]->m_name, w, h, m_font);
				if(w > legendw)
					legendw = w;
				lineheight = legendvspace + h;
				legendh += lineheight;
			}

			//Clip to window area
			cr->rectangle(0, 0, m_width, m_height);
			cr->clip();

			//Fill background
			cr->set_source_rgb(1.0, 1.0, 1.0);
			cr->rectangle(m_left, m_top, m_bodywidth, m_bodyheight);
			cr->fill();

			//Draw red lines for limits
			cr->set_source_rgb(1, 0.8, 0.8);
			if(m_minRedline > m_minScale)
			{
				cr->move_to(m_left, valueToPosition(m_minRedline));
				cr->line_to(m_right, valueToPosition(m_minRedline));
				cr->line_to(m_right, m_bottom);
				cr->line_to(m_left, m_bottom);
				cr->fill();
			}
			if(m_maxRedline < m_maxScale)
			{
				cr->move_to(m_left, valueToPosition(m_maxRedline));
				cr->line_to(m_right, valueToPosition(m_maxRedline));
				cr->line_to(m_right, m_top);
				cr->line_to(m_left, m_top);
				cr->fill();
			}

			//Draw axes
			cr->set_line_width(1.0);
			cr->set_source_rgb(0, 0, 0);
			cr->move_to(m_left + 0.5, m_top);
			cr->line_to(m_left + 0.5, m_bottom + 0.5);
			cr->line_to(m_right + 0.5, m_bottom + 0.5);
			cr->stroke();

			//Draw grid
			vector<double> dashes;
			dashes.push_back(1);
			float pos = m_right;
			for(int dt=0; ; dt += m_timeTick)	//Vertical grid lines
			{
				//Get current position
				pos = timeToPosition(m_now - dt);
				if(pos <= m_left)
					break;

				//Draw line
				int ipos = static_cast<int>(pos);
				cr->set_dash(dashes, 0);
				cr->set_line_width(0.5);
				cr->move_to(ipos + 0.5, m_bottom + 0.5);
				cr->line_to(pos + 0.5, m_top);
				cr->stroke();
				cr->unset_dash();

				//Format text
				char buf[32];
				if(m_timeTick < 3600)		//m:s
					sprintf(buf, "%d:%02d", dt / 60, dt % 60);
				else if(m_timeTick < 86400)	//h:m
					sprintf(buf, "%d:%02d", dt / 3600, (dt % 3600) / 60 );
				else						//days
					sprintf(buf, "%d", dt / 86400);
				cr->set_line_width(1.0);

				//Calculate text size
				int xw, yw;
				GetStringWidth(cr, buf, xw, yw, m_font);

				//Draw it
				int texty = m_bottom + 5;
				DrawString(pos - 20, texty, cr, buf, m_font);

				//Bump margins if we don't fit
				if(m_bmargin < (yw+5))
					m_bmargin = yw+5;
			}
			for(float i=m_minScale + m_scaleBump; i<=m_maxScale; i += m_scaleBump)		//Horizontal grid lines
			{
				//Get current position
				pos = valueToPosition(i);

				//Draw line
				int ipos = static_cast<int>(pos);
				cr->set_dash(dashes, 0);
				cr->set_line_width(0.5);
				cr->move_to(m_left, ipos + 0.5);
				cr->line_to(m_right, ipos + 0.5);
				cr->stroke();
				cr->stroke();
				cr->unset_dash();

				//Format text
				char buf[32];
				sprintf(buf, "%.0f %s", i * m_unitScale, m_units.c_str());
				if(m_unitScale <= 0.1)
					sprintf(buf, "%.1f %s", i * m_unitScale, m_units.c_str());
				if(m_unitScale <= 0.01)
					sprintf(buf, "%.2f %s", i * m_unitScale, m_units.c_str());
				if(m_unitScale <= 0.001)
					sprintf(buf, "%.3f %s", i * m_unitScale, m_units.c_str());
				cr->set_line_width(1.0);

				//Calculate text size
				int xw, yw;
				GetStringWidth(cr, buf, xw, yw, m_font);

				//Draw it
				int xleft = m_left - xw - 5;
				DrawString(xleft, pos - 5, cr, buf, m_font);

				//bump margins if we need to
				if(xleft < 5)
					m_lmargin = xw + 5;
			}

			//Draw Y axis title
			DrawStringVertical(10, m_bodyheight / 2, cr, m_yAxisTitle, m_font);

			//Draw lines for each child
			for(size_t i=0; i<m_series.size(); i++)
			{
				Graphable* pNode = m_series[i];
				if( pNode->visible() && (pNode->m_series.find(m_seriesName) != pNode->m_series.end()) )
					DrawSeries(pNode->m_series[m_seriesName], cr, pNode->m_color);
			}

			if(m_drawLegend)
			{
				//Draw legend background
				int legendmargin = 2;
				int legendoffset = 2;
				int legendright = m_left + legendw + 2*legendmargin + legendoffset;
				cr->set_source_rgb(1, 1, 1);
				cr->move_to(m_left + legendoffset,	m_top + legendoffset);
				cr->line_to(m_left + legendoffset,	legendh + 2*legendmargin + m_top + legendoffset);
				cr->line_to(legendright, 			legendh + 2*legendmargin + m_top + legendoffset);
				cr->line_to(legendright,			m_top + legendoffset);
				cr->fill();

				//Draw text
				int y = legendmargin + lineheight + legendoffset;
				for(size_t i=0; i<m_series.size(); i++)
				{
					Graphable* pSeries = m_series[i];
					Gdk::Color& color = pSeries->m_color;
					cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());

					DrawString(
						m_left + legendmargin + legendoffset,
						y,
						cr,
						pSeries->m_name,
						m_font);

					y += lineheight;
				}
			}

		cr->restore();
	}

	return true;
}

void Graph::DrawSeries(Series* pSeries, const Cairo::RefPtr<Cairo::Context>& cr, Gdk::Color color)
{
	cr->set_line_width(m_lineWidth);
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());

	cr->save();

	cr->rectangle(m_left, m_top, m_bodywidth, m_bodyheight);
	cr->clip();

	//Draw the line
	Series::iterator lit = pSeries->begin();
	float y_prev1 = valueToPosition(lit->value);
	float y_prev2 = y_prev1;
	cr->move_to(timeToPosition(lit->time), y_prev1);
	++lit;
	for(; lit!=pSeries->end(); ++lit)
	{
		float x = timeToPosition(lit->time);
		float y = valueToPosition(lit->value);
		if(x < 0)
		{
			cr->move_to(0, y);
			continue;
		}

		//Calculate moving average
		float ya = (y + y_prev1 + y_prev2) / 3;
		cr->line_to(x, ya);

		//Shift back
		y_prev2 = y_prev1;
		y_prev1 = y;
	}
	cr->stroke();

	cr->restore();
}

float Graph::valueToPosition(float val)
{
	float range = m_maxScale - m_minScale;		//units of plot height
	float scale = m_bodyheight / range;			//pixels per unit
	return m_top + m_bodyheight - (val - m_minScale)*scale;
}

float Graph::timeToPosition(double time)
{
	return m_right - ((m_now - time) * m_timeScale);
}

bool Graph::OnTimer(int nTimer)
{
	if(nTimer == 1)
	{
		//Update view
		queue_draw();
	}

	//false to stop timer
	return true;
}

double GetTime()
{
	timespec t;
	clock_gettime(CLOCK_REALTIME,&t);
	double d = static_cast<double>(t.tv_nsec) / 1E9f;
	d += t.tv_sec;
	return d;
}

void DrawStringVertical(
	float x,
	float y,
	const Cairo::RefPtr<Cairo::Context>& cr,
	const string& str,
	const Pango::FontDescription& font)
{
	cr->save();

		cr->set_line_width(1.0);

		Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
		tlayout->set_font_description(font);
		tlayout->set_text(str);

		Pango::Rectangle ink, logical;
		tlayout->get_extents(ink, logical);

		float delta = (logical.get_width()/2) / Pango::SCALE;
		cr->move_to(x, y + delta);
		cr->rotate(- M_PI / 2);

		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
		cr->stroke();

	cr->restore();
}

/**
	@brief Draws a string

	@param x	X coordinate
	@param y	Y position
	@param cr	Cairo context
	@param str	String to draw
	@param font	The font to use
 */
void DrawString(
	float x,
	float y,
	const Cairo::RefPtr<Cairo::Context>& cr,
	const string& str,
	const Pango::FontDescription& font)
{
	cr->save();

		Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
		cr->move_to(x, y);
		tlayout->set_font_description(font);
		tlayout->set_text(str);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);

	cr->restore();
}

void GetStringWidth(
	const Cairo::RefPtr<Cairo::Context>& cr,
	const string& str,
	int& width,
	int& height,
	const Pango::FontDescription& font)
{
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	tlayout->set_font_description(font);
	tlayout->set_text(str);
	tlayout->get_pixel_size(width, height);
}
