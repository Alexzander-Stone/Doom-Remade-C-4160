#include <iostream>
#include <algorithm>
#include <sstream>
#include <string>
#include <random>
#include <iomanip>
#include "smartSprite.h"
#include "multisprite.h"
#include "gamedata.h"
#include "engine.h"
#include "frameGenerator.h"
#include "player.h"
#include "wallCollidable.h"
#include "collisionStrategy.h"
#include "vector2f.h"

Engine::~Engine() { 
  for( Drawable* drawings : sprites )
  {
      delete drawings;
  }
  for( CollisionStrategy* strategy : strategies )
  {
      delete strategy;
  }
  for( WallCollidable* collide : collidables )
  {
      delete collide;
  }
  std::cout << "Terminating program" << std::endl;
}

Engine::Engine() :
  rc( RenderContext::getInstance() ),
  io( IoMod::getInstance() ),
  clock( Clock::getInstance() ),
  renderer( rc->getRenderer() ),
  world("back", Gamedata::getInstance().getXmlInt("back/factor") ),
  viewport( Viewport::getInstance() ),
  sprites(),
  strategies(),
  collidables(),
  currentStrategy(0),
  collision( false ),
  makeVideo( false ),
  currentSprite(1),
  hud()
{
  // Objects that can collide with walls.
  collidables.reserve(10);
  collidables.push_back(new Player("DoomGuy"));

  // Enemies, attach to observor in collidables[0].
  Vector2f placeholderPlayerPos(50, 50);
  collidables.push_back( new Enemy("Pinkie", placeholderPlayerPos) );
  static_cast<Player*>(collidables[0])->attach( static_cast<Enemy*>( *(collidables.end() - 1) ) );

  // Wall sprites.
  int wallCount = Gamedata::getInstance().getXmlInt("Wall/numberOfWalls");
  sprites.reserve( wallCount*2 );

  int w = static_cast<Player*>(collidables[0])->getSpriteInfo()->getScaledWidth();
  int h = static_cast<Player*>(collidables[0])->getSpriteInfo()->getScaledHeight();
  // Boxed Arena Walls
  Vector2f spritePos(100, 500);
  Vector2f spritePos2(100, 500);
  Vector2f spritePos3(100, 1524);
  Vector2f spritePos4(1124, 500);
  sprites.push_back( new SmartSprite("Wall/Horizontal", placeholderPlayerPos, w, h, spritePos) );
  sprites.push_back( new SmartSprite("Wall/Vertical", placeholderPlayerPos, w, h, spritePos2) );
  sprites.push_back( new SmartSprite("Wall/Horizontal", placeholderPlayerPos, w, h, spritePos3) );
  sprites.push_back( new SmartSprite("Wall/Vertical", placeholderPlayerPos, w, h, spritePos4) );

  // Collision strategies ( rect, pixel, distance(midpoint) ).
  strategies.push_back( new RectangularCollisionStrategy );
  strategies.push_back( new PerPixelCollisionStrategy );
  strategies.push_back( new MidPointCollisionStrategy );

  Viewport::getInstance().setObjectToTrack(collidables[0]->getSpriteInfo());
  std::cout << "Loading complete" << std::endl;
}

