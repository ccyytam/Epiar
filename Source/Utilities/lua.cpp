/**\file			lua.cpp
 * \author			Chris Thielen (chris@epiar.net)
 * \date			Created: Saturday, January 5, 2008
 * \date			Modified: Saturday, November 21, 2009
 * \brief			Provides abilities to load, store, and run Lua scripts
 * \details
 * To be used in conjunction with various other subsystems, A.I., GUI, etc.
 */

#include "includes.h"
#include "common.h"
#include "Engine/console.h"
#include "Engine/simulation.h"
#include "Engine/models.h"
#include "Utilities/log.h"
#include "Utilities/lua.h"
#include "AI/ai_lua.h"
#include "UI/ui_lua.h"
#include "UI/ui.h"
#include "UI/ui_window.h"
#include "UI/ui_label.h"
#include "UI/ui_button.h"
#include "Sprites/player.h"
#include "Sprites/sprite.h"
#include "Sprites/planets.h"
#include "Utilities/camera.h" 
#include "Input/input.h"
#include "Utilities/file.h"

#include "Engine/hud.h"

/**\class Lua
 * \brief Lua subsystem. */

bool Lua::luaInitialized = false;
lua_State *Lua::L = NULL;
vector<string> Lua::buffer;

bool Lua::Load( const string& filename ) {
	if( ! luaInitialized ) {
		if( Init() == false ) {
			Log::Warning( "Could not load Lua script. Unable to initialize Lua." );
			return( false );
		}
	}

	// Load the lua script
	if( 0 != luaL_loadfile(L, filename.c_str()) ) {
		Log::Error("Error loading '%s': %s", filename.c_str(), lua_tostring(L, -1));
		return false;
	}

	// Execute the lua script
	if( 0 != lua_pcall(L, 0, 0, 0) ) {
		Log::Error("Error Executing '%s': %s", filename.c_str(), lua_tostring(L, -1));
		return false;
	}

	Log::Message("Loaded Lua Script '%s'",filename.c_str());
	return( true );
}



// If the function is known at compile time, use 'Call' instead of 'Run'
bool Lua::Run( string line ) {
	//Log::Message("Running '%s'", (char *)line.c_str() );

	if( ! luaInitialized ) {
		if( Init() == false ) {
			Log::Warning( "Could not load Lua script. Unable to initialize Lua." );
			return( false );
		}
	}

	if( luaL_dostring(L,line.c_str()) ) {
		Log::Error("Error running '%s': %s", line.c_str(), lua_tostring(L, -1));
		lua_pop(L, 1);  /* pop error message from the stack */
	}

	return( false );
}

// This function is from the Lua PIL
// http://www.lua.org/pil/25.3.html
// It was originally named "call_va"
bool Lua::Call(const char *func, const char *sig, ...) {
	va_list vl;
	int narg, nres;  /* number of arguments and results */

	va_start(vl, sig);
	lua_getglobal(L, func);  /* get function */

	/* push arguments */
	narg = 0;
	while (*sig) {  /* push arguments */
		switch (*sig++) {

			case 'd':  /* double argument */
				lua_pushnumber(L, va_arg(vl, double));
				break;

			case 'i':  /* int argument */
				lua_pushnumber(L, va_arg(vl, int));
				break;

			case 's':  /* string argument */
				lua_pushstring(L, va_arg(vl, char *));
				break;

			case '>':
				goto endwhile;

			default:
				luaL_error(L, "invalid option (%c)", *(sig - 1));
		}
		narg++;
		luaL_checkstack(L, 1, "too many arguments");
	} endwhile:

	/* do the call */
	nres = strlen(sig);  /* number of expected results */
	if (lua_pcall(L, narg, nres, 0) != 0)  /* do the call */
		luaL_error(L, "error running function `%s': %s",
				func, lua_tostring(L, -1));

	/* retrieve results */
	nres = -nres;  /* stack index of first result */
	while (*sig) {  /* get results */
		switch (*sig++) {

			case 'd':  /* double result */
				if (!lua_isnumber(L, nres))
					luaL_error(L, "wrong result type");
				*va_arg(vl, double *) = lua_tonumber(L, nres);
				break;

			case 'i':  /* int result */
				if (!lua_isnumber(L, nres))
					luaL_error(L, "wrong result type");
				*va_arg(vl, int *) = (int)lua_tonumber(L, nres);
				break;

			case 's':  /* string result */
				if (!lua_isstring(L, nres))
					luaL_error(L, "wrong result type");
				*va_arg(vl, const char **) = lua_tostring(L, nres);
				break;

			default:
				luaL_error(L, "invalid option (%c)", *(sig - 1));
		}
		nres++;
	}
	va_end(vl);
	return true;
}

