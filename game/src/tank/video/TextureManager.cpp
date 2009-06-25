// TextureManager.cpp

#include "stdafx.h"

#include "TextureManager.h"
#include "RenderBase.h"
#include "ImageLoader.h"

#include "core/debug.h"
#include "core/Application.h"
#include "core/Console.h"

#include "gc/2dSprite.h"

#include "fs/FileSystem.h"

///////////////////////////////////////////////////////////////////////////////

class CheckerImage : public Image
{
public:
	// Image methods
	virtual const void* GetData() const { return _bytes; }
	virtual unsigned long GetBpp() const { return 24; }
	virtual unsigned long GetWidth() const { return 4; }
	virtual unsigned long GetHeight() const { return 4; }

private:
	static const unsigned char _bytes[];
};

const unsigned char CheckerImage::_bytes[] = {
	0,  0,  0,     0,  0,  0,   255,255,255,   255,255,255,
	0,  0,  0,     0,  0,  0,   255,255,255,   255,255,255,
	255,255,255,   255,255,255,     0,  0,  0,     0,  0,  0,
	255,255,255,   255,255,255,     0,  0,  0,     0,  0,  0,
};


///////////////////////////////////////////////////////////////////////////////

TextureManager::TextureManager()
{
	CreateChecker();
}

TextureManager::~TextureManager()
{
	UnloadAllTextures();
}

void TextureManager::UnloadAllTextures()
{
	TexDescIterator it = _textures.begin();
	while( it != _textures.end() )
		g_render->TexFree((it++)->id);
	_textures.clear();
	_mapFile_to_TexDescIter.clear();
	_mapDevTex_to_TexDescIter.clear();
	_mapName_to_Index.clear();
	_logicalTextures.clear();
}

void TextureManager::LoadTexture(TexDescIterator &itTexDesc, const string_t &fileName)
{
	FileToTexDescMap::iterator it = _mapFile_to_TexDescIter.find(fileName);
	if( _mapFile_to_TexDescIter.end() != it )
	{
		itTexDesc = it->second;
	}
	else
	{
		SafePtr<FS::File> file = g_fs->Open(fileName);
		SafePtr<TgaImage> image(new TgaImage(file->GetData(), file->GetSize()));

		TexDesc td;
		if( !g_render->TexCreate(td.id, GetRawPtr(image)) )
		{
			throw std::exception("error in render device");
		}

		td.width     = image->GetWidth();
		td.height    = image->GetHeight();
		td.refCount  = 0;

		_textures.push_front(td);
		itTexDesc = _textures.begin();
		_mapFile_to_TexDescIter[fileName] = itTexDesc;
		_mapDevTex_to_TexDescIter[itTexDesc->id] = itTexDesc;
	}
}

void TextureManager::Unload(TexDescIterator what)
{
	g_render->TexFree(what->id);

	FileToTexDescMap::iterator it = _mapFile_to_TexDescIter.begin();
	while( _mapFile_to_TexDescIter.end() != it )
	{
		if( it->second->id == what->id )
		{
			_mapFile_to_TexDescIter.erase(it);
			break;
		}
		++it;
	}

	_mapDevTex_to_TexDescIter.erase(what->id);
	_textures.erase(what);
}

