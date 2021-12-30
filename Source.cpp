#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <iostream>
#include <fstream>
#include <vector>  
#include <string>
#include <math.h>
#include <typeinfo>
#include <ctime>

// ------------------------------GLOBALS------------------------------

const Uint8 *keys = SDL_GetKeyboardState(NULL);
const double pi{ 3.141592653 };
const int screenw{ 640 };
const int screenh{ 416 }; // + 64 for HUD
const int viewRangeH{ 10 };
const int viewRangeV{ 8 };
SDL_Texture *zoom;
int g_format;
int g_count{ 0 };
int g_lives{ 3 };
int g_score{ 0 };
int g_levelW;
int g_levelH;
static std::vector<SDL_Texture*> backgrounds;
static std::vector<SDL_Texture*> rain;






// ------------------------------DECLARATIONS------------------------------

class Player;
class Object;
class Wall;
class Water;
class Ice;






// ------------------------------FUNCTIONS------------------------------

// returns the number of digits in a given number
int getDigits(int n)
{
	int c{ 0 };
	for (int i{ 10 }; i >= 0; i--)
	{
		if (floor(n / pow(10, i)) != 0)
			c++;
	}
	if (c == 0)
		c++;
	return c;
}


// takes 2 rects and returns true if they overlap, false otherwise (checks at intervals of 8 pixels)
bool collided(SDL_Rect &left, SDL_Rect &right)
{
	for (int h{ 0 }; h <= left.w; h += 8)
	{
		for (int v{ 0 }; v <= left.h; v += 8)
		{
			if (left.x + h > right.x && left.x + h < right.x + right.w
				&& left.y + v > right.y && left.y + v < right.y + right.h)
				return true;
		}
	}
	return false;
}


// takes 2 rects and if they collide moves them (step) backwards until they no longer collide
bool align(SDL_Rect &left, SDL_Rect &right, int xstep, int ystep)
{
	bool rvalue{ false };
	while (collided(left, right))
	{
		left = { left.x - xstep, left.y - ystep, left.w, left.h };
		rvalue = true;
	}
	return rvalue;
}


// gets the first index at which a given term appears in a vector
template <typename T>
int getIndex(std::vector<T> *vector, T &term)
{
	for (int i{ 0 }; i < vector->size(); i++)
	{
		if (vector->at(i) == term)
			return i;
	}
	return -1;
}


// draws the given string to a texture 
void stringTexture(TTF_Font *font, std::string &string, SDL_Texture *text)
{
	SDL_Surface *scoreSurface = SDL_ConvertSurfaceFormat(TTF_RenderText_Shaded(font, string.c_str(), { 255, 255, 255 }, { 0, 0, 0 }), g_format, 0);
	SDL_UpdateTexture(text, NULL, scoreSurface->pixels, scoreSurface->pitch);
	SDL_FreeSurface(scoreSurface);
}


// load the level file into the correct format
void loadLevel(std::vector<std::vector<int>>* level, int* tileSet, bool* weather, int* track, std::string path)
{
	level->clear(); // empty level vector
	std::ifstream levelInput(path); // open the file
	std::string line;
	if (levelInput.is_open()) // if file opened correctly
	{
		int whileCount{ 0 };
		std::getline(levelInput, line);
		*weather = static_cast<int>(line.at(0) - 48); // read weather effect
		*track = static_cast<int>(line.at(1) - 48); // read music track
		*tileSet = static_cast<int>(line.at(2) - 48); // read tileset used
		while (std::getline(levelInput, line)) // take the next line and store it in string
		{
			level->push_back({}); // add an empty vector
			for (char c : line) // push the ascii integer value of each character to new vector
				level->at(whileCount).push_back(static_cast<int>(c - 97));
			whileCount++;
		}
	}
	levelInput.close(); // close the file
}






// ------------------------------CLASS DEFINITIONS------------------------------

// base class for all classes but player
class Object
{
protected:
	int m_x;
	int m_y;
	int m_vx;
	int m_vy;
	int m_frame{ 0 };
	double m_traction{ 0.5 };
	SDL_Rect m_rect;
	Object(int x, int y, int w, int h, bool solid, bool hazard, bool enemy, bool collectible) :
		m_x{ x }, m_y{ y }, m_startx{ x }, m_starty{ y }, m_rect{ x, y, w, h }, m_solid{ solid }, m_hazard{ hazard }, m_enemy{ enemy }, m_collectible{ collectible }
	{}
public:
	bool m_exists{ true };
	// m_protected is used to protect an object from being removed from the update queue, for example it is used for enemies which are still
	// on screen despite their starting position being off screen.
	bool m_protected{ false };
	const int m_startx;
	const int m_starty;
	const bool m_solid;
	const bool m_hazard;
	const bool m_enemy;
	const bool m_collectible;
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) = 0;
	virtual void reset() // returns object to its starting position
	{
		m_x = m_startx;
		m_y = m_starty;
		m_exists = true;
	}
	virtual void resetStrong() // used in some inheriting classes to reset additional properties
	{
		this->reset();
	}
	virtual SDL_Rect getRect()
	{
		return m_rect;
	}
	virtual void action()
	{}
	virtual void setFrame(int i)
	{
		m_frame = i;
	}
	virtual double getTraction()
	{
		return m_traction;
	}
	int getx() { return m_x; }
	int gety() { return m_y; }
};