void Engine::draw() const {
/*
  world.draw();

  // Draw all sprites in container.
  for( auto& it : sprites )
  {
    it->draw();
  }
  for( auto& it : collidables )
  {
    it->draw();
  }
  viewport.draw();
  if(hud.getActive() == true)
    hud.draw();
*/

  /* TODO: Raycasting, May want to create seperate class*/
  // Loop through all the vertical stripes of the collidables[0]'s view (x's) based on 
  // the screen width/height. This will calculate the rays using a grid system.
    for( int vertPixelX = 0; 
      	 vertPixelX < Gamedata::getInstance().getXmlInt("view/width"); 
	       vertPixelX++ 
    ) {
      // Current X-coor in camera (-1 to 1).
      float planeCoorX = dynamic_cast<Player*>(collidables[0])->getPlaneX(); 
      float planeCoorY = dynamic_cast<Player*>(collidables[0])->getPlaneY();
      float cameraX = 2 * vertPixelX / Gamedata::getInstance().getXmlFloat("view/width") - 1;
      float rayCoorX = dynamic_cast<Player*>(collidables[0])->getXFov() + planeCoorX * cameraX;
      float rayCoorY = dynamic_cast<Player*>(collidables[0])->getYFov() + planeCoorY * cameraX;

      // Lengths of the ray from collidables[0]X and playerY to first
      // increment of the ray (x and y), and from one ray coordinates 
      // step to the next.
      float planeRayX = dynamic_cast<Player*>(collidables[0])->getX();
      float planeRayY = dynamic_cast<Player*>(collidables[0])->getY();
      float lengthRayX = 0;
      float lengthRayY = 0;
      float incrementRayX = rayCoorX != 0?fabs(1/rayCoorX):0;
      float incrementRayY = rayCoorY != 0?fabs(1/rayCoorY):0;
      float wallDistance = 0;

      // Direction to move the ray's x and y coordinates when attempting 
      // to find a "hit" (1 or -1).
      float directionRayX = 1;
      float directionRayY = 1;

      // The value that the ray hit (Wall) and side that it hit.
      int rayHit = 0; 
      int side = 0;

      // Determine which way to send the increments. Negative values will head towards 
      // the left of the viewer's plane while positive values go right.
      if(rayCoorX < 0){
	      directionRayX = -1;
	      lengthRayX = 0;
      }
      else{
	      lengthRayX = incrementRayX;	
      }
      if(rayCoorY < 0){
	      directionRayY = -1;
	      lengthRayY = 0;
      }
      else{
	      lengthRayY = incrementRayY;	
      }

      // Loop DDA until wall has been hit. Increment a single planeRay coordinate until 
      // it reaches past the other coordinate. Can be used to determine what part of the tile
      // the ray has hit. Plane ray x/y are the length while the mapx
      Sprite raySprite("Ray");
      while (rayHit == 0)
      {
	      if(planeRayX < planeRayY){
	        lengthRayX += incrementRayX;
          planeRayX += directionRayX;
	        side = 0;
	      }
	      else{
	        lengthRayY += incrementRayY;
	        planeRayY += directionRayY;
	        side = 1;
	      }

	      // Check for collision with a wall object.
        // TODO: Replace with collision that doesn't rely on an image. 
        raySprite.setX(planeRayX);
        raySprite.setY(planeRayY);
        
        std::vector<SmartSprite*>::const_iterator spriteIt = sprites.begin();
        while( spriteIt != sprites.end() && rayHit == 0){
	        // Check for collision between collidables[0] and object.
          if( strategies[currentStrategy]->execute( raySprite, **spriteIt) ){
		        rayHit = 1; 
	        }
	        ++spriteIt;
	      }
      }


      // Find the total distance to the wall from the current vertPixelX.
      // This will be used to determine the length of the line drawn for the current vertPixelX.
      if(side == 0){
	      wallDistance = ( (1 - planeRayX) / 2 ) / rayCoorX;
      }
      else{
	      wallDistance = ( (1 - planeRayY) / 2 ) / rayCoorY;
      }	
      int vertLineLength = Gamedata::getInstance().getXmlInt("view/height") / wallDistance;

      // Find starting and ending pixel to draw to.
      int drawTop = -vertLineLength / 2 + Gamedata::getInstance().getXmlInt("view/height") / 2;
      if (drawTop < 0)
	      drawTop = 0;

      int drawBottom = -vertLineLength / 2 + Gamedata::getInstance().getXmlInt("view/height") / 2;
      if (drawBottom >= Gamedata::getInstance().getXmlInt("view/height"))
	      drawBottom = Gamedata::getInstance().getXmlInt("view/height") - 1;

      // Draw the line.
      SDL_SetRenderDrawColor(renderer, side==0?255:128, side==0?255:128, side==0?255:128, 255);
      SDL_RenderDrawLine(renderer, vertPixelX, drawTop, vertPixelX, drawBottom);
  SDL_RenderPresent(renderer);
      
      std::cout << "wall found from x and y : " << drawTop << " " << drawBottom << std::endl;

  }

  SDL_RenderPresent(renderer);
}

