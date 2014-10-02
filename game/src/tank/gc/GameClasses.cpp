//GameClasses.cpp

#include "GameClasses.h"

#include "Macros.h"
#include "Player.h"
#include "RigidBody.h"
#include "World.h"

#include "constants.h"
#include "SaveFile.h"
#include "MapFile.h"

#include "core/Debug.h"
#include "config/Config.h"

#include <video/RenderBase.h>



IMPLEMENT_SELF_REGISTRATION(GC_Wood)
{
	ED_LAND( "wood", "obj_wood",  7 );
	return true;
}

IMPLEMENT_GRID_MEMBER(GC_Wood, grid_wood);

GC_Wood::GC_Wood(World &world)
{
	_tile = 0;
}

GC_Wood::GC_Wood(FromFile)
{
}

GC_Wood::~GC_Wood()
{
}

void GC_Wood::Kill(World &world)
{
    if( CheckFlags(GC_FLAG_WOOD_INTILE) )
        UpdateTile(world, false);
    GC_Actor::Kill(world);
}

void GC_Wood::UpdateTile(World &world, bool flag)
{
	static char tile1[9] = {5, 6, 7, 4,-1, 0, 3, 2, 1};
	static char tile2[9] = {1, 2, 3, 0,-1, 4, 7, 6, 5};

	FRECT frect;
	frect.left   = (GetPos().x - CELL_SIZE / 2) / LOCATION_SIZE - 0.5f;
	frect.top    = (GetPos().y - CELL_SIZE / 2) / LOCATION_SIZE - 0.5f;
	frect.right  = (GetPos().x + CELL_SIZE / 2) / LOCATION_SIZE - 0.5f;
	frect.bottom = (GetPos().y + CELL_SIZE / 2) / LOCATION_SIZE - 0.5f;

    std::vector<ObjectList*> receive;
	world.grid_wood.OverlapRect(receive, frect);

	for( auto rit = receive.begin(); rit != receive.end(); ++rit )
	{
        ObjectList *ls = *rit;
		for( auto it = ls->begin(); it != ls->end(); it = ls->next(it) )
		{
			GC_Wood *object = static_cast<GC_Wood *>(ls->at(it));
			if( this == object ) continue;

			vec2d dx = (GetPos() - object->GetPos()) / CELL_SIZE;
			if( dx.sqr() < 2.5f )
			{
				int x = int(dx.x + 1.5f);
				int y = int(dx.y + 1.5f);

				object->SetTile(tile1[x + y * 3], flag);
				SetTile(tile2[x + y * 3], flag);
			}
		}
	}
}

void GC_Wood::Serialize(World &world, SaveFile &f)
{
	GC_Actor::Serialize(world, f);
	f.Serialize(_tile);
}

void GC_Wood::SetTile(char nTile, bool value)
{
	assert(0 <= nTile && nTile < 8);

	if( value )
		_tile |=  (1 << nTile);
	else
		_tile &= ~(1 << nTile);
}

void GC_Wood::MoveTo(World &world, const vec2d &pos)
{
    if (CheckFlags(GC_FLAG_WOOD_INTILE))
        UpdateTile(world, false);
    GC_Actor::MoveTo(world, pos);
    UpdateTile(world, true);
    SetFlags(GC_FLAG_WOOD_INTILE, true);
}

/////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_HealthDaemon)
{
	return true;
}

IMPLEMENT_1LIST_MEMBER(GC_HealthDaemon, LIST_timestep);

GC_HealthDaemon::GC_HealthDaemon(World &world,
                                 GC_Player *owner,
                                 float damage, float time)
  : _time(time)
  , _damage(damage)
  , _victim(nullptr)
  , _owner(owner)
{
}

GC_HealthDaemon::GC_HealthDaemon(FromFile)
{
}

GC_HealthDaemon::~GC_HealthDaemon()
{
}

void GC_HealthDaemon::SetVictim(World &world, GC_RigidBodyStatic *victim)
{
	assert(!_victim && victim);
    
    _victim = victim;
	_victim->Subscribe(NOTIFY_ACTOR_MOVE, this, (NOTIFYPROC) &GC_HealthDaemon::OnVictimMove);
	_victim->Subscribe(NOTIFY_OBJECT_KILL, this, (NOTIFYPROC) &GC_HealthDaemon::OnVictimKill);
    
	MoveTo(world, _victim->GetPos());
}

void GC_HealthDaemon::Serialize(World &world, SaveFile &f)
{
	GC_Actor::Serialize(world, f);

	f.Serialize(_time);
	f.Serialize(_damage);
	f.Serialize(_victim);
	f.Serialize(_owner);
}

void GC_HealthDaemon::TimeStepFixed(World &world, float dt)
{
	_time -= dt;
	bool bKill = false;
	if( _time <= 0 )
	{
		dt += _time;
		bKill = true;
	}
	_victim->TakeDamage(world, dt * _damage, _victim->GetPos(), _owner);
	if( _victim && bKill )
		Kill(world);
}

void GC_HealthDaemon::OnVictimMove(World &world, GC_Object *sender, void *param)
{
	MoveTo(world, static_cast<GC_Actor*>(sender)->GetPos());
}

void GC_HealthDaemon::OnVictimKill(World &world, GC_Object *sender, void *param)
{
	Kill(world);
}

/////////////////////////////////////////////////////////////

GC_Text::GC_Text(World &world, const std::string &text, enumAlignText align)
	: _style(DEFAULT)
    , _align(align)
	, _text(text)
{
}

GC_Text::~GC_Text()
{
}

void GC_Text::Serialize(World &world, SaveFile &f)
{
	GC_Actor::Serialize(world, f);
	f.Serialize(_text);
	f.Serialize(_align);
	f.Serialize(_style);
}

/////////////////////////////////////////////////////////////

IMPLEMENT_SELF_REGISTRATION(GC_Text_ToolTip)
{
	return true;
}

IMPLEMENT_1LIST_MEMBER(GC_Text_ToolTip, LIST_timestep);

GC_Text_ToolTip::GC_Text_ToolTip(World &world, const std::string &text, Style style)
  : GC_Text(world, text, alignTextCC)
  , _time(0)
{
	SetText(text);
	SetStyle(style);
}

void GC_Text_ToolTip::Serialize(World &world, SaveFile &f)
{
	GC_Text::Serialize(world, f);
	f.Serialize(_time);
}

void GC_Text_ToolTip::TimeStepFloat(World &world, float dt)
{
	MoveTo(world, GetPos() + vec2d(0, -20.0f) * dt);
	_time += dt;
	if( _time > 1.2f )
    {
        Kill(world);
    }
}

///////////////////////////////////////////////////////////////////////////////
// end of file