// player character
class Player
{
private:
	int m_x;
	int m_y;
	int m_frame{ 0 };
	int m_flip{ 0 };
	double m_hspd;
	double m_vspd;
	SDL_Rect m_rect;
	bool m_grounded{ true };
	bool m_jumping{ false };
public:
	static std::vector<SDL_Texture*> m_imageSet;
	static std::vector<Mix_Chunk*> m_sounds;
	int v_x; // these variables mark the center of the viewpoint and are used by many other classes
	int v_y;
	Player(int x, int y) :
		m_x{ x }, m_y{ y }, m_hspd{ 0 }, m_vspd{ 0 }, m_rect{ x, y, 28, 32 }
	{}
	void draw(SDL_Renderer *ren)
	{
		SDL_Rect vrect{ v_x - 2, v_y, 32, 32 }; // (offset better fits the sprite to the hitbox)
		SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, static_cast<SDL_RendererFlip>(m_flip));
	}
	int update(SDL_Renderer *ren, std::vector<Object*> *solids, std::vector<Object*> *hazards, std::vector<Object*> *enemies, 
		std::vector<Object*> *collectibles)
	{
		int result = 0;
		int maxSpd = 4;
		double acc{ 0.25 };
		bool slide{ false };

		// first moves player downwards to check if standing on a solid object
		m_rect.y += 1;
		m_grounded = false;
		for (int i{ 0 }; i < solids->size(); i++)
		{
			if (collided(m_rect, solids->at(i)->getRect()))

			{
				m_grounded = true;
				// handles slipping on icy objects (stupid to check for specific classes, update object with additional property?)
				acc = solids->at(i)->getTraction();
				std::string className = typeid(*solids->at(i)).name();
				if (className == "class Ice" || className == "class ThinIce")
				{
					slide = true;
				}
			}
		}
		m_rect.y -= 1; // (undos shift downwards)
		// CONTROLS 
		if (keys[SDL_SCANCODE_ESCAPE]) // reset to main menu
			result = -2;
		if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) // if left control inputted
		{
			m_flip = true;
			if (m_hspd > -maxSpd)
				m_hspd -= acc; // accelerate unless at max speed
		}
		else if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) // if right control inputted
		{
			m_flip = false;
			if (m_hspd < maxSpd)
				m_hspd += acc; // accelerate unless at max speed
		}
		else if (m_hspd != 0) // if neither movement keys pressed
		{
			if (m_grounded && !slide) // if on the ground
			{
				m_hspd -= acc * m_hspd / abs(m_hspd); // decelerate
			}
			if (!m_grounded) // if in the air
			{
				m_hspd -= acc / 3 * m_hspd / abs(m_hspd); // decelerate at a lower rate
			}
			if (abs(m_hspd) < acc) // if almost stopped
				m_hspd = 0; // stop
		}
		if (abs(m_hspd) > maxSpd) // if exceeded max speed
			m_hspd = maxSpd * m_hspd / abs(m_hspd); // set speed to max speed
		if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_UP]) // if jump key pressed
		{
			if (m_grounded) // jump if on the ground
			{
				m_jumping = true;
				m_vspd = -10;
				Mix_PlayChannel(7, m_sounds[1], 0);
			}
		}
		else if (m_jumping) // if jump key released during a jump
		{
			m_jumping = false; // shorten jump height
			m_vspd *= 0.5;
		}
		if (!m_jumping && Mix_Playing(7)) // cuts out jumping noise
			Mix_FadeOutChannel(7, 125);
		if (m_jumping && m_vspd > 0)
			m_jumping = false;

		// updating coordinates and collision checks

		// solid collision
		m_vspd += 0.3; // acceleration due to gravity
		// perform x movement
		m_x += m_hspd;
		m_rect = { m_x, m_y, 28, 32 };
		// now check for collisions with solid blocks
		for (int i{ 0 }; i < solids->size(); i++)
		{
			if (align(m_rect, solids->at(i)->getRect(), m_hspd / abs(m_hspd), 0))
			{
				m_hspd = 0;
			}
			m_x = m_rect.x;
		}
		// does the same as above for y movement
		m_y += m_vspd;
		m_rect = { m_x, m_y, 28, 32 };
		for (int i{ 0 }; i < solids->size(); i++)
		{
			if (align(m_rect, solids->at(i)->getRect(), 0, m_vspd / abs(m_vspd)))
			{
				m_vspd = 0;
			}
			m_y = m_rect.y;
		}

		// level boundary checks
		if (m_y > g_levelH * 32) // if below the bottom of the screen
			result = -1; // kill player

		if (m_x > g_levelW * 32) // if past the right edge of the screen
			result = 1; // level complete

		// enemy collision
		m_rect.y += 1;
		bool enemyhit{ 0 };
		for (int i{ 0 }; i < enemies->size(); i++)
		{
			if (enemies->at(i)->m_exists && collided(m_rect, enemies->at(i)->getRect()))
			{
				if (m_y + 16 < enemies->at(i)->gety()) // if player high enough above enemy
				{
					enemyhit = 1;
					enemies->at(i)->m_exists = false; // kill enemy
					if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_UP]) // if holding jump perform larger bounce
					{
						m_vspd = -10; 
						m_jumping = true;
					}
					else // small bounce otherwise
						m_vspd = -4;
					enemies->at(i)->action(); // change score
					Mix_PlayChannel(7, m_sounds[1], 0);
					break;
				}
				else if (enemyhit) // kills any additional enemies hit if already successfully bouncing off of an enemy
					enemies->at(i)->m_exists = false;
			}
		}

		// hazard collision
		for (int i{ 0 }; i < hazards->size(); i++)
			if (hazards->at(i)->m_exists && collided(m_rect, hazards->at(i)->getRect())) // if in contact with hazard
				result = -1; // kill player
		m_rect.y -= 1;

		// collectible collision
		for (int i{ 0 }; i < collectibles->size(); i++)
			if (collided(m_rect, collectibles->at(i)->getRect()) && collectibles->at(i)->m_exists)
			{
				collectibles->at(i)->m_exists = false;
				collectibles->at(i)->action();
				Mix_PlayChannel(-1, m_sounds[0], 0);
			}

		// drawing
		if (floor(abs(m_hspd)) > 0 && m_frame != 1 && m_frame != 2) // start running animation on motion
			m_frame = 1;
		if (m_hspd == 0 || (!(keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])))
			m_frame = 0; // set to standing sprite if not moving
		if (g_count % 6 == 0 && m_grounded) // every 6 frames cycle running animation
		{
			if (m_frame != 0)
				m_frame = m_frame % 2 + 1;
		}
		// calculates viewpoint center, halting near either end of the level
		v_x = screenw / 2;
		v_y = screenh / 2 + 64;
		if (m_y + screenh / 2 > 32 * g_levelH)
			v_y = screenh + m_y - 32 * g_levelH + 64;
		else if (m_y - screenh / 2 < 0)
			v_y = m_y + 64;
		if (m_x + screenw / 2 > 32 * g_levelW)
			v_x = screenw + m_x - 32 * g_levelW;
		else if (m_x - screenw / 2 < 0)
			v_x = m_x;

		return result;
	}
	SDL_Rect getRect() { return m_rect; }
	int getx() { return m_x; }
	int gety() { return m_y; }
};
std::vector<SDL_Texture*> Player::m_imageSet{ 0 };
std::vector<Mix_Chunk*> Player::m_sounds{ 0 };


// ---------------LEVEL STRUCTURE---------------


// walls that make up the level
class Wall : public Object
{
private:
	bool m_check{ false };
	std::vector<bool> m_adjacent{ true, true, true, true }; // whether another block exists to the top, left, bottom, or right
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Wall(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 32, 32, true, false, false, false)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, 
		std::vector<Object*> *hazards) override
	{
		if (!m_check) // performs adjacency checks to fill out m_adjacent on first frame updated
		{
			int yx[2]{ m_y, m_x };
			int checks[4]{ 0, 0, -g_levelH + 1, -g_levelW + 1 }; // level boundaries to check against
			for (int i{ 0 }; i < 4; i++)
			{
				// checks the four sides for the level boundaries or adjacent blocks
				int sign = pow(-1, i / 2); // positive first two i, negative last two i
				if (sign * (yx[i % 2] / 32 - sign) < checks[i])
					m_adjacent[i] = false;
				else if (level[m_y / 32 - sign * ((i + 1) % 2)][m_x / 32 - sign * (i % 2)] != 0)
				{
					if (level[m_y / 32 - sign * ((i + 1) % 2)][m_x / 32 - sign * (i % 2)]->m_solid == true)
						m_adjacent[i] = false;
				}
			}
			m_check = true;
		}

		// calculates draw position, e.g.: p->v_x + (m_x - p->getx()) = viewpoint center + relative position
		SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
		SDL_RenderCopy(ren, m_imageSet[m_frame], NULL, &vrect);
		for (int i{ 2 }; i < 5; i++)
		{
			// draws borders on each side with no adjacent block
			if (m_adjacent[i - 1])
				SDL_RenderCopy(ren, m_imageSet[m_frame + i], NULL, &vrect);
		}
		if (m_adjacent[0]) // draws grass if no block above
		{
			vrect.y -= 4;
			vrect.x -= 2;
			vrect.h = 34;
			vrect.w = 36;
			SDL_RenderCopy(ren, m_imageSet[m_frame + 1], NULL, &vrect);
		}
	}
};
std::vector<SDL_Texture*> Wall::m_imageSet{ 0 };


// water hazards
class Water : public Object
{
private:
	bool m_check{ false };
	bool m_top{ true };
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Water(int x, int y, SDL_Renderer *ren) :
		Object(x, y + 3, 32, 29, false, true, false, false)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (!m_check) // checks if top block of water on first frame updated
		{
			Object *ptr{ level[m_y / 32 - 1][m_x / 32] };
			if (m_y / 32 - 1 < 0)
				m_top = false;
			else if (ptr != 0)
				if (ptr->m_solid || ptr->m_hazard && !ptr->m_enemy)
					m_top = false;
			m_check = true;
		}
		SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
		if (m_top) // if top block then draw wave animation
		{
			if (g_count % 40 == 0)
				m_frame = 1;
			else if (g_count % 40 == 20)
				m_frame = 0;
		}
		else
			m_frame = 2;
		SDL_RenderCopy(ren, m_imageSet[m_frame], NULL, &vrect);
	}
};
std::vector<SDL_Texture*> Water::m_imageSet{ 0 };


// thorn hazards
class Thorns : public Object
{
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Thorns(int x, int y, SDL_Renderer *ren) :
		Object(x, y + 3, 32, 29, false, true, false, false)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
		SDL_RenderCopy(ren, m_imageSet[m_frame], NULL, &vrect);
	}
};
std::vector<SDL_Texture*> Thorns::m_imageSet{ 0 };


// ice that the player slides on
class Ice : public Object
{
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Ice(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 32, 32, true, false, false, false)
	{
		m_traction = 0.1;
	}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, 
		std::vector<Object*> *hazards) override
	{
		SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
		SDL_RenderCopy(ren, m_imageSet[0], NULL, &vrect);
	}
};
std::vector<SDL_Texture*> Ice::m_imageSet{ 0 };


// thin ice that cracks to water after the player steps on it, and eventually refreezes
class ThinIce : public Object
{
private:
	int m_cracks{ 0 };
	int m_timerBase{ -1 }; // startpoint for refreeze timer
	int m_frame{ 0 };
public:
	static std::vector<SDL_Texture*>m_imageSet;
	ThinIce(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 32, 32, true, false, false, false)
	{
		m_traction = 0.1;
	}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (m_cracks == 40) // if ice cracked
		{
			if (m_timerBase == -1) // if first frame cracked
			{
				m_timerBase = g_count; // start the refreeze timer at the current frame
				// draw as water and update hitbox
				m_frame = 4;
				m_rect.y += 3;
				m_rect.h = 29;
			}
			else
			{
				// try find this object in the hazards vector
				auto i{ std::find(hazards->begin(), hazards->end(), static_cast<Object*>(this)) };
				if (g_count - m_timerBase < 100) // if not refrozen yet
				{
					if (i == hazards->end()) // if failed to find object in hazards
						hazards->push_back(static_cast<Object*>(this)); // add to hazards
					// water animation
					if (g_count % 40 == 0)
						m_frame = 5;
					else if (g_count % 40 == 20)
						m_frame = 4;
				}
				else if (i != hazards->end()) // if refrozen and still in hazards
				{
					hazards->erase(i); // remove from hazards
					// reset variables, returning to ice
					m_timerBase = -1;
					m_frame = 0;
					m_cracks = 0;
					m_rect.y -= 3;
					m_rect.h = 32;
				}
			}
		}
		else // if ice not cracked
		{
			m_frame = m_cracks / 10; // update sprite to display cracks
			if (floor((p->getx() + 14) / 32) == m_x / 32 && floor(p->gety() / 32) == m_y / 32 - 1) // if player standing on this block
				m_cracks += 1; // add a crack
			else if (g_count % 20 == 0 && m_cracks > 0) // else slowwly uncrack
				m_cracks -= 1;
		}
		SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
		SDL_RenderCopy(ren, m_imageSet[m_frame], NULL, &vrect);
	}
	virtual void reset() override
	{
		m_timerBase = -1;
		m_cracks = 0;
		if (m_frame > 3)
		{
			m_frame = 0;
			m_rect.y -= 3;
			m_rect.h = 32;
		}
	}
};
std::vector<SDL_Texture*> ThinIce::m_imageSet{ 0 };


