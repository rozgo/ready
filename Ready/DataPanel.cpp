/*  Copyright 2011, 2012 The Ready Bunch

    This file is part of Ready.

    Ready is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Ready is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Ready. If not, see <http://www.gnu.org/licenses/>.         */

// local:
#include "DataPanel.hpp"
#include "BaseRD.hpp"
#include "app.hpp"              // for wxGetApp
#include "frame.hpp"
#include "IDs.hpp"
#include "prefs.hpp"            // for readydir, etc
#include "wxutils.hpp"          // for Warning, CopyTextToClipboard

// wxWidgets:
#include <wx/filename.h>        // for wxFileName
#include <wx/html/htmlwin.h>    // for wxHtmlWindow

#if defined(__WXMAC__) && wxCHECK_VERSION(2,9,0)
    // wxMOD_CONTROL has been changed to mean Command key down
    #define wxMOD_CONTROL wxMOD_RAW_CONTROL
    #define ControlDown RawControlDown
#endif

const wxString change_prefix = wxT("change: ");

// -----------------------------------------------------------------------------

//!!! if we decide to go ahead with this panel then put HtmlInfo class in
// separate files so we can use in HelpPanel as well???

// define a child window for displaying HTML info
class HtmlInfo : public wxHtmlWindow
{
    public:

        HtmlInfo(wxWindow* parent, MyFrame* myframe, wxWindowID id = wxID_ANY,
            const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
            long style = wxHW_SCROLLBAR_AUTO)
            : wxHtmlWindow(parent, id, pos, size, style)
        {
            frame = myframe;
            panel = (DataPanel*)parent;
            editlink = false;
            linkrect = wxRect(0,0,0,0);
        }

        virtual void OnLinkClicked(const wxHtmlLinkInfo& link);
        virtual void OnCellMouseHover(wxHtmlCell* cell, wxCoord x, wxCoord y);
        
        void ClearStatus();  // clear pane's status line
        
        void SetFontSizes(int size);
        void ChangeFontSizes(int size);

    private:

        void OnSize(wxSizeEvent& event);
        void OnMouseMotion(wxMouseEvent& event);
        void OnMouseLeave(wxMouseEvent& event);
        void OnMouseDown(wxMouseEvent& event);
        
        MyFrame* frame;
        DataPanel* panel;
        
        bool editlink;       // open clicked file in editor?
        wxRect linkrect;     // rect for cell containing link

        DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(HtmlInfo, wxHtmlWindow)
    EVT_SIZE          (HtmlInfo::OnSize)
    EVT_MOTION        (HtmlInfo::OnMouseMotion)
    EVT_ENTER_WINDOW  (HtmlInfo::OnMouseMotion)
    EVT_LEAVE_WINDOW  (HtmlInfo::OnMouseLeave)
    EVT_LEFT_DOWN     (HtmlInfo::OnMouseDown)
    EVT_RIGHT_DOWN    (HtmlInfo::OnMouseDown)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

void HtmlInfo::OnLinkClicked(const wxHtmlLinkInfo& link)
{
    wxString url = link.GetHref();
    if ( url.StartsWith(wxT("http:")) || url.StartsWith(wxT("mailto:")) ) {
    // pass http/mailto URL to user's preferred browser/emailer
    #if defined(__WXMAC__)
        // wxLaunchDefaultBrowser doesn't work on Mac with IE (get msg in console.log)
        // but it's easier just to use the Mac OS X open command
        if ( wxExecute(wxT("open ") + url, wxEXEC_ASYNC) == -1 )
            Warning(_("Could not open URL!"));
    #elif defined(__WXGTK__)
        // wxLaunchDefaultBrowser is not reliable on Linux/GTK so we call gnome-open
        if ( wxExecute(wxT("gnome-open ") + url, wxEXEC_ASYNC) == -1 )
            Warning(_("Could not open URL!"));
    #else
        if ( !wxLaunchDefaultBrowser(url) )
            Warning(_("Could not launch browser!"));
    #endif

    } else if ( url.StartsWith(change_prefix) ) {
        Warning(_("Not yet implemented!!!"));

    } else if ( url.StartsWith(_T("prefs:")) ) {
        // user clicked on link to Preferences dialog
        frame->ShowPrefsDialog( url.AfterFirst(':') );

    } else if ( url.StartsWith(wxT("edit:")) ) {
        // open clicked file in user's preferred text editor
        wxString path = url.AfterFirst(':');
        #ifdef __WXMSW__
            path.Replace(wxT("/"), wxT("\\"));
        #endif
        wxFileName fname(path);
        if (!fname.IsAbsolute()) path = readydir + path;
        frame->EditFile(path);


    } else if ( url.StartsWith(wxT("open:")) ) {
        // open clicked file
        wxString path = url.AfterFirst(':');
        #ifdef __WXMSW__
            path.Replace(wxT("/"), wxT("\\"));
        #endif
        wxFileName fname(path);
        if (!fname.IsAbsolute()) path = readydir + path;
        if (editlink) {
            frame->EditFile(path);
        } else {
            frame->Raise();
            frame->OpenFile(path);
        }

    } else {
        // assume it's a link to a local target
        LoadPage(url);
    }
}

// -----------------------------------------------------------------------------

void HtmlInfo::OnCellMouseHover(wxHtmlCell* cell, wxCoord x, wxCoord y)
{
    wxHtmlLinkInfo* link = cell->GetLink(x,y);
    if (link) {
        wxString href = link->GetHref();
        href.Replace(wxT("&"), wxT("&&"));
        panel->SetStatus(href);
        wxPoint pt = ScreenToClient( wxGetMousePosition() );
        linkrect = wxRect(pt.x-x, pt.y-y, cell->GetWidth(), cell->GetHeight());
    } else {
        ClearStatus();
    }
}

// -----------------------------------------------------------------------------

void HtmlInfo::OnMouseMotion(wxMouseEvent& event)
{
    if (!linkrect.IsEmpty()) {
        int x = event.GetX();
        int y = event.GetY();
        if (!linkrect.Contains(x,y)) ClearStatus();
    }
    event.Skip();
}

// -----------------------------------------------------------------------------

void HtmlInfo::OnMouseLeave(wxMouseEvent& event)
{
    ClearStatus();
    event.Skip();
}

// -----------------------------------------------------------------------------

void HtmlInfo::ClearStatus()
{
    panel->SetStatus(wxEmptyString);
    linkrect = wxRect(0,0,0,0);
}

// -----------------------------------------------------------------------------

void HtmlInfo::OnMouseDown(wxMouseEvent& event)
{
    // set flag so ctrl/right-clicked file can be opened in editor
    editlink = event.ControlDown() || event.RightDown();
    event.Skip();
}

// -----------------------------------------------------------------------------

// avoid scroll position being reset to top when wxHtmlWindow is resized
void HtmlInfo::OnSize(wxSizeEvent& event)
{
    int x, y;
    GetViewStart(&x, &y);            // save current position

    wxHtmlWindow::OnSize(event);

    wxString currpage = GetOpenedPage();
    if ( !currpage.IsEmpty() ) {
        LoadPage(currpage);         // reload page
        Scroll(x, y);               // scroll to old position
    }
    
    // prevent wxHtmlWindow::OnSize being called again
    event.Skip(false);
}

// -----------------------------------------------------------------------------

void HtmlInfo::SetFontSizes(int size)
{
    // set font sizes for <FONT SIZE=-2> to <FONT SIZE=+4>
    int f_sizes[7];
    f_sizes[0] = int(size * 0.6);
    f_sizes[1] = int(size * 0.8);
    f_sizes[2] = size;
    f_sizes[3] = int(size * 1.2);
    f_sizes[4] = int(size * 1.4);
    f_sizes[5] = int(size * 1.6);
    f_sizes[6] = int(size * 1.8);
    #ifdef __WXOSX_COCOA__
        SetFonts(wxT("Lucida Grande"), wxT("Monaco"), f_sizes);
    #else
        SetFonts(wxEmptyString, wxEmptyString, f_sizes);
    #endif
}

// -----------------------------------------------------------------------------

void HtmlInfo::ChangeFontSizes(int size)
{
    // changing font sizes resets pos to top, so save and restore pos
    int x, y;
    GetViewStart(&x, &y);
    SetFontSizes(size);
    Scroll(x, y);
    // AKT TODO!!! fix display bug (Mac only?) when size increases so that scroll bar appears
    panel->UpdateButtons();
}

static int infofontsize;   //!!! remove if exported from prefs

// -----------------------------------------------------------------------------

// define a panel that contains a HtmlInfo window

BEGIN_EVENT_TABLE(DataPanel, wxPanel)
    EVT_BUTTON (ID::SmallerButton,  DataPanel::OnSmallerButton)
    EVT_BUTTON (ID::BiggerButton,   DataPanel::OnBiggerButton)
END_EVENT_TABLE()

DataPanel::DataPanel(MyFrame* parent, wxWindowID id) 
    : wxPanel(parent,id), frame(parent)
{
    html = new HtmlInfo(this, frame, wxID_ANY);
    
    infofontsize = helpfontsize;    //!!! remove if exported from prefs
    html->SetFontSizes(infofontsize);

    wxBoxSizer* vbox = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);