// returns the output from the last lua script and deletes it from internal buffer
vector<string> Lua::GetOutput() {
	vector<string> ret = buffer;

	buffer.clear();

	return( ret );
}

bool Lua::Init() {
	if( luaInitialized ) {
		Log::Warning( "Cannot initialize Lua. It is already initialized." );
		return( false );
	}
	
	L = lua_open();

	if( !L ) {
		Log::Warning( "Could not initialize Lua VM." );
		return( false );
	}

	luaL_openlibs( L );

	RegisterFunctions();
	
	luaInitialized = true;
	
	return( true );
}

bool Lua::Close() {
	if( luaInitialized ) {
		lua_close( L );
	} else {
		Log::Warning( "Cannot deinitialize Lua. It is either not initialized or a script is still loaded." );
		return( false );
	}
	
	return( true );
}

void Lua::RegisterFunctions() {
	// Register these functions to the lua global namespace

	static const luaL_Reg EngineFunctions[] = {
		{"echo", &Lua::console_echo},
		{"pause", &Lua::pause},
		{"unpause", &Lua::unpause},
		{"ispaused", &Lua::ispaused},
		{"getoption", &Lua::getoption},
		{"setoption", &Lua::setoption},
		{"player", &Lua::getPlayer},
		{"getCamera", &Lua::getCamera},
		{"moveCamera", &Lua::moveCamera},
		{"shakeCamera", &Lua::shakeCamera},
		{"focusCamera", &Lua::focusCamera},
		{"alliances", &Lua::getAllianceNames},
		{"models", &Lua::getModelNames},
		{"weapons", &Lua::getWeaponNames},
		{"engines", &Lua::getEngineNames},
		{"technologies", &Lua::getTechnologyNames},
		{"getSprite", &Lua::getSpriteByID},
		{"ships", &Lua::getShips},
		{"planets", &Lua::getPlanets},
		{"nearestShip", &Lua::getNearestShip},
		{"nearestPlanet", &Lua::getNearestPlanet},
		{"RegisterKey", &Input::RegisterKey},  
		{"UnRegisterKey", &Input::UnRegisterKey},  
		{"getModelInfo", &Lua::getModelInfo},
		{"setModelInfo", &Lua::setModelInfo},
		{"getPlanetInfo", &Lua::getPlanetInfo},
		{"setPlanetInfo", &Lua::setPlanetInfo},
		{"getWeaponInfo", &Lua::getWeaponInfo},
		{"setWeaponInfo", &Lua::setWeaponInfo},
		{"getEngineInfo", &Lua::getEngineInfo},
		{"setEngineInfo", &Lua::setEngineInfo},
		{"getTechnologyInfo", &Lua::getTechnologyInfo},
		{"setTechnologyInfo", &Lua::setTechnologyInfo},
		{NULL, NULL}
	};
	luaL_register(L,"Epiar",EngineFunctions);


	// Register these functions to their own lua namespaces
	AI_Lua::RegisterAI(L);
	UI_Lua::RegisterUI(L);
	Planets_Lua::RegisterPlanets(L);
	Hud::RegisterHud(L);
}

int Lua::console_echo(lua_State *L) {
	const char *str = lua_tostring(L, 1); // get argument

	if(str == NULL)
		Console::InsertResult("nil");
	else
		Console::InsertResult(str);

	return 0;
}

int Lua::pause(lua_State *L){
		Simulation::pause();
	return 0;
}

int Lua::unpause(lua_State *L){
	Simulation::unpause();
	return 0;
}

int Lua::ispaused(lua_State *L){
	lua_pushnumber(L, (int) Simulation::isPaused() );
	return 1;
}

int Lua::getoption(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if (n != 1)
		return luaL_error(L, "Got %d arguments expected 1 (option)", n);
	string path = (string)lua_tostring(L, 1);
	string value = OPTION(string,path);
	lua_pushstring(L, value.c_str());
	return 1;
}

int Lua::setoption(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if (n != 2)
		return luaL_error(L, "Got %d arguments expected 1 (option,value)", n);
	string path = (string)lua_tostring(L, 1);
	string value = (string)lua_tostring(L, 2);
	SETOPTION(path,value);
	return 0;
}

int Lua::getPlayer(lua_State *L){
	Lua::pushSprite(L,Player::Instance() );
	return 1;
}

