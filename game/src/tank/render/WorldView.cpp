#include "WorldView.h"
#include "rLight.h"
#include "rSprite.h"
#include "rWall.h"
#include "Macros.h"
#include "config/Config.h"
#include "gc/Camera.h"
#include "gc/Light.h"
#include "gc/World.h"
#include "video/RenderBase.h"

#include "gc/RigidBody.h"
#include "gc/Crate.h"


WorldView::WorldView(IRender &render, TextureManager &tm)
    : _render(render)
    , _tm(tm)
	, _terrain(tm)
{
	AddView<GC_Wall, R_Wall>(tm, "brick");
	AddView<GC_Wall_Concrete, R_Wall>(tm, "concrete");
	AddView<GC_Crate, R_Sprite>(tm, "crate01", Z_WALLS);
}

WorldView::~WorldView()
{
}

ObjectView* WorldView::GetView(const GC_Actor &actor) const
{
	ObjectType type = actor.GetType();
	return type < _type2view.size() ? _type2view[type].get() : nullptr;
}

void WorldView::Render(World &world, const FRECT &view, bool editorMode) const
{
    // FIXME: ambient will take effect starting next frame
	_render.SetAmbient(g_conf.sv_nightmode.Get() ? (editorMode ? 0.5f : 0) : 1);

	//
	// draw lights to alpha channel
	//

	_render.SetMode(RM_LIGHT);
	if( g_conf.sv_nightmode.Get() )
	{
		float xmin = std::max(0.0f, view.left);
		float ymin = std::max(0.0f, view.top);
		float xmax = std::min(world._sx, view.right);
		float ymax = std::min(world._sy, view.bottom);

		FOREACH( world.GetList(LIST_lights), GC_Light, pLight )
		{
			if( pLight->GetActive() &&
				pLight->GetPos().x + pLight->GetRenderRadius() > xmin &&
				pLight->GetPos().x - pLight->GetRenderRadius() < xmax &&
				pLight->GetPos().y + pLight->GetRenderRadius() > ymin &&
				pLight->GetPos().y - pLight->GetRenderRadius() < ymax )
			{
			//	_FpsCounter::Inst()->OneMoreLight();
				DrawLight(*g_render, *pLight);
			}
		}
	}


	//
	// draw world to rgb
	//

	_render.SetMode(RM_WORLD);

	_terrain.Draw(_render, world._sx, world._sy, editorMode);

	int xmin = std::max(0, int(view.left / LOCATION_SIZE));
	int ymin = std::max(0, int(view.top / LOCATION_SIZE));
	int xmax = std::min(world._locationsX - 1, int(view.right / LOCATION_SIZE));
	int ymax = std::min(world._locationsY - 1, int(view.bottom / LOCATION_SIZE) + 1);

    static std::vector<GC_2dSprite*> zLayers[Z_COUNT];
    for( int x = xmin; x <= xmax; ++x )
    for( int y = ymin; y <= ymax; ++y )
    {
        FOREACH(world.grid_sprites.element(x,y), GC_2dSprite, object)
        {
			if( ObjectView *view = GetView(*object) )
			{
				enumZOrder z = view->GetZ(*object);
				if( Z_NONE != z && object->GetGridSet() )
					zLayers[z].push_back(object);
			}
			else
			{
				// TODO: remove fallback to old render
				if( object->GetVisible() && Z_NONE != object->GetZ() && object->GetGridSet() )
					zLayers[object->GetZ()].push_back(object);
			}
        }
    }

    FOREACH( world.GetList(LIST_gsprites), GC_2dSprite, object )
    {
		if( ObjectView *view = GetView(*object) )
		{
			enumZOrder z = view->GetZ(*object);
			if( Z_NONE != z && !object->GetGridSet() )
				zLayers[z].push_back(object);
		}
		else
		{
			// TODO: remove fallback to old render
			if( object->GetVisible() && Z_NONE != object->GetZ() && !object->GetGridSet() )
				zLayers[object->GetZ()].push_back(object);
		}
    }

	DrawingContext &dc = static_cast<DrawingContext&>(_tm);
    for( int z = 0; z < Z_COUNT; ++z )
    {
        for( GC_2dSprite *sprite: zLayers[z] )
		{
			if( ObjectView *view = GetView(*sprite) )
				view->Draw(*sprite, dc);
			else
				// TODO: remove fallback to old render
				sprite->Draw(dc, editorMode);
		}
        zLayers[z].clear();
    }
}