// these three classes are for additional cosmetic objects I ended up not using
class Scenery3 : public Object
{
private:
	int m_type{ 0 };
	bool m_check{ false };
	std::vector<SDL_Texture*> m_imageSet;
public:
	Scenery3(int x, int y, SDL_Renderer *ren,  std::vector<SDL_Texture*> imageSet) :
		Object(x, y, 32, 32, false, false, false, false), m_imageSet{ imageSet }
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (!m_check)
		{
			for (int i{ 1 }; i < 4; i++)
				if (level[m_y / 32 + i][m_x / 32] != 0 && level[m_y / 32 + i][m_x / 32]->m_solid == true)
				{
					m_type = i - 1;
					break;
				}
			m_check = true;
		}
		SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 * (m_type + 1) };
		SDL_RenderCopy(ren, m_imageSet[m_type], NULL, &vrect);
	}
};
class Tree : public Scenery3
{
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Tree(int x, int y, SDL_Renderer *ren) :
		Scenery3{ x, y, ren, m_imageSet }
	{}
};
std::vector<SDL_Texture*> Tree::m_imageSet{ 0 };
class Flower : public Scenery3
{
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Flower(int x, int y, SDL_Renderer *ren) :
		Scenery3{ x, y, ren, m_imageSet }
	{}
};
std::vector<SDL_Texture*> Flower::m_imageSet{ 0 };


// ---------------LEVEL FEATURES---------------


// snake enemy that walks back and forth slowly
class Snake : public Object
{
private:
	int m_hspd{ 2 };
	bool m_flip{ 0 };
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Snake(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 16, 32, false, true, true, false) // 16, 32
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (m_exists)
		{
			if (g_count % 10 == 0) // if on a frame which is a multiple of 10
			{
				m_frame = (m_frame + 1) % 2; // advance animation to next frame
				m_x += m_hspd; // move forward
				m_rect = { m_x, m_y, m_rect.w, m_rect.h }; // update collision rect
				int start = m_hspd;
				for (int i{ 0 }; i < solids->size(); i++) // check for horizontal collisions
				{
					if (align(m_rect, solids->at(i)->getRect(), m_hspd / abs(m_hspd), 0)) // if a collision found
						m_hspd = -start; // reverse direction
					m_x = m_rect.x;
				}
				// the object in front and below the snake (i.e. the next object it will walk on)
				Object *adjacent1 = level[m_y / 32 + 1][(m_x + 8 + 32 * m_hspd / abs(m_hspd)) / 32];
				// the object behind and below the snake
				Object *adjacent2 = level[m_y / 32 + 1][(m_x + 8 - 32 * m_hspd / abs(m_hspd)) / 32];
				if (adjacent1 == nullptr) // if empty space in front and below the snake (i.e. at a ledge)
				{
					if (adjacent2 != nullptr) // and solid block behind and below
						m_hspd *= -1; // turn around
				}
				else if (!adjacent1->m_solid || adjacent1->m_hazard) // if there is a block but it is not solid or a hazard
				{
					if (adjacent2 != nullptr && (adjacent2->m_solid || !adjacent2->m_hazard)) // if there is a safe, solid block behind
						m_hspd *= -1; // reverse direction
				}
				m_flip = (m_hspd < 0); // flip sprite according to speed
			}
			// protects the snake from removal from the update queue if still on screen
			if (abs(m_x - p->getx() - (320 - p->v_x)) < viewRangeH * 32 && abs(m_y - p->gety() - (320 - p->v_y)) < viewRangeV * 32)
				m_protected = true;
			else
				m_protected = false;

			SDL_Rect vrect{ p->v_x + m_x - p->getx() - 8, p->v_y + m_y - p->gety(), 32, 32 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, static_cast<SDL_RendererFlip>(m_flip));
		}
		else
			m_protected = false; // don't protect the snakes update queue position if it is dead
	}
	virtual void action()
	{
		g_score += 50; // add score on death
	}
};
std::vector<SDL_Texture*> Snake::m_imageSet{ 0 };


// pterodactyl enemey that flies back and forth over a fixed distance
class Ptero : public Object
{
private:
	int m_timerBase{ -1 };
	int m_interval{ 80 }; // flight interval
	double m_acc{ -0.125 };
	double m_hspd{ m_interval/2 * m_acc };
	bool m_flip{ 0 };
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Ptero(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 32, 32, false, true, true, false)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (m_exists)
		{
			if (m_timerBase == -1) // sets reference point to time from
				m_timerBase = g_count;
			if ((g_count - m_timerBase) % m_interval == 0) // reverse at correct time
				m_acc *= -1;
			if (g_count % 10 == 0) // flap wings every 10 frames
				m_frame = (m_frame + 1) % 2;
			m_hspd += m_acc; // accelerate
			if (abs(m_hspd) < 0.05) // if almost stopped
				m_hspd = 0; // stop
			if (m_hspd != 0)
				m_x += floor(abs(m_hspd)) * m_hspd / abs(m_hspd); // update position
			m_rect = { m_x, m_y, m_rect.w, m_rect.h }; // update rect
			for (int i{ 0 }; i < solids->size(); i++) // check for collisions with solids
				if (align(m_rect, solids->at(i)->getRect(), m_hspd / abs(m_hspd), 0))
					m_hspd = 0;
			m_flip = (m_hspd < 0); // flip sprite depending on speed
			// deals with protections
			if (abs(m_x - p->getx() - (320 - p->v_x)) < viewRangeH * 32 && abs(m_y - p->gety() - (320 - p->v_y)) < viewRangeV * 32)
				m_protected = true;
			else
				m_protected = false;

			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, static_cast<SDL_RendererFlip>(m_flip));
		}
		else
		{
			m_protected = false;
			m_timerBase = -1;
		}
	}
	virtual void reset()
	{
		m_x = m_startx;
		m_y = m_starty;
		m_exists = true;
		m_timerBase = -1;
		m_acc = -abs(m_acc);
		m_hspd = m_interval / 2 * m_acc;
	}
	virtual void resetStrong() override
	{
		this->reset();
	}
	virtual void action()
	{
		g_score += 100;
	}
};
std::vector<SDL_Texture*> Ptero::m_imageSet{ 0 };


// frog enemy that jumps towards the player
class Frog : public Object
{
private:
	int m_timerBase{ -1 };
	double m_hspd = 0;
	double m_vspd = 0;
	bool m_grounded{ true };
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Frog(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 32, 32, false, true, true, false) // 16, 32
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (!m_exists)
		{
			m_protected = false;
			return;
		}
		if (m_exists)
		{
			m_rect.y += 1;
			m_grounded = false; // stores whether the frog if on the ground
			for (Object *solid : *solids) // check for solid blocks underneath
			{
				if (collided(m_rect, solid->getRect()))
					m_grounded = true;
			}
			m_rect.y -= 1;
			if (m_timerBase == -1) // sets a timing reference point
				m_timerBase = g_count;
			else if ((g_count - m_timerBase) % 50 == 0 && m_grounded) // if time up and on the ground
			{
				m_timerBase = -1; // reset timer
				// move on a trajectory onto the player
				double x = m_x - p->getx() - 16;
				double y = m_y - p->gety() - 16;
				double dir = atan((pow(10, 2) + sqrt(pow(10, 4) - 0.3*(0.3*pow(x, 2) + 2 * y*pow(10, 2)))) / (0.3 * x));
				if (!isnan(dir))
				{
					m_hspd = -(x / abs(x)) * 10 * cos(dir);
					m_vspd = -(x / abs(x)) * 10 * sin(dir);
					m_grounded = false;
				}
			}
			if (!m_grounded) // if in the air fall due to gravity
				m_vspd += 0.3;
			else
				m_hspd = 0;
			m_x += m_hspd; // update x position
			m_rect = { m_x, m_y, 32, 32 }; // update collision rect
			// performs horizontal collision checks/allignments
			for (int i{ 0 }; i < solids->size(); i++)
			{
				if (align(m_rect, solids->at(i)->getRect(), m_hspd / abs(m_hspd), 0))
				{
					m_hspd *= -1;
				}
				m_x = m_rect.x;
			}
			m_y += m_vspd; // update y position
			m_rect = { m_x, m_y, 32, 32 }; // update collision rect
			// performs vertical collision checks/allignments
			for (int i{ 0 }; i < solids->size(); i++)
			{
				if (align(m_rect, solids->at(i)->getRect(), 0, m_vspd / abs(m_vspd)))
				{
					m_vspd = 0;
					m_timerBase = -1;
				}
				m_y = m_rect.y;
			}
			// protects the frog on screen
			if (abs(m_x - p->getx() - (320 - p->v_x)) < viewRangeH * 32 && abs(m_y - p->gety() - (320 - p->v_y)) < viewRangeV * 32)
				m_protected = true;
			else
				m_protected = false;
			// gets correct sprite + direction to face
			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
			SDL_RendererFlip flip = SDL_FLIP_NONE;
			if (m_grounded)
				flip = static_cast<SDL_RendererFlip>((m_x > p->getx()));
			else
				flip = static_cast<SDL_RendererFlip>((m_hspd < 0));
			SDL_RenderCopyEx(ren, m_imageSet[abs(m_grounded - 1)], NULL, &vrect, 0, NULL, flip);
		}
	}
	virtual void reset()
	{
		m_x = m_startx;
		m_y = m_starty;
		m_rect = { m_x, m_y, 32, 32 };
		m_exists = true;
		m_timerBase = -1;
		m_hspd = 0;
		m_vspd = 0;
	}
	virtual void action()
	{
		g_score += 100;
	}
};
std::vector<SDL_Texture*> Frog::m_imageSet{ 0 };