int Lua::getCamera(lua_State *L){
    int n = lua_gettop(L);
	if (n != 0) {
		return luaL_error(L, "Getting the Camera Coordinates didn't expect %d arguments. But thanks anyway", n);
    }
    Coordinate c = Camera::Instance()->GetFocusCoordinate();
    lua_pushinteger(L,c.GetX());
    lua_pushinteger(L,c.GetY());
	return 2;
}

int Lua::moveCamera(lua_State *L){
    int n = lua_gettop(L);
	if (n != 2) {
		return luaL_error(L, "Moving the Camera needs 2 arguments (X,Y) not %d arguments", n);
    }
    int x = luaL_checkinteger(L,1);
    int y = luaL_checkinteger(L,2);
    Camera::Instance()->Focus((Sprite*)NULL); // This unattaches the Camera from the focusSprite
    Camera::Instance()->Move(-x,y);
	return 0;
}
//Allow camera shaking from Lua
int Lua::shakeCamera(lua_State *L){
	if (lua_gettop(L) == 4) {
		Camera *pInstance = Camera::Instance();
		pInstance->Shake(int(luaL_checknumber(L, 1)), int(luaL_checknumber(L,
						2)),  new Coordinate(luaL_checknumber(L, 3),luaL_checknumber(L, 2)));
	}
	return 0;
}

int Lua::focusCamera(lua_State *L){
    int n = lua_gettop(L);
	if (n == 1) {
		int id = (int)(luaL_checkint(L,1));
		SpriteManager *sprites= SpriteManager::Instance();
		Sprite* target = sprites->GetSpriteByID(id);
		if(target!=NULL)
            Camera::Instance()->Focus( target );
	} else if (n == 2) {
		double x,y;
        x = (luaL_checknumber(L,1));
        y = (luaL_checknumber(L,2));
        Camera::Instance()->Focus((Sprite*)NULL);
        Camera::Instance()->Focus(x,y);
    } else {
		return luaL_error(L, "Got %d arguments expected 1 (SpriteID) or 2 (X,Y)", n);
    }
	return 0;
}

int Lua::getAllianceNames(lua_State *L){
	list<string> *names = Alliances::Instance()->GetNames();
	pushNames(L,names);
	delete names;
    return 1;
}
int Lua::getWeaponNames(lua_State *L){
	list<string> *names = Weapons::Instance()->GetNames();
	pushNames(L,names);
	delete names;
    return 1;
}

int Lua::getModelNames(lua_State *L){
	list<string> *names = Models::Instance()->GetNames();
	pushNames(L,names);
	delete names;
    return 1;
}

int Lua::getEngineNames(lua_State *L){
	list<string> *names = Engines::Instance()->GetNames();
	pushNames(L,names);
	delete names;
    return 1;
}

int Lua::getTechnologyNames(lua_State *L){
	list<string> *names = Technologies::Instance()->GetNames();
	pushNames(L,names);
	delete names;
    return 1;
}

void Lua::pushSprite(lua_State *L,Sprite* s){
	int* id = (int*)lua_newuserdata(L, sizeof(int*));
	*id = s->GetID();
	assert(s->GetDrawOrder() & (DRAW_ORDER_SHIP | DRAW_ORDER_PLAYER | DRAW_ORDER_PLANET) );
	switch(s->GetDrawOrder()){
	case DRAW_ORDER_SHIP:
	case DRAW_ORDER_PLAYER:
		luaL_getmetatable(L, EPIAR_SHIP);
		lua_setmetatable(L, -2);
		break;
	case DRAW_ORDER_PLANET:
		luaL_getmetatable(L, EPIAR_PLANET);
		lua_setmetatable(L, -2);
		break;
	default:
		Log::Error("Accidentally pushing sprite #%d with invalid type: %d",s->GetID(),s->GetDrawOrder());
	}
}

/*
Sprite* Lua::checkSprite(lua_State *L,int id ){
	int* idptr = (int*)luaL_checkudata(L, index, EPIAR_SHIP);
	cout<<"Checking ID "<<(*idptr)<<endl;
	luaL_argcheck(L, idptr != NULL, index, "`EPIAR_SHIP' expected");
	Sprite* s;
	s = SpriteManager::Instance()->GetSpriteByID(*idptr);
	return s;
}
*/