void TextureManager::CreateChecker()
{
	TexDesc td;
	LogicalTexture tex;


	//
	// check if checker texture already exists
	//

	assert(_logicalTextures.empty()); // to be sure that checker will get index 0
	assert(_mapName_to_Index.empty());

	TRACE("Creating checker texture\n");



	//
	// create device texture
	//

	SafePtr<CheckerImage> c(new CheckerImage());

	if( !g_render->TexCreate(td.id, GetRawPtr(c) ) )
	{
		TRACE("ERROR: error in render device\n");
		assert(FALSE);
		return;
	}

	td.width     = c->GetWidth();
	td.height    = c->GetHeight();
	td.refCount  = 0;

	_textures.push_front(td);
	TexDescIterator it = _textures.begin();



	//
	// create logical texture
	//

	tex.dev_texture = it->id;
	tex.uvLeft   = 0;
	tex.uvTop    = 0;
	tex.uvRight  = 1;
	tex.uvBottom = 1;
	tex.xframes  = 1;
	tex.yframes  = 1;
	tex.uvFrameWidth  = 8.0f;
	tex.uvFrameHeight = 8.0f;
	tex.pxFrameWidth  = (float) td.width * tex.uvFrameWidth;
	tex.pxFrameHeight = (float) td.height * tex.uvFrameHeight;

	FRECT whole = {0,0,1,1};
	tex.uvFrames.push_back(whole);
	//---------------------
	_logicalTextures.push_back(tex);
	it->refCount++;
}

static int auxgetint(lua_State *L, int tblidx, const char *field, int def)
{
	lua_getfield(L, tblidx, field);
	if( lua_isnumber(L, -1) ) def = lua_tointeger(L, -1);
	lua_pop(L, 1); // pop result of getfield
	return def;
}

static float auxgetfloat(lua_State *L, int tblidx, const char *field, float def)
{
	lua_getfield(L, tblidx, field);
	if( lua_isnumber(L, -1) ) def = (float) lua_tonumber(L, -1);
	lua_pop(L, 1); // pop result of getfield
	return def;
}