// Collision Detection.
void Engine::checkForCollisions(){
    std::vector<WallCollidable*>::iterator currItr = collidables.begin();
    while( currItr != collidables.end() ){
      // Check until all collisions have been removed.
	    // Search through all the sprites to determine if collision has occurred.
      bool collisionDetected = true;
      while( collisionDetected == true ) {

        // Check against the Walls in the level.
	      auto spriteIt = sprites.begin();
	      collisionDetected = false;
	      int currentSprite = 0;
	      while( spriteIt != sprites.end() ){
	        // Check for collision between collidables[0] and object.
	        if( strategies[currentStrategy]->execute( *(*currItr)->getSpriteInfo(), **spriteIt) ){
		        (*currItr)->getSpriteInfo()->attach( sprites[currentSprite] );
		        collisionDetected = true;
	        }
	        ++spriteIt;
	        ++currentSprite;
	      }
/*
        // Check against other collidable objects.
	      auto colIt = collidables.begin();
	      while( colIt != collidables.end() ){
	        // Check for collision between collidables[0] and object.
	        if( (*colIt)->getSpriteInfo() == (*currItr)->getSpriteInfo() )
		        ; // Ignore self image.
	        else if( strategies[currentStrategy]->execute( *(*currItr)->getSpriteInfo(), *(*colIt)->getSpriteInfo() ) ){
		        (*currItr)->getSpriteInfo()->attach( sprites[currentSprite] );
		        collisionDetected = true;
	        }
	        ++colIt;
	        ++currentSprite;
	      }
*/
	      if( collisionDetected == true){
	        (*currItr)->collisionDetected();
	      }
      }
      currItr++;
    }
}

void Engine::update(Uint32 ticks) {
  for(auto& it : collidables)
  {
    it->update(ticks);
  }
  checkForCollisions();
  world.update();
  viewport.update(); // always update viewport last
  hud.update(ticks);
}

void Engine::play() {
  SDL_Event event;
  const Uint8* keystate;
  bool done = false;
  Uint32 ticks = clock.getElapsedTicks();
  FrameGenerator frameGen;

  while ( !done ) {
    // The next loop polls for events, guarding against key bounce:
    while ( SDL_PollEvent(&event) ) {
      keystate = SDL_GetKeyboardState(NULL);
      if (event.type ==  SDL_QUIT) { done = true; break; }
      if(event.type == SDL_KEYDOWN) {
        if (keystate[SDL_SCANCODE_ESCAPE]) {
          done = true;
          break;
        }
        if ( keystate[SDL_SCANCODE_P] ) {
          if ( clock.isPaused() ) clock.unpause();
          else clock.pause();
        }
	if (keystate[SDL_SCANCODE_F1]) {
          std::cout << "Initiating hud" << std::endl;
          hud.toggleActive();
        }
        if (keystate[SDL_SCANCODE_F4] && !makeVideo) {
          std::cout << "Initiating frame capture" << std::endl;
          makeVideo = true;
        }
        else if (keystate[SDL_SCANCODE_F4] && makeVideo) {
          std::cout << "Terminating frame capture" << std::endl;
          makeVideo = false;
        }
      }
    }

    // In this section of the event loop we allow key bounce:

    ticks = clock.getElapsedTicks();
    if ( ticks > 0 ) {
      clock.incrFrame();
      if (keystate[SDL_SCANCODE_A]) {
        static_cast<Player*>(collidables[0])->left();
      }
      if (keystate[SDL_SCANCODE_D]) {
        static_cast<Player*>(collidables[0])->right();
      }
      if (keystate[SDL_SCANCODE_W]) {
        static_cast<Player*>(collidables[0])->up();
      }
      if (keystate[SDL_SCANCODE_S]) {
        static_cast<Player*>(collidables[0])->down();
      }
      if(keystate[SDL_SCANCODE_LEFT]){
        static_cast<Player*>(collidables[0])->rotateLeft();
      }
      if(keystate[SDL_SCANCODE_RIGHT]){
        static_cast<Player*>(collidables[0])->rotateRight();
      }
      
      update(ticks);
      draw();
      if ( makeVideo ) {
        frameGen.makeFrame();
      }
    }
  }
}