void Lua::pushNames(lua_State *L, list<string> *names){
    lua_createtable(L, names->size(), 0);
    int newTable = lua_gettop(L);
    int index = 1;
    list<string>::const_iterator iter = names->begin();
    while(iter != names->end()) {
        lua_pushstring(L, (*iter).c_str());
        lua_rawseti(L, newTable, index);
        ++iter;
        ++index;
    }
}

void Lua::setField(const char* index, int value) {
	lua_pushstring(L, index);
	lua_pushinteger(L, value);
	lua_settable(L, -3);
}

void Lua::setField(const char* index, float value) {
	lua_pushstring(L, index);
	lua_pushnumber(L, value);
	lua_settable(L, -3);
}

void Lua::setField(const char* index, const char* value) {
	lua_pushstring(L, index);
	lua_pushstring(L, value);
	lua_settable(L, -3);
}

int Lua::getIntField(int index, const char* name) {
	int val;
	assert(lua_istable(L,index));
	lua_pushstring(L, name);
	assert(lua_istable(L,index));
	lua_gettable(L,index);
	val = luaL_checkint(L,index+1);
	lua_pop(L,1);
	return val;
}

float Lua::getNumField(int index, const char* name) {
	float val;
	assert(lua_istable(L,index));
	lua_pushstring(L, name);
	assert(lua_istable(L,index));
	lua_gettable(L, index);
	val = static_cast<float>(luaL_checknumber(L,index+1));
	lua_pop(L,1);
	return val;
}

string Lua::getStringField(int index, const char* name) {
	string val;
	assert(lua_istable(L,index));
	lua_pushstring(L, name);
	assert(lua_istable(L,index));
	lua_gettable(L, index);
	val = luaL_checkstring(L,index+1);
	lua_pop(L,1);
	return val;
}

//can be found here  http://www.lua.org/pil/24.2.3.html
void Lua::stackDump (lua_State *L) {
  int i;
  int top = lua_gettop(L);
  for (i = 1; i <= top; i++) {  /* repeat for each level */
	int t = lua_type(L, i);
	switch (t) {

	  case LUA_TSTRING:  /* strings */
		printf("`%s'", lua_tostring(L, i));
		break;

	  case LUA_TBOOLEAN:  /* booleans */
		printf(lua_toboolean(L, i) ? "true" : "false");
		break;

	  case LUA_TNUMBER:  /* numbers */
		printf("%g", lua_tonumber(L, i));
		break;

	  default:  /* other values */
		printf("%s", lua_typename(L, t));
		break;

	}
	printf("  ");  /* put a separator */
  }
  printf("\n");  /* end the listing */
}


int Lua::getSpriteByID(lua_State *L){
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (SpriteID)", n);

	// Get the Sprite using the ID
	int id = (int)(luaL_checkint(L,1));
	Sprite* sprite = SpriteManager::Instance()->GetSpriteByID(id);

	if(sprite==NULL){
		Log::Error("Lua requested sprite with unknown id %d",id);
		return luaL_error(L, "The ID %d doesn't refer to anything",id );
	}

	Lua::pushSprite(L,sprite);
	return 1;
}

int Lua::getSprites(lua_State *L, int type){
	int n = lua_gettop(L);  // Number of arguments

	list<Sprite *> *sprites = NULL;
	if( n==3 ){
		double x = luaL_checknumber (L, 1);
		double y = luaL_checknumber (L, 2);
		double r = luaL_checknumber (L, 3);
		sprites = SpriteManager::Instance()->GetSpritesNear(Coordinate(x,y),static_cast<float>(r),type);
	} else {
		sprites = SpriteManager::Instance()->GetSprites(type);
	}

	// Populate a Lua table with Sprites
    lua_createtable(L, sprites->size(), 0);
    int newTable = lua_gettop(L);
    int index = 1;
    list<Sprite *>::const_iterator iter = sprites->begin();
    while(iter != sprites->end()) {
		// push userdata
		pushSprite(L,(*iter));
        lua_rawseti(L, newTable, index);
        ++iter;
        ++index;
    }
    return 1;
}


int Lua::getShips(lua_State *L){
	return Lua::getSprites(L,DRAW_ORDER_SHIP);
}

int Lua::getPlanets(lua_State *L){
	Planets *planets = Planets::Instance();
	list<string>* planetNames = planets->GetNames();

    lua_createtable(L, planetNames->size(), 0);
    int newTable = lua_gettop(L);
    int index = 1;
	for( list<string>::iterator pname = planetNames->begin(); pname != planetNames->end(); ++pname){
		pushSprite(L,planets->GetPlanet(*pname));
        lua_rawseti(L, newTable, index);
        ++index;
	}
	return 1;
}