int TextureManager::LoadPackage(const string_t &filename)
{
	TRACE("Loading texture package '%s'\n", filename.c_str());

	lua_State *L = lua_open();

	if( 0 != (luaL_loadfile(L, filename.c_str()) || lua_pcall(L, 0, 1, 0)) )
	{
		TRACE("%s\n", lua_tostring(L, -1));
		lua_close(L);
		return 0;
	}

	if( !lua_istable(L, 1) )
	{
		lua_close(L);
		return 0;
	}

	// loop over files
	for( lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1) )
	{
		// now 'key' is at index -2 and 'value' at index -1

		// check that value is a table
		if( !lua_istable(L, -1) )
		{
			TRACE("WARNING: value is not a table; skipping.\n");
		}

		while( lua_istable(L, -1) )
		{
			TexDescIterator td;

			// get a file name; load
			lua_getfield(L, -1, "file");
			std::string f = lua_tostring(L, -1);
			lua_pop(L, 1); // pop result of lua_getfield

			try
			{
				LoadTexture(td, f);
			}
			catch( const std::exception &e )
			{
				TRACE("WARNING: could not load texture '%s' - %s\n", f.c_str(), e.what());
				continue;
			}


			// get 'content' field
			lua_getfield(L, -1, "content");
			if( lua_istable(L, -1) )
			{
				// loop over textures in 'content' table
				for( lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1) )
				{
					if( !lua_istable(L, -1) )
					{
						TRACE("WARNING: element of 'content' is not a table; skipping\n");
						continue;
					}

					lua_pushvalue(L, -2); // create copy of the key
					if( const char *texname = lua_tostring(L, -1) )
					{
						// now 'value' at index -2

						float scale_x = auxgetfloat(L, -2, "xscale", 1);
						float scale_y = auxgetfloat(L, -2, "yscale", 1);

						LogicalTexture tex;
						tex.dev_texture = td->id;

						// texture bounds
						tex.uvLeft   = (float) floorf(auxgetfloat(L, -2, "left", 0)) / (float) td->width;
						tex.uvRight  = (float) floorf(auxgetfloat(L, -2, "right", (float) td->width)) / (float) td->width;
						tex.uvTop    = (float) floorf(auxgetfloat(L, -2, "top", 0)) / (float) td->height;
						tex.uvBottom = (float) floorf(auxgetfloat(L, -2, "bottom", (float) td->height)) / (float) td->height;

						// frames count
						tex.xframes = auxgetint(L, -2, "xframes", 1);
						tex.yframes = auxgetint(L, -2, "yframes", 1);

						// frame size
						tex.uvFrameWidth  = (tex.uvRight - tex.uvLeft) / (float) tex.xframes;
						tex.uvFrameHeight = (tex.uvBottom - tex.uvTop) / (float) tex.yframes;

						// original size
						tex.pxFrameWidth  = (float) td->width  * scale_x * tex.uvFrameWidth;
						tex.pxFrameHeight = (float) td->height * scale_y * tex.uvFrameHeight;

						// pivot position
						tex.uvPivot.x = (float) auxgetfloat(L, -2, "xpivot", (float) td->width * tex.uvFrameWidth / 2) / ((float) td->width * tex.uvFrameWidth);
						tex.uvPivot.y = (float) auxgetfloat(L, -2, "ypivot", (float) td->height * tex.uvFrameHeight / 2) / ((float) td->height * tex.uvFrameHeight);

						// frames
						tex.uvFrames.reserve(tex.xframes * tex.yframes);
						for( int y = 0; y < tex.yframes; ++y )
						{
							for( int x = 0; x < tex.xframes; ++x )
							{
								FRECT rt;
								rt.left   = tex.uvLeft + tex.uvFrameWidth * (float) x;
								rt.right  = tex.uvLeft + tex.uvFrameWidth * (float) (x + 1);
								rt.top    = tex.uvTop + tex.uvFrameHeight * (float) y;
								rt.bottom = tex.uvTop + tex.uvFrameHeight * (float) (y + 1);
								tex.uvFrames.push_back(rt);
							}
						}

						//---------------------
						if( tex.xframes > 0 && tex.yframes > 0 )
						{
							td->refCount++;
							//---------------------------------------------
							std::map<string_t, size_t>::iterator it =
								_mapName_to_Index.find(texname);

							if( _mapName_to_Index.end() != it )
							{
								// replace existing logical texture
								LogicalTexture &existing = _logicalTextures[it->second];
								TexDescIterator tmp =
									_mapDevTex_to_TexDescIter[existing.dev_texture];
								existing = tex;
								tmp->refCount--;
								assert(tmp->refCount >= 0);
							}
							else
							{
								// define new texture
								_mapName_to_Index[texname] = _logicalTextures.size();
								_logicalTextures.push_back(tex);
							}
						} // end if( xframes > 0 && yframes > 0 )
					} // end if( texname )
					lua_pop(L, 1); // remove copy of the key
				} // end loop over 'content'
			} // end if 'content' is table
			else
			{
				TRACE("WARNING: 'content' field is not a table.\n");
			}
			lua_pop(L, 1); // pop the result of getfield("content")
			break;
		} // end of while( lua_istable(L, -1) )
	}
	lua_close(L);


	//
	// unload unused textures
	//
	TexDescIterator it = _textures.begin();
	while( _textures.end() != it )
	{
        TexDescIterator tmp = it++;
		assert(tmp->refCount >= 0);
		if( 0 == tmp->refCount )
			Unload(tmp);
	}

	TRACE("Total number of loaded textures: %d\n", _logicalTextures.size());
	return _logicalTextures.size();
}

int TextureManager::LoadDirectory(const string_t &dirName, const string_t &texPrefix)
{
	int count = 0;

	SafePtr<FS::FileSystem> dir = g_fs->GetFileSystem(dirName);

	std::set<string_t> files;
	dir->EnumAllFiles(files, TEXT("*.tga"));

	for( std::set<string_t>::iterator it = files.begin(); it != files.end(); ++it )
	{
		TexDescIterator td;
		string_t f = dirName + TEXT("/") + *it;
		try
		{
			LoadTexture(td, f);
		}
		catch( const std::exception &e )
		{
			TRACE("WARNING: could not load texture '%s' - %s\n", f.c_str(), e.what());
			continue;
		}

		string_t texName = texPrefix + *it;
		texName.erase(texName.length() - 4); // cut out the file extension

		LogicalTexture tex;
		tex.dev_texture = td->id;
		tex.uvLeft   = 0;
		tex.uvTop    = 0;
		tex.uvRight  = 1;
		tex.uvBottom = 1;
		tex.uvPivot  = vec2d(0.5f, 0.5f);
		tex.xframes  = 1;
		tex.yframes  = 1;
		tex.uvFrameWidth  = 1;
		tex.uvFrameHeight = 1;
		tex.pxFrameWidth  = (float) td->width;
		tex.pxFrameHeight = (float) td->height;

		FRECT frame = {0,0,1,1};
		tex.uvFrames.push_back(frame);
		//---------------------
		if( _mapName_to_Index.end() != _mapName_to_Index.find(texName) )
			continue; // �������� � ����� ������ ��� ����. ����������.
		_mapName_to_Index[texName] = _logicalTextures.size();
		_logicalTextures.push_back(tex);
		td->refCount++;
		count++;
	}
	return count;
}