// spore projectile launched by plant enemies
class Spore : public Object
{
private:
	double m_hspd;
	double m_vspd;
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Spore(int x, int y, double hspd, double vspd, SDL_Renderer *ren, std::vector<Object*> *hazards) :
		Object(x + 8, y + 8, 16, 16, false, true, false, false), m_hspd{ hspd }, m_vspd{ vspd }
	{
		hazards->push_back(static_cast<Object*>(this));
	}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (m_exists)
		{
			Object *temp = static_cast<Object*>(this); // gets an Object pointer for the spore
			if (getIndex(hazards, temp) == -1) // checks hazards for this Object pointer
				hazards->push_back(temp); // adds it if its not there
			m_vspd += 0.3; // accelerate
			m_x += m_hspd; // update x position
			m_rect = { m_x, m_y, 16, 16 }; // update collision rect
			// performs horizontal collision checks/allignments
			for (int i{ 0 }; i < solids->size(); i++)
			{
				if (align(m_rect, solids->at(i)->getRect(), m_hspd / abs(m_hspd), 0))
				{
					m_hspd = 0;
					cleanup(hazards); // safely deletes self and removes from groups
				}
				m_x = m_rect.x;
			}
			m_y += m_vspd;
			m_rect = { m_x, m_y, 16, 16 };
			// performs vertical collision checks/allignments
			for (int i{ 0 }; i < solids->size(); i++)
			{
				if (align(m_rect, solids->at(i)->getRect(), 0, m_vspd / abs(m_vspd)))
				{
					m_vspd = 0;
					cleanup(hazards); // safely deletes self and removes from groups
				}
				m_y = m_rect.y;
			}
			if (m_y > static_cast<int>(level.size() * 32)) // if outside level range
			{
				cleanup(hazards); // safely deletes self and removes from groups
			}
			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 16, 16 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, SDL_FLIP_NONE);
		}
	}
	void cleanup(std::vector<Object*> *hazards) // removes from active hazards, always does this before plant deletes the full spore object
	{
		m_exists = false;
		Object *ptr = static_cast<Object*>(this);
		if (getIndex(hazards, ptr) != -1)
			hazards->erase(hazards->begin() + getIndex(hazards, ptr));
	}
};
std::vector<SDL_Texture*> Spore::m_imageSet{ 0 };


// *WORLD 2* snowball projectile
class Snowball : public Object
{
private:
	double m_hspd;
	double m_vspd;
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Snowball(int x, int y, double hspd, double vspd, SDL_Renderer *ren, std::vector<Object*> *hazards) :
		Object(x + 8, y + 8, 16, 16, false, true, false, false), m_hspd{ hspd }, m_vspd{ vspd }
	{
		hazards->push_back(static_cast<Object*>(this));
	}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (m_exists)
		{
			Object *temp = static_cast<Object*>(this); // sets the this pointer to an object pointer
			if (getIndex(hazards, temp) == -1) // checks hazards for this changed pointer
				hazards->push_back(temp); // adds it if its not there
			m_x += m_hspd; // update x position
			m_rect = { m_x, m_y, 16, 16 }; // update collision rect
										   // performs horizontal collision checks/allignments
			for (int i{ 0 }; i < solids->size(); i++)
			{
				if (align(m_rect, solids->at(i)->getRect(), m_hspd / abs(m_hspd), 0))
				{
					m_hspd = 0;
					cleanup(hazards); // safely deletes self and removes from groups
				}
				m_x = m_rect.x;
			}
			m_y += m_vspd;
			m_rect = { m_x, m_y, 16, 16 };
			// performs vertical collision checks/allignments
			for (int i{ 0 }; i < solids->size(); i++)
			{
				if (align(m_rect, solids->at(i)->getRect(), 0, m_vspd / abs(m_vspd)))
				{
					m_vspd = 0;
					cleanup(hazards); // safely deletes self and removes from groups
				}
				m_y = m_rect.y;
			}
			if (m_y > static_cast<int>(level.size() * 32)) // if outside level range
				cleanup(hazards); // safely deletes self and removes from groups
			if (m_x > static_cast<int>(level.at(0).size()) * 32 || m_x < 0)
				cleanup(hazards);
			if (p->v_x + m_x - p->getx() < -8 || p->v_x + m_x - p->getx() > 648)
				cleanup(hazards);
			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 16, 16 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, SDL_FLIP_NONE);
		}
	}
	void cleanup(std::vector<Object*> *hazards)
	{
		m_exists = false;
		Object *ptr = static_cast<Object*>(this);
		if (getIndex(hazards, ptr) != -1)
			hazards->erase(hazards->begin() + getIndex(hazards, ptr));
	}
};
std::vector<SDL_Texture*> Snowball::m_imageSet{ 0 };


// plant enemy that fires three spores
class Plant : public Object
{
private:
	int m_timerBase{ -1 };
	std::vector<Object*> m_spores;
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Plant(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 32, 32, false, false, false, false)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (!m_exists) // this corrects enemy behaviour which doesn't apply to the plant
			m_exists = true;

		if (m_timerBase == -1) // sets a reference to time from
			m_timerBase = g_count;

		if ((g_count - m_timerBase) % 150 == 0) // every 150 frames
		{
			// create three spores and store them
			Object* spore1 = new Spore(m_x, m_y, -3, -10, ren, hazards);
			Object* spore2 = new Spore(m_x, m_y, 0, -10, ren, hazards);
			Object* spore3 = new Spore(m_x, m_y, 3, -10, ren, hazards);
			m_spores.push_back(spore1);
			m_spores.push_back(spore2);
			m_spores.push_back(spore3);
		}
		// deals with protections for proper behaviour, don't stop updating until all spores have been cleaned up
		if (m_spores.size() != 0)
			m_protected = true;
		else
			m_protected = false;

		for (int i{ 0 }; i < m_spores.size(); i++) // loop through spores
		{
			m_spores[i]->update(ren, level, p, solids, hazards); // update spore
			if (!m_spores[i]->m_exists) // if spore destroyed
			{
				delete m_spores[i]; // clear from memory
				m_spores.erase(m_spores.begin() + i--); // remove from array
			}
		}
		SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
		SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, SDL_FLIP_NONE);
	}
	virtual void reset()
	{
		m_exists = true;
		m_timerBase = -1;
		for (Object *spore : m_spores)
			delete spore;
		m_spores.clear();
	}
	~Plant()
	{
		for (int i{ 0 }; i < m_spores.size(); i++)
		{
			delete m_spores[i];
		}
	}
};
std::vector<SDL_Texture*> Plant::m_imageSet{ 0 };