    // add buttons at top of html window
    smallerbutt = new wxButton(this, ID::SmallerButton, _("-"), wxDefaultPosition, wxSize(30,wxDefaultCoord));
    biggerbutt = new wxButton(this, ID::BiggerButton, _("+"), wxDefaultPosition, wxSize(30,wxDefaultCoord));

    #ifdef __WXMAC__
        // nicer to use smaller buttons -- also do for Win/Linux???
        smallerbutt->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
        biggerbutt->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
    #endif

    hbox->Add(smallerbutt, 0, wxLEFT | wxTOP | wxBOTTOM | wxALIGN_LEFT, 10);
    hbox->Add(biggerbutt, 0, wxLEFT | wxTOP | wxBOTTOM | wxALIGN_LEFT, 10);

    status = new wxStaticText(this, wxID_STATIC, wxEmptyString);
    #ifdef __WXMAC__
        status->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
    #endif
    wxBoxSizer* statbox = new wxBoxSizer(wxHORIZONTAL);
    statbox->Add(status);
    hbox->Add(statbox, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 10);

    hbox->AddStretchSpacer(1);
    vbox->Add(hbox, 0, wxALL | wxEXPAND | wxALIGN_TOP, 0);
    vbox->Add(html, 1, wxEXPAND | wxALIGN_TOP, 0);
    SetSizer(vbox);
    
    UpdateButtons();

    // install event handlers to detect keyboard shortcuts when html window has focus
    html->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(MyFrame::OnKeyDown), NULL, frame);
    html->Connect(wxEVT_CHAR, wxKeyEventHandler(MyFrame::OnChar), NULL, frame);
}

static int rownum;

// -----------------------------------------------------------------------------

void DataPanel::Update(const BaseRD* const system)
{
    // build HTML string to display current parameters
    wxString contents;
    
    contents += _("<html><body>");
    contents += _("<p><center>");
    contents += _("<table border=1 cellspacing=0 cellpadding=1 width=\"99%\"><tr><td>");
    contents += _("<table border=0 cellspacing=0 cellpadding=4 width=\"100%\">");

    rownum = 0;
    contents += AppendRow(_("Rule name"), system->GetRuleName());
    contents += AppendRow(_("Rule description"), system->GetRuleDescription());
    contents += AppendRow(_("Pattern description"), system->GetPatternDescription());
    contents += AppendRow(_("Timestep"), FormatFloat(system->GetTimestep()));
    
    for(int iParam=0;iParam<(int)system->GetNumberOfParameters();iParam++)
    {
        contents += AppendRow(system->GetParameterName(iParam),
                              FormatFloat(system->GetParameterValue(iParam)));
        if(system->HasEditableFormula()) {
            //!!! need nice way to let user change parameter name
            // maybe just bring up a dialog containing 2 edit boxes (for name and value)???
        } else {
            // don't allow name editing for inbuilt rules
        }
    }

    if(system->HasEditableFormula())
    {
        wxString formula = system->GetFormula();
        // force linebreak after each ';' (except last)
        formula.Replace(wxT(";\n"), wxT(";<br>"));
        contents += AppendRow(_("Formula"), formula);
    }

    contents += AppendRow(_("Dimensions"), wxString::Format(wxT("XYZ = %d; %d; %d"),
                                           system->GetX(),system->GetY(),system->GetZ()));

    contents += _("</table></td></tr></table></center>");
    contents += _("</body></html>");
    
    html->SetPage(contents);
}