size_t TextureManager::FindSprite(const string_t &name) const
{
	std::map<string_t, size_t>::const_iterator it = _mapName_to_Index.find(name);
	if( _mapName_to_Index.end() != it )
		return it->second;

	// flood the console
	g_app->GetConsole()->printf("texture '%s' not found!\n", name);

	return 0; // index of checker texture
}

bool TextureManager::IsValidTexture(size_t index) const
{
	return index < _logicalTextures.size();
}

void TextureManager::GetTextureNames(std::vector<string_t> &names,
                                     const char *prefix, bool noPrefixReturn) const
{
	size_t trimLength = (prefix && noPrefixReturn) ? strlen(prefix) : 0;

	names.clear();
	std::map<string_t, size_t>::const_iterator it = _mapName_to_Index.begin();
	for(; it != _mapName_to_Index.end(); ++it )
	{
		if( prefix && 0 != it->first.find(prefix) )
			continue;
		names.push_back(it->first.substr(trimLength));
	}
}

float TextureManager::GetCharHeight(size_t fontTexture) const
{
	return Get(fontTexture).pxFrameHeight;
}

void TextureManager::DrawBitmapText(size_t tex, const string_t &str, SpriteColor color, float sx, float sy, enumAlignText align) const
{
	// grep enum enumAlignText LT CT RT LC CC RC LB CB RB
	static const float dx[] = { 0, 1, 2, 0, 1, 2, 0, 1, 2 };
	static const float dy[] = { 0, 0, 0, 1, 1, 1, 2, 2, 2 };

	std::vector<size_t> lines;
	size_t maxline = 0;
	if( align )
	{
		size_t count = 0;
		for( const string_t::value_type *tmp = str.c_str(); *tmp; )
		{
			++count;
			++tmp;
			if( '\n' == *tmp || '\0' == *tmp )
			{
				if( maxline < count ) 
					maxline = count;
				lines.push_back(count);
				count = 0;
			}
		}
	}


	const LogicalTexture &lt = Get(tex);
	g_render->TexBind(lt.dev_texture);

	size_t count = 0;
	size_t line  = 0;

	float x0 = sx - floorf(dx[align] * (lt.pxFrameWidth - 1) * (float) maxline / 2);
	float y0 = sy - floorf(dy[align] * lt.pxFrameHeight * (float) lines.size() / 2);

	for( const string_t::value_type *tmp = str.c_str(); *tmp; ++tmp )
	{
		if( '\n' == *tmp )
		{
			++line;
			count = 0;
		}

		if( (unsigned char) *tmp < 32 )
		{
			continue;
		}

		const FRECT &rt = lt.uvFrames[(unsigned char) *tmp - 32];
		float x = x0 + (float) ((count++) * (lt.pxFrameWidth - 1));
		float y = y0 + (float) (line * lt.pxFrameHeight);

		g_render->TexBind(lt.dev_texture);
		MyVertex *v = g_render->DrawQuad();

		v[0].color = color;
		v[0].u = rt.left;
		v[0].v = rt.top;
		v[0].x = x;
		v[0].y = y ;

		v[1].color = color;
		v[1].u = rt.left + lt.uvFrameWidth;
		v[1].v = rt.top;
		v[1].x = x + lt.pxFrameWidth;
		v[1].y = y;

		v[2].color = color;
		v[2].u = rt.left + lt.uvFrameWidth;
		v[2].v = rt.bottom;
		v[2].x = x + lt.pxFrameWidth;
		v[2].y = y + lt.pxFrameHeight;

		v[3].color = color;
		v[3].u = rt.left;
		v[3].v = rt.bottom;
		v[3].x = x;
		v[3].y = y + lt.pxFrameHeight;
	}
}

