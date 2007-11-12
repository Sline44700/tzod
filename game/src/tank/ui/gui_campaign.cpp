// gui_campaign.cpp

#include "stdafx.h"
#include "gui_campaign.h"
#include "gui_desktop.h"

#include "List.h"
#include "Button.h"
#include "Text.h"

#include "GuiManager.h"

#include "fs/FileSystem.h"


namespace UI
{
///////////////////////////////////////////////////////////////////////////////

NewCampaignDlg::NewCampaignDlg(Window *parent)
  : Dialog(parent, 512, 400)
{
	Text *t = new Text(this, GetWidth() / 2, 16, "����� ��������", alignTextCT);
	t->SetTexture("font_default");
	t->Resize(t->GetTextureWidth(), t->GetTextureHeight());

	_files = new List(this, 20, 56, 472, 280);
	std::set<string_t> files;
	if( g_fs->GetFileSystem("campaign")->EnumAllFiles(files, "*.lua") )
	{
		for( std::set<string_t>::iterator it = files.begin(); it != files.end(); ++it )
		{
			it->erase(it->length() - 4); // cut out the file extension
			int index = _files->AddItem(it->c_str());
		}
	}
	else
	{
		_ASSERT(FALSE); // EnumAllFiles has returned error...
	}
	_files->Sort();


	(new Button(this, 290, 360, "OK"))->eventClick.bind(&NewCampaignDlg::OnOK, this);
	(new Button(this, 400, 360, "������"))->eventClick.bind(&NewCampaignDlg::OnCancel, this);
}

NewCampaignDlg::~NewCampaignDlg()
{
}

void NewCampaignDlg::OnOK()
{
	if( -1 == _files->GetCurSel() )
	{
		return;
	}

	const string_t& name = _files->GetItemText(_files->GetCurSel());
	if( !script_exec_file(g_env.L, ("campaign/" + name + ".lua").c_str()) )
	{
		static_cast<Desktop*>(g_gui->GetDesktop())->ShowConsole(true);
	}

	Close(_resultOK);
}

void NewCampaignDlg::OnCancel()
{
	Close(_resultCancel);
}


///////////////////////////////////////////////////////////////////////////////
} // end of namespace UI

// end of file