// plant enemy that fires spores at the player and hides when approached
class Spit : public Object
{
private:
	int m_timerBase{ -1 };
	int m_shake{ -1 };
	std::vector<Object*> m_spores;
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Spit(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 32, 32, false, false, false, false)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (!m_exists) // corrects unwanted grouped enemy behaviour
			m_exists = true;
		SDL_RendererFlip flip{ SDL_FLIP_NONE };
		double pdist = sqrt(pow(m_x - p->getx() - 16, 2) + pow(m_y - p->gety(), 2)); // gets distance to player
		if (pdist < 272 && pdist > 64) // if in range
		{
			if (m_shake == 5) // if finished shaking
			{
				if (m_timerBase == -1)
					m_timerBase = g_count + 5; // set timing reference point
				m_frame = 1; // stand up
				flip = static_cast<SDL_RendererFlip>((p->getx() > m_x)); // make sprite face player
				if ((g_count - m_timerBase) % 40 == 0) // shoot spore at player
				{
					// implementation of the trajectory equation
					double x = m_x - p->getx() - 16;
					double y = m_y - p->gety() - 16;
					double dir = atan((pow(10, 2) + sqrt(pow(10, 4) - 0.3*(0.3*pow(x, 2) + 2 * y*pow(10, 2)))) / (0.3 * x));
					if (!isnan(dir))
					{
						Object* spore = new Spore(m_x, m_y, -(x / abs(x)) * 10 * cos(dir), -(x / abs(x)) * 10 * sin(dir), ren, hazards);
						m_spores.push_back(spore);
					}
				}
			}
			else if (g_count % 2 == 0) // if not finished shaking shake every 2 frames
				m_shake += 1;
		}
		else // if not in range hide again
		{
			m_timerBase = -1;
			m_frame = 0;
			m_shake = -1;
		}
		if (m_spores.size() != 0) // protected behaviour for spores
			m_protected = true;
		else
			m_protected = false;
		for (int i{ 0 }; i < m_spores.size(); i++) // spore updating
		{
			m_spores[i]->update(ren, level, p, solids, hazards);
			if (!m_spores[i]->m_exists)
			{
				delete m_spores[i];
				m_spores.erase(m_spores.begin() + i--);
			}
		}
		int shake{ 0 };
		if (m_shake != 10 && m_shake != -1)
			shake = -1 + 2 * (m_shake % 2 == 0);
		SDL_Rect vrect{ p->v_x + m_x - p->getx() + shake, p->v_y + m_y - p->gety(), 32, 32 };
			
		SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, flip);
	}
	virtual void reset()
	{
		m_exists = true;
		m_timerBase = -1;
		for (int i{ 0 }; i < m_spores.size(); i++)
		{
			delete m_spores[i];
		}
		m_spores.clear();
	}
	~Spit()
	{
		for (int i{ 0 }; i < m_spores.size(); i++)
		{
			delete m_spores[i];
		}
	}
};
std::vector<SDL_Texture*> Spit::m_imageSet{ 0 };


// *WORLD 2* yeti enemy
class Yeti : public Object
{
private:
	int m_timerBase{ -1 };
	std::vector<Object*> m_snowballs;
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Yeti(int x, int y, SDL_Renderer *ren) :
		Object(x, y, 32, 32, false, true, true, false)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		for (int i{ 0 }; i < m_snowballs.size(); i++) // spore updating
		{
			m_snowballs[i]->update(ren, level, p, solids, hazards);
			if (!m_snowballs[i]->m_exists)
			{
				delete m_snowballs[i];
				m_snowballs.erase(m_snowballs.begin() + i--);
			}
		}
		if (m_exists)
		{
			SDL_RendererFlip flip{ SDL_FLIP_NONE };
			double pdist = sqrt(pow(m_x - p->getx() - 16, 2) + pow(m_y - p->gety(), 2)); // gets distance to player
			if (pdist < 272) // if in range
			{
				if (m_timerBase == -1)
					m_timerBase = g_count; // set timing reference point
				flip = static_cast<SDL_RendererFlip>((p->getx() < m_x)); // make sprite face player
				if ((g_count - m_timerBase) % 100 == 0) // shoot snowball at player
				{
					double dir = atan2(m_y - p->gety(), m_x - p->getx());
					Object* snowball = new Snowball(m_x, m_y, -8 * cos(dir), -8 * sin(dir), ren, hazards);
					m_snowballs.push_back(snowball);
				}
			}
			else
				m_timerBase = -1;
			if (m_snowballs.size() != 0)
				m_protected = true;
			else
				m_protected = false;
			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 32, 32 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, flip);
		}
	}
	virtual void reset()
	{
		m_exists = true;
		m_timerBase = -1;
		for (int i{ 0 }; i < m_snowballs.size(); i++)
			delete m_snowballs[i];
		m_snowballs.clear();
	}
	virtual void resetStrong()
	{
		reset();
	}
	~Yeti()
	{
		for (int i{ 0 }; i < m_snowballs.size(); i++)
			delete m_snowballs[i];
	}
};
std::vector<SDL_Texture*> Yeti::m_imageSet{ 0 };


// mushroom that player can bounce on
class Mushroom : public Object
{
private:
	int m_timerBase{ -1 };
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Mushroom(int x, int y, SDL_Renderer *ren) :
		Object(x, y + 4, 32, 28, false, false, true, false)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (!m_exists) // if player bounced on it
		{
			m_exists = true; // make it exist again
			m_frame = 1; // set to squished sprite
			m_timerBase = g_count; // start counting
		}
		if (m_exists)
		{
			if (m_timerBase != -1 && g_count - m_timerBase == 10) // if 10 frames have passed while counting
			{
				m_frame = 0; // return to normal sprite
				m_timerBase = -1; // stop counting
			}
			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety() - 4, 32, 32 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, SDL_FLIP_NONE); // draw self
		}
	}
};
std::vector<SDL_Texture*> Mushroom::m_imageSet{ 0 };


// gem that gives 100 score
class Gem100 : public Object
{
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Gem100(int x, int y, SDL_Renderer *ren) :
		Object(x + 8, y + 8, 16, 16, false, false, false, true)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (m_exists)
		{
			if (g_count % 10 == 0)
				m_frame = (m_frame + 1) % 2; // switch sprites every 10 frames
			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 16, 16 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, SDL_FLIP_NONE);
		}
	}
	virtual void reset() override // don't do anything on reset
	{}
	virtual void resetStrong() override // restore gem on strong reset
	{
		m_exists = true;
	}
	virtual void action()
	{
		g_score += 100; // give 100 points when picked up
	}
};
std::vector<SDL_Texture*> Gem100::m_imageSet{ 0 };


// gem that gives 1 life
class GemL : public Object
{
public:
	static std::vector<SDL_Texture*>m_imageSet;
	GemL(int x, int y, SDL_Renderer *ren) :
		Object(x + 8, y + 8, 16, 16, false, false, false, true)
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (m_exists)
		{
			if (g_count % 10 == 0) // rotation animation
				m_frame = (m_frame + 1) % 2;
			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety(), 16, 16 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, SDL_FLIP_NONE);
		}
	}
	virtual void reset() override
	{}
	virtual void resetStrong() override
	{
		m_exists = true;
	}
	virtual void action()
	{
		g_lives += 1;
	}
};
std::vector<SDL_Texture*> GemL::m_imageSet{ 0 };


// *WORLD 2* mammoth enemy
class Mammoth : public Object
{
private:
	double m_hspd{ 1 };
	bool m_flip{ 0 };
public:
	static std::vector<SDL_Texture*>m_imageSet;
	Mammoth(int x, int y, SDL_Renderer *ren) :
		Object(x, y + 18, 64, 44, false, true, true, false) // 16, 32
	{}
	virtual void update(SDL_Renderer *ren, std::vector<std::vector<Object*>> &level, Player *p, std::vector<Object*> *solids, std::vector<Object*> *hazards) override
	{
		if (!m_exists)
			m_exists = true;
		if (m_exists)
		{
			if (g_count % 10 == 0)
				m_frame = (m_frame + 1) % 2; // advance animation to next frame
			m_x += m_hspd; // move forward
			m_rect = { m_x, m_y, m_rect.w, m_rect.h}; // update collision rect
			int start = m_hspd;
			for (int i{ 0 }; i < solids->size(); i++) // check for horizontal collisions
			{
				if (align(m_rect, solids->at(i)->getRect(), m_hspd / abs(m_hspd), 0)) // if a collision found
					m_hspd = -start; // reverse direction
				m_x = m_rect.x;
			}
			for (int i{ 0 }; i < hazards->size(); i++) // check for horizontal collisions
			{
				if (hazards->at(i) == this)
					continue;
				if (align(m_rect, hazards->at(i)->getRect(), m_hspd / abs(m_hspd), 0)) // if a collision found
					m_hspd = -start; // reverse direction
				m_x = m_rect.x;
			}
			// get the two objects on either side and below the mammoth
			Object *adjacent1 = level[(m_y + 16) / 32 + 1][(m_x - 1 + 32 + 32 * m_hspd / abs(m_hspd)) / 32];
			Object *adjacent2 = level[(m_y + 16) / 32 + 1][(m_x + 1 - 32 * m_hspd / abs(m_hspd)) / 32];
			if (adjacent1 == nullptr) // if empty space on one side
			{
				if (adjacent2 != nullptr)
					m_hspd *= -1; // reverse direction
			}
			else if (!adjacent1->m_solid || adjacent1->m_hazard) // if both non solid or hazards
			{
				if (adjacent2 != nullptr && (adjacent2->m_solid || !adjacent2->m_hazard))
					m_hspd *= -1; // reverse direction
			}
			m_flip = (m_hspd < 0);
			// protects the mammoth if still on screen
			if (abs(m_x - p->getx() - (320 - p->v_x)) < (viewRangeH + 2) * 32 && abs(m_y - p->gety() - (320 - p->v_y)) < viewRangeV * 32)
				m_protected = true;
			else
				m_protected = false;

			SDL_Rect vrect{ p->v_x + m_x - p->getx(), p->v_y + m_y - p->gety() - 2, 64, 48 };
			SDL_RenderCopyEx(ren, m_imageSet[m_frame], NULL, &vrect, 0, NULL, static_cast<SDL_RendererFlip>(m_flip));
		}
		else
			m_protected = false;
	}
	virtual void action()
	{
		g_score += 50;
	}
};
std::vector<SDL_Texture*> Mammoth::m_imageSet{ 0 };