int Lua::getNearestSprite(lua_State *L,int type) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=2 ){
		return luaL_error(L, "Got %d arguments expected 1 (ship, range)", n);
	}
	AI* ai = AI_Lua::checkShip(L,1);
	if( ai==NULL ) {
		return 0;
	}
	float r = static_cast<float>(luaL_checknumber (L, 2));
	Sprite *closest = SpriteManager::Instance()->GetNearestSprite((ai),r,type);
	if(closest!=NULL){
		assert(closest->GetDrawOrder() & (type));
		pushSprite(L,(closest));
		return 1;
	} else {
		return 0;
	}
}

int Lua::getNearestShip(lua_State *L) {
	return Lua::getNearestSprite(L,DRAW_ORDER_SHIP|DRAW_ORDER_PLAYER);
}

int Lua::getNearestPlanet(lua_State *L) {
	return Lua::getNearestSprite(L,DRAW_ORDER_PLANET);
}

int Lua::getModelInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (modelName)", n);
	string modelName = (string)luaL_checkstring(L,1);
	Model *model = Models::Instance()->GetModel(modelName);

    lua_newtable(L);
	setField("Name", model->GetName().c_str());
	setField("Mass", model->GetMass());
	setField("Thrust", model->GetThrustOffset());
	setField("Engine", model->GetEngine()->GetName().c_str() );
	setField("Rotation", model->GetRotationsPerSecond());
	setField("MaxSpeed", model->GetMaxSpeed());
	setField("MaxHull", model->getMaxEnergyAbsorption());

	return 1;
}

int Lua::setModelInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (modelInfo)", n);
	if( !lua_istable(L,1) )
		return luaL_error(L, "Argument 1 is not a table");

	string name = getStringField(1,"Name");
	float mass = getNumField(1,"Mass");
	int thrust = getIntField(1,"Thrust");
	string engine = getStringField(1,"Engine");
	float rot = getNumField(1,"Rotation");
	float speed = getNumField(1,"MaxSpeed");
	int hull = getIntField(1,"MaxHull");

	Model* oldModel = Models::Instance()->GetModel(name);
	if(oldModel==NULL) return 0; // If the name changes then the below doesn't work.
	Model newModel(name,oldModel->GetImage(),oldModel->GetEngine(),mass,thrust,rot,speed,hull);
	*oldModel = newModel;

	return 0;
}

int Lua::getPlanetInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (planetID)", n);
	int id = luaL_checkinteger(L,1);
	Sprite* sprite = SpriteManager::Instance()->GetSpriteByID(id);
	if( sprite->GetDrawOrder() != DRAW_ORDER_PLANET)
		return luaL_error(L, "ID #%d does not point to a Planet", id);

	Planet* p = (Planet*)(sprite);

    lua_newtable(L);
	setField("Name", p->GetName().c_str());
	setField("Alliance", p->GetAlliance().c_str());
	setField("Traffic", p->GetTraffic());
	setField("Militia", p->GetMilitiaSize());
	setField("Landable", p->GetLandable());
	return 1;
}

int Lua::setPlanetInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (planetInfo)", n);
	if( !lua_istable(L,1) )
		return luaL_error(L, "Argument 1 is not a table");

	string name = getStringField(1,"Name");
	string alliance = getStringField(1,"Alliance");
	int traffic = getIntField(1,"Traffic");
	int militia = getIntField(1,"Militia");
	int landable = getIntField(1,"Landable");

	Planet* oldPlanet = Planets::Instance()->GetPlanet(name);
	if(oldPlanet==NULL) return 0; // If the name changes then the below doesn't work.
	*oldPlanet = Planet(name,alliance,(bool)landable,traffic,militia,oldPlanet->GetInfluence(), oldPlanet->GetMilitia(), oldPlanet->GetTechnologies());

	return 0;
}

int Lua::getWeaponInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (weaponName)", n);
	string weaponName = (string)luaL_checkstring(L,1);
	Weapon* weapon = Weapons::Instance()->GetWeapon(weaponName);
	if( weapon == NULL)
		return luaL_error(L, "There is no weapon named '%s'.", weaponName.c_str());

    lua_newtable(L);
	setField("Name", weapon->GetName().c_str());
	setField("Payload", weapon->GetPayload());
	setField("Velocity", weapon->GetVelocity());
	setField("Acceleration", weapon->GetAcceleration());
	setField("FireDelay", weapon->GetFireDelay());
	setField("Lifetime", weapon->GetLifetime());
	return 1;
}

