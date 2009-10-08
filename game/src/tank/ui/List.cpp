// List.cpp

#include "stdafx.h"

#include "List.h"
#include "Scroll.h"
#include "GuiManager.h"

#include "video/TextureManager.h"

namespace UI
{

///////////////////////////////////////////////////////////////////////////////
// class ListDataSourceDefault


///////////////////////////////////////////////////////////////////////////////
// class List::ListCallbackImpl

List::ListCallbackImpl::ListCallbackImpl(List *list)
  : _list(list)
{
	assert(_list);
}

void List::ListCallbackImpl::OnDeleteAllItems()
{
	_list->_scrollBar->SetLimit(-1);
	_list->SetCurSel(-1, false);
}

void List::ListCallbackImpl::OnDeleteItem(int idx)
{
	_list->_scrollBar->SetLimit((float) _list->_data->GetItemCount() - _list->GetNumLinesVisible());
	if( -1 != _list->GetCurSel() )
	{
		if( _list->GetCurSel() > idx )
		{
			_list->SetCurSel(_list->GetCurSel() - 1, false);
		}
		else if( _list->GetCurSel() == idx )
		{
			_list->SetCurSel(-1, false);
		}
	}
}

void List::ListCallbackImpl::OnAddItem()
{
	_list->_scrollBar->SetLimit((float) _list->_data->GetItemCount() - _list->GetNumLinesVisible());
}

///////////////////////////////////////////////////////////////////////////////
// class List

List* List::Create(Window *parent, ListDataSource* dataSource, float x, float y, float width, float height)
{
	List *res = new List(parent, dataSource);
	res->Move(x, y);
	res->Resize(width, height);
	return res;
}

List::List(Window *parent, ListDataSource* dataSource)
  : Window(parent)
  , _callbacks(this)
  , _curSel(-1)
  , _hotItem(-1)
  , _font(GetManager()->GetTextureManager()->FindSprite("font_small"))
  , _selection(GetManager()->GetTextureManager()->FindSprite("ui/listsel"))
  , _data(dataSource)
  , _scrollBar(ScrollBarVertical::Create(this, 0, 0, 0))
{
	_data->AddListener(&_callbacks);

	SetTexture("ui/list", false);
	SetDrawBorder(true);
	SetTabPos(0, 1); // first column
	SetClipChildren(true);
}

List::~List()
{
}

ListDataSource* List::GetData() const
{
	return _data;
}

void List::SetTabPos(int index, float pos)
{
	assert(index >= 0);
	if( index >= (int) _tabs.size() )
		_tabs.insert(_tabs.end(), 1+index - _tabs.size(), pos);
	else
		_tabs[index] = pos;
}

float List::GetItemHeight() const
{
	return g_texman->Get(_font).pxFrameHeight + 1;
}

int List::GetCurSel() const
{
	return _curSel;
}

void List::SetCurSel(int sel, bool scroll)
{
	if( 0 == _data->GetItemCount() )
	{
		sel = -1;
	}

	if( _curSel != sel )
	{
		_curSel = sel;
		if( scroll )
		{
			float fs = (float) sel;
			if( fs < _scrollBar->GetPos() )
				_scrollBar->SetPos(fs);
			else if( fs > _scrollBar->GetPos() + GetNumLinesVisible() - 1 )
				_scrollBar->SetPos(fs - GetNumLinesVisible() + 1);
		}

		if( eventChangeCurSel )
		{
			INVOKE(eventChangeCurSel) (sel);
		}
	}
}

int List::HitTest(float y)
{
	int index = int(y / GetItemHeight() + _scrollBar->GetPos());
	if( index < 0 || index >= _data->GetItemCount() )
	{
		index = -1;
	}
	return index;
}

float List::GetNumLinesVisible() const
{
	return GetHeight() / GetItemHeight();
}

float List::GetScrollPos() const
{
	return _scrollBar->GetPos();
}

void List::SetScrollPos(float pos)
{
	_scrollBar->SetPos(pos);
}

void List::AlignHeightToContent(float maxHeight)
{
	Resize(GetWidth(), __min(maxHeight, GetItemHeight() * (float) _data->GetItemCount()));
}

void List::OnSize(float width, float height)
{
	_scrollBar->Resize(_scrollBar->GetWidth(), height);
	_scrollBar->Move(width - _scrollBar->GetWidth(), 0);
	_scrollBar->SetLimit( (float) _data->GetItemCount() - GetNumLinesVisible() );
}

bool List::OnMouseMove(float x, float y)
{
	_hotItem = HitTest(y);
	return true;
}

bool List::OnMouseLeave()
{
	_hotItem = -1;
	return true;
}

bool List::OnMouseDown(float x, float y, int button)
{
	if( 1 == button && x < _scrollBar->GetX() )
	{
		int index = HitTest(y);
		SetCurSel(index, false);
		if( -1 != index && eventClickItem )
			INVOKE(eventClickItem) (index);
	}
	return true;
}

bool List::OnMouseUp(float x, float y, int button)
{
	return true;
}

bool List::OnMouseWheel(float x, float y, float z)
{
	_scrollBar->SetPos(_scrollBar->GetPos() - z * 3.0f);
	return true;
}

bool List::OnRawChar(int c)
{
	switch(c)
	{
	case VK_UP:
		SetCurSel(__max(0, GetCurSel() - 1), true);
		break;
	case VK_DOWN:
		SetCurSel(__min(_data->GetItemCount() - 1, GetCurSel() + 1), true);
		break;
	case VK_HOME:
		SetCurSel(0, true);
		break;
	case VK_END:
		SetCurSel(_data->GetItemCount() - 1, true);
		break;
	case VK_PRIOR: // page up
		SetCurSel(__max(0, GetCurSel() - (int) ceil(GetNumLinesVisible()) + 1), true);
		break;
	case VK_NEXT:  // page down
		SetCurSel(__min(_data->GetItemCount() - 1, GetCurSel() + (int) ceil(GetNumLinesVisible()) - 1), true);
		break;
	default:
		return false;
	}
	return true;
}

bool List::OnFocus(bool focus)
{
	return true;
}

void List::DrawChildren(const DrawingContext *dc, float sx, float sy) const
{
	Window::DrawChildren(dc, sx, sy);

	int i_min = (int) _scrollBar->GetPos();
	int i_max = i_min + (int) GetNumLinesVisible() + 2;

	for( int i = i_min; i < std::min(_data->GetItemCount(), i_max); ++i )
	{
		SpriteColor c = 0xc0c0c0c0;
		if( _hotItem == i )
		{
			c  = 0xffccccff;
		}
		else if( _curSel == i )
		{
			c  = 0xffffffff;
		}

		float y = floorf(GetItemHeight() * ((float) i - _scrollBar->GetPos()) + 0.5f);
		for( int k = 0; k < _data->GetSubItemCount(i); ++k )
		{
			dc->DrawBitmapText(sx + _tabs[__min(k, (int) _tabs.size()-1)], sy + y, _font, c, _data->GetItemText(i, k));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
} // end of namespace UI

// end of file