// ------------------------------MAIN FUNCTIONS------------------------------

// add an object into the correct groups based on its properties
void groupInstance(Object* ptr, std::vector<Object*>& instances, std::vector<Object*>& solids, std::vector<Object*>& hazards, std::vector<Object*>& enemies, std::vector<Object*>& collectibles)
{
	instances.push_back(ptr); // push onto instances
	if (ptr->m_solid)
		solids.push_back(ptr);
	if (ptr->m_hazard)
		hazards.push_back(ptr);
	if (ptr->m_enemy)
		enemies.push_back(ptr);
	if (ptr->m_collectible)
		collectibles.push_back(ptr);
}


// This function is called on level start. It contains the main game loop.
int play(std::vector<std::vector<Object*>> &level, TTF_Font *font, SDL_Renderer *ren, Mix_Chunk* music, bool weather, int tileSet)
{
	// TODO: there are quite a few files loaded in this function, leftover from when this was the main game loop. These are now being loaded
	// and unloaded each time a new level is presented, which is a waste of time. Pull these out and pass them into the function so all loading
	// is done at the start of the program.


	// ---------------PREP FOR LEVEL START---------------
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
	SDL_RenderClear(ren);
	SDL_Event e;
	std::vector<Object*> instances;
	std::vector<Object*> solids;
	std::vector<Object*> hazards;
	std::vector<Object*> enemies;
	std::vector<Object*> collectibles;
	std::vector<Object*> protQueue;
	int startScore{ g_score };
	int bWidth{ 2 };
	const SDL_Rect hud1Rect{ 0, 0, screenw, 64 };
	const SDL_Rect hud2Rect{ 0, 0, screenw, 64 - bWidth };
	SDL_Texture* scoreText{ SDL_CreateTexture(ren, g_format, SDL_TEXTUREACCESS_STREAMING, 250, 36) };
	SDL_Texture* livesText{ SDL_CreateTexture(ren, g_format, SDL_TEXTUREACCESS_STREAMING, 150, 36) };
	SDL_SetTextureBlendMode(rain[0], SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(rain[1], SDL_BLENDMODE_BLEND);
	int lastScore{ -1 };
	int lastLives{ -1 };
	int lastgridx(screenw / 64);
	int lastgridy(level.size() - screenh / 64);
	bool running = true;
	bool first = true;
	static Mix_Chunk* newLife = Mix_LoadWAV("sound/newlife.wav");
	Mix_VolumeChunk(newLife, MIX_MAX_VOLUME / 2);
	static Mix_Chunk* death = Mix_LoadWAV("sound/death.wav");
	Mix_VolumeChunk(death, MIX_MAX_VOLUME / 2);
	static Mix_Chunk* thunder = Mix_LoadWAV("sound/thunder.wav");
	Mix_VolumeChunk(thunder, MIX_MAX_VOLUME / 4);
	Mix_HaltChannel(-1);
	
	// find tallest block in 2nd column
	int y = 0;
	for (int i(level.size() - 1); i > 0; i--)
	{
		if (level.at(i).at(2) != nullptr)
		{
			if (!level.at(i).at(2)->m_solid)
			{
				y = i;
				break;
			}
		}
		else
		{
			y = i;
			break;
		}
	}
	// start the player on top of this tallest block
	Player player{ 64, y * 32 };


	// ---------------LEVEL START SCREEN---------------
	std::string lives2String{ "x " };
	lives2String += std::to_string(g_lives);

	// display life count
	SDL_Texture *lives2Text = SDL_CreateTextureFromSurface(ren, TTF_RenderText_Shaded(font, lives2String.c_str(), { 255, 255, 255 }, { 0, 0, 0 }));
	SDL_Rect lives2Rect{ screenw / 2 - 20, screenh / 2 + 16, 20 * lives2String.length() - 15, 36 };
	SDL_RenderCopy(ren, lives2Text, NULL, &lives2Rect);
	SDL_DestroyTexture(lives2Text);

	// display player sprite
	SDL_Rect iconRect{ screenw / 2 - 64, screenh / 2 + 16, 32, 32 };
	SDL_RenderCopy(ren, SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/player.png")), NULL, &iconRect);

	SDL_RenderPresent(ren);

	// play new life music and wait
	Mix_PlayChannel(0, newLife, -1);
	for (int i{ 0 }; i < 160; i++)
	{
		SDL_PollEvent(&e);
		SDL_Delay(10);
	}

	// begin playing the level's music
	Mix_FadeInChannel(0, music, -1, 1000);

	// ---------------------------------------------MAIN GAME LOOP---------------------------------------------
	while (running)
	{
		while (SDL_PollEvent(&e)) // get events
		{
			if (e.type == SDL_QUIT) // end the program if quit clicked
			{
				g_lives = -2;
				return 0;
			}
		}

		int newgridx{ lastgridx };
		int newgridy{ lastgridy };

		if (!first)
		{
			if (player.v_x == screenw / 2 || newgridx < 0)
				newgridx = floor(player.getx() / 32);
			if (player.v_y == screenh / 2 + 64 || newgridy < 0)
				newgridy = floor(player.gety() / 32);
		}

		// instance management
		for (Object *instance : instances) // fill protected queue
			if (instance->m_protected && getIndex(&protQueue, instance) == -1) // if protected and not in queue
			{
				protQueue.push_back(instance);
			}

		if (newgridx != lastgridx || newgridy != lastgridy || first) // if player has moved a grid square, reload the active instances
		{
			// store a copy of previous instances
			std::vector<Object*> lastInstances;
			lastInstances.swap(instances);
			// empty previous active instances
			instances.clear(); 
			solids.clear();
			hazards.clear();
			enemies.clear();
			collectibles.clear();
			for (int y{ -viewRangeV }; y < viewRangeV + 1; y++) // loop through the region with size viewRange about the new grid coordinate
			{
				if (newgridy + y > level.size() - 1 || newgridy + y < 0) // range check to avoid errors
					continue;
				for (int x{ -viewRangeH }; x < viewRangeH + 1; x++)
				{
					if (newgridx + x > level.at(newgridy + y).size() - 1 || newgridx + x < 0) // second range check
						continue;
					Object *ptr = level[newgridy + y][newgridx + x]; // retrieve object from level array
					if (ptr) // if an instance found
					{
						groupInstance(ptr, instances, solids, hazards, enemies, collectibles); // sort the object into its groups
					}
				}
			}
			for (Object *ptr : protQueue) // for protected instances
			{
				if (getIndex(&instances, ptr) == -1) // if it wasn't loaded in
				{
					groupInstance(ptr, instances, solids, hazards, enemies, collectibles); // add it to the active instances
				}
			}
			for (Object *instance : lastInstances) // for instances in the last region
				if (getIndex(&instances, instance) == -1) // if no longer in this region
					instance->reset(); // reset them to perform normally if reloaded
		}

		// protected queue cleanup
		for (int i{ 0 }; i < protQueue.size(); i++) // for every protected instance
			if (!protQueue[i]->m_protected) // if no longer protected
			{
				int igridx{ protQueue[i]->m_startx / 32 };
				int igridy{ protQueue[i]->m_starty / 32 }; 
				// check if should be loaded
				if (igridx < newgridx - viewRangeH - 1 || igridx > newgridx + viewRangeH || igridy < newgridy - viewRangeV || igridy > newgridy + viewRangeV)
				{
					// if not remove it from vectors + cleanup
					protQueue[i]->reset(); 
					instances.erase(instances.begin() + getIndex(&instances, protQueue[i]));
					if (protQueue[i]->m_solid)
						solids.erase(solids.begin() + getIndex(&solids, protQueue[i]));
					if (protQueue[i]->m_hazard)
						hazards.erase(hazards.begin() + getIndex(&hazards, protQueue[i]));
					if (protQueue[i]->m_enemy)
						enemies.erase(enemies.begin() + getIndex(&enemies, protQueue[i]));
					if (protQueue[i]->m_collectible)
						collectibles.erase(collectibles.begin() + getIndex(&collectibles, protQueue[i]));
				}
				protQueue.erase(protQueue.begin() + i--); // erase it from the queue
			}

		if (first) first = false;
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
		
		// update the player and store the result
		int result{ player.update(ren, &solids, &hazards, &enemies, &collectibles) };

		// draw background layers
		SDL_Rect bgrect{ -320 * floor(player.getx() - player.v_x) / (g_levelW * 32), 0, 960, 480 };
		SDL_RenderCopyEx(ren, backgrounds[((g_count/120) % 2 == 0)], NULL, &bgrect, 0, NULL, SDL_FLIP_NONE);

		SDL_Rect fgrect{ -640 * floor(player.getx() - player.v_x) / (g_levelW * 32), 0, 1920, 480 };
		SDL_RenderCopyEx(ren, backgrounds[2], NULL, &fgrect, 0, NULL, SDL_FLIP_NONE);

		// update non-solids
		for (int i{ 0 }; i < instances.size(); i++)
		{
			if (!instances.at(i)->m_solid)
				instances.at(i)->update(ren, level, &player, &solids, &hazards);
		}

		// draw the player
		player.draw(ren);

		// update solids
		for (int i{ 0 }; i < instances.size(); i++)
		{
			if (instances.at(i)->m_solid)
				instances.at(i)->update(ren, level, &player, &solids, &hazards);
		}

		lastgridx = newgridx;
		lastgridy = newgridy;

		// draw weather effects
		if (weather == 1) // rain
		{
			// draws rain images translated to give scrolling effect, the second covering areas missed by the first
			SDL_Rect rainrect1{ -(player.getx() - player.v_x) % 640, 0, 640, 480 };
			SDL_Rect rainrect2{ 640 - (player.getx() - player.v_x) % 640, 0, 640, 480 };
			SDL_RenderCopyEx(ren, rain[g_count / 10 % 2], NULL, &rainrect1, 0, NULL, SDL_FLIP_NONE);
			SDL_RenderCopyEx(ren, rain[g_count / 10 % 2], NULL, &rainrect2, 0, NULL, SDL_FLIP_NONE);
			if (rand() % 200 == 0) // 1/200 chance every frame to flash lightning
			{
				SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
				SDL_Rect fill{ 0, 0, 640, 480 }; 
				SDL_RenderFillRect(ren, &fill); // fill screen white for the flash
				Mix_PlayChannel(-1, thunder, 0); // play thunder sound effect
			}
		}

		// draw GUI borders at the top of the screen
		SDL_SetRenderDrawColor(ren, 255, 255, 255, 255); // draw in white
		SDL_RenderFillRect(ren, &hud1Rect); // draw box on top of screen
		SDL_SetRenderDrawColor(ren, 0, 0, 0, 255); // draw in black
		SDL_RenderFillRect(ren, &hud2Rect); // fill in majority of the first box

		if (g_score != lastScore) // if score has changed
		{
			std::string scoreString{ "SCORE  " }; // construct string and make a texture
			while (scoreString.length() < 14 - getDigits(g_score))
				scoreString += '0';
			scoreString += std::to_string(g_score);
			stringTexture(font, scoreString, scoreText);
		}
		if (g_lives != lastLives) // if lives has changed
		{
			std::string livesString{ "LIVES  " }; // construct string and make a texture
			while (livesString.length() < 9 - getDigits(g_lives))
				livesString += '0';
			livesString += std::to_string(g_lives);
			stringTexture(font, livesString, livesText);
		}

		// draw the score and lives strings
		SDL_Rect scoreRect{ 40, 10, 240, 36 };
		SDL_RenderCopy(ren, scoreText, NULL, &scoreRect);
		SDL_Rect livesRect{ 450, 10, 140, 36 };
		SDL_RenderCopy(ren, livesText, NULL, &livesRect);

		lastScore = g_score;
		lastLives = g_lives;

		SDL_RenderPresent(ren);
		SDL_PumpEvents();
		g_count++;
		
		// if player has died
		if (result == -1)
		{
			// subtract a life
			g_lives -= 1;
			// stop the music
			Mix_HaltChannel(-1);
			// play the death music
			Mix_PlayChannel(0, death, -1);
			// draw death animation
			for (int i{ 0 }; i < 200; i++)
			{
				if (i > 100)
				{
					int diameter = 1440 / pow(100, 6) * pow((200 - i), 6); // width of circle to draw about the player
					SDL_Rect destRect{ player.v_x - (diameter - 32) / 2, player.v_y - (diameter - 32) / 2, diameter, diameter }; // circle rect
					// four black rects that make up the rest of the animation
					SDL_Rect rect1{ 0, 0, player.v_x - (diameter - 32) / 2, 480 };
					SDL_Rect rect2{ player.v_x + 32 + (diameter - 32) / 2, 0, 688 - player.v_x,  480 };
					SDL_Rect rect3{ player.v_x - (diameter - 32) / 2, 0, diameter, player.v_y - (diameter - 32) / 2 };
					SDL_Rect rect4{ player.v_x - (diameter - 32) / 2, player.v_y + 32 + (diameter - 32) / 2, diameter, 454 - player.v_y };

					SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
					SDL_RenderFillRect(ren, &rect1);
					SDL_RenderFillRect(ren, &rect2);
					SDL_RenderFillRect(ren, &rect3);
					SDL_RenderFillRect(ren, &rect4);
					SDL_RenderCopy(ren, zoom, NULL, &destRect);
				}
				SDL_RenderPresent(ren);

				SDL_PollEvent(&e);
				SDL_Delay(10);
			}
			g_score = startScore;
			break;
		}
		// if player has quit the game
		if (result == -2)
		{
			g_lives = -3;
			return -1;
		}
		// if player has beaten the level
		if (result == 1)
			return 1;
		SDL_Delay(10);
	}

	// strongly reset all objects before level is restarted
	for (std::vector<Object*> row : level)
		for (Object *instance : row)
			if (instance != nullptr)
				instance->resetStrong();

	SDL_DestroyTexture(scoreText);
	SDL_DestroyTexture(livesText);
	return 0;
}


// This function is called on program start. It manages the start screen and level loading.
int main(int, char**)
{
	// ------------------------------SETUP------------------------------
	srand(time(0));
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	SDL_Window *win{ SDL_CreateWindow("Dino", 50, 50, screenw, screenh + 64, SDL_WINDOW_SHOWN) };// | SDL_WINDOW_FULLSCREEN_DESKTOP)
	SDL_Renderer *ren{ SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) };
	TTF_Init();
	IMG_Init(IMG_INIT_PNG);
	Mix_Init(MIX_INIT_FLAC);
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 1, 1024);
	g_format = SDL_GetWindowPixelFormat(win);
	int SDL_EnableKeyRepeat(2);
	TTF_Font *font = TTF_OpenFont("arcadeclassic/ARCADECLASSIC.ttf", 36);
	std::vector<std::vector<int>> preLevel;
	std::vector<std::vector<Object*>> level;
	int levelNum{ 0 };
	bool first{ true };
	bool weather{ false };
	int track{ 0 };
	int tileSet{ 0 };

	// ------------------------------LOADING IMAGES------------------------------
	Player::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/player.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/player1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/player2.png"))
	};
	Wall::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/wall1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/top1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/left1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/bottom1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/right1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/wall2.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/top2.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/left2.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/bottom2.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/right2.png"))
	};
	Water::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/water1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/water2.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/water3.png"))
	};
	Thorns::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/thorns.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/icicle.png"))
	};
	Ice::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/iceTop.png"))
	};
	ThinIce::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/iceThin1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/iceThin2.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/iceThin3.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/iceThin4.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/water1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/water2.png"))
	};
	Tree::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/tree1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/tree2.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/tree3.png"))
	};
	Flower::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/flower1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/flower2.png"))
	};
	Snake::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/snake1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/snake2.png"))
	};
	Ptero::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/ptero1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/ptero2.png"))
	};
	Frog::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/frog1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/frog2.png"))
	};
	Spore::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/spore.png"))
	};
	Snowball::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/snowball.png"))
	};
	Plant::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/plant1.png"))
	};
	Spit::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/spit1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/spit2.png"))
	};
	Yeti::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/yeti.png"))
	};
	Gem100::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/gem1001.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/gem1002.png"))
	};
	GemL::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/gemL1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/gemL2.png"))
	};
	Mushroom::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/mushroom1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/mushroom2.png"))
	};
	Mammoth::m_imageSet = {
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/mammoth1.png")),
		SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/mammoth2.png"))
	};
	backgrounds.push_back(SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/background11.png")));
	backgrounds.push_back(SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/background12.png")));
	backgrounds.push_back(SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/foreground1.png")));
	backgrounds.push_back(SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/background2.png")));
	backgrounds.push_back(SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/foreground2.png")));
	rain.push_back(SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/rain1.tga")));
	rain.push_back(SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/rain2.tga")));
	SDL_Surface *zoomSurface{ IMG_Load("sprites/zoom.png") };
	SDL_SetSurfaceBlendMode(zoomSurface, SDL_BLENDMODE_MOD);
	zoom = SDL_CreateTextureFromSurface(ren, zoomSurface);
	SDL_Texture *start{ SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/startScreen.png")) };
	SDL_Texture *border{ SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/border.png")) };
	std::vector<SDL_Texture*> startButton{ SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/start1.png")), SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/start2.png")) };
	std::vector<SDL_Texture*> exitButton{ SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/exit1.png")), SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/exit2.png")) };
	SDL_Texture *demo{ SDL_CreateTextureFromSurface(ren, IMG_Load("sprites/demo.png")) };

	// ------------------------------LOADING SOUNDS------------------------------
	std::vector<Mix_Chunk*> music{ Mix_LoadWAV("sound/music/journey's start.wav"), Mix_LoadWAV("sound/music/raindrop march.wav") };
	Player::m_sounds =
	{
		Mix_LoadWAV("sound/gem.wav"),
		Mix_LoadWAV("sound/jump.wav")
	};
	for (Mix_Chunk *sound : Player::m_sounds)
		Mix_VolumeChunk(sound, MIX_MAX_VOLUME/2);
	for (Mix_Chunk *track : music)
	{
		Mix_VolumeChunk(track, MIX_MAX_VOLUME / 4);
	}

	// ------------------------------MAIN MENU------------------------------
	SDL_Rect bgrect = { 100, 0, 320, 240 };
	SDL_Rect titleRect = { 0, 0, 640, 480 };
	SDL_Rect startRect = { 165, 240, 310, 34 };
	SDL_Rect exitRect = { 181, 290, 278, 34 };
	SDL_Rect playerRect = { 304, 416, 32, 32 };
	SDL_Rect hiscoreBorder = { 168, 68, 304, 304 };
	SDL_Rect mouseRect = { 0, 0, 1, 1 };
	SDL_Rect hiscoreRect = { 170, 70, 300, 300 };
	SDL_Texture* hiscores{ SDL_CreateTexture(ren, g_format, SDL_TEXTUREACCESS_STREAMING, 216, 216) };
	
	// play menu music
	Mix_HaltChannel(-1);
	Mix_FadeInChannel(0, music[0], -1, 1000);
	
	// dummy player variables
	bool flip{ false };
	int walkCount{ 0 };
	int dir{ 1 };
	int frame{ 0 };

	// main menu loop
	bool running{ true };
	while (running)
	{
		SDL_RenderCopy(ren, backgrounds[((g_count++) / 120 % 2 == 0)], &bgrect, NULL); // draw background
		SDL_RenderCopy(ren, backgrounds[2], &bgrect, NULL); // draw foreground
		if ((g_count + 23) / 22 % 20 == 0) // make title bounce
			titleRect.y += (g_count) % 11 - 5;
		// make player sprite walk back and forth
		if (g_count % 120 == 0) 
		{
			walkCount += rand() % 40 + 20; // walk a random distance
			dir = pow(-1, rand() % 2); // in a random direction
			frame = 1;
			// make sprite face correct way
			if (dir == -1)
				flip = true;
			else
				flip = false;
		}
		if (walkCount > 0) // if walking
		{
			walkCount -= 1;
			playerRect.x += dir; // update pos
			if (g_count % 6 == 0) // animate
				frame = frame % 2 + 1;
		}
		else
			frame = 0;

		// draw menu elements
		SDL_GetMouseState(&mouseRect.x, &mouseRect.y);
		SDL_RenderCopy(ren, start, NULL, &titleRect);
		SDL_RenderCopy(ren, startButton[(collided(mouseRect, startRect))], NULL, &startRect);
		SDL_RenderCopy(ren, exitButton[(collided(mouseRect, exitRect))], NULL, &exitRect);
		SDL_RenderCopyEx(ren, Player::m_imageSet[frame], NULL, &playerRect, NULL, NULL, static_cast<SDL_RendererFlip>(flip));
		SDL_RenderCopy(ren, demo, NULL, NULL);

		// loop through events
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			// if exit game clicked
			if (e.type == SDL_QUIT || e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && collided(mouseRect, exitRect))
			{
				running = false; // end the program
				break;
			}
			// if play game clicked, start main game setup
			if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN || e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && collided(mouseRect, startRect))
			{
				// game setup
				g_score = 0;
				g_lives = 3;
				levelNum = 0;
				first = true;

				// level control loop
				while (true)
				{
					while (g_lives >= 0)
						// play the level, if the player beats it then load the next, if not then play the same level.
						if (first == true || play(level, font, ren, music[track], weather, tileSet) == 1)
						{
							if (first)
								first = false;
							if (levelNum == 8) // if final level completed
							{
								g_lives = -3; // return to main menu
								break;
							}
							// construct file path for level to load
							std::string path{ "levels/level" };
							path += std::to_string(++levelNum);
							path += ".txt";
							// Load file into prelevel and get parameters. prelevel is an array of integers corresponding to the objects the
							// level is to be filled with
							loadLevel(&preLevel, &tileSet, &weather, &track, path); 
							g_levelH = preLevel.size(); // get level size
							g_levelW = preLevel.at(0).size();
							for (int y{ 0 }; y < level.size(); y++) // empty previous level vector
							{
								for (int x{ 0 }; x < level[y].size(); x++)
									delete level[y][x];
							}
							level.clear(); // reset level sizes
							for (int y{ 0 }; y < preLevel.size(); y++) // construct new level vector
							{
								level.push_back({});
								for (int x{ 0 }; x < preLevel.at(y).size(); x++)
								{
									Object* ptr{ 0 };
									// create the object indicated in prelevel at the correct position, and pass a pointer to it into the
									// level array
									switch (tileSet)
									{
									case 0:
										switch (preLevel.at(y).at(x))
										{
										case 1:
											ptr = new Wall(x * 32, y * 32, ren);
											ptr->setFrame(tileSet * 5);
											break;
										case 2:
											ptr = new Water(x * 32, y * 32, ren);
											break;
										case 3:
											ptr = new Thorns(x * 32, y * 32, ren);
											ptr->setFrame(tileSet);
											break;
										case 4:
											ptr = new Gem100(x * 32, y * 32, ren);
											break;
										case 5:
											ptr = new GemL(x * 32, y * 32, ren);
											break;
										case 6:
											ptr = new Snake(x * 32, y * 32, ren);
											break;
										case 7:
											ptr = new Ptero(x * 32, y * 32, ren);
											break;
										case 8:
											ptr = new Plant(x * 32, y * 32, ren);
											break;
										case 9:
											ptr = new Spit(x * 32, y * 32, ren);
											break;
										case 10:
											ptr = new Mushroom(x * 32, y * 32, ren);
											break;
										case 11:
											ptr = new Tree(x * 32, y * 32, ren);
											break;
										case 12:
											ptr = new Flower(x * 32, y * 32, ren);
											break;
										case 13:
											ptr = new Frog(x * 32, y * 32, ren);
											break;
										}
										level.at(y).push_back(ptr);
										break;
									case 1:
										switch (preLevel.at(y).at(x))
										{
										case 1:
											ptr = new Wall(x * 32, y * 32, ren);
											ptr->setFrame(tileSet * 5);
											break;
										case 2:
											ptr = new Water(x * 32, y * 32, ren);
											break;
										case 3:
											ptr = new Thorns(x * 32, y * 32, ren);
											ptr->setFrame(tileSet);
											break;
										case 4:
											ptr = new Gem100(x * 32, y * 32, ren);
											break;
										case 5:
											ptr = new GemL(x * 32, y * 32, ren);
											break;
										case 6:
											ptr = new Ice(x * 32, y * 32, ren);
											break;
										case 7:
											ptr = new ThinIce(x * 32, y * 32, ren);
											break;
										case 8:
											ptr = new Mammoth(x * 32, y * 32, ren);
											break;
										case 9:
											ptr = new Yeti(x * 32, y * 32, ren);
											break;
										}
										level.at(y).push_back(ptr);
										break;
									}
								}
							}
						}
					if (g_lives == -1) // on game over
					{
						g_score = 0; // reset score
						g_lives = 3; // reset lives
					}
					else if (g_lives == -2) // close clicked
					{
						running = false; // exit program
						break;
					}
					else if (g_lives == -3) // exit to main menu
					{
						playerRect = { 304, 416, 32, 32 };
						Mix_HaltChannel(-1);
						Mix_FadeInChannel(0, music[0], -1, 1000);
						break;
					}
				}
			}
		}
		SDL_RenderPresent(ren);
		SDL_Delay(10);
	}

	// prep for program end
	for (Mix_Chunk *track : music)
	{
		Mix_FreeChunk(track);
	}
	for (Mix_Chunk *sound : Player::m_sounds)
	{
		Mix_FreeChunk(sound);
	}
	Mix_CloseAudio();
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	TTF_Quit();
	IMG_Quit();
	Mix_Quit();
	SDL_Quit();

	return 0;
}