void TextureManager::DrawSprite(size_t tex, unsigned int frame, SpriteColor color, float x, float y, float rot) const
{
	const LogicalTexture &lt = Get(tex);
	const FRECT &rt = lt.uvFrames[frame];

	g_render->TexBind(lt.dev_texture);
	MyVertex *v = g_render->DrawQuad();

	float c = cosf(rot);
	float s = sinf(rot);

	float width = lt.pxFrameWidth;
	float height = lt.pxFrameHeight;

	float px = lt.uvPivot.x * width;
	float py = lt.uvPivot.y * height;


	v[0].color = color;
	v[0].u = rt.left;
	v[0].v = rt.top;
	v[0].x = x - px * c + py * s;
	v[0].y = y - px * s - py * c;

	v[1].color = color;
	v[1].u = rt.right;
	v[1].v = rt.top;
	v[1].x = x + (width - px) * c + py * s;
	v[1].y = y + (width - px) * s - py * c;

	v[2].color = color;
	v[2].u = rt.right;
	v[2].v = rt.bottom;
	v[2].x = x + (width - px) * c - (height - py) * s;
	v[2].y = y + (width - px) * s + (height - py) * c;

	v[3].color = color;
	v[3].u = rt.left;
	v[3].v = rt.bottom;
	v[3].x = x - px * c - (height - py) * s;
	v[3].y = y - px * s + (height - py) * c;
}

void TextureManager::DrawSprite(size_t tex, unsigned int frame, SpriteColor color, float x, float y, float width, float height, float rot) const
{
	const LogicalTexture &lt = Get(tex);
	const FRECT &rt = lt.uvFrames[frame];

	g_render->TexBind(lt.dev_texture);
	MyVertex *v = g_render->DrawQuad();

	float c = cosf(rot);
	float s = sinf(rot);

	float px = lt.uvPivot.x * width;
	float py = lt.uvPivot.y * height;


	v[0].color = color;
	v[0].u = rt.left;
	v[0].v = rt.top;
	v[0].x = x - px * c + py * s;
	v[0].y = y - px * s - py * c;

	v[1].color = color;
	v[1].u = rt.right;
	v[1].v = rt.top;
	v[1].x = x + (width - px) * c + py * s;
	v[1].y = y + (width - px) * s - py * c;

	v[2].color = color;
	v[2].u = rt.right;
	v[2].v = rt.bottom;
	v[2].x = x + (width - px) * c - (height - py) * s;
	v[2].y = y + (width - px) * s + (height - py) * c;

	v[3].color = color;
	v[3].u = rt.left;
	v[3].v = rt.bottom;
	v[3].x = x - px * c - (height - py) * s;
	v[3].y = y - px * s + (height - py) * c;
}