int Lua::setWeaponInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (planetInfo)", n);
	if( !lua_istable(L,1) )
		return luaL_error(L, "Argument 1 is not a table");

	string name = getStringField(1,"Name");
	int payload = getIntField(1,"Payload");
	int velocity = getIntField(1,"Velocity");
	int acceleration = getIntField(1,"Acceleration");
	int fireDelay = getIntField(1,"FireDelay");
	int lifetime = getIntField(1,"Lifetime");

	Weapon* oldWeapon = Weapons::Instance()->GetWeapon(name);
	if(oldWeapon==NULL) return 0; // If the name changes then the below doesn't work.
	*oldWeapon = Weapon(name,oldWeapon->GetImage(),oldWeapon->GetPicture(),oldWeapon->GetType(),payload,velocity,acceleration,oldWeapon->GetAmmoType(),oldWeapon->GetAmmoConsumption(),fireDelay,lifetime,oldWeapon->sound);

	return 0;
}

int Lua::getEngineInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (weaponName)", n);
	string engineName = (string)luaL_checkstring(L,1);
	Engine* engine = Engines::Instance()->GetEngine(engineName);
	if( engine == NULL)
		return luaL_error(L, "There is no engine named '%s'.", engineName.c_str());

    lua_newtable(L);
	setField("Name", engine->GetName().c_str());
	setField("Force", engine->GetForceOutput());
	setField("Animation", engine->GetFlareAnimation().c_str());
	setField("MSRP", engine->GetMSRP());
	setField("Fold Drive", engine->GetFoldDrive());
	return 1;
}

int Lua::setEngineInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (planetInfo)", n);
	if( !lua_istable(L,1) )
		return luaL_error(L, "Argument 1 is not a table");

	string name = getStringField(1,"Name");
	int force = getIntField(1,"Force");
	string flare = getStringField(1,"Animation");
	int msrp = getIntField(1,"MSRP");
	int foldDrive = getIntField(1,"Fold Drive");

	Engine* oldEngine = Engines::Instance()->GetEngine(name);
	if(oldEngine==NULL) return 0; // If the name changes then the below doesn't work.
	*oldEngine = Engine(name,oldEngine->thrustsound,force,msrp,foldDrive,flare);

	return 0;
}

int Lua::getTechnologyInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (techName)", n);
	string techName = (string)luaL_checkstring(L,1);
	Technology* tech = Technologies::Instance()->GetTechnology(techName);
	if( tech == NULL)
		return luaL_error(L, "There is no technology named '%s'.", techName.c_str());
	
	// The info for a technology is a nested table:
	// techInfo = { Weapons={...}, Models={...}, Engines={...} }

	// Push the Main Table
	int i;
	list<Model*>::iterator iter_m;
	list<Weapon*>::iterator iter_w;
	list<Engine*>::iterator iter_e;

	// Push the Models Table
	list<Model*> models = tech->GetModels();
    lua_createtable(L,models.size(),0);
	int modeltable = lua_gettop(L);
	for(i=1,iter_m=models.begin();iter_m!=models.end();++iter_m,++i) {
		lua_pushstring(L, (*iter_m)->GetName().c_str() );
        lua_rawseti(L, modeltable, i);
	}

	// Push the Weapons Table
	list<Weapon*> weapons = tech->GetWeapons();
    lua_newtable(L);
	int weapontable = lua_gettop(L);
	for(i=1,iter_w=weapons.begin();iter_w!=weapons.end();++iter_w,++i) {
		lua_pushstring(L, (*iter_w)->GetName().c_str() );
        lua_rawseti(L, weapontable, i);
	}

	// Push the Engines Table
	list<Engine*> engines = tech->GetEngines();
    lua_newtable(L);
	int enginetable = lua_gettop(L);
	for(i=1,iter_e=engines.begin();iter_e!=engines.end();++iter_e,++i) {
		lua_pushstring(L, (*iter_e)->GetName().c_str() );
        lua_rawseti(L, enginetable, i);
	}

	return 3;
}

int Lua::setTechnologyInfo(lua_State *L) {
	int n = lua_gettop(L);  // Number of arguments
	if( n!=1 )
		return luaL_error(L, "Got %d arguments expected 1 (planetInfo)", n);
	if( !lua_istable(L,1) )
		return luaL_error(L, "Argument 1 is not a table");

	cerr<<"Error Not Implemented!"<<endl;

	return 0;
}