// -----------------------------------------------------------------------------

wxString DataPanel::AppendRow(const wxString& label, const wxString& value)
{
    wxString result;
    rownum++;
    if (rownum & 1)
        result += _("<tr bgcolor=\"#F0F0F0\">");
    else
        result += _("<tr>");

    result += _("<td valign=top width=\"25%\">");
    result += _("<b>");
    result += label;
    result += _("</b>");
    result += _("</a>");
    result += _("</td><td width=10> </td>");
    result += _("<td valign=top>");
    result += value;
    result += _("</td>");

    result += _("<td>");
    result += _("<a href=\"");
    result += change_prefix;
    result += label;
    result += _("\">edit</a>");
    result += _("</td>");
    
    result += _("</tr>");
    return result;
}

// -----------------------------------------------------------------------------

wxString DataPanel::FormatFloat(const float& f)
{
    wxString result = wxString::Format(wxT("%f"),f);
    // strip any trailing zeros
    while (result.GetChar(result.Length()-1) == wxChar('0')) {
        result.Truncate(result.Length()-1);
    }
    // strip any trailing '.'
    if (result.GetChar(result.Length()-1) == wxChar('.')) {
        result.Truncate(result.Length()-1);
    }
    return result;
}

// -----------------------------------------------------------------------------

void DataPanel::OnSmallerButton(wxCommandEvent& WXUNUSED(event))
{
    if ( infofontsize > minfontsize ) {
        infofontsize--;
        html->ChangeFontSizes(infofontsize);
    }
}

// -----------------------------------------------------------------------------

void DataPanel::OnBiggerButton(wxCommandEvent& WXUNUSED(event))
{
    if ( infofontsize < maxfontsize ) {
        infofontsize++;
        html->ChangeFontSizes(infofontsize);
    }
}

// -----------------------------------------------------------------------------

void DataPanel::UpdateButtons()
{
    smallerbutt->Enable( infofontsize > minfontsize );
    biggerbutt->Enable( infofontsize < maxfontsize );
    
    html->ClearStatus();
    html->SetFocus();       // for keyboard shortcuts
}

// -----------------------------------------------------------------------------

bool DataPanel::HtmlHasFocus()
{
    return html->HasFocus();
}

// -----------------------------------------------------------------------------

void DataPanel::SelectAllText()
{
    html->SelectAll();
}

// -----------------------------------------------------------------------------

void DataPanel::CopySelection()
{
    wxString text = html->SelectionToText();
    if (text.length() > 0) {
        CopyTextToClipboard(text);
    }
}

// -----------------------------------------------------------------------------

bool DataPanel::DoKey(int key, int mods)
{
    // first look for keys that should be passed to the default handler
    if ( mods == wxMOD_NONE ) {
        if ( key == WXK_UP || key == WXK_DOWN || key == WXK_LEFT || key == WXK_RIGHT ||
             key == WXK_PAGEUP || key == WXK_PAGEDOWN ) {
            // let default handler see arrow keys and page up/down keys (for scroll bars)
            return false;
        }
    }
    
    // now do keyboard shortcuts specific to this pane
    if ( mods == wxMOD_NONE ) {
        if ( key == '+' || key == '=' || key == WXK_ADD ) {
            if ( infofontsize < maxfontsize ) {
                infofontsize++;
                html->ChangeFontSizes(infofontsize);
            }
            return true;
        }
        if ( key == '-' || key == WXK_SUBTRACT ) {
            if ( infofontsize > minfontsize ) {
                infofontsize--;
                html->ChangeFontSizes(infofontsize);
            }
            return true;
        }
    }
    
    // finally do other keyboard shortcuts
    frame->ProcessKey(key, mods);
    return true;
}