void TextureManager::DrawIndicator(size_t tex, float x, float y, float value) const
{
	const LogicalTexture &lt = Get(tex);
	const FRECT &rt = lt.uvFrames[0];

	float px = lt.uvPivot.x * lt.pxFrameWidth;
	float py = lt.uvPivot.y * lt.pxFrameHeight;

	g_render->TexBind(lt.dev_texture);
	MyVertex *v = g_render->DrawQuad();

	v[0].color = 0xffffffff;
	v[0].u = rt.left;
	v[0].v = rt.top;
	v[0].x = x - px;
	v[0].y = y - py;

	v[1].color = 0xffffffff;
	v[1].u = rt.left + lt.uvFrameWidth * value;
	v[1].v = rt.top;
	v[1].x = x - px + lt.pxFrameWidth * value;
	v[1].y = y - py;

	v[2].color = 0xffffffff;
	v[2].u = rt.left + lt.uvFrameWidth * value;
	v[2].v = rt.bottom;
	v[2].x = x - px + lt.pxFrameWidth * value;
	v[2].y = y - py + lt.pxFrameHeight;

	v[3].color = 0xffffffff;
	v[3].u = rt.left;
	v[3].v = rt.bottom;
	v[3].x = x - px;
	v[3].y = y - py + lt.pxFrameHeight;
}

void TextureManager::DrawLine(size_t tex, SpriteColor color,
                              float x0, float y0, float x1, float y1, float phase) const
{
	const LogicalTexture &lt = Get(tex);

	g_render->TexBind(lt.dev_texture);
	MyVertex *v = g_render->DrawQuad();

	float len = sqrtf((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
	float phase1 = phase + len / lt.pxFrameWidth;
	float c = (x1-x0) / len;
	float s = (y1-y0) / len;
	float py = lt.pxFrameHeight / 2;

	v[0].color = color;
	v[0].u = phase;
	v[0].v = 0;
	v[0].x = x0 + py * s;
	v[0].y = y0 - py * c;

	v[1].color = color;
	v[1].u = phase1;
	v[1].v = 0;
	v[1].x = x0 + len * c + py * s;
	v[1].y = y0 + len * s - py * c;

	v[2].color = color;
	v[2].u = phase1;
	v[2].v = 1;
	v[2].x = x0 + len * c - py * s;
	v[2].y = y0 + len * s + py * c;

	v[3].color = color;
	v[3].u = phase;
	v[3].v = 1;
	v[3].x = x0 - py * s;
	v[3].y = y0 + py * c;
}

////////////////////////////////////////////////////////////////////

ThemeManager::ThemeManager()
{
	TCHAR curdir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, curdir);

	if( !SetCurrentDirectory(DIR_THEMES) )
		return;

	WIN32_FIND_DATA fd = {0};
	HANDLE hSearch = FindFirstFile("*.lua", &fd);
	if( INVALID_HANDLE_VALUE != hSearch )
	{
		do {
			ThemeDesc td;
			td.fileName = fd.cFileName;
			_themes.push_back(td);
		} while( FindNextFile(hSearch, &fd) );
		FindClose(hSearch);
	}
	SetCurrentDirectory(curdir);
}

ThemeManager::~ThemeManager()
{
}

size_t ThemeManager::GetThemeCount()
{
	return _themes.size() + 1;
}

size_t ThemeManager::FindTheme(const string_t &name)
{
	for( size_t i = 0; i < _themes.size(); i++ )
	{
		if( GetThemeName(i+1) == name )
		{
			return i+1;
		}
	}
	return 0;
}

string_t ThemeManager::GetThemeName(size_t index)
{
	if( 0 == index )
		return "<standard>";
	return _themes[index-1].fileName.substr(
		0, _themes[index-1].fileName.size() - 4); // throw off the extension
}

bool ThemeManager::ApplyTheme(size_t index)
{
	bool res = (g_texman->LoadPackage(FILE_TEXTURES) > 0);
	if( index > 0 )
	{
		string_t filename = DIR_THEMES;
		filename += "\\";
		filename += _themes[index-1].fileName;
		res = res && (g_texman->LoadPackage(filename) > 0);
	}
/*
	FOREACH( g_level->GetList(LIST_objects), GC_Object, object )
	{
		GC_2dSprite *pSprite = dynamic_cast<GC_2dSprite*>(object);
		if( pSprite && !pSprite->IsKilled() )
		{
			pSprite->UpdateTexture();
		}
	}
*/
	return res;
}

///////////////////////////////////////////////////////////////////////////////
// end of file
